#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>

namespace dr
{
// Analisi di un file IR per la visualizzazione: dati generali, spettro (160 punti log tra 20 Hz e 20 kHz,
// relativo alla zona 300 Hz - 3 kHz) e inviluppo temporale. Da chiamare in un thread di background.
struct IrInfo
{
    static constexpr int kSpectrumPoints = 160, kEnvelopePoints = 160;
    juce::String path;
    bool valid = false;
    double sampleRate = 0, lengthMs = 0;
    int channels = 0;
    float peakDb = -100.0f;
    std::vector<float> spectrumDb, envelope;     // envelope: 0..1 (picco per intervallo, normalizzato)

    static float frequencyAt (int i, double sampleRate)
    {
        const double fMax = std::min (20000.0, sampleRate * 0.49);
        return (float) (20.0 * std::pow (fMax / 20.0, (double) i / (kSpectrumPoints - 1)));
    }
};

inline IrInfo analyseIr (const juce::File& file)
{
    IrInfo info;
    info.path = file.getFullPathName();
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0) return info;

    info.sampleRate = reader->sampleRate;
    info.channels = (int) reader->numChannels;
    info.lengthMs = 1000.0 * (double) reader->lengthInSamples / reader->sampleRate;

    const int total = (int) std::min<juce::int64> (reader->lengthInSamples, 1 << 17);
    juce::AudioBuffer<float> buf (1, total);
    buf.clear();
    reader->read (&buf, 0, total, 0, true, false);                  // canale sinistro: quello che usa il cabinet
    const float* x = buf.getReadPointer (0);

    float peak = 0.0f;
    for (int i = 0; i < total; ++i) peak = std::max (peak, std::abs (x[i]));
    info.peakDb = peak > 1.0e-9f ? 20.0f * std::log10 (peak) : -100.0f;

    // spettro: FFT da 32768 punti sui primi campioni, con dissolvenza finale per non troncare la coda
    constexpr int order = 15, N = 1 << order;
    const int len = std::min (total, N);
    std::vector<float> data ((size_t) N * 2, 0.0f);
    for (int i = 0; i < len; ++i)
    {
        float w = 1.0f;
        const int fadeStart = len * 3 / 4;
        if (i > fadeStart) w = 0.5f + 0.5f * std::cos (juce::MathConstants<float>::pi * (float) (i - fadeStart) / (float) (len - fadeStart));
        data[(size_t) i] = x[i] * w;
    }
    juce::dsp::FFT fft (order);
    fft.performFrequencyOnlyForwardTransform (data.data());

    const double binHz = info.sampleRate / N;
    info.spectrumDb.resize (IrInfo::kSpectrumPoints);
    for (int i = 0; i < IrInfo::kSpectrumPoints; ++i)
    {
        const double f = IrInfo::frequencyAt (i, info.sampleRate);
        const double lo = f * std::pow (2.0, -1.0 / 6.0) / binHz, hi = f * std::pow (2.0, 1.0 / 6.0) / binHz;
        const int b0 = std::max (1, (int) std::floor (lo)), b1 = std::min (N / 2 - 1, std::max (b0, (int) std::ceil (hi)));
        double p = 0.0; int n = 0;
        for (int b = b0; b <= b1; ++b) { p += (double) data[(size_t) b] * data[(size_t) b]; ++n; }
        info.spectrumDb[(size_t) i] = (float) (10.0 * std::log10 (p / std::max (n, 1) + 1.0e-18));
    }
    double ref = 0.0; int nref = 0;
    for (int i = 0; i < IrInfo::kSpectrumPoints; ++i)
    {
        const float f = IrInfo::frequencyAt (i, info.sampleRate);
        if (f >= 300.0f && f <= 3000.0f) { ref += info.spectrumDb[(size_t) i]; ++nref; }
    }
    ref /= std::max (nref, 1);
    for (auto& v : info.spectrumDb) v = juce::jlimit (-60.0f, 30.0f, v - (float) ref);

    // inviluppo temporale
    info.envelope.assign (IrInfo::kEnvelopePoints, 0.0f);
    for (int b = 0; b < IrInfo::kEnvelopePoints; ++b)
    {
        const int s0 = (int) ((juce::int64) b * total / IrInfo::kEnvelopePoints), s1 = std::max (s0 + 1, (int) ((juce::int64) (b + 1) * total / IrInfo::kEnvelopePoints));
        float m = 0.0f;
        for (int i = s0; i < s1 && i < total; ++i) m = std::max (m, std::abs (x[i]));
        info.envelope[(size_t) b] = peak > 0.0f ? m / peak : 0.0f;
    }
    info.valid = true;
    return info;
}

// Risposta dei filtri del cabinet (2° ordine, Butterworth): low cut + high cut, in dB
inline float cabFilterDb (float f, float lowCutHz, float highCutHz)
{
    const double hp = 1.0 + std::pow ((double) lowCutHz / f, 4.0), lp = 1.0 + std::pow ((double) f / highCutHz, 4.0);
    return (float) (-10.0 * std::log10 (hp) - 10.0 * std::log10 (lp));
}
} // namespace dr
