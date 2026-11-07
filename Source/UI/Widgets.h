// Reusable controls: glowing metal knobs, pill buttons, toggle switches, panels, orbit pad, spectrum, envelope view.
#pragma once
#include "../PluginProcessor.h"
#include "Theme.h"

namespace PV
{
class PVLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PVLookAndFeel();
    void applyTheme();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parent) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int w, int h) override;
};

void drawMetalKnob (juce::Graphics&, juce::Rectangle<float> area, float pos01, bool bipolar, bool enabled = true);
void drawLed (juce::Graphics&, juce::Point<float> c, float r, bool on, juce::Colour col);
void drawPanel (juce::Graphics&, juce::Rectangle<float> r, const juce::String& title, bool led = true);
void drawScrew (juce::Graphics&, juce::Point<float> c, float r);

/** Slider with FL style handling: shift-drag fine control (ctrl/cmd = JUCE velocity mode), wheel, right-click menu. */
class PVKnob : public juce::Slider
{
public:
    PVKnob() { setSliderStyle (juce::Slider::RotaryVerticalDrag); setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0); setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
               setMouseDragSensitivity (220); setScrollWheelEnabled (true); setVelocityModeParameters (1.0, 1, 0.0, true, juce::ModifierKeys::ctrlAltCommandModifiers); }
    std::function<void()> onContextMenu;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
private:
    double startProp = 0; bool fineActive = false; int fineOrigin = 0;
};

/** knob + caption + value read-out, bound to one host parameter (re-bindable, e.g. when the selected layer changes). */
class KnobControl : public juce::Component, private juce::SettableTooltipClient
{
public:
    KnobControl (PlanetVelProcessor&, const juce::String& caption = {}, int paramIndex = -1, float knobDiameter = 56.0f);
    void attach (int paramIndex);
    void setCaption (const juce::String& c) { caption = c; repaint(); }
    int  paramIndex() const { return idx; }
    void paint (juce::Graphics&) override;
    void resized() override;
    void setCompact (bool c) { compact = c; resized(); }
    juce::String valueText() const;
private:
    void updateText();
    PlanetVelProcessor& proc; PVKnob knob; juce::String caption; int idx = -1; float diameter; bool compact = false;
    std::unique_ptr<juce::SliderParameterAttachment> att; juce::String text;
};

/** A row/column of selectable pills bound to a choice parameter. `subset` maps pill -> choice index. */
class ChoicePills : public juce::Component
{
public:
    ChoicePills (PlanetVelProcessor&, int paramIndex, const juce::StringArray& labels = {}, bool vertical = true, std::vector<int> subset = {});
    void attach (int paramIndex);
    void setLabels (const juce::StringArray& l) { labels = l; repaint(); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    int  current() const { return value; }
private:
    int pillAt (juce::Point<int>) const;
    PlanetVelProcessor& proc; int idx = -1; juce::StringArray labels; bool vertical; std::vector<int> subset; int value = 0;
    std::unique_ptr<juce::ParameterAttachment> att;
};

/** Plain pills bound to a callback value (non-parameter settings). */
class SettingPills : public juce::Component
{
public:
    SettingPills (const juce::StringArray& labels, std::function<void (int)> onPick, bool vertical = false) : labels (labels), vertical (vertical), onPick (std::move (onPick)) {}
    void setValue (int v) { value = v; repaint(); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    juce::StringArray labels; bool vertical; int value = 0; std::function<void (int)> onPick;
};

class ToggleSwitch : public juce::Component
{
public:
    enum Look { Switch, Led, Letter };
    ToggleSwitch (PlanetVelProcessor&, int paramIndex, const juce::String& label, Look look = Switch);
    void attach (int paramIndex);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool isOn() const { return on; }
    void setLabel (const juce::String& l) { label = l; repaint(); }
    std::function<void (bool)> onChange;
private:
    PlanetVelProcessor& proc; int idx = -1; juce::String label; Look look; bool on = false;
    std::unique_ptr<juce::ParameterAttachment> att;
};

/** Simple text button in the hardware style. */
class PVButton : public juce::Component
{
public:
    PVButton (const juce::String& t) : text (t) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { down = true; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override { down = false; repaint(); if (contains (e.getPosition()) && onClick) onClick(); }
    void setText (const juce::String& t) { text = t; repaint(); }
    void setActive (bool a) { active = a; repaint(); }
    std::function<void()> onClick;
private:
    juce::String text; bool down = false, active = false;
};

class PanelBg : public juce::Component
{
public:
    PanelBg (const juce::String& t, bool led = true) : title (t), hasLed (led) { setInterceptsMouseClicks (false, false); }
    void setTitle (const juce::String& t) { title = t; repaint(); }
    void paint (juce::Graphics& g) override { drawPanel (g, getLocalBounds().toFloat().reduced (0.5f), title, hasLed); }
private:
    juce::String title; bool hasLed;
};

/** XY blend pad for the 4 layers. */
class OrbitPad : public juce::Component
{
public:
    explicit OrbitPad (PlanetVelProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
private:
    void setFrom (juce::Point<float>);
    PlanetVelProcessor& proc;
};

class SpectrumView : public juce::Component
{
public:
    explicit SpectrumView (PlanetVelProcessor&);
    void paint (juce::Graphics&) override;
    void tick();
private:
    PlanetVelProcessor& proc; juce::dsp::FFT fft { 10 }; std::array<float, 2048> work {}; std::array<float, 16> bars {}, peaks {};
};

class EnvView : public juce::Component
{
public:
    explicit EnvView (PlanetVelProcessor& p) : proc (p) { setInterceptsMouseClicks (false, false); }
    void setLayer (int l) { layer = l; repaint(); }
    void paint (juce::Graphics&) override;
private:
    PlanetVelProcessor& proc; int layer = 0;
};
} // namespace PV
