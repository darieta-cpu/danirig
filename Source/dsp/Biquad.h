#pragma once
#include <cmath>

namespace dr
{
constexpr double kPi = 3.14159265358979323846;

inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }

// Biquad RBJ (forma TDF2, stato in double) senza allocazioni: sicuro nel thread audio.
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    void reset() noexcept { z1 = z2 = 0; }

    float process (float x) noexcept
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return (float) y;
    }

    void set (double nb0, double nb1, double nb2, double a0, double na1, double na2) noexcept
    {
        b0 = nb0 / a0; b1 = nb1 / a0; b2 = nb2 / a0; a1 = na1 / a0; a2 = na2 / a0;
    }

    void setLowPass (double sr, double f, double q = 0.7071) noexcept
    {
        f = clampF (f, sr); const double w = 2 * kPi * f / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
        set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void setHighPass (double sr, double f, double q = 0.7071) noexcept
    {
        f = clampF (f, sr); const double w = 2 * kPi * f / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
        set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void setPeak (double sr, double f, double q, double gainDb) noexcept
    {
        f = clampF (f, sr); const double A = std::pow (10.0, gainDb / 40), w = 2 * kPi * f / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
        set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void setLowShelf (double sr, double f, double gainDb, double q = 0.7071) noexcept
    {
        f = clampF (f, sr); const double A = std::pow (10.0, gainDb / 40), w = 2 * kPi * f / sr, c = std::cos (w);
        const double al = std::sin (w) / (2 * q), t = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) - (A - 1) * c + t), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - t),
             (A + 1) + (A - 1) * c + t, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - t);
    }
    void setHighShelf (double sr, double f, double gainDb, double q = 0.7071) noexcept
    {
        f = clampF (f, sr); const double A = std::pow (10.0, gainDb / 40), w = 2 * kPi * f / sr, c = std::cos (w);
        const double al = std::sin (w) / (2 * q), t = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * c + t), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - t),
             (A + 1) - (A - 1) * c + t, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - t);
    }

private:
    static double clampF (double f, double sr) noexcept { return f < 5.0 ? 5.0 : (f > sr * 0.49 ? sr * 0.49 : f); }
};

// Filtro a un polo TPT (zero-delay feedback): LP e HP dallo stesso stato.
struct OnePole
{
    double g = 0.1, s = 0;
    void setCutoff (double sr, double fc) noexcept
    {
        if (fc > sr * 0.45) fc = sr * 0.45;
        g = std::tan (kPi * fc / sr);
    }
    void reset() noexcept { s = 0; }
    // restituisce il passa-basso; il passa-alto è x - lp
    float lp (float x) noexcept
    {
        const double v = (x - s) * g / (1 + g);
        const double y = v + s;
        s = y + v;
        return (float) y;
    }
};
} // namespace dr
