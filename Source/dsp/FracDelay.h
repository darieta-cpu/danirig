#pragma once
#include <algorithm>
#include <vector>

namespace dr
{
// Linea di ritardo circolare con lettura frazionaria (Hermite cubico). Ritardo minimo: 2 campioni.
struct FracDelay
{
    void prepare (int maxDelaySamples)
    {
        int sz = 8;
        while (sz < maxDelaySamples + 8) sz <<= 1;
        buf.assign ((size_t) sz, 0.0f);
        mask = sz - 1;
        w = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
    int maxDelay() const { return mask - 4; }

    inline void write (float x) noexcept { buf[(size_t) w] = x; w = (w + 1) & mask; }

    inline float at (int k) const noexcept { return buf[(size_t) ((w - 1 - k) & mask)]; }

    // d = campioni dietro l'ultimo scritto (>= 2)
    inline float read (float d) const noexcept
    {
        d = std::clamp (d, 2.0f, (float) (mask - 4));
        const int i = (int) d;
        const float t = d - (float) i;
        const float ym1 = at (i - 1), y0 = at (i), y1 = at (i + 1), y2 = at (i + 2);
        const float c1 = 0.5f * (y1 - ym1);
        const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
        const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
        return ((c3 * t + c2) * t + c1) * t + y0;
    }

    std::vector<float> buf;
    int mask = 7, w = 0;
};
} // namespace dr
