#include "LcdDisplay.h"

namespace PV
{
using juce::Colour; using juce::Rectangle; using juce::Point;

LcdDisplay::LcdDisplay (PlanetVelProcessor& p) : proc (p)
{
    cats.add ("ALL"); cats.add ("FAVORITES"); for (auto& c : PresetBank::categories()) cats.add (c); cats.add ("USER");
    for (size_t i = 0; i < PresetBank::factory().size(); ++i) { all.push_back (&PresetBank::factory()[i]); factoryIndex.push_back ((int) i); }
    addAndMakeVisible (searchBox);
    searchBox.setTextToShowWhenEmpty ("search name / tag...", theme().lcdText.withAlpha (0.4f)); searchBox.setFont (monoFont (11.5f)); searchBox.setMultiLine (false); searchBox.addListener (this);
    searchBox.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack); searchBox.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    searchBox.setIndents (4, 2); searchBox.setBorder ({ 0, 2, 0, 2 });
    // The search box must NOT steal the typing keys from the on-screen piano: it only takes keyboard focus when clicked,
    // and gives it up again on Enter / Esc / focus loss.
    searchBox.setWantsKeyboardFocus (false);
    searchBox.addMouseListener (this, false);
    searchBox.onFocusLost = [this] { searchBox.setWantsKeyboardFocus (false); };
    searchBox.onReturnKey = [this] { searchBox.giveAwayKeyboardFocus(); if (onSearchDone) onSearchDone(); };
    searchBox.onEscapeKey = [this] { searchBox.clear(); searchBox.giveAwayKeyboardFocus(); if (onSearchDone) onSearchDone(); };
    proc.addChangeListener (this);
    rebuildFilter();
}
LcdDisplay::~LcdDisplay() { proc.removeChangeListener (this); }

void LcdDisplay::ensureUserPresets()
{
    if (userLoaded) return; userLoaded = true; userPresets = PresetBank::loadUser();
    for (auto& u : userPresets) { all.push_back (&u); factoryIndex.push_back (-1); }
}

void LcdDisplay::rebuildFilter()
{
    if (catSel == (int) cats.size() - 1) ensureUserPresets();
    visible.clear(); const auto q = searchBox.getText().trim().toLowerCase();
    for (size_t i = 0; i < all.size(); ++i)
    {
        const auto& p = *all[i];
        if (catSel == 1 && ! proc.isFavorite (p.name)) continue;
        if (catSel >= 2 && catSel < cats.size() - 1 && p.category != PresetBank::categories()[catSel - 2]) continue;
        if (catSel == cats.size() - 1 && p.factory) continue;
        if (engineMask != 0 && (p.engineMask & engineMask) == 0) continue;
        if (q.isNotEmpty()) { const auto hay = (p.name + " " + p.category + " " + p.tags + " " + p.author).toLowerCase(); bool ok = true; for (auto& tok : juce::StringArray::fromTokens (q, " ", "")) if (! hay.contains (tok)) ok = false; if (! ok) continue; }
        visible.push_back ((int) i);
    }
    listScroll = 0; repaint();
}

LcdDisplay::Layout LcdDisplay::layout() const
{
    auto s = screen(); Layout l;
    l.header = s.removeFromTop (30).reduced (8, 0);
    l.tabs = s.removeFromTop (26).reduced (8, 2);
    l.footer = s.removeFromBottom (24).reduced (8, 2);
    s.removeFromTop (6); auto r = s.reduced (8, 0);
    l.cats = r.removeFromLeft (r.getWidth() * 27 / 100); r.removeFromLeft (8);
    auto top = r.removeFromTop (20); l.search = top.removeFromLeft (top.getWidth() * 2 / 3 - 4); top.removeFromLeft (6); l.chips = top;
    r.removeFromTop (4); l.list = r; return l;
}

void LcdDisplay::ensureCurrentVisible()
{
    const int rows = juce::jmax (1, layout().list.getHeight() / 19);
    for (size_t i = 0; i < visible.size(); ++i)
        if (all[(size_t) visible[i]]->name == proc.currentName())
        { if ((int) i < listScroll || (int) i >= listScroll + rows) listScroll = juce::jlimit (0, juce::jmax (0, (int) visible.size() - rows), (int) i - rows / 2); return; }
}

void LcdDisplay::resized() { searchBox.setBounds (layout().search); }

void LcdDisplay::tick (bool animate)
{
    if (animate) { t += 0.033f; } sigPhase = (int) (t * 1.2f) % 4;
    repaint();
}

// ---- planet / radar orb ------------------------------------------------------------------------------------------
void LcdDisplay::drawOrb (juce::Graphics& g, Rectangle<float> a)
{
    const auto& th = theme(); const auto c = a.getCentre(); const float R = a.getHeight() * 0.40f;
    // soft glow
    g.setGradientFill (juce::ColourGradient (th.accent.withAlpha (0.30f), c.x, c.y, th.accent.withAlpha (0.0f), c.x + R * 2.2f, c.y, true)); g.fillEllipse (c.x - R * 2.2f, c.y - R * 2.2f, R * 4.4f, R * 4.4f);
    const float tilt = -0.22f, rx = R * 2.2f, ry = R * 0.55f;
    auto ring = [&] (bool front)
    {
        juce::Path p; p.addCentredArc (0, 0, rx, ry, 0.0f, front ? 0.0f : juce::MathConstants<float>::pi, front ? juce::MathConstants<float>::pi : juce::MathConstants<float>::twoPi, true);
        auto tf = juce::AffineTransform::rotation (tilt).translated (c.x, c.y); p.applyTransform (tf);
        g.setColour (th.accent.withAlpha (0.18f)); g.strokePath (p, juce::PathStrokeType (6.0f)); g.setColour (th.hot.withAlpha (0.85f)); g.strokePath (p, juce::PathStrokeType (1.8f));
        g.setColour (th.accent2.withAlpha (0.8f)); g.strokePath (p, juce::PathStrokeType (0.8f));
    };
    ring (false);
    juce::Path disc; disc.addEllipse (c.x - R, c.y - R, R * 2, R * 2);
    g.saveState(); g.reduceClipRegion (disc);
    g.setGradientFill (juce::ColourGradient (th.hot.brighter (0.2f), c.x - R * 0.5f, c.y - R * 0.6f, th.accent2.darker (0.6f), c.x + R * 0.8f, c.y + R, false)); g.fillRect (a);
    const int bands = 9;
    for (int i = 0; i < bands; ++i)                                         // rotating banded atmosphere
    {
        const float y = c.y - R + (R * 2.0f) * ((float) i + 0.5f) / (float) bands, h = R * 2.0f / (float) bands * 0.55f;
        const float shift = std::sin (t * 0.8f + (float) i * 1.7f) * R * 0.35f;
        g.setColour ((i % 2 ? th.accent : th.hot.brighter (0.4f)).withAlpha (0.55f)); g.fillRoundedRectangle (c.x - R * 1.2f + shift, y - h * 0.5f, R * 2.4f, h, h * 0.5f);
    }
    const float spotX = c.x + std::sin (t * 0.6f) * R * 0.9f; g.setColour (th.accent.darker (0.3f).withAlpha (0.8f)); g.fillEllipse (spotX - R * 0.22f, c.y + R * 0.2f, R * 0.44f, R * 0.2f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, c.x - R * 0.3f, c.y - R * 0.3f, juce::Colours::black.withAlpha (0.65f), c.x + R * 0.9f, c.y + R * 0.9f, false)); g.fillRect (a);
    g.restoreState();
    ring (true);
    const float ma = t * 1.1f; Point<float> moon (std::cos (ma) * rx, std::sin (ma) * ry);                         // the amber orbiting moon / radar blip
    moon = moon.transformedBy (juce::AffineTransform::rotation (tilt).translated (c.x, c.y));
    if (std::sin (ma) > 0 || std::abs (moon.x - c.x) > R) drawLed (g, moon, 2.6f, true, th.led);
}

// ---- main paint ----------------------------------------------------------------------------------------------------
void LcdDisplay::paint (juce::Graphics& g)
{
    const auto& th = theme(); auto b = getLocalBounds().toFloat();
    // curved silver bezel
    g.setGradientFill (juce::ColourGradient (Colour (0xffe7e5ec), 0, b.getY(), Colour (0xff6a6674), 0, b.getBottom(), false)); g.fillRoundedRectangle (b, 22.0f);
    g.setGradientFill (juce::ColourGradient (Colour (0xff9a96a4), 0, b.getY(), Colour (0xff3c3844), 0, b.getBottom(), false)); g.fillRoundedRectangle (b.reduced (4.0f), 19.0f);
    g.setGradientFill (juce::ColourGradient (Colour (0xffdad8e0), 0, b.getY(), Colour (0xff7a7684), 0, b.getBottom(), false)); g.fillRoundedRectangle (b.reduced (6.0f), 17.0f);
    g.setColour (juce::Colours::white.withAlpha (0.35f)); g.drawRoundedRectangle (b.reduced (1.0f), 22.0f, 1.2f);
    // orb area has its own inset glass
    drawOrb (g, Rectangle<float> (b.getCentreX() - 80.0f, b.getY() + 6.0f, 160.0f, b.getHeight() * 0.25f));

    const auto s = screen().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (s.expanded (3.0f), 10.0f);
    g.setGradientFill (juce::ColourGradient (th.lcd.brighter (0.25f), 0, s.getY(), th.lcd, 0, s.getBottom(), false)); g.fillRoundedRectangle (s, 8.0f);
    g.saveState(); { juce::Path clip; clip.addRoundedRectangle (s, 8.0f); g.reduceClipRegion (clip); }
    const auto L = layout();
    // ---- header
    g.setColour (th.lcdText.withAlpha (0.65f)); g.setFont (monoFont (10.5f));
    const int prog = proc.getCurrentProgram(); const auto cn = proc.currentCategory(); const int ci = PresetBank::categories().indexOf (cn);
    g.drawText (juce::String::formatted ("PROG P-%03d  BANK %c-%c", prog + 1, 'U', (char) ('A' + juce::jmax (0, ci))), L.header.withWidth (L.header.getWidth() / 3), juce::Justification::centredLeft);
    g.setColour (th.lcdText); g.setFont (sansFont (21.0f)); g.drawText (proc.currentName().toUpperCase(), L.header, juce::Justification::centred);
    g.setColour (th.lcdText.withAlpha (0.65f)); g.setFont (monoFont (10.5f)); g.drawText (juce::String (juce::roundToInt (proc.getBpmForUi())) + " BPM", L.header, juce::Justification::centredRight);
    // ---- sub tabs
    static const char* subNames[] = { "PLAY", "LAYER", "EDIT", "ARP", "FX", "BROWSE" };
    tabRects.clear(); const int tw = L.tabs.getWidth() / 6;
    for (int i = 0; i < 6; ++i)
    {
        auto r = Rectangle<int> (L.tabs.getX() + i * tw, L.tabs.getY(), tw - 2, L.tabs.getHeight()); tabRects.push_back (r);
        g.setColour (i == subTab ? th.accent : th.accent.withAlpha (0.10f)); g.fillRect (r); g.setColour (th.accent.withAlpha (0.5f)); g.drawRect (r, 1);
        g.setColour (i == subTab ? juce::Colours::white : th.lcdText.withAlpha (0.6f)); drawSpaced (g, subNames[i], r.toFloat(), juce::Justification::centred, monoFont (10.0f), 0.2f);
    }
    // ---- categories (left)
    catRects.clear(); const int rowH = 19; const int visRows = L.cats.getHeight() / rowH;
    g.setFont (monoFont (11.0f));
    for (int i = 0; i < visRows && i + catScroll < cats.size(); ++i)
    {
        const int ci2 = i + catScroll; auto r = Rectangle<int> (L.cats.getX(), L.cats.getY() + i * rowH, L.cats.getWidth(), rowH - 1); catRects.push_back (r);
        if (ci2 == catSel) { g.setColour (th.accent.withAlpha (0.25f)); g.fillRect (r); g.setColour (th.accent); g.drawRect (r, 1); }
        g.setColour (ci2 == catSel ? juce::Colours::white : th.lcdText); g.drawText ((ci2 == catSel ? u8 ("\xe2\x96\xb8 ") : juce::String ("  ")) + cats[ci2].toUpperCase(), r.reduced (4, 0), juce::Justification::centredLeft, true);
    }
    g.setColour (th.accent.withAlpha (0.4f)); g.drawVerticalLine (L.cats.getRight() + 4, (float) L.cats.getY(), (float) L.cats.getBottom());
    // ---- search / engine chips
    g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillRect (L.search); g.setColour (th.accent.withAlpha (0.4f)); g.drawRect (L.search, 1);
    static const char* chipNames[] = { "ORG", "WT", "GR", "VA", "SP" }; chipRects.clear(); const int cw = L.chips.getWidth() / 5;
    for (int i = 0; i < 5; ++i)
    { auto r = Rectangle<int> (L.chips.getX() + i * cw, L.chips.getY(), cw - 2, L.chips.getHeight()); chipRects.push_back (r); const bool on = ((engineMask >> i) & 1) != 0;
      g.setColour (on ? th.accent : th.accent.withAlpha (0.1f)); g.fillRect (r); g.setColour (th.accent.withAlpha (0.5f)); g.drawRect (r, 1); g.setColour (on ? juce::Colours::white : th.lcdText.withAlpha (0.6f)); g.setFont (monoFont (9.0f)); g.drawText (chipNames[i], r, juce::Justification::centred); }
    // ---- preset list
    rowHits.clear(); const int listRows = L.list.getHeight() / rowH; g.setFont (monoFont (11.5f));
    for (int i = 0; i < listRows && i + listScroll < (int) visible.size(); ++i)
    {
        const int vi = i + listScroll; const int ai = visible[(size_t) vi]; const auto& p = *all[(size_t) ai];
        auto r = Rectangle<int> (L.list.getX(), L.list.getY() + i * rowH, L.list.getWidth(), rowH - 1);
        const bool cur = p.name == proc.currentName() && p.category == proc.currentCategory();
        if (cur) { g.setColour (th.accent.withAlpha (0.25f)); g.fillRect (r); g.setColour (th.accent); g.drawRect (r, 1); }
        g.setColour (cur ? juce::Colours::white : th.lcdText);
        const int fi = factoryIndex[(size_t) ai];
        g.drawText ((cur ? u8 ("\xe2\x96\xb8 ") : juce::String ("  ")) + (fi >= 0 ? juce::String::formatted ("%03d ", fi + 1) : juce::String ("USR ")) + p.name, r.reduced (4, 0).withTrimmedRight (18), juce::Justification::centredLeft, true);
        auto star = Rectangle<int> (r.getRight() - 18, r.getY(), 18, r.getHeight());
        if (proc.isFavorite (p.name)) { g.setColour (th.led); g.drawText (u8 ("\xe2\x98\x85"), star, juce::Justification::centred); }
        else if (r.contains (getMouseXYRelative())) { g.setColour (th.lcdText.withAlpha (0.35f)); g.drawText (u8 ("\xe2\x98\x86"), star, juce::Justification::centred); }
        rowHits.push_back ({ r, star, vi });
    }
    if (visible.empty()) { g.setColour (th.lcdText.withAlpha (0.5f)); g.drawText ("no presets match", L.list, juce::Justification::centred); }
    if ((int) visible.size() > listRows) { const float f = (float) listRows / (float) visible.size(); const float y0 = (float) L.list.getY() + (float) listScroll / (float) visible.size() * (float) L.list.getHeight();
        g.setColour (th.accent.withAlpha (0.5f)); g.fillRect ((float) L.list.getRight() - 3.0f, y0, 3.0f, (float) L.list.getHeight() * f); }
    // ---- footer
    g.setColour (th.accent.withAlpha (0.35f)); g.drawHorizontalLine (L.footer.getY() - 1, (float) L.footer.getX(), (float) L.footer.getRight());
    g.setColour (th.lcdText.withAlpha (0.7f)); g.setFont (monoFont (9.5f));
    g.drawText (juce::String (kLayers) + " LAYERS  " + juce::String ((int) PresetBank::factory().size()) + "+ PRESETS", L.footer, juce::Justification::centredLeft);
    juce::String sig = "SIG "; for (int i = 0; i < 4; ++i) sig += i <= sigPhase ? u8 ("\xe2\x96\xae") : u8 ("\xe2\x96\xaf"); sig += " 88.4 MHz";
    g.drawText (sig, L.footer, juce::Justification::centred);
    g.drawText (juce::String::formatted ("POLY %d/%d  CPU %d%%", proc.voiceCount.load(), proc.settings.polyphony.load(), juce::roundToInt (proc.cpuLoad.load() * 100.0f)), L.footer, juce::Justification::centredRight);
    g.restoreState();
    // scanlines + glass reflection
    g.saveState(); { juce::Path clip; clip.addRoundedRectangle (s, 8.0f); g.reduceClipRegion (clip); }
    g.setColour (juce::Colours::black.withAlpha (0.16f)); for (float y = s.getY(); y < s.getBottom(); y += 3.0f) g.fillRect (s.getX(), y, s.getWidth(), 1.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.07f), 0, s.getY(), juce::Colours::transparentWhite, 0, s.getY() + s.getHeight() * 0.4f, false)); g.fillRect (s);
    g.restoreState();
    // nav button strip below the bezel's lower edge is too tight; draw compact nav in the bezel footer
    const int ny = (int) s.getBottom() + 9; const int nx = (int) s.getCentreX() - 125;
    btnPrev = { nx, ny, 38, 18 }; btnNext = { nx + 42, ny, 38, 18 }; btnExplore = { nx + 84, ny, 60, 18 }; btnAB = { nx + 148, ny, 44, 18 }; btnSave = { nx + 196, ny, 54, 18 };
    auto bez = [&] (Rectangle<int> r, const juce::String& txt, bool on)
    { g.setGradientFill (juce::ColourGradient (on ? th.accent : Colour (0xff4b4756), 0, (float) r.getY(), on ? th.accent.darker (0.5f) : Colour (0xff2a2630), 0, (float) r.getBottom(), false)); g.fillRoundedRectangle (r.toFloat(), 4.0f);
      g.setColour (Colour (0xff17131c)); g.drawRoundedRectangle (r.toFloat(), 4.0f, 1.0f); g.setColour (juce::Colours::white.withAlpha (0.9f)); g.setFont (sansFont (10.0f)); g.drawText (txt, r, juce::Justification::centred); };
    bez (btnPrev, u8 ("\xe2\x97\x80"), false); bez (btnNext, u8 ("\xe2\x96\xb6"), false); bez (btnExplore, "EXPLORE", false);
    bez (btnAB, proc.abSlot() == 0 ? "A | b" : "a | B", proc.abSlot() == 1); bez (btnSave, "SAVE", false);
}

// ---- interaction --------------------------------------------------------------------------------------------------
void LcdDisplay::load (int vi)
{
    if (vi < 0 || vi >= (int) visible.size()) return;
    const int ai = visible[(size_t) vi]; proc.loadPreset (*all[(size_t) ai], factoryIndex[(size_t) ai]);
}
void LcdDisplay::step (int d)
{
    if (visible.empty()) return;
    int cur = -1; for (size_t i = 0; i < visible.size(); ++i) if (all[(size_t) visible[i]]->name == proc.currentName()) cur = (int) i;
    const int n = (int) visible.size(); const int next = ((cur < 0 ? (d > 0 ? -1 : 0) : cur) + d + n) % n; load (next);
    const int rows = layout().list.getHeight() / 19; if (next < listScroll || next >= listScroll + rows) listScroll = juce::jlimit (0, juce::jmax (0, n - rows), next - rows / 2);
}
void LcdDisplay::explore() { if (! visible.empty()) { const int i = juce::Random::getSystemRandom().nextInt ((int) visible.size()); load (i); const int rows = layout().list.getHeight() / 19; listScroll = juce::jlimit (0, juce::jmax (0, (int) visible.size() - rows), i - rows / 2); } }

void LcdDisplay::mouseDown (const juce::MouseEvent& e)
{
    if (e.eventComponent == &searchBox) { searchBox.setWantsKeyboardFocus (true); searchBox.grabKeyboardFocus(); return; }
    const auto p = e.getPosition();
    if (btnPrev.contains (p)) { step (-1); return; } if (btnNext.contains (p)) { step (1); return; } if (btnExplore.contains (p)) { explore(); return; }
    if (btnAB.contains (p)) { proc.toggleAB(); return; }
    if (btnSave.contains (p))
    {
        auto* aw = new juce::AlertWindow ("Save preset", "Name for the user preset (saved as .pvpreset):", juce::MessageBoxIconType::NoIcon, this);
        aw->addTextEditor ("name", proc.currentName(), ""); aw->addButton ("Save", 1); aw->addButton ("Cancel", 0);
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw] (int r) { if (r == 1) { proc.saveUserPreset (aw->getTextEditorContents ("name")); if (userLoaded) { userLoaded = false; all.resize (PresetBank::factory().size()); factoryIndex.resize (all.size()); userPresets.clear(); } rebuildFilter(); } }), true);
        return;
    }
    for (size_t i = 0; i < tabRects.size(); ++i) if (tabRects[i].contains (p))
    {
        subTab = (int) i; if (onSubTab) onSubTab ((int) i); repaint(); return;
    }
    for (size_t i = 0; i < catRects.size(); ++i) if (catRects[i].contains (p)) { catSel = (int) i + catScroll; rebuildFilter(); return; }
    for (size_t i = 0; i < chipRects.size(); ++i) if (chipRects[i].contains (p)) { engineMask ^= 1 << (int) i; rebuildFilter(); return; }
    for (auto& h : rowHits) if (h.row.contains (p))
    {
        if (h.star.contains (p)) { proc.toggleFavorite (all[(size_t) visible[(size_t) h.vis]]->name); if (catSel == 1) rebuildFilter(); repaint(); }
        else load (h.vis);
        return;
    }
}
void LcdDisplay::mouseDoubleClick (const juce::MouseEvent&) {}
void LcdDisplay::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const auto L = layout(); const int dir = w.deltaY > 0 ? -1 : 1;
    if (L.cats.contains (e.getPosition())) catScroll = juce::jlimit (0, juce::jmax (0, cats.size() - L.cats.getHeight() / 19), catScroll + dir);
    else listScroll = juce::jlimit (0, juce::jmax (0, (int) visible.size() - L.list.getHeight() / 19), listScroll + dir * 2);
    repaint();
}
} // namespace PV
