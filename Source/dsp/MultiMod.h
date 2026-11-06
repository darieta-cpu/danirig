#pragma once
#include "Module.h"
#include "Biquad.h"
#include "FracDelay.h"
#include <juce_core/juce_core.h>

namespace dr
{
// Pedale multi-modulazione in stile TC Electronic Plethora X1, con tre modi:
//   Phase    - phaser a 4 stadi allpass (stile "script" Phase 90, senza feedback)
//   Chorus   - chorus analogico BBD mono (stile CE-2): LFO triangolare, delay ~7 ms, wet filtrato
//   Tremolo  - modulazione d'ampiezza con tap tempo e suddivisioni
// Il tap tempo imposta la frequenza dell'LFO (e ne riallinea la fase) in tutti i modi.
class MultiMod : public MonoModule
{
public:
    MultiMod() : MonoModule ("mod", "MULTI MOD")
    {
        mode  = &addChoice ("mode", "Mode", { "Phase", "Chorus", "Tremolo" }, 0);
        rate  = &addFloat ("rate", "Rate", 0.1f, 12.0f, 1.2f, " Hz", 2);
        depth = &addFloat ("depth", "Depth", 0.0f, 10.0f, 5.0f, "", 1);
        level = &addFloat ("level", "Level", -12.0f, 12.0f, 0.0f, " dB", 1);
        div   = &addChoice ("div", "Tap Div", { "Tap 1/2", "Tap 1/4", "Tap 1/8 dot", "Tap 1/8", "Tap 1/16" }, 1);
    }

    static float divMultiplier (int idx)
    {
        static const float m[] = { 0.5f, 1.0f, 4.0f / 3.0f, 2.0f, 4.0f };
        return m[juce::jlimit (0, 4, idx)];
    }

    // Thread messaggi: la media degli ultimi intervalli diventa la nota da 1/4; la frequenza = 1/intervallo * suddivisione.
    void tap()
    {
        const double now = juce::Time::getMillisecondCounterHiRes();
        if (! taps.empty() && now - taps.back() > 2000.0) taps.clear();
        taps.push_back (now);
        if (taps.size() > 5) taps.erase (taps.begin());
        resync.store (true);
        if (taps.size() >= 2)
        {
            const double avgMs = (taps.back() - taps.front()) / (double) (taps.size() - 1);
            rate->set ((float) (1000.0 / avgMs) * divMultiplier (div->index()));
        }
    }

    void prepare (double sampleRate, int maxBlock) override
    {
        sr = sampleRate;
        chorusLine.prepare ((int) (0.030 * sr));
        rateCoef = (float) (1.0 - std::exp (-1.0 / (0.030 * sr)));
        fadeStep = (float) (1.0 / (0.010 * sr));
        prepareBypass (sampleRate, maxBlock);
        lastMode = -1;
        reset();
    }

    void reset() override
    {
        for (auto& s : ap) s = {};
        chorusLine.clear();
        bbd.reset();
        phase = 0.0;
        rateSm = rate->get();
        levelSm = dbToGain (level->get());
        modeFade = 0.0f;
    }

    void process (float* x, int n) override
    {
        const int m = mode->index();
        if (m != lastMode) { lastMode = m; for (auto& s : ap) s = {}; chorusLine.clear(); bbd.reset(); modeFade = 0.0f; }
        if (resync.exchange (false)) phase = 0.0;

        const float d = depth->get() * 0.1f;
        const float targetRate = rate->get();
        const float targetLevel = dbToGain (level->get());
        bbd.setCutoff (sr, 6500.0);

        for (int i = 0; i < n; ++i)
        {
            rateSm += (targetRate - rateSm) * rateCoef;
            levelSm += (targetLevel - levelSm) * 0.002f;
            phase += (double) rateSm / sr;
            if (phase >= 1.0) phase -= 1.0;

            const float in = x[i];
            float y = in;
            switch (m)
            {
                case 0:   // Phase: 4 stadi allpass spazzolati in modo esponenziale (~120 Hz .. ~3.2 kHz)
                {
                    if ((i & 7) == 0)
                    {
                        const float l = 0.5f + 0.5f * std::sin ((float) (2.0 * kPi * phase));
                        const float e = 0.5f + (l - 0.5f) * (0.2f + 0.8f * d);
                        const float f = 120.0f * std::pow (26.7f, e);
                        const float t = std::tan ((float) kPi * f / (float) sr);
                        coefA = (1.0f - t) / (1.0f + t);
                    }
                    float v = in;
                    for (auto& s : ap) { const float out = coefA * v + s.x1 - coefA * s.y1; s.x1 = v; s.y1 = out; v = out; }
                    y = 0.5f * (in + v);
                    break;
                }
                case 1:   // Chorus CE-2: delay ~7 ms modulato da LFO triangolare, wet filtrato come un BBD
                {
                    const float tri = 1.0f - 4.0f * std::abs ((float) phase - 0.5f);
                    const float delayMs = 7.0f + d * 3.2f * tri;
                    chorusLine.write (in);
                    const float wet = bbd.lp (chorusLine.read (delayMs * 0.001f * (float) sr));
                    y = 0.7f * (in + wet);
                    break;
                }
                default:  // Tremolo: g = 1 sul "battere" (fase 0), scende fino a 1-depth
                {
                    const float g = 1.0f - d * (0.5f - 0.5f * std::cos ((float) (2.0 * kPi * phase)));
                    y = in * g;
                    break;
                }
            }
            modeFade = std::min (1.0f, modeFade + fadeStep);
            x[i] = (in + (y - in) * modeFade) * levelSm;
        }
    }

    float getLfoPhase() const { return (float) phase; }

private:
    struct Stage { float x1 = 0, y1 = 0; };

    Param *mode, *rate, *depth, *level, *div;
    Stage ap[4];
    FracDelay chorusLine;
    OnePole bbd;
    std::atomic<bool> resync { false };
    std::vector<double> taps;
    double sr = 48000, phase = 0.0;
    float rateSm = 1.2f, levelSm = 1.0f, rateCoef = 0.001f, coefA = 0.5f, modeFade = 1.0f, fadeStep = 0.01f;
    int lastMode = -1;
};
} // namespace dr
