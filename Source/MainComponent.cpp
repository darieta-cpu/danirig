#include "MainComponent.h"

namespace dr
{
MainComponent::MainComponent()
{
    loadSettings();
    ui = std::make_unique<RigUI> (engine, [this] { showAudioSettings(); }, [this] { return deviceManager.getCpuUsage(); });
    addAndMakeVisible (*ui);
    setSize (1100, 765);

    // audio: 48 kHz / 128 campioni come punto di partenza, poi lo stato salvato dell'utente
    std::unique_ptr<juce::XmlElement> savedAudio;
    const auto audioFile = settingsDir().getChildFile ("audio.xml");
    const bool firstRun = ! audioFile.existsAsFile();
    if (! firstRun) savedAudio = juce::XmlDocument::parse (audioFile);
    juce::AudioDeviceManager::AudioDeviceSetup preferred;
    preferred.sampleRate = 48000.0;
    preferred.bufferSize = 128;
    deviceManager.initialise (8, 2, savedAudio.get(), true, {}, &preferred);
    deviceManager.addAudioCallback (&engine);

    if (firstRun) juce::MessageManager::callAsync ([this] { showAudioSettings(); });
}

MainComponent::~MainComponent()
{
    saveSettings();
    deviceManager.removeAudioCallback (&engine);
    deviceManager.closeAudioDevice();
}

void MainComponent::showAudioSettings()
{
    auto* sel = new juce::AudioDeviceSelectorComponent (deviceManager, 0, 8, 0, 8, false, false, false, false);
    sel->setSize (560, 480);
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (sel);
    o.dialogTitle = "Audio Settings";
    o.componentToCentreAround = this;
    o.dialogBackgroundColour = juce::Colour (0xff1b1e22);
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = false;
    o.launchAsync();
}

juce::File MainComponent::settingsDir() const
{
    auto d = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("DaniRig");
    d.createDirectory();
    return d;
}

void MainComponent::saveSettings()
{
    if (auto xml = engine.saveState()) xml->writeTo (settingsDir().getChildFile ("last.xml"));
    if (auto audio = deviceManager.createStateXml()) audio->writeTo (settingsDir().getChildFile ("audio.xml"));
}

void MainComponent::loadSettings()
{
    auto f = settingsDir().getChildFile ("last.xml");
    if (f.existsAsFile())
        if (auto xml = juce::XmlDocument::parse (f)) engine.loadState (*xml);
}
} // namespace dr
