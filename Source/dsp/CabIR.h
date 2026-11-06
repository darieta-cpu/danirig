#pragma once
#include "Module.h"
#include "Biquad.h"
#include <juce_dsp/juce_dsp.h>

namespace dr
{
// Cabinet: convoluzione IR (JUCE, latenza zero, partizionata non uniforme) + low/high cut + livello.
class CabIR : public MonoModule
{
public:
    CabIR() : MonoModule ("cab", "CABINET"), conv (juce::dsp::Convolution::Latency { 0 })
    {
        lowCut  = &addFloat ("lowcut",  "Low Cut",  20.0f, 500.0f, 70.0f, " Hz", 0);
        highCut = &addFloat ("highcut", "High Cut", 2000.0f, 20000.0f, 14000.0f, " Hz", 0);
        level   = &addFloat ("level",   "Level",    -12.0f, 12.0f, 0.0f, " dB", 1);
    }

    void prepare (double sampleRate, int maxBlock) override
    {
        sr = sampleRate;
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlock, 1 };
        conv.prepare (spec);
        prepareBypass (sampleRate, maxBlock);
        lastLow = lastHigh = -1.0f;
        reset();
    }

    void reset() override { conv.reset(); hp.reset(); lp.reset(); }

    // Thread messaggi. Il caricamento avviene in background, lo scambio è senza click.
    void loadIR (const juce::File& f)
    {
        irPath = f.getFullPathName();
        irRequested.store (true);
        conv.loadImpulseResponse (f, juce::dsp::Convolution::Stereo::no, juce::dsp::Convolution::Trim::yes, 0,
                                  juce::dsp::Convolution::Normalise::yes);
    }
    void clearIR() { irPath = {}; irRequested.store (false); }
    juce::String getIRPath() const { return irPath; }
    bool hasIR() const { return conv.getCurrentIRSize() > 1; }

    void process (float* x, int n) override
    {
        // NB: la convolution di JUCE installa un nuovo IR dentro process() (thread audio). Va quindi chiamata
        // finché un IR è stato richiesto, senza aspettare che getCurrentIRSize() sia > 0, altrimenti non si carica mai.
        if (irRequested.load())
        {
            float* ch[1] = { x };
            juce::dsp::AudioBlock<float> block (ch, 1, (size_t) n);
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            conv.process (ctx);
        }
        const float lc = lowCut->get(), hc = highCut->get();
        if (lc != lastLow)  { hp.setHighPass (sr, lc); lastLow = lc; }
        if (hc != lastHigh) { lp.setLowPass (sr, hc);  lastHigh = hc; }
        const float target = dbToGain (level->get());
        for (int i = 0; i < n; ++i)
        {
            gain += (target - gain) * 0.002f;
            x[i] = lp.process (hp.process (x[i])) * gain;
        }
    }

private:
    Param *lowCut, *highCut, *level;
    juce::dsp::Convolution conv;
    juce::String irPath;
    std::atomic<bool> irRequested { false };
    Biquad hp, lp;
    double sr = 48000;
    float lastLow = -1, lastHigh = -1, gain = 1;
};
} // namespace dr
