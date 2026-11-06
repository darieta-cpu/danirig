#include "RigUI.h"
#include <limits>

namespace dr
{
using namespace ui;

void RigUI::Content::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff262a30), 0, 0, juce::Colour (0xff15171a), 0, 66, false));
    g.fillRect (0.0f, 0.0f, (float) W, 66.0f);
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

RigUI::RigUI (RigEngine& e, std::function<void()> audioSettings, std::function<double()> cpu)
    : engine (e), audioSettingsCb (std::move (audioSettings)), cpuCb (std::move (cpu))
{
    setLookAndFeel (&laf);
    addAndMakeVisible (content);

    gatePanel  = std::make_unique<GatePanel> (engine.gate);
    modPanel   = std::make_unique<MultiModPanel> (engine.mod);
    pedalPanel = std::make_unique<NamPedalPanel> (engine.pedal);
    odPanel    = std::make_unique<PedalPanel> (engine.od,   juce::Colour (0xff3f9a45), juce::Colours::white, juce::Colour (0xff1c2a1e), juce::Colours::white);
    distPanel  = std::make_unique<PedalPanel> (engine.dist, juce::Colour (0xffe8791d), juce::Colour (0xff160c04), juce::Colour (0xff1b1109), juce::Colour (0xfff2f2f2));
    ampPanel   = std::make_unique<AmpPanel> (engine.amp);
    cabPanel   = std::make_unique<CabPanel> (engine.cab);
    delayPanel = std::make_unique<DelayPanel> (engine.delay);
    reverbPanel = std::make_unique<ReverbPanel> (engine.reverb);
    flow       = std::make_unique<FlowPanel> (engine);
    flow->showDriverInfo = (bool) cpuCb;

    for (juce::Component* c : std::initializer_list<juce::Component*> { gatePanel.get(), modPanel.get(), pedalPanel.get(), odPanel.get(), distPanel.get(),
                                                                       flow.get(), ampPanel.get(), cabPanel.get(), delayPanel.get(), reverbPanel.get() })
        content.addAndMakeVisible (c);

    // Layout 1280x800: barra 0-66, schema 72-108, pedali 116-336, ampli 344-554, cab/delay/reverb 562-782
    for (auto* p : std::initializer_list<PanelBase*> { gatePanel.get(), modPanel.get(), pedalPanel.get(), odPanel.get(), distPanel.get() }) p->setSize (240, 220);
    delayPanel->setSize (440, 220);
    reverbPanel->setSize (300, 220);
    flow->setBounds (20, 72, 1240, 36);
    ampPanel->setBounds (20, 344, 1240, 210);
    cabPanel->setBounds (20, 562, 480, 220);

    preGroup.panels  = { gatePanel.get(), pedalPanel.get(), odPanel.get(), distPanel.get(), modPanel.get() };   // indici = engine.preMods
    preGroup.x = 20;  preGroup.y = 116;
    postGroup.panels = { delayPanel.get(), reverbPanel.get() };                                                  // indici = engine.postMods
    postGroup.x = 510; postGroup.y = 562;
    for (auto* g : { &preGroup, &postGroup })
        for (auto* p : g->panels)
        {
            p->reorderable = true;
            p->onReorder = [this, g] (PanelBase& pb, int phase) { handleReorder (*g, pb, phase); };
        }

    // barra superiore
    for (auto* b : { &audioBtn, &saveBtn, &loadBtn, &resetBtn }) content.addAndMakeVisible (*b);
    audioBtn.setBounds (200, 18, 140, 30); saveBtn.setBounds (348, 18, 64, 30);
    loadBtn.setBounds (418, 18, 64, 30);   resetBtn.setBounds (488, 18, 64, 30);
    audioBtn.setVisible ((bool) audioSettingsCb);
    audioBtn.onClick = [this] { if (audioSettingsCb) audioSettingsCb(); };
    saveBtn.onClick  = [this] { savePreset(); };
    loadBtn.onClick  = [this] { loadPreset(); };
    resetBtn.onClick = [this] { engine.resetAll(); refreshAll(); };
    if (! audioSettingsCb) { saveBtn.setTopLeftPosition (200, 18); loadBtn.setTopLeftPosition (270, 18); resetBtn.setTopLeftPosition (340, 18); }

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

    setSize (1100, 688);
    lastStateVersion = engine.stateVersion.load();
    refreshAll();
    startTimerHz (30);
}

RigUI::~RigUI()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void RigUI::resized()
{
    const float s = std::min ((float) getWidth() / W, (float) getHeight() / H);
    content.setBounds (0, 0, W, H);
    content.setTransform (juce::AffineTransform::scale (s).translated (((float) getWidth() - W * s) * 0.5f, ((float) getHeight() - H * s) * 0.5f));
}

void RigUI::timerCallback()
{
    inMeter.setPeak (engine.inPeak.exchange (0.0f));
    outMeter.setPeak (engine.outPeak.exchange (0.0f));
    engine.collectGarbage();
    if (cpuCb) flow->cpu = (float) cpuCb();
    if (engine.stateVersion.load() != lastStateVersion) { lastStateVersion = engine.stateVersion.load(); refreshAll(); }
    flow->repaint();
    gatePanel->repaint();
    if ((++tick & 3) == 0) { ampPanel->repaint(); cabPanel->repaint(); delayPanel->repaint(); pedalPanel->repaint(); }
}

void RigUI::refreshAll()
{
    for (auto* p : std::initializer_list<PanelBase*> { gatePanel.get(), modPanel.get(), pedalPanel.get(), odPanel.get(), distPanel.get(),
                                                      ampPanel.get(), cabPanel.get(), delayPanel.get(), reverbPanel.get() })
        p->refresh();
    trimKnob->refresh(); masterKnob->refresh();
    inputBox.setSelectedId (engine.inputChannel->index(), juce::dontSendNotification);
    applyOrderFromEngine (isShowing());
}

// ---------------------------------------------------------------------------------------------
void RigUI::layoutGroup (Group& g, PanelBase* skip, bool animate)
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

void RigUI::handleReorder (Group& g, PanelBase& p, int phase)
{
    if (phase == 1)
    {
        std::vector<PanelBase*> others;
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

void RigUI::pushOrderToEngine()
{
    std::vector<int> pre, post;
    for (auto* p : preGroup.order)  pre.push_back (engine.preIndexOf (&p->module));
    for (auto* p : postGroup.order) post.push_back (engine.postIndexOf (&p->module));
    engine.setPreOrder (pre);
    engine.setPostOrder (post);
}

void RigUI::applyOrderFromEngine (bool animate)
{
    preGroup.order.clear(); postGroup.order.clear();
    for (int idx : engine.getPreOrder())  preGroup.order.push_back (preGroup.panels[(size_t) idx]);
    for (int idx : engine.getPostOrder()) postGroup.order.push_back (postGroup.panels[(size_t) idx]);
    layoutGroup (preGroup, nullptr, animate);
    layoutGroup (postGroup, nullptr, animate);
}

void RigUI::savePreset()
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

void RigUI::loadPreset()
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
} // namespace dr
