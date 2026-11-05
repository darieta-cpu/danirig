#pragma once
#include "Module.h"
#include "Biquad.h"
#include "Resampler.h"
#include <juce_core/juce_core.h>
#include <memory>

namespace dr
{
struct ModelInfo
{
    juce::String name, error;
    double modelRate = 0;
    bool loaded = false, loading = false, slimmable = false;
};

// Modulo ampli: carica profili NAM (.nam, incl. A2 Full/Lite) in un thread di background,
// li passa al thread audio senza lock e fa un crossfade tra vecchio e nuovo modello.
class NamAmp : public MonoModule
{
public:
    explicit NamAmp (const juce::String& id = "amp", const juce::String& titleText = "NAM AMP", bool withEq = true);
    ~NamAmp() override;

    void prepare (double sampleRate, int maxBlock) override;
    void reset() override;
    void process (float* x, int n) override;
    int  getLatencySamples() const override;

    // Thread messaggi
    void loadModelAsync (const juce::File& file);
    void reloadCurrent();                     // es. dopo aver cambiato Full/Lite
    juce::String getModelPath() const;
    ModelInfo getInfo() const;
    void collectGarbage();                    // libera i modelli scartati dal thread audio

    Param *inputDb, *bass = nullptr, *mid = nullptr, *treble = nullptr, *outputDb, *normalize, *quality;   // bass/mid/treble: solo con EQ

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
} // namespace dr
