#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "../dsp/Param.h"

namespace dr::ui
{
inline juce::Font uiFont (float h, bool bold = false)
{
    return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
}

namespace col
{
    const juce::Colour bg        { 0xff0e1012 };
    const juce::Colour topBar    { 0xff1b1e22 };
    const juce::Colour gold      { 0xffd4b15f };
    const juce::Colour led       { 0xffff3b2f };
    const juce::Colour ledGreen  { 0xff46e06b };
    const juce::Colour text      { 0xffe6e6e6 };
    const juce::Colour dim       { 0xff8a8f96 };
}

// ---------------------------------------------------------------------------------------------
class RigLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RigLookAndFeel()
    {
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff15171a));
        setColour (juce::ComboBox::textColourId, col::text);
        setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff3a3e44));
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1b1e22));
        setColour (juce::PopupMenu::textColourId, col::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff3c4048));
        setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a2d33));
        setColour (juce::TextButton::textColourOffId, col::text);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1,
                           juce::Slider& s) override
    {
        auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
        const float r = std::min (b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const float rb = r * 0.74f;
        const float ang = a0 + pos * (a1 - a0);

        // tacche
        g.setColour (juce::Colours::white.withAlpha (0.45f));
        for (int i = 0; i <= 10; ++i)
        {
            const float a = a0 + (float) i / 10.0f * (a1 - a0);
            const juce::Point<float> d (std::sin (a), -std::cos (a));
            g.drawLine (juce::Line<float> (c + d * (r * 0.86f), c + d * (r * (i % 5 == 0 ? 1.0f : 0.95f))), i % 5 == 0 ? 1.6f : 1.0f);
        }

        const auto base = s.findColour (juce::Slider::rotarySliderFillColourId);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillEllipse (juce::Rectangle<float> (rb * 2, rb * 2).withCentre (c + juce::Point<float> (1.5f, 3.0f)));

        auto body = juce::Rectangle<float> (rb * 2, rb * 2).withCentre (c);
        g.setGradientFill (juce::ColourGradient (base.brighter (0.7f), body.getX(), body.getY(),
                                                 base.darker (0.8f), body.getRight(), body.getBottom(), false));
        g.fillEllipse (body);
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawEllipse (body, 1.2f);

        auto cap = body.reduced (rb * 0.26f);
        g.setGradientFill (juce::ColourGradient (base.darker (0.3f), cap.getX(), cap.getY(),
                                                 base.brighter (0.5f), cap.getRight(), cap.getBottom(), false));
        g.fillEllipse (cap);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawEllipse (cap, 0.8f);

        const juce::Point<float> d (std::sin (ang), -std::cos (ang));
        g.setColour (s.findColour (juce::Slider::thumbColourId));
        g.drawLine (juce::Line<float> (c + d * (rb * 0.28f), c + d * (rb * 0.95f)), 3.0f);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (box.findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
        juce::Path p;
        const float cx = (float) w - 12.0f, cy = (float) h * 0.5f;
        p.addTriangle (cx - 4, cy - 2, cx + 4, cy - 2, cx, cy + 3);
        g.setColour (col::dim);
        g.fillPath (p);
    }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return uiFont (13.0f, true); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return uiFont (13.0f, true); }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bc, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        auto c = bc.brighter (over ? 0.15f : 0.0f).darker (down ? 0.25f : 0.0f);
        g.setGradientFill (juce::ColourGradient (c.brighter (0.25f), 0, r.getY(), c.darker (0.3f), 0, r.getBottom(), false));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool) override
    {
        auto r = b.getLocalBounds().toFloat();
        const float d = std::min (14.0f, r.getHeight() - 4.0f);
        auto led = juce::Rectangle<float> (d, d).withCentre ({ r.getX() + d * 0.5f + 2.0f, r.getCentreY() });
        const bool on = b.getToggleState();
        const auto c = b.findColour (juce::ToggleButton::tickColourId);
        if (on) { g.setColour (c.withAlpha (0.35f)); g.fillEllipse (led.expanded (3.0f)); }
        g.setColour (on ? c : juce::Colour (0xff3a2a2a));
        g.fillEllipse (led);
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawEllipse (led, 1.0f);
        g.setColour (b.findColour (juce::ToggleButton::textColourId));
        g.setFont (uiFont (12.0f, true));
        g.drawText (b.getButtonText(), (int) (led.getRight() + 6), 0, (int) (r.getWidth() - d - 8), (int) r.getHeight(),
                    juce::Justification::centredLeft);
    }
};

// ---------------------------------------------------------------------------------------------
// Manopola collegata a un Param (nome + valore sotto). Doppio click = valore di default.
class Knob : public juce::Component
{
public:
    Knob (Param& p, juce::Colour body, juce::Colour pointer, juce::Colour labelColour)
        : param (p), labelCol (labelColour)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
        slider.setRange (p.minV, p.maxV, 0.0);
        if (p.minV > 0.0f && p.maxV / p.minV > 10.0f)
            slider.setSkewFactorFromMidPoint (std::sqrt (p.minV * p.maxV));
        slider.setDoubleClickReturnValue (true, p.defV);
        slider.setColour (juce::Slider::rotarySliderFillColourId, body);
        slider.setColour (juce::Slider::thumbColourId, pointer);
        slider.setValue (p.get(), juce::dontSendNotification);
        slider.onValueChange = [this] { param.set ((float) slider.getValue()); updateText(); };
        addAndMakeVisible (slider);
        updateText();
    }

    void refresh() { slider.setValue (param.get(), juce::dontSendNotification); updateText(); }

    void resized() override
    {
        auto b = getLocalBounds();
        valueArea = b.removeFromBottom (13);
        nameArea = b.removeFromBottom (15);
        slider.setBounds (b);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (labelCol);
        g.setFont (uiFont (12.0f, true));
        g.drawText (param.name.toUpperCase(), nameArea, juce::Justification::centred);
        g.setColour (labelCol.withAlpha (0.65f));
        g.setFont (uiFont (11.0f));
        g.drawText (valueText, valueArea, juce::Justification::centred);
    }

private:
    void updateText()
    {
        valueText = juce::String (param.get(), param.decimals) + param.suffix;
        repaint (valueArea);
    }

    Param& param;
    juce::Slider slider;
    juce::Colour labelCol;
    juce::Rectangle<int> nameArea, valueArea;
    juce::String valueText;
};

// Menu a tendina collegato a un Param di tipo Choice
class ChoiceBox : public juce::ComboBox
{
public:
    explicit ChoiceBox (Param& p) : param (p)
    {
        for (int i = 0; i < p.choices.size(); ++i) addItem (p.choices[i], i + 1);
        setSelectedId (p.index() + 1, juce::dontSendNotification);
        onChange = [this] { param.set ((float) (getSelectedId() - 1)); if (extra) extra(); };
    }
    void refresh() { setSelectedId (param.index() + 1, juce::dontSendNotification); }
    std::function<void()> extra;
private:
    Param& param;
};

// Toggle a LED collegato a un Param
class LedToggle : public juce::ToggleButton
{
public:
    LedToggle (Param& p, const juce::String& text, juce::Colour ledColour, juce::Colour textColour) : param (p)
    {
        setButtonText (text);
        setColour (juce::ToggleButton::tickColourId, ledColour);
        setColour (juce::ToggleButton::textColourId, textColour);
        setToggleState (p.on(), juce::dontSendNotification);
        onClick = [this] { param.set (getToggleState() ? 1.0f : 0.0f); if (extra) extra(); };
    }
    void refresh() { setToggleState (param.on(), juce::dontSendNotification); }
    std::function<void()> extra;
private:
    Param& param;
};

// Footswitch cromato (pedali) o piccolo pulsante power (rack/ampli)
class Footswitch : public juce::Button
{
public:
    enum class Style { Stomp, Power };
    Footswitch (Param& p, Style s, juce::Colour ledColour) : juce::Button ("fs"), param (p), style (s), ledCol (ledColour)
    {
        onClick = [this] { param.set (param.on() ? 0.0f : 1.0f); if (extra) extra(); };
    }
    std::function<void()> extra;

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        const bool on = param.on();
        if (style == Style::Power)
        {
            const float d = std::min (r.getWidth(), r.getHeight());
            auto led = juce::Rectangle<float> (d, d).withCentre (r.getCentre()).reduced (d * 0.2f);
            if (on) { g.setColour (ledCol.withAlpha (0.35f)); g.fillEllipse (led.expanded (4.0f)); }
            g.setColour (on ? ledCol : juce::Colour (0xff3a2a2a));
            g.fillEllipse (led);
            g.setColour (juce::Colours::black); g.drawEllipse (led, 1.2f);
            return;
        }
        const float d = std::min (r.getWidth(), r.getHeight());
        auto c = juce::Rectangle<float> (d, d).withCentre (r.getCentre());
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (c.translated (1.5f, 3.0f));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff2f2f2), c.getX(), c.getY(), juce::Colour (0xff6c7077), c.getRight(), c.getBottom(), false));
        g.fillEllipse (c);
        auto in = c.reduced (d * 0.14f);
        const float k = down ? 0.75f : (over ? 1.1f : 1.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff9aa0a8).withMultipliedBrightness (k), in.getX(), in.getY(),
                                                 juce::Colour (0xffd8dbe0).withMultipliedBrightness (k), in.getRight(), in.getBottom(), false));
        g.fillEllipse (in);
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawEllipse (c, 1.2f);
    }
private:
    Param& param;
    Style style;
    juce::Colour ledCol;
};

// Meter a LED orizzontale (-60 .. 0 dBFS)
class LevelMeter : public juce::Component
{
public:
    void setPeak (float lin)
    {
        const float db = lin > 1.0e-4f ? 20.0f * std::log10 (lin) : -90.0f;
        shown = std::max (db, shown - 3.0f);
        clipHold = (lin >= 0.999f) ? 30 : std::max (0, clipHold - 1);
        repaint();
    }
    void paint (juce::Graphics& g) override
    {
        const int segs = 24;
        const float w = (float) getWidth() / segs;
        for (int i = 0; i < segs; ++i)
        {
            const float segDb = -60.0f + (float) (i + 1) * 60.0f / segs;
            const bool lit = shown >= segDb - 2.5f;
            juce::Colour c = segDb > -3.0f ? juce::Colour (0xffff4040) : segDb > -12.0f ? juce::Colour (0xffe8c93a) : juce::Colour (0xff46e06b);
            g.setColour (lit ? c : c.withAlpha (0.12f));
            g.fillRoundedRectangle (i * w + 1.0f, 2.0f, w - 2.0f, (float) getHeight() - 4.0f, 1.5f);
        }
        if (clipHold > 0) { g.setColour (juce::Colours::red); g.fillEllipse ((float) getWidth() + 2.0f, 4.0f, 6.0f, 6.0f); }
    }
private:
    float shown = -90.0f;
    int clipHold = 0;
};
} // namespace dr::ui
