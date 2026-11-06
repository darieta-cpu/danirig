#pragma once
#include "RigEngine.h"
#include "ui/RigUI.h"

namespace dr
{
// App standalone: gestisce il device audio e ospita l'interfaccia del rig.
class MainComponent : public juce::Component
{
public:
    MainComponent();
    ~MainComponent() override;
    void resized() override { ui->setBounds (getLocalBounds()); }

private:
    void showAudioSettings();
    juce::File settingsDir() const;
    void saveSettings();
    void loadSettings();

    RigEngine engine;
    juce::AudioDeviceManager deviceManager;
    std::unique_ptr<RigUI> ui;
};
} // namespace dr
