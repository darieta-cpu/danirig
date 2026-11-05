#pragma once
#include "Module.h"
#include "Biquad.h"
#include "FracDelay.h"

namespace dr
{
// Riverbero FDN a 8 linee: pre-delay, 4 allpass di diffusione, matrice di Hadamard, damping per linea,
// leggera modulazione dei ritardi (niente suono metallico), uscita stereo decorrelata.
class ReverbModule : public StereoModule
{
public:
    static constexpr int N = 8;

    ReverbModule() : StereoModule ("reverb", "REVERB")
    {
        decay    = &addFloat ("decay", "Decay", 0.3f, 12.0f, 2.5f, " s", 1);
        size     = &addFloat ("size", "Size", 0.0f, 10.0f, 5.0f, "", 1);
        predelay = &addFloat ("predelay", "Pre-Delay", 0.0f, 200.0f, 15.0f, " ms", 0);
        damp     = &addFloat ("damp", "Damping", 0.0f, 10.0f, 5.0f, "", 1);
        lowcut   = &addFloat ("lowcut", "Low Cut", 20.0f, 600.0f, 120.0f, " Hz", 0);
        mix      = &addFloat ("mix", "Mix", 0.0f, 100.0f, 22.0f, " %", 0);
    }

    void prepare (double sampleRate, int maxBlock) override
    {
        sr = sampleRate;
        const double scale = sr / 48000.0;
        for (int i = 0; i < N; ++i)
            lines[i].prepare ((int) (baseLen[i] * 2.2 * scale) + 64);
        pre.prepare ((int) (0.21 * sr));
        for (int i = 0; i < 4; ++i) ap[i].prepare ((int) (apLen[i] * scale) + 8, (int) (apLen[i] * scale));
        prepareBypass (sampleRate, maxBlock);
        reset();
    }

    void reset() override
    {
        for (auto& d : lines) d.clear();
        pre.clear();
        for (auto& a : ap) a.clear();
        for (auto& v : dampState) v = 0;
        hpL.reset(); hpR.reset();
        sizeSm = size->get();
        for (int i = 0; i < N; ++i) lfoPhase[i] = (double) i / N;
    }

    void process (float* l, float* r, int n) override
    {
        const double scale = sr / 48000.0;
        const float rt60 = decay->get();
        const float dampHz = 16000.0f * std::pow (0.1f, damp->get() / 10.0f * 1.2f) + 1500.0f;   // ~17.5k .. ~2k
        const float dA = (float) (1.0 - std::exp (-2.0 * kPi * dampHz / sr));
        const float wetG = mix->get() * 0.01f * 2.4f;
        const int preS = (int) (predelay->get() * 0.001 * sr);
        hpL.setCutoff (sr, lowcut->get()); hpR.setCutoff (sr, lowcut->get());
        const float modDepth = (float) (0.0007 * sr);          // ~0.7 ms
        constexpr float norm = 0.35355339f;                     // 1/sqrt(8)

        for (int i = 0; i < n; ++i)
        {
            sizeSm += (size->get() - sizeSm) * 0.0003f;
            const float sizeF = 0.55f + sizeSm * 0.15f;         // 0.55 .. 2.05
            const float in = 0.5f * (l[i] + r[i]);

            pre.write (in);
            float x = preS > 2 ? pre.read ((float) preS) : in;
            for (auto& a : ap) x = a.process (x);

            float y[N];
            for (int k = 0; k < N; ++k)
            {
                lfoPhase[k] += lfoRate[k] / sr; if (lfoPhase[k] >= 1.0) lfoPhase[k] -= 1.0;
                const float lenS = (float) (baseLen[k] * scale) * sizeF;
                float v = lines[k].read (lenS + modDepth * std::sin ((float) (2 * kPi * lfoPhase[k])));
                dampState[k] += dA * (v - dampState[k]);                          // damping HF
                const float g = std::pow (10.0f, -3.0f * (lenS / (float) sr) / rt60);
                y[k] = dampState[k] * g;
            }

            float outL = 0, outR = 0;
            for (int k = 0; k < N; ++k) { outL += y[k] * sL[k]; outR += y[k] * sR[k]; }

            // Hadamard 8x8 (trasformata di Walsh-Hadamard veloce), normalizzata
            for (int len = 1; len < N; len <<= 1)
                for (int a = 0; a < N; a += len << 1)
                    for (int b = a; b < a + len; ++b) { const float u = y[b], w = y[b + len]; y[b] = u + w; y[b + len] = u - w; }
            for (int k = 0; k < N; ++k) lines[k].write (x * 0.25f + y[k] * norm);

            const float wl = hpL.lp (outL * 0.5f), wr = hpR.lp (outR * 0.5f);
            l[i] += wetG * (outL * 0.5f - wl);
            r[i] += wetG * (outR * 0.5f - wr);
        }
    }

private:
    struct Allpass
    {
        void prepare (int maxLen, int len_) { buf.assign ((size_t) maxLen + 1, 0.0f); len = len_; pos = 0; }
        void clear() { std::fill (buf.begin(), buf.end(), 0.0f); pos = 0; }
        float process (float in)
        {
            const float d = buf[(size_t) pos];
            const float w = in + g * d;
            buf[(size_t) pos] = w;
            if (++pos >= len) pos = 0;
            return d - g * w;
        }
        std::vector<float> buf; int len = 1, pos = 0; float g = 0.6f;
    };

    Param *decay, *size, *predelay, *damp, *lowcut, *mix;
    FracDelay lines[N], pre;
    Allpass ap[4];
    OnePole hpL, hpR;
    float dampState[N] {};
    double lfoPhase[N] {};
    double sr = 48000;
    float sizeSm = 5;

    static constexpr double baseLen[N] = { 1087, 1283, 1481, 1699, 1907, 2137, 2381, 2657 };
    static constexpr int apLen[4] = { 142, 107, 379, 277 };
    static constexpr double lfoRate[N] = { 0.11, 0.17, 0.23, 0.29, 0.13, 0.19, 0.31, 0.37 };
    static constexpr float sL[N] = { 1, 1, -1, -1, 1, -1, 1, -1 };
    static constexpr float sR[N] = { 1, -1, 1, -1, -1, 1, 1, -1 };
};
} // namespace dr
