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
    juce::String name, error;              // name = nome del file; error = motivo del fallimento
    double modelRate = 0;
    bool loaded = false, loading = false, slimmable = false;

    // Metadati letti dal file .nam (quelli che l'autore del profilo ha compilato)
    juce::String architecture, version, title, modeledBy, gearMake, gearModel, gearType, toneType, date;
    double loudness = 0, inputDbu = 0, outputDbu = 0;
    bool hasLoudness = false, hasInputDbu = false, hasOutputDbu = false;

    juce::String displayName() const { return title.isNotEmpty() ? title : name; }
    juce::String gearLine() const
    {
        juce::StringArray parts;
        if (gearMake.isNotEmpty() || gearModel.isNotEmpty()) parts.add ((gearMake + " " + gearModel).trim());
        if (gearType.isNotEmpty()) parts.add (gearType);
        if (toneType.isNotEmpty()) parts.add (toneType);
        return parts.joinIntoString (" - ");
    }
    juce::String technicalLine() const
    {
        juce::StringArray parts;
        parts.add (architecture == "SlimmableContainer" ? juce::String ("A2 (slimmable)") : architecture.isNotEmpty() ? architecture : juce::String ("NAM"));
        if (modelRate > 0) parts.add (juce::String (modelRate / 1000.0, 1) + " kHz");
        if (hasInputDbu) parts.add ("in " + juce::String (inputDbu, 1) + " dBu");
        if (hasOutputDbu) parts.add ("out " + juce::String (outputDbu, 1) + " dBu");
        if (hasLoudness) parts.add ("loudness " + juce::String (loudness, 1) + " dB");
        return parts.joinIntoString (" - ");
    }
    juce::String tooltip() const
    {
        juce::StringArray l;
        l.add (displayName());
        if (modeledBy.isNotEmpty()) l.add ("Creato da: " + modeledBy);
        if (gearLine().isNotEmpty()) l.add (gearLine());
        l.add (technicalLine());
        if (date.isNotEmpty()) l.add ("Data: " + date);
        return l.joinIntoString ("\n");
    }
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
