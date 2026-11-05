#pragma once
#include "Module.h"
#include "Biquad.h"
#include "FracDelay.h"
#include <juce_core/juce_core.h>

namespace dr
{
// Delay stereo con tap tempo, suddivisioni, modulazione, modo Digital/Analog e ping-pong.
class DelayPedal : public StereoModule
{
public:
    DelayPedal() : StereoModule ("delay", "DELAY")
    {
        time    = &addFloat ("time", "Time", 60.0f, 1500.0f, 420.0f, " ms", 0);
        subdiv  = &addChoice ("subdiv", "Subdiv", { "1/4", "1/8 dot", "1/8", "1/8 trip" }, 0);
        repeats = &addFloat ("repeats", "Repeats", 0.0f, 100.0f, 35.0f, " %", 0);
        tone    = &addFloat ("tone", "Tone", 0.0f, 10.0f, 6.0f, "", 1);
        mod     = &addFloat ("mod", "Mod", 0.0f, 10.0f, 2.0f, "", 1);
        mix     = &addFloat ("mix", "Mix", 0.0f, 100.0f, 30.0f, " %", 0);
        mode    = &addChoice ("mode", "Mode", { "Digital", "Analog" }, 0);
        pingpong = &addToggle ("pingpong", "Ping-Pong", false);
    }

    static float subdivMultiplier (int idx)
    {
        static const float m[] = { 1.0f, 0.75f, 0.5f, 1.0f / 3.0f };
        return m[juce::jlimit (0, 3, idx)];
    }
    float effectiveMs() const { return time->get() * subdivMultiplier (subdiv->index()); }

    // Thread messaggi: imposta il tempo (nota da 1/4) dalla media degli ultimi intervalli di tap.
    void tap()
    {
        const double now = juce::Time::getMillisecondCounterHiRes();
        if (! taps.empty() && now - taps.back() > 2000.0) taps.clear();
        taps.push_back (now);
        if (taps.size() > 5) taps.erase (taps.begin());
        if (taps.size() >= 2)
        {
            const double avg = (taps.back() - taps.front()) / (double) (taps.size() - 1);
            time->set ((float) avg);
        }
    }

    void prepare (double sampleRate, int maxBlock) override
    {
        sr = sampleRate;
        const int maxD = (int) (1.6 * sr);
        lineL.prepare (maxD); lineR.prepare (maxD);
        smCoef = (float) (1.0 - std::exp (-1.0 / (0.080 * sr)));
        prepareBypass (sampleRate, maxBlock);
        reset();
    }

    void reset() override
    {
        lineL.clear(); lineR.clear();
        lpL.reset(); lpR.reset(); hpL.reset(); hpR.reset();
        delaySm = effectiveMs() * 0.001f * (float) sr;
        phase = 0.0;
    }

    void process (float* l, float* r, int n) override
    {
        const bool analog = mode->index() == 1, pp = pingpong->on();
        const float target = effectiveMs() * 0.001f * (float) sr;
        const float fb = std::min (repeats->get() * 0.01f, analog ? 0.92f : 0.97f);
        const float wetG = mix->get() * 0.01f;
        const double fcTone = (analog ? 500.0 : 900.0) * std::pow (analog ? 9.0 : 14.0, tone->get() / 10.0);
        lpL.setCutoff (sr, fcTone); lpR.setCutoff (sr, fcTone);
        hpL.setCutoff (sr, analog ? 140.0 : 30.0); hpR.setCutoff (sr, analog ? 140.0 : 30.0);
        const float modAmt = mod->get() / 10.0f;
        const double rate = analog ? 0.7 : 0.45;
        const float depth = modAmt * (analog ? 0.0035f : 0.0022f) * (float) sr;   // fino a ~3.5 ms

        for (int i = 0; i < n; ++i)
        {
            delaySm += (target - delaySm) * smCoef;
            phase += rate / sr; if (phase >= 1.0) phase -= 1.0;
            const float mL = std::sin ((float) (2 * kPi * phase));
            const float mR = std::cos ((float) (2 * kPi * phase));
            const float yL = lineL.read (delaySm + depth * mL);
            const float yR = lineR.read (delaySm + depth * mR);

            const float x = 0.5f * (l[i] + r[i]);
            const float fL = fbProcess (yL, lpL, hpL, analog);
            const float fR = fbProcess (yR, lpR, hpR, analog);
            if (pp) { lineL.write (x + fb * fR); lineR.write (fb * fL); }
            else    { lineL.write (x + fb * fL); lineR.write (x + fb * fR); }

            l[i] = x + wetG * yL;
            r[i] = x + wetG * yR;
        }
    }

private:
    static float fbProcess (float y, OnePole& lp, OnePole& hp, bool analog)
    {
        float v = lp.lp (y);
        if (analog) { v = v - hp.lp (v); v = std::tanh (v * 0.9f) / 0.9f; }
        return std::clamp (v, -4.0f, 4.0f);
    }

    Param *time, *subdiv, *repeats, *tone, *mod, *mix, *mode, *pingpong;
    FracDelay lineL, lineR;
    OnePole lpL, lpR, hpL, hpR;
    double sr = 48000, phase = 0;
    float delaySm = 20000, smCoef = 0.0003f;
    std::vector<double> taps;
};
} // namespace dr
