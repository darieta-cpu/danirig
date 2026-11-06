#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include "dsp/NoiseGate.h"
#include "dsp/MultiMod.h"
#include "dsp/Pedals.h"
#include "dsp/NamAmp.h"
#include "dsp/CabIR.h"
#include "dsp/Delay.h"
#include "dsp/Reverb.h"

namespace dr
{
// Motore audio condiviso da app standalone e plugin. Catena (ordine di default):
//   Ingresso -> Gate -> Multi Mod -> Overdrive -> Distortion -> NAM Pedal -> NAM Amp -> Cabinet IR -> Delay -> Reverb -> Uscita
// Sezione mono fino al cab, poi stereo. I moduli PRE (davanti all'ampli) e POST (dopo il cab) sono riordinabili.
class RigEngine : public juce::AudioIODeviceCallback
{
public:
    RigEngine();

    // --- uso come callback di un device (app standalone)
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override { release(); }
    void audioDeviceIOCallbackWithContext (const float* const* in, int numIn, float* const* out, int numOut,
                                           int numSamples, const juce::AudioIODeviceCallbackContext&) override
    {
        process (in, numIn, out, numOut, numSamples);
    }

    // --- uso diretto (plugin e test)
    void prepare (double sampleRate, int blockSize, int driverLatencySamples = 0);
    void process (const float* const* in, int numIn, float* const* out, int numOut, int numSamples);
    void release() { prepared = false; }

    // Moduli
    NoiseGate gate;
    NamAmp pedal { "nampedal", "NAM PEDAL", false };   // profilo NAM di un pedale, davanti all'ampli
    TubeScreamer od;
    Ds1 dist;
    MultiMod mod;
    NamAmp amp { "amp", "NAM AMP", true };
    CabIR cab;
    DelayPedal delay;
    ReverbModule reverb;

    static constexpr int kPre = 5, kPost = 2;
    MonoModule*   preMods[kPre];      // 0 gate, 1 pedal NAM, 2 overdrive, 3 distortion, 4 multi mod
    StereoModule* postMods[kPost];    // 0 delay, 1 reverb
    std::vector<Module*> allModules() const;

    static std::vector<int> defaultPreOrder()  { return { 0, 4, 2, 3, 1 }; }   // gate, mod, od, dist, pedale NAM
    static std::vector<int> defaultPostOrder() { return { 0, 1 }; }
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

    // Stato per la UI (scritto dal thread audio)
    std::atomic<float> inPeak { 0.0f }, outPeak { 0.0f };
    std::atomic<double> sampleRate { 0.0 };
    std::atomic<int> bufferSize { 0 }, driverLatency { 0 }, chainLatency { 0 };
    std::atomic<int> stateVersion { 0 };     // incrementato quando viene caricato uno stato: la UI si aggiorna

    // Preset / stato
    std::unique_ptr<juce::XmlElement> saveState() const;
    void loadState (const juce::XmlElement& xml);
    void resetAll();
    void sanitizeOrders();
    void collectGarbage() { amp.collectGarbage(); pedal.collectGarbage(); }   // thread messaggi

private:
    std::atomic<uint32_t> preOrder { pack ({ 0, 4, 2, 3, 1 }) };
    std::atomic<uint32_t> postOrder { pack ({ 0, 1 }) };
    uint64_t lastOrderCode = 0;
    int fadeInLeft = 0, fadeInTotal = 1;
    bool prepared = false;
    int maxBlock = 512;
    std::vector<float> mono, left, right;
};
} // namespace dr
