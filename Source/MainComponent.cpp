#include "MainComponent.h"

namespace dr
{
using namespace ui;

void MainComponent::Content::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    auto bar = juce::Rectangle<float> (0, 0, (float) W, 66.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff262a30), 0, 0, juce::Colour (0xff15171a), 0, 66, false));
    g.fillRect (bar);
    g.setColour (col::gold.withAlpha (0.6f)); g.drawHorizontalLine (65, 0, (float) W);

    g.setColour (col::gold);
    g.setFont (juce::Font (juce::FontOptions (30.0f, juce::Font::bold | juce::Font::italic)));
    g.drawText ("DANIRIG", 20, 8, 170, 40, juce::Justification::centredLeft);
    g.setColour (col::dim); g.setFont (uiFont (10.0f, true));
    g.drawText ("NAM  -  IR  -  FX", 22, 44, 160, 14, juce::Justification::centredLeft);

    g.setColour (col::dim); g.setFont (uiFont (11.0f, true));
    g.drawText ("IN", 772, 6, 120, 14, juce::Justification::centredLeft);
    g.drawText ("OUT", 975, 6, 120, 14, juce::Justification::centredLeft);

    g.setColour (col::gold);
    g.setFont (juce::Font (juce::FontOptions (24.0f, juce::Font::bold | juce::Font::italic)));
    g.drawText ("DaniArieta", W - 190, 14, 170, 36, juce::Justification::centredRight);
}

MainComponent::MainComponent()
{
    setLookAndFeel (&laf);
    addAndMakeVisible (content);

    gatePanel  = std::make_unique<GatePanel> (engine.gate);
    odPanel    = std::make_unique<PedalPanel> (engine.od,   juce::Colour (0xff3f9a45), juce::Colours::white, juce::Colour (0xff1c2a1e), juce::Colours::white);
    distPanel  = std::make_unique<PedalPanel> (engine.dist, juce::Colour (0xffe8791d), juce::Colour (0xff160c04), juce::Colour (0xff1b1109), juce::Colour (0xfff2f2f2));
    ampPanel   = std::make_unique<AmpPanel> (engine.amp);
    cabPanel   = std::make_unique<CabPanel> (engine.cab);
    delayPanel = std::make_unique<DelayPanel> (engine.delay);
    reverbPanel = std::make_unique<ReverbPanel> (engine.reverb);
    flow       = std::make_unique<FlowPanel> (engine);

    for (juce::Component* c : std::initializer_list<juce::Component*> { gatePanel.get(), odPanel.get(), distPanel.get(), flow.get(),
                                                                       ampPanel.get(), cabPanel.get(), delayPanel.get(), reverbPanel.get() })
        content.addAndMakeVisible (c);

    gatePanel->setBounds (20, 78, 210, 220);
    odPanel->setBounds (240, 78, 240, 220);
    distPanel->setBounds (490, 78, 240, 220);
    flow->setBounds (740, 78, 520, 220);
    ampPanel->setBounds (20, 306, 1240, 210);
    cabPanel->setBounds (20, 524, 480, 220);
    delayPanel->setBounds (510, 524, 440, 220);
    reverbPanel->setBounds (960, 524, 300, 220);

    // barra superiore
    for (auto* b : { &audioBtn, &saveBtn, &loadBtn, &resetBtn }) content.addAndMakeVisible (*b);
    audioBtn.setBounds (200, 18, 140, 30); saveBtn.setBounds (348, 18, 64, 30);
    loadBtn.setBounds (418, 18, 64, 30);   resetBtn.setBounds (488, 18, 64, 30);
    audioBtn.onClick = [this] { showAudioSettings(); };
    saveBtn.onClick  = [this] { savePreset(); };
    loadBtn.onClick  = [this] { loadPreset(); };
    resetBtn.onClick = [this] { engine.resetAll(); refreshAll(); };

    for (int i = 1; i <= 8; ++i) inputBox.addItem ("Input " + juce::String (i), i);
    inputBox.setSelectedId (engine.inputChannel->index(), juce::dontSendNotification);
    inputBox.onChange = [this] { engine.inputChannel->set ((float) inputBox.getSelectedId()); };
    content.addAndMakeVisible (inputBox);
    inputBox.setBounds (572, 18, 104, 30);

    trimKnob   = std::make_unique<Knob> (*engine.inputTrim, juce::Colour (0xffbfc3c9), juce::Colour (0xff111214), col::text);
    masterKnob = std::make_unique<Knob> (*engine.master,    juce::Colour (0xffbfc3c9), juce::Colour (0xff111214), col::text);
    content.addAndMakeVisible (*trimKnob);   trimKnob->setBounds (690, 0, 68, 66);
    content.addAndMakeVisible (*masterKnob); masterKnob->setBounds (900, 0, 68, 66);
    content.addAndMakeVisible (inMeter);     inMeter.setBounds (772, 24, 120, 14);
    content.addAndMakeVisible (outMeter);    outMeter.setBounds (975, 24, 120, 14);

    setSize (1100, 653);

    loadSettings();
    refreshAll();

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

    startTimerHz (30);
    if (firstRun) juce::MessageManager::callAsync ([this] { showAudioSettings(); });
}

MainComponent::~MainComponent()
{
    stopTimer();
    saveSettings();
    deviceManager.removeAudioCallback (&engine);
    deviceManager.closeAudioDevice();
    setLookAndFeel (nullptr);
}

void MainComponent::resized()
{
    const float s = std::min ((float) getWidth() / W, (float) getHeight() / H);
    content.setBounds (0, 0, W, H);
    content.setTransform (juce::AffineTransform::scale (s).translated (((float) getWidth() - W * s) * 0.5f, ((float) getHeight() - H * s) * 0.5f));
}

void MainComponent::timerCallback()
{
    inMeter.setPeak (engine.inPeak.exchange (0.0f));
    outMeter.setPeak (engine.outPeak.exchange (0.0f));
    engine.amp.collectGarbage();
    flow->cpu = (float) deviceManager.getCpuUsage();
    flow->repaint();
    gatePanel->repaint();
    if ((++tick & 3) == 0) { ampPanel->repaint(); cabPanel->repaint(); delayPanel->repaint(); }
}

void MainComponent::refreshAll()
{
    for (auto* p : std::initializer_list<PanelBase*> { gatePanel.get(), odPanel.get(), distPanel.get(), ampPanel.get(), cabPanel.get(), delayPanel.get(), reverbPanel.get() })
        p->refresh();
    trimKnob->refresh(); masterKnob->refresh();
    inputBox.setSelectedId (engine.inputChannel->index(), juce::dontSendNotification);
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

void MainComponent::savePreset()
{
    chooser = std::make_unique<juce::FileChooser> ("Salva preset", FileStepper::lastDir().getChildFile ("preset.dprig"), "*.dprig");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f == juce::File()) return;
                              if (auto xml = engine.saveState()) xml->writeTo (f.withFileExtension ("dprig"));
                          });
}

void MainComponent::loadPreset()
{
    chooser = std::make_unique<juce::FileChooser> ("Carica preset", FileStepper::lastDir(), "*.dprig");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (! f.existsAsFile()) return;
                              if (auto xml = juce::XmlDocument::parse (f)) { engine.loadState (*xml); refreshAll(); }
                          });
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
