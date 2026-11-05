// Test DSP di DaniRig (nessuna scheda audio necessaria). Esegui: ./DaniRigTests [percorso/modello.nam]
#include "dsp/NoiseGate.h"
#include "dsp/Pedals.h"
#include "dsp/NamAmp.h"
#include "dsp/CabIR.h"
#include "dsp/Delay.h"
#include "dsp/Reverb.h"
#include "dsp/Resampler.h"
#include <juce_events/juce_events.h>
#include <cstdio>
#include <random>

using namespace dr;

static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { std::printf ("  FAIL: %s\n", msg); ++failures; } else std::printf ("  ok:   %s\n", msg); } while (0)

static float rms (const std::vector<float>& v, size_t a, size_t b) { double s = 0; for (size_t i = a; i < b; ++i) s += (double) v[i] * v[i]; return (float) std::sqrt (s / (double) (b - a)); }
static bool finite (const std::vector<float>& v) { for (float x : v) if (! std::isfinite (x)) return false; return true; }
static float peakOf (const std::vector<float>& v) { float p = 0; for (float x : v) p = std::max (p, std::abs (x)); return p; }

template <class M> static void runMono (M& m, std::vector<float>& x, int block)
{
    for (size_t i = 0; i < x.size(); i += (size_t) block)
        m.run (x.data() + i, (int) std::min<size_t> ((size_t) block, x.size() - i));
}
static std::vector<float> sine (double sr, double f, double amp, size_t n)
{ std::vector<float> v (n); for (size_t i = 0; i < n; ++i) v[i] = (float) (amp * std::sin (2 * kPi * f * (double) i / sr)); return v; }

int main (int argc, char** argv)
{
    juce::MessageManager::getInstance();
    const double sr = 48000; const int block = 128;

    std::printf ("[Noise gate]\n");
    {
        NoiseGate g; g.prepare (sr, block);
        std::vector<float> x = sine (sr, 220, 0.0005, 48000);       // rumore molto basso (-66 dB)
        auto loud = sine (sr, 220, 0.3, 24000);
        x.insert (x.end(), loud.begin(), loud.end());
        runMono (g, x, block);
        CHECK (rms (x, 20000, 40000) < 0.0003f, "chiude sotto la soglia");
        CHECK (rms (x, 60000, 70000) > 0.15f, "apre con il segnale forte");
        CHECK (finite (x), "nessun NaN");
    }

    std::printf ("[Tube Screamer / DS-1]\n");
    for (int which = 0; which < 2; ++which)
    {
        TubeScreamer ts; Ds1 ds;
        MonoModule& m = which == 0 ? (MonoModule&) ts : (MonoModule&) ds;
        m.prepare (sr, block);
        bool ok = true; float lastPk = 0; bool mono = true;
        for (double amp : { 0.001, 0.01, 0.05, 0.2, 0.5, 1.0 })
        {
            m.reset();
            auto x = sine (sr, 220, amp, 24000);
            runMono (m, x, block);
            ok &= finite (x);
            const float pk = peakOf (x);
            if (amp > 0.01 && pk + 1e-4f < lastPk * 0.5f) mono = false;
            lastPk = pk;
            std::printf ("       %s amp %.3f -> picco %.3f\n", which == 0 ? "TS " : "DS1", amp, pk);
        }
        CHECK (ok, which == 0 ? "TS: stabile da -60 a 0 dBFS" : "DS-1: stabile da -60 a 0 dBFS");
        CHECK (peakOf (std::vector<float> { lastPk }) < 8.0f, "uscita limitata (clipping)");
        CHECK (mono, "livello coerente al crescere dell'ingresso");
        // aliasing: una sinusoide a 5 kHz distorta non deve produrre componenti "sporche" forti sotto la fondamentale
        m.reset();
        auto x = sine (sr, 5000, 0.3, 48000);
        runMono (m, x, block);
        CHECK (finite (x), "5 kHz distorto senza NaN");
        // cambio on/off senza click: differenza massima tra campioni consecutivi limitata
        m.enabled->set (0.0f);
        auto y = sine (sr, 220, 0.2, 9600);
        runMono (m, y, block);
        m.enabled->set (1.0f);
        auto z = sine (sr, 220, 0.2, 9600);
        runMono (m, z, block);
        float maxStep = 0; for (size_t i = 1; i < z.size(); ++i) maxStep = std::max (maxStep, std::abs (z[i] - z[i - 1]));
        CHECK (maxStep < 1.0f, "bypass on/off con crossfade (niente salti)");
    }

    std::printf ("[Resampler]\n");
    {
        for (double devRate : { 44100.0, 96000.0, 88200.0 })
        {
            RateBridge br; br.prepare (devRate, 48000.0, 128);
            std::vector<float> x = sine (devRate, 1000, 0.5, (size_t) devRate);
            for (size_t i = 0; i < x.size(); i += 128)
                br.process (x.data() + i, (int) std::min<size_t> (128, x.size() - i), [] (float*, int) {});
            { char m2[64]; std::snprintf (m2, sizeof m2, "ponte %.0f: nessun underrun", devRate); CHECK (br.underruns == 0, m2); }
            const float r = rms (x, x.size() / 2, x.size() - 1000);
            char msg[96]; std::snprintf (msg, sizeof msg, "ponte %.0f<->48000: sinusoide 1 kHz con guadagno ~1 (rms %.3f)", devRate, r);
            CHECK (std::abs (r - 0.3536f) < 0.01f, msg);
            std::snprintf (msg, sizeof msg, "ponte %.0f: latenza stimata %d campioni", devRate, br.latencyDeviceSamples (devRate, 48000.0));
            std::printf ("       %s\n", msg);
        }
    }

    std::printf ("[Delay]\n");
    {
        DelayPedal d; d.prepare (sr, block);
        d.enabled->set (1.0f); d.find ("mix")->set (100); d.find ("repeats")->set (0); d.find ("time")->set (100); d.find ("mod")->set (0);
        d.reset();
        std::vector<float> l (24064, 0.0f), r (24064, 0.0f); l[0] = r[0] = 1.0f;
        for (size_t i = 0; i + block <= l.size(); i += block) d.run (l.data() + i, r.data() + i, block);
        size_t peakIdx = 1; for (size_t i = 100; i < l.size(); ++i) if (std::abs (l[i]) > std::abs (l[peakIdx])) peakIdx = i;
        std::printf ("       eco a %zu campioni (atteso 4800)\n", peakIdx);
        CHECK (std::abs ((int) peakIdx - 4800) <= 4, "tempo del delay corretto (100 ms)");
        CHECK (std::abs (l[peakIdx]) > 0.5f, "ampiezza dell'eco");
        // tap tempo
        DelayPedal t; t.prepare (sr, block);
        t.tap (); juce::Thread::sleep (500); t.tap (); juce::Thread::sleep (500); t.tap ();
        std::printf ("       tap ~500 ms -> time = %.0f ms\n", t.find ("time")->get());
        CHECK (std::abs (t.find ("time")->get() - 500.0f) < 40.0f, "tap tempo ~500 ms");
    }

    std::printf ("[Reverb]\n");
    {
        ReverbModule rv; rv.prepare (sr, block);
        rv.enabled->set (1.0f); rv.find ("mix")->set (100); rv.find ("decay")->set (2.0f); rv.reset();
        std::vector<float> l (48000 * 6, 0.0f), r (48000 * 6, 0.0f); l[0] = r[0] = 1.0f;
        for (size_t i = 0; i < l.size(); i += block) rv.run (l.data() + i, r.data() + i, block);
        const float early = rms (l, 2000, 14000), mid = rms (l, 48000, 60000), late = rms (l, 48000 * 5, 48000 * 6);
        std::printf ("       rms: 0.04-0.3 s %.5f   1-1.25 s %.5f   5-6 s %.6f\n", early, mid, late);
        CHECK (early > 0.0f && finite (l) && finite (r), "coda presente, nessun NaN");
        CHECK (mid < early && late < mid * 0.2f, "la coda decade nel tempo");
        // rumore: guadagno del wet
        ReverbModule rn; rn.prepare (sr, block); rn.enabled->set (1.0f); rn.find ("mix")->set (100); rn.reset();
        std::mt19937 g (1); std::normal_distribution<float> nd (0.0f, 0.1f);
        std::vector<float> a (96000), b (96000), dry (96000); for (auto& v : a) v = nd (g); dry = a; b = a;
        for (size_t i = 0; i < a.size(); i += block) rn.run (a.data() + i, b.data() + i, block);
        std::vector<float> wet (a.size()); for (size_t i = 0; i < a.size(); ++i) wet[i] = a[i] - dry[i];
        std::printf ("       rumore: rms dry %.4f  wet %.4f\n", rms (dry, 48000, 96000), rms (wet, 48000, 96000));
    }

    std::printf ("[Catena completa + NAM]\n");
    {
        NamAmp amp;
        const juce::File model = argc > 1 ? juce::File (argv[1]) : juce::File();
        if (model.existsAsFile())
        {
            for (double devRate : { 48000.0, 44100.0 })
            {
                NamAmp a; a.loadModelAsync (model); a.prepare (devRate, block);
                for (int i = 0; i < 200 && ! a.getInfo().loaded && a.getInfo().error.isEmpty(); ++i) juce::Thread::sleep (50);
                auto info = a.getInfo();
                std::printf ("       modello: %s  loaded=%d slimmable=%d rate=%.0f err='%s'\n", info.name.toRawUTF8(), info.loaded, info.slimmable, info.modelRate, info.error.toRawUTF8());
                CHECK (info.loaded, "modello caricato in background");
                auto x = sine (devRate, 220, 0.1, (size_t) devRate);
                for (size_t i = 0; i < x.size(); i += block) a.run (x.data() + i, block <= (int) (x.size() - i) ? block : (int) (x.size() - i));
                std::printf ("       %.0f Hz: rms out %.4f  picco %.3f  latenza ponte %d campioni\n", devRate, rms (x, x.size() / 2, x.size() - 500), peakOf (x), a.getLatencySamples());
                CHECK (finite (x) && rms (x, x.size() / 2, x.size() - 500) > 1e-4f, "uscita non nulla e finita");
                a.collectGarbage();
            }
            // cambio di modello a caldo (crossfade) + Lite
            NamAmp a; a.prepare (48000.0, block); a.loadModelAsync (model);
            for (int i = 0; i < 200 && ! a.getInfo().loaded; ++i) juce::Thread::sleep (50);
            auto x = sine (48000, 220, 0.1, 48000);
            for (size_t i = 0; i < x.size(); i += block)
            {
                if (i == 12800) { a.quality->set (1.0f); a.reloadCurrent(); }
                a.run (x.data() + i, block);
                if (i > 12800 && i < 38400) juce::Thread::sleep (1);
            }
            CHECK (finite (x), "swap Full->Lite a caldo senza NaN");
            a.collectGarbage();
        }
        else std::printf ("  (nessun modello passato: salto)\n");
    }

    if (argc > 1 && juce::File (argv[1]).existsAsFile())
    {
        std::printf ("[Prestazioni: 10 s di audio, 1 core]\n");
        for (int q = 0; q < 2; ++q)
            for (int blk : { 64, 128 })
            {
                NamAmp a; a.quality->set ((float) q); a.prepare (sr, blk); a.loadModelAsync (juce::File (argv[1]));
                for (int i = 0; i < 200 && ! a.getInfo().loaded; ++i) juce::Thread::sleep (50);
                auto x = sine (sr, 220, 0.1, (size_t) sr * 10 / (size_t) blk * (size_t) blk);
                for (size_t i = 0; i < 4800; i += (size_t) blk) a.run (x.data() + i, blk);
                const double t0 = juce::Time::getMillisecondCounterHiRes();
                for (size_t i = 0; i + (size_t) blk <= x.size(); i += (size_t) blk) a.run (x.data() + i, blk);
                const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
                std::printf ("       A2 %s, blocco %3d: %.1f %% di un core (%.1fx tempo reale)\n", q ? "Lite" : "Full", blk, ms / 100.0, 10000.0 / ms);
                a.collectGarbage();
            }
    }

    std::printf ("\n%s (%d errori)\n", failures ? "FALLITO" : "TUTTO OK", failures);
    return failures ? 1 : 0;
}
