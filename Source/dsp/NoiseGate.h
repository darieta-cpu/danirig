#pragma once
#include "Module.h"
#include "Biquad.h"
#include <atomic>

namespace dr
{
// Noise gate senza lookahead (zero latenza) con isteresi e hold: pensato per davanti ai pedali di gain.
class NoiseGate : public MonoModule
{
public:
    NoiseGate() : MonoModule ("gate", "NOISE GATE")
    {
        threshold = &addFloat ("thr", "Thresh", -90.0f, -30.0f, -66.0f, " dB", 0);
        release   = &addFloat ("rel", "Release", 20.0f, 500.0f, 120.0f, " ms", 0);
        range     = &addFloat ("range", "Range", -90.0f, -10.0f, -60.0f, " dB", 0);
    }

    void prepare (double sampleRate, int maxBlock) override
    {
        sr = sampleRate;
        detRel = (float) std::exp (-1.0 / (0.020 * sr));
        atkCoef = (float) (1.0 - std::exp (-1.0 / (0.0004 * sr)));
        holdSamples = (int) (0.030 * sr);
        prepareBypass (sampleRate, maxBlock);
        reset();
    }

    void reset() override { env = 0; gain = 1; open = false; hold = 0; isOpen.store (false); }

    void process (float* x, int n) override
    {
        const float openThr  = dbToGain (threshold->get());
        const float closeThr = openThr * 0.6f;
        const float floorG   = dbToGain (range->get());
        const float relCoef  = (float) (1.0 - std::exp (-1.0 / (release->get() * 0.001 * sr)));

        for (int i = 0; i < n; ++i)
        {
            const float a = std::abs (x[i]);
            env = a > env ? a : env * detRel;

            if (! open) { if (env > openThr) { open = true; hold = holdSamples; } }
            else if (env < closeThr) { if (--hold <= 0) open = false; }
            else hold = holdSamples;

            const float target = open ? 1.0f : floorG;
            gain += (target - gain) * (target > gain ? atkCoef : relCoef);
            x[i] *= gain;
        }
        isOpen.store (open, std::memory_order_relaxed);
    }

    std::atomic<bool> isOpen { false };

private:
    Param *threshold, *release, *range;
    double sr = 48000;
    float env = 0, gain = 1, detRel = 0.99f, atkCoef = 0.05f;
    bool open = false;
    int hold = 0, holdSamples = 1440;
};
} // namespace dr
