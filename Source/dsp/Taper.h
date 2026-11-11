#pragma once
#include "Biquad.h"
#include <algorithm>
#include <cmath>

namespace dr
{
// Potenziometro di livello/volume come nei pedali veri (scala 0..10, unity a metà corsa):
//   0        = segnale completamente chiuso (silenzio)
//   0 .. 5   = curva audio (log): 3 = circa -11 dB
//   5        = unity gain (0 dB), manopola a metà corsa
//   5 .. 10  = boost, fino a +maxBoostDb a fondo corsa
inline float levelTaperGain (float pos, float maxBoostDb = 12.0f) noexcept
{
    pos = std::clamp (pos, 0.0f, 10.0f);
    if (pos <= 5.0f)
    {
        const float t = pos / 5.0f;
        return t * t * std::sqrt (t);                        // t^2.5
    }
    return dbToGain ((pos - 5.0f) * 0.2f * maxBoostDb);
}

// Conversione dei livelli salvati con la scala precedente (unity a 6) a quella attuale (unity a 5),
// a parità di guadagno: sotto l'unity 5/6 del valore, sopra l'unity la pendenza passa da 3 dB a 2.4 dB per punto.
inline float migrateLevelV2toV3 (float oldPos) noexcept
{
    oldPos = std::clamp (oldPos, 0.0f, 10.0f);
    return oldPos <= 6.0f ? oldPos * 5.0f / 6.0f : 5.0f + (oldPos - 6.0f) * 1.25f;
}
} // namespace dr
