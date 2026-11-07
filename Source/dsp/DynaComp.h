#pragma once
#include "Module.h"
#include "Biquad.h"
#include <cmath>

namespace dr
{
// Compressore stile MXR Dyna Comp: pochi controlli (Sens, Output), attacco veloce, rilascio medio, rapporto alto,
// ginocchio morbido, make-up automatico parziale (più Sens = più sustain). Zero latenza, nessun lookahead.
// Mix: compressione parallela (100 % = come il pedale).
class DynaComp : public MonoModule
{
public:
    DynaComp() : MonoModule ("comp", "COMPRESSOR")
    {
        sens   = &addFloat ("sens", "Sens", 0.0f, 10.0f, 5.0f, "", 1);
        output = &addFloat ("output", "Output", -12.0f, 12.0f, 0.0f, " dB", 1);
        mix    = &addFloat ("mix", "Mix", 0.0f, 100.0f, 100.0f, " %", 0);
    }

    void prepare (double sampleRate, int maxBlock) override
    {
        sr = sampleRate;
        detAtk = (float) (1.0 - std::exp (-1.0 / (0.0005 * sr)));    // rivelatore di picco: attacco 0.5 ms
        detRel = (float) std::exp (-1.0 / (0.050 * sr));             //                      rilascio 50 ms
        grAtk  = (float) (1.0 - std::exp (-1.0 / (0.0025 * sr)));    // gain reduction: attacco 2.5 ms
        grRel  = (float) (1.0 - std::exp (-1.0 / (0.160 * sr)));     //                 rilascio 160 ms
        prepareBypass (sampleRate, maxBlock);
        reset();
    }

    void reset() override { env = 0.0f; grSm = 0.0f; }

    void process (float* x, int n) override
    {
        const float s = sens->get();
        const float thr = -6.0f - s * 4.4f;                          // soglia: -6 dB (Sens 0) .. -50 dB (Sens 10)
        constexpr float ratio = 8.0f, knee = 8.0f;
        const float slope = 1.0f - 1.0f / ratio;
        const float makeup = 0.55f * (-thr) * slope;                 // recupero parziale del livello perso
        const float outDb = output->get();
        const float mixV = mix->get() * 0.01f;

        for (int i = 0; i < n; ++i)
        {
            const float in = x[i];
            const float a = std::abs (in);
            env = a > env ? env + detAtk * (a - env) : env * detRel;
            const float levelDb = 20.0f * std::log10 (std::max (env, 1.0e-6f));
            const float over = levelDb - thr;
            float gr = 0.0f;
            if (over >= knee * 0.5f) gr = over * slope;
            else if (over > -knee * 0.5f) { const float t = over + knee * 0.5f; gr = slope * t * t / (2.0f * knee); }
            grSm += (gr - grSm) * (gr > grSm ? grAtk : grRel);
            const float g = std::exp ((makeup - grSm + outDb) * 0.11512925f);
            x[i] = in * (1.0f - mixV) + in * g * mixV;
        }
    }

private:
    Param *sens, *output, *mix;
    double sr = 48000;
    float env = 0, grSm = 0, detAtk = 0.1f, detRel = 0.99f, grAtk = 0.05f, grRel = 0.001f;
};
} // namespace dr
