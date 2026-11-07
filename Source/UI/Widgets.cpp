#include "Widgets.h"

namespace PV
{
using juce::Colour; using juce::Rectangle; using juce::Point;

// ================================================================================================ drawing helpers
void drawLed (juce::Graphics& g, Point<float> c, float r, bool on, Colour col)
{
    if (on)
    {
        g.setGradientFill (juce::ColourGradient (col.withAlpha (0.55f), c, col.withAlpha (0.0f), c.translated (r * 3.2f, 0), true)); g.fillEllipse (c.x - r * 3.2f, c.y - r * 3.2f, r * 6.4f, r * 6.4f);
        g.setColour (col); g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
        g.setColour (juce::Colours::white.withAlpha (0.55f)); g.fillEllipse (c.x - r * 0.45f, c.y - r * 0.55f, r * 0.7f, r * 0.6f);
    }
    else { g.setColour (col.darker (0.8f).withAlpha (0.9f)); g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2); g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 0.8f); }
}

void drawScrew (juce::Graphics& g, Point<float> c, float r)
{
    g.setGradientFill (juce::ColourGradient (Colour (0xffc8c4d0), c.translated (-r * 0.4f, -r * 0.5f), Colour (0xff3a3542), c.translated (r, r), true)); g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    g.setColour (Colour (0xff1a1620)); g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 1.0f);
    g.setColour (Colour (0xff24202c)); g.drawLine (c.x - r * 0.55f, c.y, c.x + r * 0.55f, c.y, 1.6f); g.drawLine (c.x, c.y - r * 0.55f, c.x, c.y + r * 0.55f, 1.6f);
}

void drawPanel (juce::Graphics& g, Rectangle<float> r, const juce::String& title, bool led)
{
    const auto& t = theme();
    g.setGradientFill (juce::ColourGradient (t.panel.brighter (0.12f), r.getX(), r.getY(), t.panel.darker (0.35f), r.getX(), r.getBottom(), false)); g.fillRoundedRectangle (r, 9.0f);
    g.setColour (t.border.withAlpha (0.55f)); g.drawRoundedRectangle (r.reduced (0.5f), 9.0f, 1.0f);
    g.setColour (t.accent2.withAlpha (0.12f)); g.drawRoundedRectangle (r.reduced (2.5f), 7.0f, 1.0f);          // subtle inner glow
    if (title.isNotEmpty())
    {
        const float lx = r.getX() + 15.0f, ly = r.getY() + 14.0f;
        if (led) drawLed (g, { lx, ly }, 3.0f, true, t.led);
        g.setColour (t.text); drawSpaced (g, title, { lx + (led ? 10.0f : -4.0f), ly - 9.0f, r.getWidth() - 40.0f, 18.0f }, juce::Justification::centredLeft, sansFont (12.5f), 0.12f);
        auto line = Rectangle<float> (r.getX() + 12.0f, ly + 12.0f, r.getWidth() - 24.0f, 1.2f);
        g.setGradientFill (juce::ColourGradient (t.accent.withAlpha (0.65f), line.getX(), line.getY(), t.accent2.withAlpha (0.0f), line.getRight(), line.getY(), false)); g.fillRect (line);
    }
}

void drawMetalKnob (juce::Graphics& g, Rectangle<float> a, float pos, bool bipolar, bool enabled)
{
    const auto& t = theme();
    const float d = juce::jmin (a.getWidth(), a.getHeight()), r = d * 0.5f - 1.0f;
    const auto c = a.getCentre();
    const float s0 = juce::MathConstants<float>::pi * 1.25f, s1 = juce::MathConstants<float>::pi * 2.75f, ang = s0 + pos * (s1 - s0);
    const float ringR = r - 1.5f, stroke = juce::jmax (2.2f, d * 0.06f);
    juce::Path track; track.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, s0, s1, true);
    g.setColour (t.accent2.withAlpha (0.22f)); g.strokePath (track, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    // value arc with bloom
    const float aFrom = bipolar ? (s0 + s1) * 0.5f : s0, aTo = ang;
    if (std::abs (aTo - aFrom) > 0.004f && enabled)
    {
        juce::Path arc; arc.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, juce::jmin (aFrom, aTo), juce::jmax (aFrom, aTo), true);
        g.setColour (t.accent.withAlpha (0.10f)); g.strokePath (arc, juce::PathStrokeType (stroke * 3.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (t.accent.withAlpha (0.22f)); g.strokePath (arc, juce::PathStrokeType (stroke * 2.1f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (t.accent); g.strokePath (arc, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    // body
    const float br = r * 0.70f;
    g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillEllipse (c.x - br - 1.5f, c.y - br + 2.0f, br * 2 + 3.0f, br * 2 + 3.0f);
    g.setGradientFill (juce::ColourGradient (Colour (0xfff2f0f5), c.x - br * 0.5f, c.y - br * 0.7f, Colour (0xff4a4654), c.x + br * 0.6f, c.y + br * 0.9f, false));
    g.fillEllipse (c.x - br, c.y - br, br * 2, br * 2);
    g.setColour (Colour (0xff16121a)); g.drawEllipse (c.x - br, c.y - br, br * 2, br * 2, 1.2f);
    for (int i = 1; i <= 5; ++i)                                                                     // brushed rings
    { const float rr = br * (float) i / 5.5f; g.setColour (juce::Colours::white.withAlpha (i % 2 ? 0.07f : 0.03f)); g.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, 0.7f); }
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.35f), c.x - br * 0.4f, c.y - br * 0.6f, juce::Colours::transparentWhite, c.x, c.y, true));
    g.fillEllipse (c.x - br * 0.9f, c.y - br * 0.95f, br * 1.2f, br * 0.8f);
    // pointer
    const float cs = std::cos (ang - juce::MathConstants<float>::halfPi), sn = std::sin (ang - juce::MathConstants<float>::halfPi);
    g.setColour (Colour (0xff1c1220)); g.drawLine (c.x + cs * br * 0.28f, c.y + sn * br * 0.28f, c.x + cs * br * 0.9f, c.y + sn * br * 0.9f, juce::jmax (2.0f, d * 0.05f));
}

// ================================================================================================ look and feel
PVLookAndFeel::PVLookAndFeel() { applyTheme(); }
void PVLookAndFeel::applyTheme()
{
    const auto& t = theme();
    setColour (juce::PopupMenu::backgroundColourId, t.panel.darker (0.4f)); setColour (juce::PopupMenu::textColourId, t.text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, t.accent.withAlpha (0.8f)); setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TooltipWindow::backgroundColourId, t.lcd); setColour (juce::TooltipWindow::textColourId, t.lcdText); setColour (juce::TooltipWindow::outlineColourId, t.border);
    setColour (juce::TextEditor::backgroundColourId, t.lcd); setColour (juce::TextEditor::textColourId, t.lcdText); setColour (juce::TextEditor::outlineColourId, t.border.withAlpha (0.5f));
    setColour (juce::TextEditor::focusedOutlineColourId, t.accent); setColour (juce::CaretComponent::caretColourId, t.lcdText); setColour (juce::TextEditor::highlightColourId, t.accent.withAlpha (0.4f));
    setColour (juce::AlertWindow::backgroundColourId, t.panel); setColour (juce::AlertWindow::textColourId, t.text); setColour (juce::TextButton::buttonColourId, t.panel.brighter (0.2f));
}
void PVLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider& s)
{ drawMetalKnob (g, Rectangle<float> ((float) x, (float) y, (float) w, (float) h), pos, (bool) s.getProperties()["bipolar"], s.isEnabled()); }

void PVLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle style, juce::Slider& s)
{
    const auto& t = theme();
    if (style == juce::Slider::LinearVertical)             // pitch / mod wheel
    {
        auto slot = Rectangle<float> ((float) x + w * 0.22f, (float) y, w * 0.56f, (float) h);
        g.setGradientFill (juce::ColourGradient (Colour (0xff0a050c), slot.getX(), 0, Colour (0xff2a1830), slot.getRight(), 0, false)); g.fillRoundedRectangle (slot, 5.0f);
        g.setColour (t.border.withAlpha (0.6f)); g.drawRoundedRectangle (slot, 5.0f, 1.0f);
        const float hh = 20.0f, py = pos - hh * 0.5f;
        auto cap = Rectangle<float> ((float) x + 1.0f, py, (float) w - 2.0f, hh);
        g.setGradientFill (juce::ColourGradient (Colour (0xfff0eef4), cap.getX(), cap.getY(), Colour (0xff6c6878), cap.getX(), cap.getBottom(), false)); g.fillRoundedRectangle (cap, 3.0f);
        g.setColour (Colour (0xff2a2430)); g.drawRoundedRectangle (cap, 3.0f, 1.0f); g.setColour (t.accent.withAlpha (0.9f)); g.fillRect (cap.getX() + 3.0f, cap.getCentreY() - 1.0f, cap.getWidth() - 6.0f, 2.0f);
        return;
    }
    // horizontal bar (mod matrix amounts, level bars)
    auto r = Rectangle<float> ((float) x, (float) y + h * 0.5f - 3.0f, (float) w, 6.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (r, 3.0f);
    const bool bip = (bool) s.getProperties()["bipolar"]; const float c0 = bip ? r.getCentreX() : r.getX();
    g.setColour (t.accent); g.fillRoundedRectangle (Rectangle<float> (juce::jmin (c0, pos), r.getY(), std::abs (pos - c0), r.getHeight()), 3.0f);
    g.setColour (Colour (0xffe8e4ee)); g.fillEllipse (pos - 6.0f, r.getCentreY() - 6.0f, 12.0f, 12.0f); g.setColour (Colour (0xff1c1220)); g.drawEllipse (pos - 6.0f, r.getCentreY() - 6.0f, 12.0f, 12.0f, 1.0f);
}

Rectangle<int> PVLookAndFeel::getTooltipBounds (const juce::String& text, Point<int> pos, Rectangle<int> parent)
{
    juce::GlyphArrangement ga; ga.addLineOfText (monoFont (13.0f), text, 0.0f, 0.0f);
    const int w = (int) ga.getBoundingBox (0, -1, true).getWidth() + 18, h = 24;
    return Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - w - 6 : pos.x + 14, pos.y + 8, w, h).constrainedWithin (parent);
}
void PVLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    const auto& t = theme(); g.setColour (t.lcd); g.fillRoundedRectangle (0, 0, (float) w, (float) h, 5.0f);
    g.setColour (t.accent.withAlpha (0.7f)); g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1, (float) h - 1, 5.0f, 1.0f);
    g.setColour (t.lcdText); g.setFont (monoFont (13.0f)); g.drawText (text, 0, 0, w, h, juce::Justification::centred);
}

// ================================================================================================ knob
void PVKnob::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { if (onContextMenu) onContextMenu(); return; }
    startProp = valueToProportionOfLength (getValue()); fineActive = false;
    juce::Slider::mouseDown (e);
}
void PVKnob::mouseDrag (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    if (e.mods.isShiftDown())                // FL habit: shift+drag = fine control (ctrl/cmd uses JUCE's velocity mode)
    {
        if (! fineActive) { fineActive = true; startProp = valueToProportionOfLength (getValue()); fineOrigin = e.getPosition().y + e.getPosition().x; }
        const double delta = (double) (fineOrigin - (e.getPosition().y + e.getPosition().x)) / 1800.0;
        setValue (proportionOfLengthToValue (juce::jlimit (0.0, 1.0, startProp + delta)), juce::sendNotificationSync);
        return;
    }
    fineActive = false; juce::Slider::mouseDrag (e);
}
void PVKnob::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const double step = (e.mods.isShiftDown() || e.mods.isCtrlDown() ? 0.002 : 0.02) * (w.isReversed ? -1.0 : 1.0) * (w.deltaY > 0 ? 1.0 : -1.0);
    if (std::abs (w.deltaY) < 1.0e-6f) return;
    const auto* d = getNumDecimalPlacesToDisplay() == 0 ? this : this; juce::ignoreUnused (d);
    setValue (proportionOfLengthToValue (juce::jlimit (0.0, 1.0, valueToProportionOfLength (getValue()) + step * (getInterval() >= 1.0 ? 3.0 : 1.0))), juce::sendNotificationSync);
}

KnobControl::KnobControl (PlanetVelProcessor& p, const juce::String& cap, int pi, float dia) : proc (p), caption (cap), diameter (dia)
{
    addAndMakeVisible (knob);
    knob.onValueChange = [this] { updateText(); };
    knob.onContextMenu = [this]
    {
        if (idx < 0) return;
        juce::PopupMenu m; const auto& d = defs()[(size_t) idx];
        m.addSectionHeader (d.name);
        m.addItem (1, "Reset to default");
        const int cc = proc.ccFor (idx);
        m.addItem (2, proc.midiLearnTarget() == idx ? "MIDI Learn: move a controller..." : "MIDI Learn");
        if (cc >= 0) m.addItem (3, "Clear MIDI mapping (CC " + juce::String (cc) + ")");
        m.addSeparator(); m.addItem (4, "Automate: right-click the knob in FL > Create automation clip", false, false);
        m.showMenuAsync (juce::PopupMenu::Options(), [this] (int r)
        {
            if (idx < 0) return;
            if (r == 1) proc.setParamNormalised (idx, proc.param (idx)->getDefaultValue(), true);
            else if (r == 2) proc.startMidiLearn (idx);
            else if (r == 3) proc.clearMidiMap (idx);
        });
    };
    if (pi >= 0) attach (pi);
}

void KnobControl::attach (int pi)
{
    att.reset(); idx = pi; if (pi < 0) return;
    const auto& d = defs()[(size_t) pi]; auto* prm = proc.param (pi);
    knob.getProperties().set ("bipolar", d.bipolarUi);
    att = std::make_unique<juce::SliderParameterAttachment> (*prm, knob, nullptr);
    knob.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
    knob.setNumDecimalPlacesToDisplay (d.kind == Kind::Float ? 3 : 0);
    updateText();
}
void KnobControl::updateText()
{
    if (idx < 0) return;
    text = proc.param (idx)->getText (proc.param (idx)->getValue(), 12); repaint();
    setTooltip (defs()[(size_t) idx].name + ": " + text);
    knob.setTooltip (defs()[(size_t) idx].name + ": " + text);
}
juce::String KnobControl::valueText() const { return text; }
void KnobControl::resized()
{
    const float d = juce::jmin (diameter, (float) getWidth());
    knob.setBounds (juce::Rectangle<float> ((getWidth() - d) * 0.5f, compact ? 0.0f : 0.0f, d, d).toNearestInt());
}
void KnobControl::paint (juce::Graphics& g)
{
    const auto& t = theme(); const float d = juce::jmin (diameter, (float) getWidth());
    g.setColour (t.text); drawSpaced (g, caption, { 0.0f, d + 2.0f, (float) getWidth(), 14.0f }, juce::Justification::centred, sansFont (10.5f), 0.1f);
    g.setColour (t.accent.withAlpha (0.85f)); g.setFont (monoFont (9.5f)); g.drawText (text, 0, (int) d + 14, getWidth(), 12, juce::Justification::centred);
}

// ================================================================================================ pills
static void drawPill (juce::Graphics& g, Rectangle<float> r, const juce::String& txt, bool active, bool hover = false)
{
    const auto& t = theme();
    if (active)
    {
        g.setGradientFill (juce::ColourGradient (t.accent2.brighter (0.2f), r.getX(), r.getY(), t.accent2.darker (0.3f), r.getX(), r.getBottom(), false)); g.fillRoundedRectangle (r, 4.0f);
        g.setColour (t.accent2.brighter (0.5f).withAlpha (0.8f)); g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
    }
    else { g.setColour (juce::Colours::black.withAlpha (0.38f)); g.fillRoundedRectangle (r, 4.0f); g.setColour (t.border.withAlpha (hover ? 0.9f : 0.5f)); g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f); }
    g.setColour (active ? juce::Colours::white : t.dim); drawSpaced (g, txt, r, juce::Justification::centred, sansFont (juce::jmin (11.0f, r.getHeight() * 0.5f + 3.0f)), 0.06f);
}

ChoicePills::ChoicePills (PlanetVelProcessor& p, int pi, const juce::StringArray& l, bool v, std::vector<int> sub) : proc (p), labels (l), vertical (v), subset (std::move (sub)) { if (pi >= 0) attach (pi); }
void ChoicePills::attach (int pi)
{
    att.reset(); idx = pi; if (pi < 0) return;
    auto* prm = proc.param (pi);
    if (labels.isEmpty()) labels = defs()[(size_t) pi].choices;
    att = std::make_unique<juce::ParameterAttachment> (*prm, [this] (float v) { value = (int) std::round (v); repaint(); }, nullptr);
    att->sendInitialUpdate();
}
int ChoicePills::pillAt (Point<int> p) const
{
    const int n = subset.empty() ? labels.size() : (int) subset.size(); if (n <= 0) return -1;
    const int i = vertical ? p.y * n / juce::jmax (1, getHeight()) : p.x * n / juce::jmax (1, getWidth());
    return juce::jlimit (0, n - 1, i);
}
void ChoicePills::paint (juce::Graphics& g)
{
    const int n = subset.empty() ? labels.size() : (int) subset.size(); if (n <= 0) return;
    const float gap = 3.0f; const auto b = getLocalBounds().toFloat();
    const float w = vertical ? b.getWidth() : (b.getWidth() - gap * (float) (n - 1)) / (float) n, h = vertical ? (b.getHeight() - gap * (float) (n - 1)) / (float) n : b.getHeight();
    for (int i = 0; i < n; ++i)
    {
        const int choice = subset.empty() ? i : subset[(size_t) i];
        auto r = vertical ? Rectangle<float> (0, (float) i * (h + gap), w, h) : Rectangle<float> ((float) i * (w + gap), 0, w, h);
        drawPill (g, r, choice < labels.size() ? labels[choice] : juce::String (choice), choice == value);
    }
}
void ChoicePills::mouseDown (const juce::MouseEvent& e)
{
    if (att == nullptr) return; const int i = pillAt (e.getPosition()); if (i < 0) return;
    const int choice = subset.empty() ? i : subset[(size_t) i]; att->setValueAsCompleteGesture ((float) choice);
}

void SettingPills::paint (juce::Graphics& g)
{
    const int n = labels.size(); if (n <= 0) return; const float gap = 3.0f; const auto b = getLocalBounds().toFloat();
    const float w = vertical ? b.getWidth() : (b.getWidth() - gap * (float) (n - 1)) / (float) n, h = vertical ? (b.getHeight() - gap * (float) (n - 1)) / (float) n : b.getHeight();
    for (int i = 0; i < n; ++i) drawPill (g, vertical ? Rectangle<float> (0, (float) i * (h + gap), w, h) : Rectangle<float> ((float) i * (w + gap), 0, w, h), labels[i], i == value);
}
void SettingPills::mouseDown (const juce::MouseEvent& e)
{
    const int n = labels.size(); if (n <= 0) return;
    const int i = juce::jlimit (0, n - 1, vertical ? e.y * n / juce::jmax (1, getHeight()) : e.x * n / juce::jmax (1, getWidth()));
    value = i; repaint(); if (onPick) onPick (i);
}

ToggleSwitch::ToggleSwitch (PlanetVelProcessor& p, int pi, const juce::String& l, Look lk) : proc (p), label (l), look (lk) { setMouseCursor (juce::MouseCursor::PointingHandCursor); if (pi >= 0) attach (pi); }
void ToggleSwitch::attach (int pi)
{
    att.reset(); idx = pi; if (pi < 0) return;
    att = std::make_unique<juce::ParameterAttachment> (*proc.param (pi), [this] (float v) { on = v > 0.5f; repaint(); if (onChange) onChange (on); }, nullptr);
    att->sendInitialUpdate();
}
void ToggleSwitch::mouseDown (const juce::MouseEvent&) { if (att) att->setValueAsCompleteGesture (on ? 0.0f : 1.0f); }
void ToggleSwitch::paint (juce::Graphics& g)
{
    const auto& t = theme(); const auto b = getLocalBounds().toFloat();
    if (look == Switch)
    {
        auto sw = Rectangle<float> (0, b.getCentreY() - 8.5f, 38.0f, 17.0f);
        g.setGradientFill (juce::ColourGradient (Colour (0xff0a0610), 0, sw.getY(), Colour (0xff2a1c30), 0, sw.getBottom(), false)); g.fillRoundedRectangle (sw, 8.5f);
        g.setColour (t.border.withAlpha (0.7f)); g.drawRoundedRectangle (sw.reduced (0.5f), 8.5f, 1.0f);
        const float kx = on ? sw.getRight() - 9.0f : sw.getX() + 9.0f;
        g.setGradientFill (juce::ColourGradient (Colour (0xfff4f2f8), kx - 6, sw.getY(), Colour (0xff8a8696), kx + 6, sw.getBottom(), false)); g.fillEllipse (kx - 6.5f, sw.getCentreY() - 6.5f, 13.0f, 13.0f);
        drawLed (g, { sw.getRight() + 11.0f, b.getCentreY() }, 2.8f, on, t.led);
        g.setColour (on ? t.text : t.dim); drawSpaced (g, label, { sw.getRight() + 19.0f, 0, b.getWidth() - sw.getWidth() - 19.0f, b.getHeight() }, juce::Justification::centredLeft, sansFont (10.0f), 0.08f);
    }
    else if (look == Led)
    {
        drawLed (g, { 9.0f, b.getCentreY() }, 4.0f, on, t.led);
        g.setColour (on ? t.text : t.dim); drawSpaced (g, label, { 20.0f, 0, b.getWidth() - 20.0f, b.getHeight() }, juce::Justification::centredLeft, sansFont (10.5f), 0.08f);
    }
    else drawPill (g, b.reduced (1.0f), label, on);
}

void PVButton::paint (juce::Graphics& g)
{
    const auto& t = theme(); auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setGradientFill (juce::ColourGradient (active || down ? t.accent : t.panel.brighter (0.25f), 0, r.getY(), active || down ? t.accent.darker (0.5f) : t.panel.darker (0.2f), 0, r.getBottom(), false)); g.fillRoundedRectangle (r, 5.0f);
    g.setColour (t.border); g.drawRoundedRectangle (r, 5.0f, 1.0f); g.setColour (juce::Colours::white.withAlpha (0.9f));
    drawSpaced (g, text, r, juce::Justification::centred, sansFont (10.5f), 0.1f);
}

// ================================================================================================ orbit pad
OrbitPad::OrbitPad (PlanetVelProcessor& p) : proc (p) { setMouseCursor (juce::MouseCursor::CrosshairCursor); }
void OrbitPad::paint (juce::Graphics& g)
{
    const auto& t = theme(); auto r = getLocalBounds().toFloat();
    g.setColour (Colour (0xff0b0510)); g.fillRoundedRectangle (r, 6.0f);
    g.setColour (t.border.withAlpha (0.8f)); g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    g.setColour (t.accent2.withAlpha (0.13f));
    for (int i = 1; i < 10; ++i) { g.drawVerticalLine ((int) (r.getWidth() * (float) i / 10.0f), 1, r.getBottom() - 1); g.drawHorizontalLine ((int) (r.getHeight() * (float) i / 8.0f), 1, r.getRight() - 1); }
    const auto c = r.getCentre();
    g.setColour (t.accent.withAlpha (0.35f)); const float dashes[] = { 3.0f, 4.0f };
    juce::Path e1, e2; e1.addEllipse (c.x - r.getWidth() * 0.16f, c.y - r.getHeight() * 0.16f, r.getWidth() * 0.32f, r.getHeight() * 0.32f); e2.addEllipse (c.x - r.getWidth() * 0.36f, c.y - r.getHeight() * 0.36f, r.getWidth() * 0.72f, r.getHeight() * 0.72f);
    juce::Path d1, d2; juce::PathStrokeType st (1.0f); st.createDashedStroke (d1, e1, dashes, 2); st.createDashedStroke (d2, e2, dashes, 2); g.fillPath (d1); g.fillPath (d2);
    g.setColour (t.dim); g.setFont (monoFont (9.5f)); g.drawText (u8 ("Y \xc2\xb7 SPACE"), r.reduced (8, 4).toNearestInt(), juce::Justification::topRight); g.drawText (u8 ("X \xc2\xb7 MORPH"), r.reduced (8, 4).toNearestInt(), juce::Justification::bottomLeft);
    g.setColour (t.dim.withAlpha (0.8f)); g.setFont (monoFont (9.0f, true));
    g.drawText ("A", 6, 4, 12, 12, juce::Justification::left); g.drawText ("B", getWidth() - 18, getHeight() - 16, 12, 12, juce::Justification::right);
    // set point (ring) and effective position (glowing dot, includes the orbit animation + modulation)
    const float sx = proc.getRaw (G_ORBIT_X) * r.getWidth(), sy = proc.getRaw (G_ORBIT_Y) * r.getHeight();
    g.setColour (t.hot.withAlpha (0.6f)); g.drawEllipse (sx - 6, sy - 6, 12, 12, 1.2f);
    const float ex = proc.orbitXNow.load() * r.getWidth(), ey = proc.orbitYNow.load() * r.getHeight();
    g.setGradientFill (juce::ColourGradient (t.accent.withAlpha (0.7f), ex, ey, t.accent.withAlpha (0.0f), ex + 18, ey, true)); g.fillEllipse (ex - 18, ey - 18, 36, 36);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white, ex - 2, ey - 2, t.accent, ex + 6, ey + 6, true)); g.fillEllipse (ex - 6, ey - 6, 12, 12);
}
void OrbitPad::setFrom (Point<float> p)
{
    proc.param (G_ORBIT_X)->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, p.x / (float) getWidth()));
    proc.param (G_ORBIT_Y)->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, p.y / (float) getHeight()));
}
void OrbitPad::mouseDown (const juce::MouseEvent& e) { proc.param (G_ORBIT_X)->beginChangeGesture(); proc.param (G_ORBIT_Y)->beginChangeGesture(); setFrom (e.position); }
void OrbitPad::mouseDrag (const juce::MouseEvent& e) { setFrom (e.position); }
void OrbitPad::mouseUp (const juce::MouseEvent&) { proc.param (G_ORBIT_X)->endChangeGesture(); proc.param (G_ORBIT_Y)->endChangeGesture(); }
void OrbitPad::mouseDoubleClick (const juce::MouseEvent&) { proc.setParamNormalised (G_ORBIT_X, 0.5f, true); proc.setParamNormalised (G_ORBIT_Y, 0.5f, true); }

// ================================================================================================ spectrum
SpectrumView::SpectrumView (PlanetVelProcessor& p) : proc (p) { setInterceptsMouseClicks (false, false); }
void SpectrumView::tick()
{
    const int pos = proc.scopePos.load();
    for (int i = 0; i < 1024; ++i) work[(size_t) i] = proc.scope[(size_t) ((pos - 1024 + i) & 2047)] * (0.5f - 0.5f * std::cos (kTwoPi * (float) i / 1023.0f));
    std::fill (work.begin() + 1024, work.end(), 0.0f);
    fft.performFrequencyOnlyForwardTransform (work.data());
    for (int b = 0; b < 16; ++b)
    {
        const int lo = (int) std::pow (2.0f, 1.0f + (float) b * 0.52f), hi = juce::jmax (lo + 1, (int) std::pow (2.0f, 1.0f + (float) (b + 1) * 0.52f));
        float m = 0; for (int k = lo; k < juce::jmin (hi, 512); ++k) m = juce::jmax (m, work[(size_t) k]);
        const float db = juce::Decibels::gainToDecibels (m / 24.0f + 1.0e-6f), v = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 55.0f);
        bars[(size_t) b] = v > bars[(size_t) b] ? v : bars[(size_t) b] * 0.82f + v * 0.18f;
        peaks[(size_t) b] = juce::jmax (bars[(size_t) b], peaks[(size_t) b] - 0.012f);
    }
    repaint();
}
void SpectrumView::paint (juce::Graphics& g)
{
    const auto& t = theme(); auto r = getLocalBounds().toFloat();
    g.setColour (Colour (0xff0a0510)); g.fillRoundedRectangle (r, 4.0f); g.setColour (t.border.withAlpha (0.6f)); g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
    const float bw = (r.getWidth() - 8.0f) / 16.0f;
    for (int i = 0; i < 16; ++i)
    {
        const float h = bars[(size_t) i] * (r.getHeight() - 8.0f), x = 4.0f + (float) i * bw;
        g.setGradientFill (juce::ColourGradient (t.accent, 0, r.getY() + 4, t.accent2, 0, r.getBottom() - 4, false)); g.fillRect (x + 1.0f, r.getBottom() - 4.0f - h, bw - 2.0f, h);
        g.setColour (t.led); g.fillRect (x + 1.0f, r.getBottom() - 5.0f - peaks[(size_t) i] * (r.getHeight() - 8.0f), bw - 2.0f, 1.5f);
    }
}

// ================================================================================================ envelope
void EnvView::paint (juce::Graphics& g)
{
    const auto& t = theme(); const auto b = getLocalBounds().toFloat().reduced (2.0f);
    const float a = proc.getRaw (lp (layer, L_ATK)), d = proc.getRaw (lp (layer, L_DEC)), s = proc.getRaw (lp (layer, L_SUS)), r = proc.getRaw (lp (layer, L_REL));
    auto sq = [] (float x) { return std::pow (juce::jlimit (0.0f, 1.0f, x), 0.45f); };
    const float wa = 0.18f * sq (a / 10.0f) + 0.04f, wd = 0.18f * sq (d / 10.0f) + 0.04f, wr = 0.22f * sq (r / 20.0f) + 0.04f, ws = 0.25f;
    const float tot = wa + wd + ws + wr; const float x0 = b.getX(), x1 = x0 + b.getWidth() * wa / tot, x2 = x1 + b.getWidth() * wd / tot, x3 = x2 + b.getWidth() * ws / tot, x4 = b.getRight();
    const float yb = b.getBottom(), yt = b.getY() + 2.0f, ys = yb - (yb - yt) * juce::jlimit (0.0f, 1.0f, s);
    juce::Path p; p.startNewSubPath (x0, yb); p.lineTo (x1, yt); p.lineTo (x2, ys); p.lineTo (x3, ys); p.lineTo (x4, yb);
    juce::Path fill (p); fill.closeSubPath(); g.setColour (t.accent2.withAlpha (0.16f)); g.fillPath (fill);
    g.setColour (t.accent2.withAlpha (0.3f)); g.strokePath (p, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (t.accent2.brighter (0.2f)); g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
} // namespace PV
