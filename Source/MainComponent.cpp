#include "MainComponent.h"
#include <limits>

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
    pedalPanel = std::make_unique<NamPedalPanel> (engine.pedal);
    odPanel    = std::make_unique<PedalPanel> (engine.od,   juce::Colour (0xff3f9a45), juce::Colours::white, juce::Colour (0xff1c2a1e), juce::Colours::white);
    distPanel  = std::make_unique<PedalPanel> (engine.dist, juce::Colour (0xffe8791d), juce::Colour (0xff160c04), juce::Colour (0xff1b1109), juce::Colour (0xfff2f2f2));
    ampPanel   = std::make_unique<AmpPanel> (engine.amp);
    cabPanel   = std::make_unique<CabPanel> (engine.cab);
    delayPanel = std::make_unique<DelayPanel> (engine.delay);
    reverbPanel = std::make_unique<ReverbPanel> (engine.reverb);
    flow       = std::make_unique<FlowPanel> (engine);

    for (juce::Component* c : std::initializer_list<juce::Component*> { gatePanel.get(), pedalPanel.get(), odPanel.get(), distPanel.get(), flow.get(),
                                                                       ampPanel.get(), cabPanel.get(), delayPanel.get(), reverbPanel.get() })
        content.addAndMakeVisible (c);

    // dimensioni fisse dei pannelli; le posizioni dei gruppi riordinabili le decide layoutGroup()
    for (auto* p : std::initializer_list<ui::PanelBase*> { gatePanel.get(), pedalPanel.get(), odPanel.get(), distPanel.get() }) p->setSize (230, 220);
    delayPanel->setSize (440, 220);
    reverbPanel->setSize (300, 220);
    flow->setBounds (980, 78, 280, 220);
    ampPanel->setBounds (20, 306, 1240, 210);
    cabPanel->setBounds (20, 524, 480, 220);

    preGroup.panels  = { gatePanel.get(), pedalPanel.get(), odPanel.get(), distPanel.get() };   // indici = engine.preMods
    preGroup.x = 20;  preGroup.y = 78;
    postGroup.panels = { delayPanel.get(), reverbPanel.get() };                                  // indici = engine.postMods
    postGroup.x = 510; postGroup.y = 524;
    for (auto* g : { &preGroup, &postGroup })
        for (auto* p : g->panels)
        {
            p->reorderable = true;
            p->onReorder = [this, g] (ui::PanelBase& pb, int phase) { handleReorder (*g, pb, phase); };
        }

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
    applyOrderFromEngine (false);

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
    if ((++tick & 3) == 0) { ampPanel->repaint(); cabPanel->repaint(); delayPanel->repaint(); pedalPanel->repaint(); }
}

void MainComponent::refreshAll()
{
    for (auto* p : std::initializer_list<PanelBase*> { gatePanel.get(), pedalPanel.get(), odPanel.get(), distPanel.get(), ampPanel.get(), cabPanel.get(), delayPanel.get(), reverbPanel.get() })
        p->refresh();
    applyOrderFromEngine (true);
    trimKnob->refresh(); masterKnob->refresh();
    inputBox.setSelectedId (engine.inputChannel->index(), juce::dontSendNotification);
}

// ---------------------------------------------------------------------------------------------
void MainComponent::layoutGroup (Group& g, ui::PanelBase* skip, bool animate)
{
    int x = g.x;
    auto& animator = juce::Desktop::getInstance().getAnimator();
    for (auto* p : g.order)
    {
        const juce::Rectangle<int> b (x, g.y, p->getWidth(), p->getHeight());
        if (p != skip)
        {
            if (animate) animator.animateComponent (p, b, 1.0f, 140, false, 1.0, 1.0);
            else { animator.cancelAnimation (p, false); p->setBounds (b); }
        }
        x += p->getWidth() + g.gap;
    }
}

void MainComponent::handleReorder (Group& g, ui::PanelBase& p, int phase)
{
    if (phase == 1)
    {
        std::vector<ui::PanelBase*> others;
        for (auto* q : g.order) if (q != &p) others.push_back (q);
        const int dragCentre = p.getX() + p.getWidth() / 2;
        int bestK = 0, bestDist = std::numeric_limits<int>::max();
        for (int k = 0; k <= (int) others.size(); ++k)       // dove inserire il pannello trascinato
        {
            int x = g.x, centre = 0;
            for (int i = 0; i <= (int) others.size(); ++i)
            {
                if (i == k) { centre = x + p.getWidth() / 2; x += p.getWidth() + g.gap; }
                if (i < (int) others.size()) x += others[(size_t) i]->getWidth() + g.gap;
            }
            if (std::abs (centre - dragCentre) < bestDist) { bestDist = std::abs (centre - dragCentre); bestK = k; }
        }
        others.insert (others.begin() + bestK, &p);
        if (others != g.order) { g.order = others; layoutGroup (g, &p, true); }
    }
    else if (phase == 2)
    {
        layoutGroup (g, nullptr, true);     // il pannello trascinato scivola nel suo slot
        pushOrderToEngine();
    }
}

void MainComponent::pushOrderToEngine()
{
    std::vector<int> pre, post;
    for (auto* p : preGroup.order)  pre.push_back (engine.preIndexOf (&p->module));
    for (auto* p : postGroup.order) post.push_back (engine.postIndexOf (&p->module));
    engine.setPreOrder (pre);
    engine.setPostOrder (post);
}

void MainComponent::applyOrderFromEngine (bool animate)
{
    preGroup.order.clear(); postGroup.order.clear();
    for (int idx : engine.getPreOrder())  preGroup.order.push_back (preGroup.panels[(size_t) idx]);
    for (int idx : engine.getPostOrder()) postGroup.order.push_back (postGroup.panels[(size_t) idx]);
    layoutGroup (preGroup, nullptr, animate);
    layoutGroup (postGroup, nullptr, animate);
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
