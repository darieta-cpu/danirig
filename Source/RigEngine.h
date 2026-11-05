#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include "dsp/NoiseGate.h"
#include "dsp/Pedals.h"
#include "dsp/NamAmp.h"
#include "dsp/CabIR.h"
#include "dsp/Delay.h"
#include "dsp/Reverb.h"

namespace dr
{
// Motore audio: riceve il callback del device e fa girare la catena
//   Ingresso -> Gate -> Overdrive -> Distortion -> NAM Amp -> Cabinet IR -> Delay -> Reverb -> Uscita
// Sezione mono (fino al cab) + sezione stereo (delay, reverb). L'ordine dentro ogni sezione è un semplice vettore.
class RigEngine : public juce::AudioIODeviceCallback
{
public:
    RigEngine();

    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                           float* const* outputChannelData, int numOutputChannels,
                                           int numSamples, const juce::AudioIODeviceCallbackContext& context) override;

    // Moduli
    NoiseGate gate;
    TubeScreamer od;
    Ds1 dist;
    NamAmp amp;
    CabIR cab;
    DelayPedal delay;
    ReverbModule reverb;

    std::vector<MonoModule*>   monoChain;
    std::vector<StereoModule*> stereoChain;
    std::vector<Module*> allModules() const;

    // Parametri globali
    ParamOwner globals;
    Param *inputChannel, *inputTrim, *master;

    // Stato per la UI (letto da timer, scritto dal thread audio)
    std::atomic<float> inPeak { 0.0f }, outPeak { 0.0f };
    std::atomic<double> sampleRate { 0.0 };
    std::atomic<int> bufferSize { 0 }, driverLatency { 0 }, chainLatency { 0 };

    // Preset / stato
    std::unique_ptr<juce::XmlElement> saveState() const;
    void loadState (const juce::XmlElement& xml);
    void resetAll();

private:
    bool prepared = false;
    int maxBlock = 512;
    std::vector<float> mono, left, right;
};
} // namespace dr
