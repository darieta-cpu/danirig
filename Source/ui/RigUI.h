#pragma once
#include "../RigEngine.h"
#include "Panels.h"

namespace dr
{
// Interfaccia completa del rig (1280x800 scalata alla finestra). Usata sia dall'app standalone sia dall'editor del plugin.
class RigUI : public juce::Component, private juce::Timer
{
public:
    // audioSettings: vuota nel plugin (il device lo gestisce l'host). cpu: percentuale CPU 0..1 (vuota nel plugin).
    RigUI (RigEngine& e, std::function<void()> audioSettings = {}, std::function<double()> cpu = {});
    ~RigUI() override;

    void paint (juce::Graphics& g) override { g.fillAll (ui::col::bg); }
    void resized() override;
    void refreshAll();

    static constexpr int W = 1280, H = 890;

private:
    class Content : public juce::Component
    {
    public:
        void paint (juce::Graphics& g) override;
        bool showAudioButton = true;
    };

    struct Group { std::vector<ui::PanelBase*> panels, order; int x = 0, y = 0, gap = 10; };
    void timerCallback() override;
    void layoutGroup (Group& g, ui::PanelBase* skip, bool animate);
    void handleReorder (Group& g, ui::PanelBase& p, int phase);
    void applyOrderFromEngine (bool animate);
    void pushOrderToEngine();
    void savePreset();
    void loadPreset();

    RigEngine& engine;
    std::function<void()> audioSettingsCb;
    std::function<double()> cpuCb;
    ui::RigLookAndFeel laf;
    Content content;

    std::unique_ptr<ui::GatePanel> gatePanel;
    std::unique_ptr<ui::MultiModPanel> modPanel;
    std::unique_ptr<ui::PedalPanel> odPanel, distPanel, compPanel;
    std::unique_ptr<ui::PatchBar> patchBar;
    std::unique_ptr<ui::NamPedalPanel> pedalPanel;
    std::unique_ptr<ui::AmpPanel> ampPanel;
    std::unique_ptr<ui::CabPanel> cabPanel;
    std::unique_ptr<ui::DelayPanel> delayPanel;
    std::unique_ptr<ui::ReverbPanel> reverbPanel;
    std::unique_ptr<ui::FlowPanel> flow;
    Group preGroup, postGroup;

    juce::TextButton audioBtn { "AUDIO SETTINGS" }, saveBtn { "SAVE" }, loadBtn { "LOAD" }, resetBtn { "RESET" };
    juce::ComboBox inputBox;
    std::unique_ptr<ui::Knob> trimKnob, masterKnob;
    ui::LevelMeter inMeter, outMeter;
    std::unique_ptr<juce::FileChooser> chooser;
    int tick = 0, lastStateVersion = 0;
};
} // namespace dr
