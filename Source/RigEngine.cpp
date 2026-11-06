#include "RigEngine.h"

namespace dr
{
RigEngine::RigEngine()
{
    inputChannel = &globals.addFloat ("input", "Input", 1.0f, 8.0f, 1.0f, "", 0);
    inputTrim    = &globals.addFloat ("trim", "Input Trim", -24.0f, 24.0f, 0.0f, " dB", 1);
    master       = &globals.addFloat ("master", "Master", -40.0f, 6.0f, -6.0f, " dB", 1);

    preMods[0] = &gate; preMods[1] = &pedal; preMods[2] = &od; preMods[3] = &dist; preMods[4] = &mod;
    postMods[0] = &delay; postMods[1] = &reverb;

    // Valori iniziali: pedali spenti (si accendono con il footswitch), amp/cab/delay/reverb attivi.
    for (Module* m : std::initializer_list<Module*> { &pedal, &od, &dist, &mod, &delay, &reverb })
    {
        m->enabled->set (0.0f);
        m->enabled->defV = 0.0f;
    }
}

std::vector<Module*> RigEngine::allModules() const
{
    std::vector<Module*> v;
    for (auto* m : preMods) v.push_back (m);
    v.push_back (const_cast<NamAmp*> (&amp));
    v.push_back (const_cast<CabIR*> (&cab));
    for (auto* m : postMods) v.push_back (m);
    return v;
}

void RigEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    prepare (device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples(),
             device->getInputLatencyInSamples() + device->getOutputLatencyInSamples());
}

void RigEngine::prepare (double sr, int blockSize, int driverLatencySamples)
{
    prepared = false;
    maxBlock = std::max (blockSize, 32);   // blocchi più grandi vengono spezzati in process()
    mono.assign ((size_t) maxBlock, 0.0f);
    left.assign ((size_t) maxBlock, 0.0f);
    right.assign ((size_t) maxBlock, 0.0f);
    for (auto* m : allModules()) m->prepare (sr, maxBlock);

    sampleRate.store (sr);
    bufferSize.store (blockSize);
    driverLatency.store (driverLatencySamples);
    lastOrderCode = (uint64_t) preOrder.load() | ((uint64_t) postOrder.load() << 32);
    fadeInTotal = std::max (1, (int) (0.012 * sr));
    fadeInLeft = 0;
    prepared = true;
}

void RigEngine::process (const float* const* in, int numIn, float* const* out, int numOut, int numSamples)
{
    juce::ScopedNoDenormals noDenormals;

    if (! prepared)
    {
        for (int c = 0; c < numOut; ++c) if (out[c]) juce::FloatVectorOperations::clear (out[c], numSamples);
        return;
    }

    const int inCh = std::clamp (inputChannel->index() - 1, 0, std::max (numIn - 1, 0));
    const float trim = dbToGain (inputTrim->get());
    const float mst = dbToGain (master->get());
    float inPk = 0, outPk = 0;

    for (int done = 0; done < numSamples; )
    {
        const int n = std::min (numSamples - done, maxBlock);

        if (numIn > 0 && in[inCh] != nullptr)
            for (int i = 0; i < n; ++i) mono[(size_t) i] = in[inCh][done + i] * trim;
        else
            std::fill (mono.begin(), mono.begin() + n, 0.0f);

        for (int i = 0; i < n; ++i) inPk = std::max (inPk, std::abs (mono[(size_t) i]));

        const uint32_t pre = preOrder.load (std::memory_order_relaxed), post = postOrder.load (std::memory_order_relaxed);
        const uint64_t code = (uint64_t) pre | ((uint64_t) post << 32);
        if (code != lastOrderCode) { lastOrderCode = code; fadeInLeft = fadeInTotal; }   // ordine cambiato: fade-in di 12 ms anti-click

        for (int k = 0; k < kPre; ++k) preMods[(pre >> (4 * k)) & 0xF]->run (mono.data(), n);
        amp.run (mono.data(), n);
        cab.run (mono.data(), n);

        std::copy (mono.begin(), mono.begin() + n, left.begin());
        std::copy (mono.begin(), mono.begin() + n, right.begin());
        for (int k = 0; k < kPost; ++k) postMods[(post >> (4 * k)) & 0xF]->run (left.data(), right.data(), n);

        for (int i = 0; i < n; ++i)
        {
            // limitatore di sicurezza: morbido sopra 0.9, mai oltre 1.0
            auto lim = [] (float v) { const float a = std::abs (v); if (a < 0.9f) return v; const float s = v < 0 ? -1.0f : 1.0f; return s * (0.9f + 0.1f * std::tanh ((a - 0.9f) / 0.1f)); };
            float g = mst;
            if (fadeInLeft > 0) { const float t = 1.0f - (float) fadeInLeft / (float) fadeInTotal; g *= t * t; --fadeInLeft; }
            left[(size_t) i]  = lim (left[(size_t) i]  * g);
            right[(size_t) i] = lim (right[(size_t) i] * g);
            outPk = std::max (outPk, std::max (std::abs (left[(size_t) i]), std::abs (right[(size_t) i])));
        }

        if (numOut == 1)
        {
            if (out[0]) for (int i = 0; i < n; ++i) out[0][done + i] = 0.5f * (left[(size_t) i] + right[(size_t) i]);
        }
        else
        {
            for (int c = 0; c < numOut; ++c)
            {
                if (out[c] == nullptr) continue;
                if (c == 0)      std::copy (left.begin(),  left.begin() + n,  out[c] + done);
                else if (c == 1) std::copy (right.begin(), right.begin() + n, out[c] + done);
                else             juce::FloatVectorOperations::clear (out[c] + done, n);
            }
        }
        done += n;
    }

    // peak per i meter della UI
    auto bump = [] (std::atomic<float>& a, float v) { float cur = a.load (std::memory_order_relaxed); while (v > cur && ! a.compare_exchange_weak (cur, v)) {} };
    bump (inPeak, inPk); bump (outPeak, outPk);

    int lat = 0;
    for (auto* m : allModules()) if (m->isActive()) lat += m->getLatencySamples();
    chainLatency.store (lat, std::memory_order_relaxed);
}

// ------------------------------------------------------------------------------------------------
std::unique_ptr<juce::XmlElement> RigEngine::saveState() const
{
    auto xml = std::make_unique<juce::XmlElement> ("DaniRigPreset");
    xml->setAttribute ("version", 1);
    for (auto& p : globals.params) xml->setAttribute ("g." + p->id, (double) p->get());
    for (auto* m : allModules())
        for (auto& p : m->params) xml->setAttribute (m->moduleId + "." + p->id, (double) p->get());
    xml->setAttribute ("namPath", amp.getModelPath());
    xml->setAttribute ("pedalPath", pedal.getModelPath());
    xml->setAttribute ("preOrder", (int) preOrder.load());
    xml->setAttribute ("postOrder", (int) postOrder.load());
    xml->setAttribute ("irPath", cab.getIRPath());
    return xml;
}

void RigEngine::loadState (const juce::XmlElement& xml)
{
    if (! xml.hasTagName ("DaniRigPreset")) return;
    for (auto& p : globals.params)
        if (xml.hasAttribute ("g." + p->id)) p->set ((float) xml.getDoubleAttribute ("g." + p->id));
    for (auto* m : allModules())
        for (auto& p : m->params)
        {
            const auto key = m->moduleId + "." + p->id;
            if (xml.hasAttribute (key)) p->set ((float) xml.getDoubleAttribute (key));
        }
    if (xml.hasAttribute ("preOrder"))  preOrder.store ((uint32_t) xml.getIntAttribute ("preOrder"));
    if (xml.hasAttribute ("postOrder")) postOrder.store ((uint32_t) xml.getIntAttribute ("postOrder"));
    sanitizeOrders();
    juce::File nam (xml.getStringAttribute ("namPath")), ir (xml.getStringAttribute ("irPath")), ped (xml.getStringAttribute ("pedalPath"));
    if (nam.existsAsFile()) amp.loadModelAsync (nam);
    if (ped.existsAsFile()) pedal.loadModelAsync (ped);
    if (ir.existsAsFile()) cab.loadIR (ir);
}

// Una permutazione non valida (file di preset vecchio o corrotto) ripristina l'ordine di default.
void RigEngine::sanitizeOrders()
{
    auto valid = [] (const std::vector<int>& o, int n) { std::vector<int> s (o); std::sort (s.begin(), s.end()); for (int i = 0; i < n; ++i) if (s[(size_t) i] != i) return false; return (int) o.size() == n; };
    if (! valid (getPreOrder(), kPre))   setPreOrder (defaultPreOrder());
    if (! valid (getPostOrder(), kPost)) setPostOrder (defaultPostOrder());
}

void RigEngine::resetAll()
{
    setPreOrder (defaultPreOrder());
    setPostOrder (defaultPostOrder());
    for (auto& p : globals.params) p->reset();
    for (auto* m : allModules()) for (auto& p : m->params) p->reset();
}
} // namespace dr
