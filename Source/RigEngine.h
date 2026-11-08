#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include "dsp/NoiseGate.h"
#include "dsp/DynaComp.h"
#include "dsp/MultiMod.h"
#include "dsp/Pedals.h"
#include "dsp/NamAmp.h"
#include "dsp/CabIR.h"
#include "dsp/Delay.h"
#include "dsp/Reverb.h"

namespace dr
{
// Motore audio condiviso da app standalone e plugin. Catena (ordine di default):
//   Ingresso -> Gate -> Compressor -> Multi Mod -> Overdrive -> Distortion -> NAM Pedal -> NAM Amp -> Cabinet IR -> Delay -> Reverb -> Uscita
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
    DynaComp comp;
    NamAmp amp { "amp", "NAM AMP", true };
    CabIR cab;
    DelayPedal delay;
    ReverbModule reverb;

    static constexpr int kPre = 6, kPost = 2;
    MonoModule*   preMods[kPre];      // 0 gate, 1 pedal NAM, 2 overdrive, 3 distortion, 4 multi mod
    StereoModule* postMods[kPost];    // 0 delay, 1 reverb
    std::vector<Module*> allModules() const;

    static std::vector<int> defaultPreOrder()  { return { 0, 5, 4, 2, 3, 1 }; }   // gate, comp, mod, od, dist, pedale NAM
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
    std::unique_ptr<juce::XmlElement> saveState (bool includeBank = true) const;
    void loadState (const juce::XmlElement& xml);

    // Banco patch/scene (solo thread messaggi).
    //  - PATCH: stato completo (parametri, ordine dei moduli, modello NAM, pedale NAM, IR).
    //  - SCENE: istantanea dei soli parametri dei moduli (on/off inclusi) dentro il patch corrente: cambio istantaneo,
    //           senza ricaricare modelli.
    static constexpr int kSlots = 3;
    bool sceneMode = false;
    int currentPatch = -1, currentScene = -1;
    bool hasSlot (bool scene, int i) const;
    juce::String slotName (bool scene, int i) const;
    void setSlotName (bool scene, int i, const juce::String& name);
    void storeSlot (bool scene, int i);
    bool recallSlot (bool scene, int i);
    std::unique_ptr<juce::XmlElement> saveSnapshot() const;      // solo parametri dei moduli
    void applySnapshot (const juce::XmlElement& snap);
    void resetAll();
    void sanitizeOrders();

    // Preset portatili: i percorsi relativi (es. "models/amp.nam") si risolvono rispetto alla cartella del preset.
    static void resolveRelativePaths (juce::XmlElement& xml, const juce::File& baseDir);
    // Salva il preset corrente (con il banco) in <cartella>/<nome>/<nome>.dprig copiando modelli NAM e IR
    // in models/ e ir/ e scrivendo percorsi relativi. report: avvisi (file non trovati, copie fallite).
    juce::Result exportBundle (const juce::File& presetFile, juce::StringArray& report, int& numModels, int& numIrs) const;
    // File che lo stato caricato non ha trovato (modelli NAM / IR). Svuota la lista. Thread messaggi.
    juce::StringArray takeWarnings();
    std::atomic<bool> hasWarnings { false };
    void collectGarbage() { amp.collectGarbage(); pedal.collectGarbage(); }   // thread messaggi

private:
    mutable juce::CriticalSection warnLock;
    juce::StringArray warnings;
    std::atomic<uint32_t> preOrder { pack ({ 0, 5, 4, 2, 3, 1 }) };
    std::atomic<uint32_t> postOrder { pack ({ 0, 1 }) };
    struct PatchSlot
    {
        std::unique_ptr<juce::XmlElement> state;
        juce::String name;
        std::unique_ptr<juce::XmlElement> scene[kSlots];
        juce::String sceneName[kSlots];
    } patches[kSlots];
    int sceneOwner() const { return currentPatch >= 0 ? currentPatch : 0; }
    void loadBank (const juce::XmlElement& bank);
    std::unique_ptr<juce::XmlElement> saveBank() const;

    uint64_t lastOrderCode = 0;
    int fadeInLeft = 0, fadeInTotal = 1;
    bool prepared = false;
    int maxBlock = 512;
    std::vector<float> mono, left, right;
};
} // namespace dr
