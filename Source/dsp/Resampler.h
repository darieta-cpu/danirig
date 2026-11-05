#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace dr
{
// Resampler streaming a sinc finestrato CORTO (12 lobi per lato) pensato per il live:
// latenza ~12 campioni al rate d'ingresso (0.25 ms a 48 kHz). Nessuna allocazione dopo prepare().
class SincResampler
{
public:
    static constexpr int kLobes = 12;
    static constexpr int kTableRes = 512;

    // ratio = campioni d'ingresso consumati per ogni campione d'uscita (in/out)
    void prepare (double ratioInOverOut, int maxPushSamples)
    {
        ratio = ratioInOverOut;
        fc = ratio > 1.0 ? 1.0 / ratio : 1.0;
        half = (int) std::ceil (kLobes / fc);
        const size_t cap = (size_t) (maxPushSamples + 4 * half + 64);
        buf.assign (cap, 0.0f);
        buildTable();
        reset();
    }

    void reset()
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        size = half * 2;   // silenzio iniziale = latenza
        pos = (double) half;
    }

    int latencyInputSamples() const { return half; }

    void push (const float* in, int n)
    {
        const int room = (int) buf.size() - size;
        n = std::min (n, std::max (room, 0));
        std::copy (in, in + n, buf.begin() + size);
        size += n;
    }

    // produce fino a maxOut campioni; ritorna quanti ne ha prodotti
    int pull (float* out, int maxOut)
    {
        int produced = 0;
        while (produced < maxOut)
        {
            const int i0 = (int) std::floor (pos);
            if (i0 + half >= size) break;
            const double frac = pos - i0;
            double acc = 0.0, norm = 0.0;
            for (int k = -half + 1; k <= half; ++k)
            {
                const double d = std::abs (k - frac) * fc;           // distanza scalata, in lobi
                const double w = kernel (d);
                acc += w * buf[(size_t) (i0 + k)];
                norm += w;
            }
            out[produced++] = (float) (norm != 0.0 ? acc / norm : 0.0);
            pos += ratio;
        }
        // scarta lo storico non più necessario
        const int drop = (int) std::floor (pos) - half;
        if (drop > 0)
        {
            std::copy (buf.begin() + drop, buf.begin() + size, buf.begin());
            size -= drop;
            pos -= drop;
        }
        return produced;
    }

private:
    double kernel (double d) const
    {
        if (d >= kLobes) return 0.0;
        const double x = d * kTableRes;
        const int i = (int) x;
        const double f = x - i;
        return table[(size_t) i] * (1.0 - f) + table[(size_t) i + 1] * f;
    }

    void buildTable()
    {
        table.assign ((size_t) (kLobes * kTableRes + 2), 0.0);
        const double pi = 3.14159265358979323846;
        for (size_t i = 0; i < table.size(); ++i)
        {
            const double d = (double) i / kTableRes;
            const double s = d < 1e-9 ? 1.0 : std::sin (pi * d) / (pi * d);
            const double t = std::min (d / kLobes, 1.0);
            const double win = 0.42 + 0.5 * std::cos (pi * t) + 0.08 * std::cos (2 * pi * t); // Blackman
            table[i] = s * win;
        }
    }

    double ratio = 1.0, fc = 1.0, pos = 0.0;
    int half = kLobes, size = 0;
    std::vector<float> buf;
    std::vector<double> table;
};

// Porta un processore "a rate fisso" (es. un modello NAM a 48 kHz) su un device a rate diverso.
class RateBridge
{
public:
    void prepare (double deviceRate, double modelRate, int maxBlock)
    {
        active = std::abs (deviceRate - modelRate) > 0.5;
        if (! active) { modelMax = maxBlock; return; }
        const double down = deviceRate / modelRate;     // in per out
        modelMax = (int) std::ceil (maxBlock / down) + 8;
        downR.prepare (down, maxBlock);
        upR.prepare (1.0 / down, modelMax + 8);
        modelBuf.assign ((size_t) modelMax + 64, 0.0f);
        // pre-riempimento: evita underrun del ricampionatore in uscita
        std::vector<float> zeros ((size_t) kPrefill, 0.0f);
        upR.push (zeros.data(), (int) zeros.size());
        extraLatency = kPrefill;
    }

    bool isActive() const { return active; }
    int  maxModelBlock() const { return modelMax; }

    // Latenza aggiunta, in campioni del device (approssimata).
    int latencyDeviceSamples (double deviceRate, double modelRate) const
    {
        if (! active) return 0;
        return downR.latencyInputSamples() + (int) ((upR.latencyInputSamples() + extraLatency) * deviceRate / modelRate);
    }

    template <typename Fn>
    void process (float* x, int n, Fn&& fn)
    {
        if (! active) { fn (x, n); return; }
        downR.push (x, n);
        const int m = downR.pull (modelBuf.data(), modelMax);
        if (m > 0)
        {
            fn (modelBuf.data(), m);
            upR.push (modelBuf.data(), m);
        }
        const int got = upR.pull (x, n);
        if (got < n) ++underruns;                                              // non dovrebbe accadere
        for (int i = got; i < n; ++i) x[i] = got > 0 ? x[got - 1] : 0.0f;
    }

    void reset()
    {
        if (! active) return;
        downR.reset(); upR.reset();
        std::vector<float> zeros ((size_t) kPrefill, 0.0f);
        upR.push (zeros.data(), (int) zeros.size());
    }

    int underruns = 0;

private:
    static constexpr int kPrefill = 16;
    bool active = false;
    int modelMax = 512, extraLatency = 0;
    SincResampler downR, upR;
    std::vector<float> modelBuf;
};
} // namespace dr
