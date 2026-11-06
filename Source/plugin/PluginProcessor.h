#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../RigEngine.h"

// Plugin (VST3 / AU): stesso motore e stessa interfaccia dell'app standalone.
// Ingresso mono o stereo (si sceglie il canale con "Input"), uscita stereo.
class DaniRigProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    DaniRigProcessor();
    ~DaniRigProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override { engine.prepare (sampleRate, samplesPerBlock); }
    void releaseResources() override { engine.release(); }
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "DaniRig"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    dr::RigEngine engine;

private:
    void timerCallback() override { engine.collectGarbage(); }   // libera i modelli NAM scartati anche a editor chiuso
};
