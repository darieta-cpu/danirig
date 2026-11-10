#pragma once
#include "Biquad.h"
#include <algorithm>
#include <cmath>

namespace dr
{
// Potenziometro di livello/volume come nei pedali veri (scala 0..10):
//   0        = segnale completamente chiuso (silenzio)
//   0 .. 6   = curva audio (log): 3 = circa -15 dB, 5 = circa -4 dB
//   6        = unity gain (0 dB)
//   6 .. 10  = boost, fino a +maxBoostDb a fondo corsa
inline float levelTaperGain (float pos, float maxBoostDb = 12.0f) noexcept
{
    pos = std::clamp (pos, 0.0f, 10.0f);
    if (pos <= 6.0f)
    {
        const float t = pos / 6.0f;
        return t * t * std::sqrt (t);                        // t^2.5
    }
    return dbToGain ((pos - 6.0f) * 0.25f * maxBoostDb);
}
} // namespace dr
