#pragma once
#include "Module.h"
#include "Biquad.h"
#include <juce_dsp/juce_dsp.h>

namespace dr
{
// Risolve v + k*sinh(v/nVt) = u  (coppia di diodi antiparalleli 1N914/1N4148 in parallelo a una resistenza,
// o dietro una resistenza serie). Newton partendo da destra della radice: convergenza monotona, 3 iterazioni.
inline double diodeSolve (double u, double k) noexcept
{
    constexpr double nVt = 1.752 * 0.02585;
    const double a = std::abs (u);
    double v = nVt * std::asinh (a / k);
    for (int i = 0; i < 3; ++i)
    {
        const double e = std::exp (std::min (v / nVt, 40.0));
        const double sh = 0.5 * (e - 1.0 / e), ch = 0.5 * (e + 1.0 / e);
        v -= (v + k * sh - a) / (1.0 + (k / nVt) * ch);
    }
    return u < 0 ? -v : v;
}
constexpr double kIs = 2.52e-9;   // corrente di saturazione 1N4148/1N914

// Base per i pedali non lineari: oversampling 4x (IIR a mezza banda, latenza di pochi campioni).
class OversampledPedal : public MonoModule
{
public:
    using MonoModule::MonoModule;

    void prepare (double sampleRate, int maxBlock) override
    {
        sr = sampleRate;
        os = std::make_unique<juce::dsp::Oversampling<float>> (1, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
        os->initProcessing ((size_t) maxBlock);
        prepareBypass (sampleRate, maxBlock);
        prepareDsp (sampleRate * 4.0);
        reset();
    }

    void process (float* x, int n) override
    {
        float* ch[1] = { x };
        juce::dsp::AudioBlock<float> block (ch, 1, (size_t) n);
        auto up = os->processSamplesUp (block);
        float* d = up.getChannelPointer (0);
        const int m = (int) up.getNumSamples();
        runDsp (d, m);
        os->processSamplesDown (block);
    }

    int getLatencySamples() const override { return os ? (int) std::ceil (os->getLatencyInSamples()) : 0; }

protected:
    virtual void prepareDsp (double osRate) = 0;
    virtual void runDsp (float* d, int m) = 0;

    double sr = 48000;
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
};

// ---------------------------------------------------------------------------------------------
// Overdrive stile Tube Screamer: stadio op-amp non invertente, diodi nel feedback (clipping morbido),
// gobba sui medi dal filtro 4.7k/47nF, tone passa-basso. Modello analogico semplificato.
class TubeScreamer : public OversampledPedal
{
public:
    TubeScreamer() : OversampledPedal ("od", "OVERDRIVE")
    {
        drive = &addFloat ("drive", "Drive", 0.0f, 10.0f, 5.0f, "", 1);
        tone  = &addFloat ("tone",  "Tone",  0.0f, 10.0f, 5.0f, "", 1);
        level = &addFloat ("level", "Level", 0.0f, 10.0f, 5.0f, "", 1);
    }

    void reset() override
    {
        if (os) os->reset();
        lpHp.reset(); lpFb.reset(); lpTone.reset(); dc.reset();
        driveSm = drive->get(); toneSm = tone->get(); levelSm = level->get();
    }

protected:
    void prepareDsp (double osRate) override
    {
        fs = osRate;
        smCoef = (float) (1.0 - std::exp (-1.0 / (0.020 * (osRate / 4.0))));
        lpHp.setCutoff (fs, 1.0 / (2 * kPi * 4.7e3 * 47e-9));      // ~720 Hz
        dc.setHighPass (fs, 25.0);
    }

    void runDsp (float* d, int m) override
    {
        for (int i = 0; i < m; ++i)
        {
            if ((i & 3) == 0)   // coefficienti a "control rate" (rate base)
            {
                driveSm += (drive->get() - driveSm) * smCoef;
                toneSm  += (tone->get()  - toneSm)  * smCoef;
                levelSm += (level->get() - levelSm) * smCoef;
                rf = 51e3 + 500e3 * (driveSm / 10.0);
                gain = rf / 4.7e3;
                k = 2.0 * kIs * rf;
                lpFb.setCutoff (fs, 1.0 / (2 * kPi * rf * 51e-12));
                lpTone.setCutoff (fs, 720.0 * std::pow (4.6, toneSm / 10.0));
                outGain = dbToGain ((levelSm - 5.0f) * 2.4f);
            }
            const float x = d[i];
            const float hp = x - lpHp.lp (x);                        // s*t1/(1+s*t1)
            const double vf = lpFb.lp (hp * (float) gain);          // * 1/(1+s*Rf*Cf)
            const float y = x + (float) diodeSolve (vf, k);          // uscita = ingresso + tensione sui diodi
            d[i] = dc.process (lpTone.lp (y)) * outGain;
        }
    }

private:
    Param *drive, *tone, *level;
    double fs = 192000, rf = 100e3, gain = 20, k = 1e-4;
    float driveSm = 5, toneSm = 5, levelSm = 5, smCoef = 0.001f, outGain = 1;
    OnePole lpHp, lpFb, lpTone;
    Biquad dc;
};

// ---------------------------------------------------------------------------------------------
// Distorsore stile Boss DS-1: booster a transistor (clipping asimmetrico), stadio op-amp ad alto gain,
// clipping duro a diodi verso massa, tone a "scoop" (LP/HP miscelati). Modello ispirato, non un clone esatto.
class Ds1 : public OversampledPedal
{
public:
    Ds1() : OversampledPedal ("dist", "DISTORTION")
    {
        dist  = &addFloat ("dist",  "Dist",  0.0f, 10.0f, 5.0f, "", 1);
        tone  = &addFloat ("tone",  "Tone",  0.0f, 10.0f, 5.0f, "", 1);
        level = &addFloat ("level", "Level", 0.0f, 10.0f, 5.0f, "", 1);
    }

    void reset() override
    {
        if (os) os->reset();
        lpIn.reset(); lpBoost.reset(); lpOpHp.reset(); lpOpFb.reset(); lpTone.reset(); lpTone2.reset(); dc.reset();
        distSm = dist->get(); toneSm = tone->get(); levelSm = level->get();
    }

protected:
    void prepareDsp (double osRate) override
    {
        fs = osRate;
        smCoef = (float) (1.0 - std::exp (-1.0 / (0.020 * (osRate / 4.0))));
        lpIn.setCutoff (fs, 60.0);
        lpBoost.setCutoff (fs, 9000.0);
        lpOpHp.setCutoff (fs, 1.0 / (2 * kPi * 470.0 * 2.2e-6));   // ~154 Hz
        lpTone.setCutoff (fs, 800.0);
        lpTone2.setCutoff (fs, 1100.0);
        dc.setHighPass (fs, 25.0);
    }

    void runDsp (float* d, int m) override
    {
        constexpr double kDiodeR = 2.2e3;
        for (int i = 0; i < m; ++i)
        {
            if ((i & 3) == 0)
            {
                distSm  += (dist->get()  - distSm)  * smCoef;
                toneSm  += (tone->get()  - toneSm)  * smCoef;
                levelSm += (level->get() - levelSm) * smCoef;
                rf = 2e3 + 100e3 * std::pow (distSm / 10.0, 1.6);
                opGain = rf / 470.0;
                lpOpFb.setCutoff (fs, 1.0 / (2 * kPi * rf * 100e-12));
                outGain = dbToGain ((levelSm - 5.0f) * 3.0f);
            }
            // 1) booster a transistor: HP 60 Hz, ~ +18 dB, clipping asimmetrico, LP 9 kHz
            const float x = d[i];
            float t = (x - lpIn.lp (x)) * 8.0f;
            t = t > 0 ? 0.9f * std::tanh (t / 0.9f) : 1.3f * std::tanh (t / 1.3f);
            t = lpBoost.lp (t);
            // 2) stadio op-amp non invertente: guadagno = Rf/Rg con HP ~154 Hz e LP da Rf*100pF
            const float hp = t - lpOpHp.lp (t);
            float y = t + lpOpFb.lp (hp * (float) opGain);
            y = 4.2f * std::tanh (y / 4.2f);                       // saturazione alle rotaie (9 V)
            // 3) clipping duro: diodi verso massa dietro resistenza serie
            y = (float) diodeSolve (y, 2.0 * kIs * kDiodeR);
            // 4) tone: LP 800 Hz / HP 1.1 kHz miscelati (scoop a metà corsa)
            const float lo = lpTone.lp (y), hi = y - lpTone2.lp (y);
            const float tn = toneSm / 10.0f;
            d[i] = dc.process (((1.0f - tn) * lo + tn * hi) * 2.4f) * outGain;
        }
    }

private:
    Param *dist, *tone, *level;
    double fs = 192000, rf = 50e3, opGain = 100;
    float distSm = 5, toneSm = 5, levelSm = 5, smCoef = 0.001f, outGain = 1;
    OnePole lpIn, lpBoost, lpOpHp, lpOpFb, lpTone, lpTone2;
    Biquad dc;
};
} // namespace dr
