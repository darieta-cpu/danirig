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
    NamAmp pedal { "nampedal", "NAM PEDAL", false };   // profilo NAM di un pedale, davanti all'ampli
    TubeScreamer od;
    Ds1 dist;
    NamAmp amp { "amp", "NAM AMP", true };
    CabIR cab;
    DelayPedal delay;
    ReverbModule reverb;

    // Gruppi riordinabili: PRE (davanti all'ampli, mono) e POST (dopo il cab, stereo).
    // L'ordine è una permutazione impacchettata in un solo atomic: il thread audio la legge una volta per blocco.
    static constexpr int kPre = 4, kPost = 2;
    MonoModule*   preMods[kPre];      // gate, pedal, od, dist
    StereoModule* postMods[kPost];    // delay, reverb
    std::vector<Module*> allModules() const;

    std::vector<int> getPreOrder() const  { return unpack (preOrder.load(), kPre); }
    std::vector<int> getPostOrder() const { return unpack (postOrder.load(), kPost); }
    void setPreOrder (const std::vector<int>& o)  { preOrder.store (pack (o)); }
    void setPostOrder (const std::vector<int>& o) { postOrder.store (pack (o)); }
    int preIndexOf (const Module* m) const  { for (int i = 0; i < kPre; ++i)  if (preMods[i] == m)  return i; return -1; }
    int postIndexOf (const Module* m) const { for (int i = 0; i < kPost; ++i) if (postMods[i] == m) return i; return -1; }
    static uint32_t pack (const std::vector<int>& o) { uint32_t c = 0; for (size_t i = 0; i < o.size(); ++i) c |= (uint32_t) (o[i] & 0xF) << (4 * i); return c; }
    static std::vector<int> unpack (uint32_t c, int n) { std::vector<int> o; for (int i = 0; i < n; ++i) o.push_back ((int) ((c >> (4 * i)) & 0xF)); return o; }

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
    void sanitizeOrders();

private:
    std::atomic<uint32_t> preOrder { pack ({ 0, 2, 3, 1 }) };   // gate, od, dist, pedal
    std::atomic<uint32_t> postOrder { pack ({ 0, 1 }) };        // delay, reverb
    uint32_t lastOrderCode = 0;
    int fadeInLeft = 0, fadeInTotal = 1;
    bool prepared = false;
    int maxBlock = 512;
    std::vector<float> mono, left, right;
};
} // namespace dr
