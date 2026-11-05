#include "NamAmp.h"
#include <juce_events/juce_events.h>
#include <array>
#include <filesystem>

#include <NAM/get_dsp.h>
#include <NAM/dsp.h>
#include <NAM/slimmable.h>

namespace dr
{
namespace
{
struct Loaded
{
    std::unique_ptr<nam::DSP> dsp;
    RateBridge bridge;
    std::vector<float> scratch;
    double modelRate = 48000;
    bool hasLoudness = false;
    double loudness = 0;

    void run (float* x, int n)
    {
        bridge.process (x, n, [this] (float* d, int m)
        {
            const int maxM = (int) scratch.size();
            for (int off = 0; off < m; off += maxM)
            {
                const int c = std::min (maxM, m - off);
                NAM_SAMPLE* in[1]  = { d + off };
                NAM_SAMPLE* out[1] = { scratch.data() };
                dsp->process (in, out, c);
                std::copy (scratch.begin(), scratch.begin() + c, d + off);
            }
        });
    }
};
} // namespace

struct NamAmp::Impl
{
    explicit Impl (NamAmp& o) : owner (o) {}
    ~Impl()
    {
        pool.removeAllJobs (true, 5000);
        delete pending.exchange (nullptr);
        delete current; delete fading;
        collect();
    }

    void retire (Loaded* l)
    {
        if (l == nullptr) return;
        int s1, n1, s2, n2;
        trashFifo.prepareToWrite (1, s1, n1, s2, n2);
        if (n1 > 0) { trash[(size_t) s1] = l; trashFifo.finishedWrite (1); }
        // se la fifo fosse piena (impossibile con 16 slot) il modello viene "perso": meglio di un delete real-time
    }
    void collect()
    {
        int s1, n1, s2, n2;
        trashFifo.prepareToRead (trashFifo.getNumReady(), s1, n1, s2, n2);
        for (int i = 0; i < n1; ++i) delete trash[(size_t) (s1 + i)];
        for (int i = 0; i < n2; ++i) delete trash[(size_t) (s2 + i)];
        trashFifo.finishedRead (n1 + n2);
    }

    void startLoad (const juce::File& f)
    {
        const int gen = ++generation;
        { juce::ScopedLock sl (lock); path = f.getFullPathName(); info = {}; info.name = f.getFileNameWithoutExtension(); info.loading = prepared; }
        if (! prepared) return;                           // verrà caricato da prepare(), con il sample rate giusto
        const double dRate = devRate; const int mBlock = maxBlock; const bool lite = owner.quality->index() == 1;
        pool.addJob ([this, f, gen, dRate, mBlock, lite]
        {
            try
            {
                auto L = std::make_unique<Loaded>();
                L->dsp = nam::get_dsp (std::filesystem::path (f.getFullPathName().toStdString()));
                double rate = L->dsp->GetExpectedSampleRate();
                if (rate <= 0) rate = 48000.0;
                L->modelRate = rate;
                L->bridge.prepare (dRate, rate, mBlock);
                const int mm = L->bridge.maxModelBlock();
                L->scratch.assign ((size_t) mm, 0.0f);
                L->dsp->Reset (rate, mm);                        // prewarm incluso (qui, non nel thread audio)
                bool slim = false;
                if (auto* s = dynamic_cast<nam::SlimmableModel*> (L->dsp.get()))
                {
                    slim = true;
                    s->SetSlimmableSize (lite ? 0.0 : 1.0);      // A2-Lite / A2-Full
                    std::vector<float> z ((size_t) mm, 0.0f);    // un blocco a vuoto installa il modello "slim"
                    NAM_SAMPLE* io[1] = { z.data() };
                    L->dsp->process (io, io, mm);
                }
                if (L->dsp->HasLoudness()) { L->hasLoudness = true; L->loudness = L->dsp->GetLoudness(); }
                if (gen != generation.load()) return;            // nel frattempo richiesto un altro file
                {
                    juce::ScopedLock sl (lock);
                    info.loaded = true; info.loading = false; info.slimmable = slim; info.modelRate = rate;
                }
                delete pending.exchange (L.release());
            }
            catch (const std::exception& e)
            {
                juce::ScopedLock sl (lock);
                info.loading = false; info.loaded = false; info.error = juce::String (e.what()).substring (0, 120);
            }
        });
    }

    NamAmp& owner;
    double devRate = 48000;
    int maxBlock = 512;
    bool prepared = false;
    bool hasEq = true;

    // thread audio
    Loaded* current = nullptr;
    Loaded* fading = nullptr;
    std::vector<float> fadeBuf;
    Biquad eqLow, eqMid, eqHigh;
    float lastBass = -99, lastMid = -99, lastTreble = -99;
    float inGain = 1, outGain = 1;

    std::atomic<Loaded*> pending { nullptr };
    std::atomic<int> generation { 0 };
    juce::AbstractFifo trashFifo { 16 };
    std::array<Loaded*, 16> trash {};
    juce::CriticalSection lock;
    ModelInfo info;
    juce::String path;
    juce::ThreadPool pool { 1 };   // ultimo membro: distrutto per primo
};

NamAmp::NamAmp (const juce::String& id, const juce::String& titleText, bool withEq)
    : MonoModule (id, titleText), impl (std::make_unique<Impl> (*this))
{
    impl->hasEq = withEq;
    inputDb   = &addFloat ("gain",   "Gain",   -12.0f, 24.0f, 0.0f, " dB", 1);
    if (withEq)
    {
        bass   = &addFloat ("bass",   "Bass",   -12.0f, 12.0f, 0.0f, " dB", 1);
        mid    = &addFloat ("mid",    "Mid",    -12.0f, 12.0f, 0.0f, " dB", 1);
        treble = &addFloat ("treble", "Treble", -12.0f, 12.0f, 0.0f, " dB", 1);
    }
    outputDb  = &addFloat ("output", "Output", -24.0f, 12.0f, 0.0f, " dB", 1);
    normalize = &addToggle ("norm", "Normalize", true);
    quality   = &addChoice ("quality", "A2 Size", { "Full", "Lite" }, 0);
}

NamAmp::~NamAmp() = default;

void NamAmp::prepare (double sampleRate, int maxBlock)
{
    impl->devRate = sampleRate;
    impl->maxBlock = maxBlock;
    impl->prepared = true;
    impl->fadeBuf.assign ((size_t) maxBlock, 0.0f);
    prepareBypass (sampleRate, maxBlock);
    impl->lastBass = impl->lastMid = impl->lastTreble = -99.0f;
    reset();
    impl->retire (impl->current);                    // il vecchio modello ha un ponte di rate non più valido
    impl->current = nullptr;
    if (impl->path.isNotEmpty())
        impl->startLoad (juce::File (impl->path));   // (ri)carica con il rate corretto del device
}

void NamAmp::reset()
{
    impl->eqLow.reset(); impl->eqMid.reset(); impl->eqHigh.reset();
    if (impl->current) impl->current->bridge.reset();
}

int NamAmp::getLatencySamples() const
{
    return impl->current ? impl->current->bridge.latencyDeviceSamples (impl->devRate, impl->current->modelRate) : 0;
}

void NamAmp::loadModelAsync (const juce::File& file) { impl->startLoad (file); }
void NamAmp::reloadCurrent() { if (impl->path.isNotEmpty()) impl->startLoad (juce::File (impl->path)); }
juce::String NamAmp::getModelPath() const { juce::ScopedLock sl (impl->lock); return impl->path; }
ModelInfo NamAmp::getInfo() const { juce::ScopedLock sl (impl->lock); return impl->info; }
void NamAmp::collectGarbage() { impl->collect(); }

void NamAmp::process (float* x, int n)
{
    auto& I = *impl;
    if (auto* p = I.pending.exchange (nullptr))
    {
        I.retire (I.fading);
        I.fading = I.current;
        I.current = p;
    }
    if (I.current == nullptr) return;       // nessun modello: passa il segnale invariato

    if (I.hasEq)
    {
        const float bT = bass->get(), mT = mid->get(), tT = treble->get();
        if (bT != I.lastBass)   { I.eqLow.setLowShelf (I.devRate, 110.0, bT);        I.lastBass = bT; }
        if (mT != I.lastMid)    { I.eqMid.setPeak (I.devRate, 750.0, 0.8, mT);       I.lastMid = mT; }
        if (tT != I.lastTreble) { I.eqHigh.setHighShelf (I.devRate, 3200.0, tT);     I.lastTreble = tT; }
    }

    const float inT = dbToGain (inputDb->get());
    float outDb = outputDb->get();
    if (normalize->on() && I.current->hasLoudness) outDb += (float) (-18.0 - I.current->loudness);
    const float outT = dbToGain (outDb);

    if (I.fading) std::copy (x, x + n, I.fadeBuf.begin());

    for (int i = 0; i < n; ++i) { I.inGain += (inT - I.inGain) * 0.002f; x[i] *= I.inGain; }
    if (I.fading) for (int i = 0; i < n; ++i) I.fadeBuf[(size_t) i] *= I.inGain;

    I.current->run (x, n);
    if (I.fading)
    {
        I.fading->run (I.fadeBuf.data(), n);
        for (int i = 0; i < n; ++i)
        {
            const float t = (float) (i + 1) / (float) n;
            x[i] = I.fadeBuf[(size_t) i] * (1.0f - t) + x[i] * t;
        }
        I.retire (I.fading);
        I.fading = nullptr;
    }

    for (int i = 0; i < n; ++i)
    {
        I.outGain += (outT - I.outGain) * 0.002f;
        x[i] = (I.hasEq ? I.eqHigh.process (I.eqMid.process (I.eqLow.process (x[i]))) : x[i]) * I.outGain;
    }
}
} // namespace dr
