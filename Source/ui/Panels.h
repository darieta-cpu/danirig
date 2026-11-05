#pragma once
#include "Controls.h"
#include "../RigEngine.h"

namespace dr::ui
{
// Base di tutti i pannelli: disegna lo "chassis" (pedale, ampli, cab, rack) e ospita i controlli.
class PanelBase : public juce::Component
{
public:
    enum class Look { Stomp, Amp, Cab, Rack };

    PanelBase (Module& m, Look l, juce::Colour base_, juce::Colour textCol_, juce::Colour knobBody_, juce::Colour pointer_)
        : module (m), look (l), base (base_), textCol (textCol_), knobBody (knobBody_), pointer (pointer_),
          footswitch (*m.enabled, l == Look::Stomp ? Footswitch::Style::Stomp : Footswitch::Style::Power,
                      l == Look::Rack ? juce::Colour (0xffff3b2f) : l == Look::Amp ? juce::Colour (0xffffa43b) : juce::Colour (0xffff3b2f))
    {
        footswitch.extra = [this] { repaint(); };
        addAndMakeVisible (footswitch);
    }

    Knob* addKnob (Param& p)
    {
        knobs.push_back (std::make_unique<Knob> (p, knobBody, pointer, textCol));
        addAndMakeVisible (*knobs.back());
        return knobs.back().get();
    }
    ChoiceBox* addChoice (Param& p)
    {
        choices.push_back (std::make_unique<ChoiceBox> (p));
        addAndMakeVisible (*choices.back());
        return choices.back().get();
    }
    LedToggle* addToggle (Param& p, const juce::String& text)
    {
        toggles.push_back (std::make_unique<LedToggle> (p, text, juce::Colour (0xff46e06b), textCol));
        addAndMakeVisible (*toggles.back());
        return toggles.back().get();
    }

    // Riordino: si trascina dalla fascia alta (primi 26 px) del pannello.
    bool reorderable = false;
    std::function<void (PanelBase&, int)> onReorder;   // 0 = inizio, 1 = trascinamento, 2 = rilascio

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (reorderable && e.y < 26 && onReorder) { dragging = true; grabX = e.x; toFront (false); onReorder (*this, 0); }
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! dragging || getParentComponent() == nullptr) return;
        setTopLeftPosition (e.getEventRelativeTo (getParentComponent()).x - grabX, getY());
        onReorder (*this, 1);
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragging) { dragging = false; onReorder (*this, 2); }
    }
    void mouseMove (const juce::MouseEvent& e) override
    {
        setMouseCursor (reorderable && e.y < 26 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
    }

    virtual void refresh()
    {
        for (auto& k : knobs) k->refresh();
        for (auto& c : choices) c->refresh();
        for (auto& t : toggles) t->refresh();
        repaint();
    }

    static void rowOfKnobs (juce::Rectangle<int> area, const std::vector<Knob*>& ks)
    {
        if (ks.empty()) return;
        const int w = area.getWidth() / (int) ks.size();
        for (size_t i = 0; i < ks.size(); ++i)
            ks[i]->setBounds (area.getX() + (int) i * w, area.getY(), w, area.getHeight());
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        switch (look)
        {
            case Look::Stomp: paintStomp (g, r); break;
            case Look::Amp:   paintAmp (g, r);   break;
            case Look::Cab:   paintCab (g, r);   break;
            case Look::Rack:  paintRack (g, r);  break;
        }
        if (reorderable)   // maniglia di trascinamento
        {
            const float x0 = (float) getWidth() - (look == Look::Rack ? 80.0f : 48.0f);
            g.setColour (juce::Colours::black.withAlpha (look == Look::Rack ? 0.45f : 0.35f));
            for (int c = 0; c < 2; ++c) for (int rr = 0; rr < 3; ++rr) g.fillEllipse (x0 + (float) c * 5.0f, 8.0f + (float) rr * 5.0f, 2.6f, 2.6f);
        }
    }

    Module& module;
    Look look;
    juce::Colour base, textCol, knobBody, pointer;

protected:
    void paintScrew (juce::Graphics& g, juce::Point<float> c, float rad = 5.0f)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffdadde2), c.x - rad, c.y - rad, juce::Colour (0xff50555c), c.x + rad, c.y + rad, false));
        g.fillEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2);
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2, 1.0f);
        g.drawLine (c.x - rad * 0.6f, c.y + rad * 0.3f, c.x + rad * 0.6f, c.y - rad * 0.3f, 1.2f);
    }

    void paintStomp (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (r.translated (0, 3), 14.0f);
        g.setGradientFill (juce::ColourGradient (base.brighter (0.35f), 0, r.getY(), base.darker (0.45f), 0, r.getBottom(), false));
        g.fillRoundedRectangle (r, 14.0f);
        g.setColour (juce::Colours::black.withAlpha (0.85f));
        g.drawRoundedRectangle (r, 14.0f, 1.5f);
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawRoundedRectangle (r.reduced (2.0f), 12.0f, 1.0f);
        paintScrew (g, { r.getX() + 12, r.getY() + 12 }); paintScrew (g, { r.getRight() - 12, r.getY() + 12 });
        // LED on/off
        const bool on = module.isActive();
        const juce::Point<float> lc (r.getCentreX(), r.getY() + 14.0f);
        if (on) { g.setColour (juce::Colour (0xffff3b2f).withAlpha (0.35f)); g.fillEllipse (lc.x - 9, lc.y - 9, 18, 18); }
        g.setColour (on ? juce::Colour (0xffff3b2f) : juce::Colour (0xff4a2020));
        g.fillEllipse (lc.x - 5, lc.y - 5, 10, 10);
        g.setColour (juce::Colours::black); g.drawEllipse (lc.x - 5, lc.y - 5, 10, 10, 1.0f);
        paintTitle (g, titleArea);
    }

    void paintAmp (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff23262a), 0, r.getY(), juce::Colour (0xff0f1012), 0, r.getBottom(), false));
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (juce::Colours::black); g.drawRoundedRectangle (r, 10.0f, 2.0f);
        g.setColour (col::gold.withAlpha (0.85f));
        g.drawRoundedRectangle (r.reduced (6.0f), 7.0f, 1.6f);
        g.setColour (col::gold.withAlpha (0.35f));
        g.drawRoundedRectangle (r.reduced (10.0f), 5.0f, 0.8f);
        paintTitle (g, titleArea);
    }

    void paintCab (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (juce::Colour (0xff17181a)); g.fillRoundedRectangle (r, 10.0f);
        auto cloth = r.reduced (10.0f).withTrimmedTop (30.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a342b), 0, cloth.getY(), juce::Colour (0xff1f1b16), 0, cloth.getBottom(), false));
        g.fillRoundedRectangle (cloth, 6.0f);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        for (float x = cloth.getX(); x < cloth.getRight(); x += 4.0f) g.drawVerticalLine ((int) x, cloth.getY(), cloth.getBottom());
        for (float y = cloth.getY(); y < cloth.getBottom(); y += 4.0f) g.drawHorizontalLine ((int) y, cloth.getX(), cloth.getRight());
        g.setColour (juce::Colours::black); g.drawRoundedRectangle (r, 10.0f, 2.0f);
        g.setColour (col::gold.withAlpha (0.6f)); g.drawRoundedRectangle (cloth, 6.0f, 1.2f);
        paintTitle (g, titleArea);
    }

    void paintRack (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffc9cdd2), 0, r.getY(), juce::Colour (0xff8d9299), 0, r.getBottom(), false));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        for (float y = r.getY() + 2; y < r.getBottom(); y += 3.0f) g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
        g.setColour (juce::Colours::black.withAlpha (0.8f)); g.drawRoundedRectangle (r, 6.0f, 1.5f);
        for (float y : { r.getY() + 14, r.getBottom() - 14 }) { paintScrew (g, { r.getX() + 11, y }); paintScrew (g, { r.getRight() - 11, y }); }
        paintTitle (g, titleArea);
    }

    virtual void paintTitle (juce::Graphics& g, juce::Rectangle<int> a)
    {
        g.setColour (look == Look::Amp || look == Look::Cab ? col::gold : textCol);
        g.setFont (juce::Font (juce::FontOptions (look == Look::Stomp ? 22.0f : 18.0f, juce::Font::bold | juce::Font::italic)));
        g.drawText (module.title, a, look == Look::Stomp ? juce::Justification::centred : juce::Justification::centredLeft);
    }

    juce::Rectangle<int> titleArea;
    bool dragging = false;
    int grabX = 0;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<ChoiceBox>> choices;
    std::vector<std::unique_ptr<LedToggle>> toggles;
    Footswitch footswitch;
};

// ---------------------------------------------------------------------------------------------
// Helpers per scegliere file e passare al file precedente/successivo della stessa cartella
class FileStepper
{
public:
    void choose (const juce::String& title, const juce::String& patterns, std::function<void (const juce::File&)> cb)
    {
        chooser = std::make_unique<juce::FileChooser> (title, lastDir(), patterns);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this, cb] (const juce::FileChooser& fc)
                              {
                                  auto f = fc.getResult();
                                  if (f.existsAsFile()) { lastDir() = f.getParentDirectory(); cb (f); }
                              });
    }

    static juce::File step (const juce::File& current, const juce::String& ext, int dir)
    {
        if (! current.existsAsFile()) return {};
        auto files = current.getParentDirectory().findChildFiles (juce::File::findFiles, false, "*" + ext);
        std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });
        const int i = files.indexOf (current);
        if (files.isEmpty() || i < 0) return {};
        return files[(i + dir + files.size()) % files.size()];
    }

    static juce::File& lastDir() { static juce::File d = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory); return d; }

private:
    std::unique_ptr<juce::FileChooser> chooser;
};

// ---------------------------------------------------------------------------------------------
class GatePanel : public PanelBase
{
public:
    explicit GatePanel (NoiseGate& g)
        : PanelBase (g, Look::Stomp, juce::Colour (0xff4a505a), juce::Colours::white, juce::Colour (0xff222428), juce::Colours::white), gate (g)
    {
        for (auto& p : g.params) if (p->kind == Param::Kind::Float) addKnob (*p);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (10);
        r.removeFromTop (28);
        std::vector<Knob*> ks; for (auto& k : knobs) ks.push_back (k.get());
        rowOfKnobs (r.removeFromTop (84), ks);
        titleArea = r.removeFromTop (26);
        footswitch.setBounds (juce::Rectangle<int> (52, 52).withCentre ({ getWidth() / 2, getHeight() - 38 }));
    }
    void paint (juce::Graphics& g) override
    {
        PanelBase::paint (g);
        const bool o = gate.isOpen.load (std::memory_order_relaxed) && module.isActive();
        g.setColour (o ? col::ledGreen : juce::Colour (0xff1f3a26));
        g.fillEllipse ((float) getWidth() - 28.0f, 22.0f, 9.0f, 9.0f);
    }
private:
    NoiseGate& gate;
};

class PedalPanel : public PanelBase
{
public:
    PedalPanel (MonoModule& m, juce::Colour body, juce::Colour text, juce::Colour knobBodyCol, juce::Colour pointerCol)
        : PanelBase (m, Look::Stomp, body, text, knobBodyCol, pointerCol)
    {
        for (auto& p : m.params) if (p->kind == Param::Kind::Float) addKnob (*p);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (10);
        r.removeFromTop (28);
        std::vector<Knob*> ks; for (auto& k : knobs) ks.push_back (k.get());
        rowOfKnobs (r.removeFromTop (84), ks);
        titleArea = r.removeFromTop (26);
        footswitch.setBounds (juce::Rectangle<int> (52, 52).withCentre ({ getWidth() / 2, getHeight() - 38 }));
    }
};

// ---------------------------------------------------------------------------------------------
// Pedale "NAM": carica il profilo NAM di un pedale (overdrive, boost, fuzz...) da mettere davanti all'ampli.
class NamPedalPanel : public PanelBase
{
public:
    explicit NamPedalPanel (NamAmp& a)
        : PanelBase (a, Look::Stomp, juce::Colour (0xff6a3fa0), juce::Colours::white, juce::Colour (0xff1e1428), juce::Colours::white), pedal (a)
    {
        addKnob (*a.inputDb); addKnob (*a.outputDb);
        norm = addToggle (*a.normalize, "AUTO");
        sizeBox = addChoice (*a.quality);
        sizeBox->extra = [this] { pedal.reloadCurrent(); };
        addAndMakeVisible (load); addAndMakeVisible (prev); addAndMakeVisible (next);
        load.setButtonText ("LOAD..."); prev.setButtonText ("<"); next.setButtonText (">");
        load.onClick = [this] { stepper.choose ("Scegli il profilo NAM di un pedale", "*.nam", [this] (const juce::File& f) { pedal.loadModelAsync (f); }); };
        prev.onClick = [this] { auto f = FileStepper::step (juce::File (pedal.getModelPath()), ".nam", -1); if (f.existsAsFile()) pedal.loadModelAsync (f); };
        next.onClick = [this] { auto f = FileStepper::step (juce::File (pedal.getModelPath()), ".nam", +1); if (f.existsAsFile()) pedal.loadModelAsync (f); };
    }

    void resized() override
    {
        const int w = getWidth();
        titleArea = juce::Rectangle<int> (16, 6, 110, 22);
        slotArea = juce::Rectangle<int> (12, 32, w - 24, 22);
        prev.setBounds (12, 58, 32, 24); next.setBounds (w - 44, 58, 32, 24); load.setBounds (48, 58, w - 96, 24);
        knobs[0]->setBounds (10, 86, 66, 78); knobs[1]->setBounds (78, 86, 66, 78);
        norm->setBounds (w - 78, 96, 70, 22);
        sizeBox->setBounds (w - 76, 126, 62, 26);
        footswitch.setBounds (juce::Rectangle<int> (48, 48).withCentre ({ w / 2, getHeight() - 28 }));
    }

    void paintTitle (juce::Graphics& g, juce::Rectangle<int> a) override
    {
        g.setColour (textCol);
        g.setFont (juce::Font (juce::FontOptions (16.0f, juce::Font::bold | juce::Font::italic)));
        g.drawText (module.title, a, juce::Justification::centredLeft);
    }

    void paint (juce::Graphics& g) override
    {
        PanelBase::paint (g);
        auto info = pedal.getInfo();
        auto s = slotArea.toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (s, 4.0f);
        g.setFont (uiFont (12.0f, true));
        g.setColour (info.error.isNotEmpty() ? juce::Colours::orange : juce::Colours::white);
        auto t = info.error.isNotEmpty() ? info.error : info.loading ? juce::String ("Caricamento...") : info.name.isEmpty() ? juce::String ("Nessun profilo") : info.name;
        g.drawFittedText (t, slotArea.reduced (6, 0), juce::Justification::centredLeft, 1);
    }

private:
    NamAmp& pedal;
    FileStepper stepper;
    juce::TextButton load, prev, next;
    LedToggle* norm; ChoiceBox* sizeBox;
    juce::Rectangle<int> slotArea;
};

// ---------------------------------------------------------------------------------------------
class AmpPanel : public PanelBase
{
public:
    explicit AmpPanel (NamAmp& a)
        : PanelBase (a, Look::Amp, juce::Colour (0xff17191c), col::text, juce::Colour (0xffbfc3c9), juce::Colour (0xff111214)), amp (a)
    {
        for (auto* p : { a.inputDb, a.bass, a.mid, a.treble, a.outputDb }) addKnob (*p);
        norm = addToggle (*a.normalize, "AUTO LEVEL");
        sizeBox = addChoice (*a.quality);
        sizeBox->extra = [this] { amp.reloadCurrent(); };
        addAndMakeVisible (load); addAndMakeVisible (prev); addAndMakeVisible (next);
        load.onClick = [this] { stepper.choose ("Scegli un profilo NAM", "*.nam", [this] (const juce::File& f) { amp.loadModelAsync (f); }); };
        prev.onClick = [this] { auto f = FileStepper::step (juce::File (amp.getModelPath()), ".nam", -1); if (f.existsAsFile()) amp.loadModelAsync (f); };
        next.onClick = [this] { auto f = FileStepper::step (juce::File (amp.getModelPath()), ".nam", +1); if (f.existsAsFile()) amp.loadModelAsync (f); };
        load.setButtonText ("LOAD MODEL..."); prev.setButtonText ("<"); next.setButtonText (">");
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (22, 16);
        titleArea = r.removeFromTop (28);
        footswitch.setBounds (getLocalBounds().removeFromTop (44).removeFromRight (44).reduced (6));
        auto left = r.removeFromLeft (370);
        slotArea = left.removeFromTop (70);
        auto btns = left.removeFromTop (34);
        prev.setBounds (btns.removeFromLeft (40));
        next.setBounds (btns.removeFromRight (40));
        load.setBounds (btns.reduced (6, 0));
        auto right = r.removeFromRight (190);
        norm->setBounds (right.getX(), right.getY() + 14, 160, 28);
        sizeArea = juce::Rectangle<int> (right.getX(), right.getY() + 62, 160, 18);
        sizeBox->setBounds (right.getX(), right.getY() + 84, 110, 28);
        std::vector<Knob*> ks; for (auto& k : knobs) ks.push_back (k.get());
        rowOfKnobs (r.reduced (10, 0).withTrimmedTop (6), ks);
    }

    void paint (juce::Graphics& g) override
    {
        PanelBase::paint (g);
        auto info = amp.getInfo();
        auto s = slotArea.toFloat();
        g.setColour (juce::Colour (0xff0b0d0f)); g.fillRoundedRectangle (s, 5.0f);
        g.setColour (col::gold.withAlpha (0.5f)); g.drawRoundedRectangle (s, 5.0f, 1.0f);
        g.setFont (uiFont (17.0f, true));
        g.setColour (info.error.isNotEmpty() ? juce::Colours::orangered : col::text);
        auto nameText = info.error.isNotEmpty() ? info.error : (info.name.isEmpty() ? juce::String ("Nessun modello caricato") : info.name);
        g.drawFittedText (nameText, slotArea.reduced (10, 6).removeFromTop (30), juce::Justification::centredLeft, 1);
        g.setFont (uiFont (12.0f)); g.setColour (col::dim);
        juce::String detail = info.loading ? "Caricamento..." : info.loaded ? juce::String (info.slimmable ? "A2 / slimmable" : "NAM") + "  -  " + juce::String (info.modelRate / 1000.0, 1) + " kHz" : "";
        g.drawText (detail, slotArea.reduced (10, 6).removeFromBottom (24), juce::Justification::centredLeft);
        g.setColour (col::gold); g.setFont (uiFont (11.0f, true));
        g.drawText ("A2 SIZE", sizeArea, juce::Justification::centredLeft);
    }

private:
    NamAmp& amp;
    FileStepper stepper;
    juce::TextButton load, prev, next;
    LedToggle* norm; ChoiceBox* sizeBox;
    juce::Rectangle<int> slotArea, sizeArea;
};

class CabPanel : public PanelBase
{
public:
    explicit CabPanel (CabIR& c)
        : PanelBase (c, Look::Cab, juce::Colour (0xff17181a), col::text, juce::Colour (0xffb5a77c), juce::Colour (0xff1a1710)), cab (c)
    {
        for (auto* p : { c.params[1].get(), c.params[2].get(), c.params[3].get() }) addKnob (*p);
        addAndMakeVisible (load); addAndMakeVisible (clear); addAndMakeVisible (prev); addAndMakeVisible (next);
        load.setButtonText ("LOAD IR..."); clear.setButtonText ("X"); prev.setButtonText ("<"); next.setButtonText (">");
        load.onClick = [this] { stepper.choose ("Scegli un file IR", "*.wav;*.aif;*.aiff;*.flac", [this] (const juce::File& f) { cab.loadIR (f); repaint(); }); };
        clear.onClick = [this] { cab.clearIR(); repaint(); };
        prev.onClick = [this] { stepIr (-1); };
        next.onClick = [this] { stepIr (+1); };
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (20, 14);
        titleArea = r.removeFromTop (28);
        footswitch.setBounds (getLocalBounds().removeFromTop (44).removeFromRight (44).reduced (6));
        slotArea = r.removeFromTop (26);
        auto b = r.removeFromTop (30).reduced (0, 2);
        prev.setBounds (b.removeFromLeft (34)); clear.setBounds (b.removeFromRight (34)); next.setBounds (b.removeFromRight (34));
        load.setBounds (b.reduced (6, 0));
        std::vector<Knob*> ks; for (auto& k : knobs) ks.push_back (k.get());
        rowOfKnobs (r.withTrimmedTop (22), ks);
    }

    void paint (juce::Graphics& g) override
    {
        PanelBase::paint (g);
        const auto path = cab.getIRPath();
        g.setFont (uiFont (14.0f, true));
        g.setColour (path.isEmpty() ? col::dim : col::text);
        g.drawFittedText (path.isEmpty() ? juce::String ("Nessun IR caricato (cab bypassato)") : juce::File (path).getFileNameWithoutExtension(),
                          slotArea, juce::Justification::centredLeft, 1);
    }

private:
    void stepIr (int dir)
    {
        const juce::File cur (cab.getIRPath());
        auto f = FileStepper::step (cur, cur.getFileExtension(), dir);
        if (f.existsAsFile()) { cab.loadIR (f); repaint(); }
    }
    CabIR& cab;
    FileStepper stepper;
    juce::TextButton load, clear, prev, next;
    juce::Rectangle<int> slotArea;
};

class DelayPanel : public PanelBase
{
public:
    explicit DelayPanel (DelayPedal& d)
        : PanelBase (d, Look::Stomp, juce::Colour (0xff2c6db8), juce::Colours::white, juce::Colour (0xff1b2330), juce::Colours::white), delay (d)
    {
        for (const char* id : { "time", "repeats", "tone", "mod", "mix" }) addKnob (*d.find (id));
        subdiv = addChoice (*d.find ("subdiv"));
        modeBox = addChoice (*d.find ("mode"));
        ping = addToggle (*d.find ("pingpong"), "PING-PONG");
        subdiv->extra = [this] { repaint(); };
        tap.setButtonText ("TAP");
        tap.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff10151c));
        tap.onClick = [this] { delay.tap(); for (auto& k : knobs) k->refresh(); repaint(); };
        addAndMakeVisible (tap);
    }

    void refresh() override { PanelBase::refresh(); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12, 10);
        r.removeFromTop (26);
        std::vector<Knob*> ks; for (auto& k : knobs) ks.push_back (k.get());
        rowOfKnobs (r.removeFromTop (84), ks);
        auto row = r.removeFromTop (30);
        subdiv->setBounds (row.removeFromLeft (92));
        row.removeFromLeft (8);
        modeBox->setBounds (row.removeFromLeft (92));
        row.removeFromLeft (10);
        ping->setBounds (row.removeFromLeft (110));
        auto bottom = r.withTrimmedTop (6);
        titleArea = bottom.removeFromLeft (110).withTrimmedTop (6);
        footswitch.setBounds (bottom.removeFromRight (56).reduced (2));
        tap.setBounds (bottom.removeFromRight (76).reduced (4, 6));
        timeArea = bottom;
    }

    void paint (juce::Graphics& g) override
    {
        PanelBase::paint (g);
        const float ms = delay.effectiveMs();
        g.setColour (juce::Colours::white.withAlpha (0.85f)); g.setFont (uiFont (13.0f, true));
        g.drawText (juce::String ((int) std::lround (ms)) + " ms  -  " + juce::String (juce::roundToInt (60000.0f / ms)) + " BPM", timeArea, juce::Justification::centred);
    }

private:
    DelayPedal& delay;
    ChoiceBox *subdiv, *modeBox; LedToggle* ping;
    juce::TextButton tap;
    juce::Rectangle<int> timeArea;
};

class ReverbPanel : public PanelBase
{
public:
    explicit ReverbPanel (ReverbModule& m)
        : PanelBase (m, Look::Rack, juce::Colour (0xffbec3c9), juce::Colour (0xff15171a), juce::Colour (0xff23262b), juce::Colours::white)
    {
        for (const char* id : { "decay", "size", "predelay", "damp", "lowcut", "mix" }) addKnob (*m.find (id));
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (24, 12);
        titleArea = r.removeFromTop (30);
        footswitch.setBounds (getLocalBounds().removeFromTop (44).removeFromRight (50).reduced (8));
        const int h = r.getHeight() / 2;
        auto r1 = r.removeFromTop (h), r2 = r;
        std::vector<Knob*> a { knobs[0].get(), knobs[1].get(), knobs[2].get() }, b { knobs[3].get(), knobs[4].get(), knobs[5].get() };
        rowOfKnobs (r1, a); rowOfKnobs (r2, b);
    }
};

// ---------------------------------------------------------------------------------------------
// Schema del segnale (segue l'ordine reale dei moduli) + stato audio
class FlowPanel : public juce::Component
{
public:
    explicit FlowPanel (RigEngine& e) : engine (e) {}
    float cpu = 0.0f;

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff191c20), 0, r.getY(), juce::Colour (0xff0e1012), 0, r.getBottom(), false));
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (juce::Colour (0xff30343a)); g.drawRoundedRectangle (r, 10.0f, 1.5f);

        g.setColour (col::gold); g.setFont (uiFont (13.0f, true));
        g.drawText ("SIGNAL CHAIN", 16, 10, 200, 18, juce::Justification::centredLeft);

        struct Node { juce::String label; bool active; };
        std::vector<Node> nodes { { "IN", true } };
        auto shortName = [] (const juce::String& id)
        {
            if (id == "gate") return "GATE"; if (id == "nampedal") return "NAM P"; if (id == "od") return "OD"; if (id == "dist") return "DIST";
            if (id == "amp") return "AMP"; if (id == "cab") return "CAB"; if (id == "delay") return "DLY"; return "REV";
        };
        for (int k : engine.getPreOrder())  nodes.push_back ({ shortName (engine.preMods[k]->moduleId),  engine.preMods[k]->isActive() });
        nodes.push_back ({ "AMP", engine.amp.isActive() });
        nodes.push_back ({ "CAB", engine.cab.isActive() });
        for (int k : engine.getPostOrder()) nodes.push_back ({ shortName (engine.postMods[k]->moduleId), engine.postMods[k]->isActive() });
        nodes.push_back ({ "OUT", true });

        float x = 16.0f, y = 36.0f;
        const float maxX = (float) getWidth() - 14.0f, h = 22.0f;
        g.setFont (uiFont (11.0f, true));
        for (auto& n : nodes)
        {
            const float w = 16.0f + 6.6f * (float) n.label.length();
            if (x + w > maxX) { x = 16.0f; y += h + 6.0f; }
            auto box = juce::Rectangle<float> (x, y, w, h);
            g.setColour (n.active ? juce::Colour (0xff2a3a2e) : juce::Colour (0xff202226));
            g.fillRoundedRectangle (box, 5.0f);
            g.setColour (n.active ? col::ledGreen : juce::Colour (0xff45494f));
            g.drawRoundedRectangle (box, 5.0f, 1.2f);
            g.setColour (n.active ? col::text : col::dim);
            g.drawText (n.label, box, juce::Justification::centred);
            x += w + 5.0f;
        }

        const double sr = engine.sampleRate.load();
        const int buf = engine.bufferSize.load();
        const int drv = engine.driverLatency.load(), chain = engine.chainLatency.load();
        auto text = r.reduced (16.0f, 0).withTrimmedTop (y + h + 14.0f);
        g.setFont (uiFont (12.0f)); g.setColour (col::text);
        if (sr > 0)
        {
            const double msBuf = 1000.0 * buf / sr, msDrv = 1000.0 * drv / sr, msChain = 1000.0 * chain / sr;
            g.drawText (juce::String (sr / 1000.0, 1) + " kHz  -  buffer " + juce::String (buf) + " (" + juce::String (msBuf, 1) + " ms)",
                        text.removeFromTop (18), juce::Justification::centredLeft);
            g.setColour (col::dim);
            g.drawText ("Driver in+out: " + juce::String (msDrv, 1) + " ms  -  DSP: +" + juce::String (msChain, 2) + " ms",
                        text.removeFromTop (18), juce::Justification::centredLeft);
            g.drawText ("CPU audio: " + juce::String (juce::roundToInt (cpu * 100.0f)) + " %", text.removeFromTop (18), juce::Justification::centredLeft);
        }
        else
        {
            g.setColour (juce::Colours::orange);
            g.drawText ("Nessun device audio: apri Audio Settings", text.removeFromTop (18), juce::Justification::centredLeft);
        }
    }
private:
    RigEngine& engine;
};
} // namespace dr::ui
