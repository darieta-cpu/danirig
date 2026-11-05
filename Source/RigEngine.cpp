#include "RigEngine.h"

namespace dr
{
RigEngine::RigEngine()
{
    inputChannel = &globals.addFloat ("input", "Input", 1.0f, 8.0f, 1.0f, "", 0);
    inputTrim    = &globals.addFloat ("trim", "Input Trim", -24.0f, 24.0f, 0.0f, " dB", 1);
    master       = &globals.addFloat ("master", "Master", -40.0f, 6.0f, -6.0f, " dB", 1);

    monoChain   = { &gate, &od, &dist, &amp, &cab };
    stereoChain = { &delay, &reverb };

    // Valori iniziali: pedali spenti (si accendono con il footswitch), amp/cab/delay/reverb attivi.
    od.enabled->set (0.0f);
    dist.enabled->set (0.0f);
    delay.enabled->set (0.0f);
    reverb.enabled->set (0.0f);
    od.enabled->defV = dist.enabled->defV = delay.enabled->defV = reverb.enabled->defV = 0.0f;
}

std::vector<Module*> RigEngine::allModules() const
{
    std::vector<Module*> v;
    for (auto* m : monoChain) v.push_back (m);
    for (auto* m : stereoChain) v.push_back (m);
    return v;
}

void RigEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    prepared = false;
    const double sr = device->getCurrentSampleRate();
    maxBlock = std::max (device->getCurrentBufferSizeSamples(), 32);   // blocchi più grandi vengono spezzati nel callback
    mono.assign ((size_t) maxBlock, 0.0f);
    left.assign ((size_t) maxBlock, 0.0f);
    right.assign ((size_t) maxBlock, 0.0f);
    for (auto* m : allModules()) m->prepare (sr, maxBlock);

    sampleRate.store (sr);
    bufferSize.store (device->getCurrentBufferSizeSamples());
    driverLatency.store (device->getInputLatencyInSamples() + device->getOutputLatencyInSamples());
    prepared = true;
}

void RigEngine::audioDeviceStopped() { prepared = false; }

void RigEngine::audioDeviceIOCallbackWithContext (const float* const* in, int numIn, float* const* out, int numOut,
                                                  int numSamples, const juce::AudioIODeviceCallbackContext&)
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

        for (auto* m : monoChain) m->run (mono.data(), n);

        std::copy (mono.begin(), mono.begin() + n, left.begin());
        std::copy (mono.begin(), mono.begin() + n, right.begin());
        for (auto* m : stereoChain) m->run (left.data(), right.data(), n);

        for (int i = 0; i < n; ++i)
        {
            // limitatore di sicurezza: morbido sopra 0.9, mai oltre 1.0
            auto lim = [] (float v) { const float a = std::abs (v); if (a < 0.9f) return v; const float s = v < 0 ? -1.0f : 1.0f; return s * (0.9f + 0.1f * std::tanh ((a - 0.9f) / 0.1f)); };
            left[(size_t) i]  = lim (left[(size_t) i]  * mst);
            right[(size_t) i] = lim (right[(size_t) i] * mst);
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
    juce::File nam (xml.getStringAttribute ("namPath")), ir (xml.getStringAttribute ("irPath"));
    if (nam.existsAsFile()) amp.loadModelAsync (nam);
    if (ir.existsAsFile()) cab.loadIR (ir);
}

void RigEngine::resetAll()
{
    for (auto& p : globals.params) p->reset();
    for (auto* m : allModules()) for (auto& p : m->params) p->reset();
}
} // namespace dr
