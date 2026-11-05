#pragma once
#include "Param.h"
#include <algorithm>
#include <vector>

namespace dr
{
// Base di ogni modulo della catena. Gestisce on/off con crossfade (niente click).
class Module : public ParamOwner
{
public:
    Module (juce::String id, juce::String titleText) : moduleId (std::move (id)), title (std::move (titleText))
    {
        enabled = &addToggle ("on", "On", true);
    }
    virtual ~Module() = default;

    virtual void prepare (double sampleRate, int maxBlock) = 0;
    virtual void reset() {}
    virtual int getLatencySamples() const { return 0; }
    bool isActive() const noexcept { return enabled->on(); }

    const juce::String moduleId, title;
    Param* enabled = nullptr;

protected:
    void prepareBypass (double sampleRate, int maxBlock)
    {
        step = (float) (1.0 / (0.010 * sampleRate));
        dryL.assign ((size_t) maxBlock, 0.0f);
        dryR.assign ((size_t) maxBlock, 0.0f);
        wet = enabled->on() ? 1.0f : 0.0f;
    }

    // true se il modulo deve elaborare; aggiorna la rampa wet/dry
    float wet = 1.0f, step = 0.01f;
    std::vector<float> dryL, dryR;
};

// Modulo mono (sezione pre-cab: gate, pedali, ampli, cab).
class MonoModule : public Module
{
public:
    using Module::Module;
    virtual void process (float* x, int n) = 0;

    void run (float* x, int n)
    {
        const float target = enabled->on() ? 1.0f : 0.0f;
        if (wet == target) { if (target > 0.5f) process (x, n); return; }
        if (wet == 0.0f) reset();
        std::copy (x, x + n, dryL.begin());
        process (x, n);
        for (int i = 0; i < n; ++i)
        {
            wet += std::clamp (target - wet, -step, step);
            x[i] = dryL[(size_t) i] * (1.0f - wet) + x[i] * wet;
        }
    }
};

// Modulo stereo (sezione post-cab: delay, riverbero).
class StereoModule : public Module
{
public:
    using Module::Module;
    virtual void process (float* l, float* r, int n) = 0;

    void run (float* l, float* r, int n)
    {
        const float target = enabled->on() ? 1.0f : 0.0f;
        if (wet == target) { if (target > 0.5f) process (l, r, n); return; }
        if (wet == 0.0f) reset();
        std::copy (l, l + n, dryL.begin());
        std::copy (r, r + n, dryR.begin());
        process (l, r, n);
        for (int i = 0; i < n; ++i)
        {
            wet += std::clamp (target - wet, -step, step);
            l[i] = dryL[(size_t) i] * (1.0f - wet) + l[i] * wet;
            r[i] = dryR[(size_t) i] * (1.0f - wet) + r[i] * wet;
        }
    }
};
} // namespace dr
