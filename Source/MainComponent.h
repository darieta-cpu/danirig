#pragma once
#include "RigEngine.h"
#include "ui/Panels.h"

namespace dr
{
class MainComponent : public juce::Component, private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics& g) override { g.fillAll (ui::col::bg); }
    void resized() override;

    static constexpr int W = 1280, H = 760;

private:
    class Content : public juce::Component
    {
    public:
        void paint (juce::Graphics& g) override;
    };

    void timerCallback() override;
    void showAudioSettings();
    void savePreset();
    void loadPreset();
    void refreshAll();

    // Gruppi di pannelli riordinabili con drag (PRE: davanti all'ampli, POST: dopo il cab)
    struct Group { std::vector<ui::PanelBase*> panels;   // allineati agli indici del motore
                   std::vector<ui::PanelBase*> order;    // ordine corrente a schermo
                   int x = 0, y = 0, gap = 10; };
    void layoutGroup (Group& g, ui::PanelBase* skip, bool animate);
    void handleReorder (Group& g, ui::PanelBase& p, int phase);
    void applyOrderFromEngine (bool animate);
    void pushOrderToEngine();
    juce::File settingsDir() const;
    void saveSettings();
    void loadSettings();

    RigEngine engine;
    juce::AudioDeviceManager deviceManager;
    ui::RigLookAndFeel laf;
    Content content;

    std::unique_ptr<ui::GatePanel> gatePanel;
    std::unique_ptr<ui::NamPedalPanel> pedalPanel;
    Group preGroup, postGroup;
    std::unique_ptr<ui::PedalPanel> odPanel, distPanel;
    std::unique_ptr<ui::AmpPanel> ampPanel;
    std::unique_ptr<ui::CabPanel> cabPanel;
    std::unique_ptr<ui::DelayPanel> delayPanel;
    std::unique_ptr<ui::ReverbPanel> reverbPanel;
    std::unique_ptr<ui::FlowPanel> flow;

    juce::TextButton audioBtn { "AUDIO SETTINGS" }, saveBtn { "SAVE" }, loadBtn { "LOAD" }, resetBtn { "RESET" };
    juce::ComboBox inputBox;
    std::unique_ptr<ui::Knob> trimKnob, masterKnob;
    ui::LevelMeter inMeter, outMeter;
    std::unique_ptr<juce::FileChooser> chooser;
    int tick = 0;
};
} // namespace dr
