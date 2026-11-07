#include "PluginEditor.h"

namespace PV
{
using juce::Rectangle; using juce::Colour; using juce::Point;

static Rectangle<int> R (float x, float y, float w, float h) { return Rectangle<float> (x * 0.8f, y * 0.8f, w * 0.8f, h * 0.8f).toNearestInt(); }   // reference-screenshot (2000x1250) -> base (1600x1000)

// ================================================================================================== keyboard
void PVKeyboard::drawWhiteNote (int note, juce::Graphics& g, Rectangle<float> a, bool down, bool over, Colour line, Colour text)
{
    const auto& t = theme();
    g.setGradientFill (juce::ColourGradient (down ? t.hot.brighter (0.5f) : Colour (0xfffbf8fc), 0, a.getY(), down ? t.hot.withAlpha (0.85f) : Colour (0xffd3cfd8), 0, a.getBottom(), false)); g.fillRect (a);
    if (down) { g.setColour (t.accent.withAlpha (0.35f)); g.fillRect (a.removeFromBottom (a.getHeight() * 0.25f)); }
    else if (over) { g.setColour (t.hot.withAlpha (0.12f)); g.fillRect (a); }
    g.setColour (line); g.fillRect (a.withWidth (1.0f));
    if (note % 12 == 0) { g.setColour (text); g.setFont (monoFont (9.5f)); g.drawText ("C" + juce::String (note / 12 - 1 + 0), a.withTrimmedBottom (3.0f), juce::Justification::centredBottom); }
}
void PVKeyboard::drawBlackNote (int, juce::Graphics& g, Rectangle<float> a, bool down, bool, Colour)
{
    const auto& t = theme();
    g.setGradientFill (juce::ColourGradient (down ? t.accent : Colour (0xff2a2030), 0, a.getY(), Colour (0xff0b060d), 0, a.getBottom(), false)); g.fillRoundedRectangle (a.withTrimmedTop (-4.0f), 2.5f);
    g.setColour (juce::Colours::white.withAlpha (0.10f)); g.fillRect (a.reduced (a.getWidth() * 0.18f, 0.0f).withHeight (a.getHeight() * 0.8f).withTrimmedLeft (a.getWidth() * 0.45f));
    if (down) { g.setColour (t.accent.withAlpha (0.5f)); g.fillRect (a.removeFromBottom (a.getHeight() * 0.15f)); }
}

// ================================================================================================== layer list
LayerList::LayerList (PlanetVelProcessor& p, std::function<void (int)> pk) : proc (p), pick (std::move (pk)) {}
void LayerList::paint (juce::Graphics& g)
{
    const auto& t = theme(); const float rh = (float) getHeight() / (float) kLayers;
    const Colour tint[4] = { t.accent, t.accent2, t.hot, t.led };
    for (int l = 0; l < kLayers; ++l)
    {
        auto r = Rectangle<float> (0, (float) l * rh + 1.0f, (float) getWidth(), rh - 3.0f);
        const bool muted = proc.getRaw (lp (l, L_MUTE)) > 0.5f;
        g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (r, 5.0f);
        if (l == sel) { g.setColour (t.accent.withAlpha (0.18f)); g.fillRoundedRectangle (r.expanded (1.5f), 6.0f); g.setColour (t.accent); g.drawRoundedRectangle (r, 5.0f, 1.3f); }
        else { g.setColour (t.border.withAlpha (0.4f)); g.drawRoundedRectangle (r, 5.0f, 1.0f); }
        g.setColour (tint[l].withAlpha (muted ? 0.35f : 1.0f)); g.setFont (monoFont (11.0f, true)); g.drawText (juce::String::charToString ((juce::juce_wchar) ('A' + l)), (int) r.getX() + 6, (int) r.getY(), 14, (int) r.getHeight(), juce::Justification::centred);
        drawLed (g, { r.getX() + 29.0f, r.getCentreY() }, 3.2f, ! muted, t.led);
        const int e = juce::jlimit (0, ENG_COUNT - 1, (int) proc.getRaw (lp (l, L_ENGINE)));
        g.setColour (muted ? t.dim.withAlpha (0.5f) : t.text); g.setFont (monoFont (11.5f));
        g.drawText (engineNames()[e] + u8 (" \xc2\xb7 ") + typeNames (e)[juce::jlimit (0, 7, (int) proc.getRaw (lp (l, L_TYPE)))], (int) r.getX() + 42, (int) r.getY(), (int) r.getWidth() - 140, (int) r.getHeight(), juce::Justification::centredLeft, true);
        g.setColour (t.dim); g.setFont (monoFont (8.5f));
        g.drawText (juce::MidiMessage::getMidiNoteName ((int) proc.getRaw (lp (l, L_KEYLO)), true, true, 4) + u8 ("\xe2\x80\x93") + juce::String ((int) proc.getRaw (lp (l, L_KEYHI))), (int) r.getRight() - 96, (int) r.getY(), 52, (int) r.getHeight(), juce::Justification::centredRight);
        const float lev = proc.getRaw (lp (l, L_LEVEL)); auto bar = Rectangle<float> (r.getRight() - 40.0f, r.getCentreY() - 2.5f, 34.0f, 5.0f);
        g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (bar, 2.5f);
        g.setGradientFill (juce::ColourGradient (t.accent2, bar.getX(), 0, t.accent, bar.getRight(), 0, false)); g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * lev), 2.5f);
    }
}
void LayerList::mouseDown (const juce::MouseEvent& e)
{
    const int l = rowAt (e.y); dragRow = -1;
    if (e.x > getWidth() - 44) { dragRow = l; mouseDrag (e); return; }                          // level bar
    if (e.x > 18 && e.x < 42) { proc.setParamNormalised (lp (l, L_MUTE), proc.getRaw (lp (l, L_MUTE)) > 0.5f ? 0.0f : 1.0f, true); repaint(); }
    pick (l);
}
void LayerList::mouseDrag (const juce::MouseEvent& e)
{
    if (dragRow < 0) return; const float v = juce::jlimit (0.0f, 1.0f, (float) (e.x - (getWidth() - 40)) / 34.0f);
    proc.setParamNormalised (lp (dragRow, L_LEVEL), proc.param (lp (dragRow, L_LEVEL))->convertTo0to1 (v), false); repaint();
}

// ================================================================================================== content
class PlanetVelEditor::Content : public juce::Component
{
public:
    Content (PlanetVelEditor& o) : ed (o), proc (o.proc),
        lcd (o.proc), orbit (o.proc), spectrum (o.proc), env (o.proc),
        layerList (o.proc, [&o] (int l) { o.selectLayer (l); }),
        layers (o.proc), combi (o.proc), fx (o.proc), arp (o.proc),
        global (o.proc, [&o] (int s) { o.setScalePercent (s); }, [&o] { o.applyTheme(); })
    { build(); }

    void build();
    void paint (juce::Graphics&) override;

    PlanetVelEditor& ed; PlanetVelProcessor& proc;
    // header
    std::vector<std::unique_ptr<PVButton>> tabs; std::unique_ptr<KnobControl> master; PVButton scaleBtn { "100%" };
    // program page
    LcdDisplay lcd; OrbitPad orbit; SpectrumView spectrum; EnvView env;
    std::vector<std::unique_ptr<PanelBg>> panels; std::vector<std::unique_ptr<KnobControl>> knobs; std::vector<std::pair<KnobControl*, int>> layerKnobs;
    std::vector<std::unique_ptr<ChoicePills>> pills; std::vector<std::unique_ptr<ToggleSwitch>> toggles; std::vector<std::unique_ptr<PVButton>> layerChips;
    std::vector<juce::Component*> programGroup; std::array<KnobControl*, 8> macroKnobs {}; KnobControl* dlyTime = nullptr; std::unique_ptr<juce::ParameterAttachment> syncAtt;
    LayerList layerList;
    // other pages
    LayersPage layers; CombiPage combi; FxPage fx; ArpPage arp; GlobalPage global;
    // keyboard
    std::unique_ptr<PVKeyboard> keyboard; std::unique_ptr<WheelSlider> pb, mod;
    juce::String macroLabels[8];
};

void PlanetVelEditor::Content::build()
{
    // ---- header
    static const char* names[] = { "PROGRAM", "COMBI", "LAYERS", "FX", "ARP", "GLOBAL" };
    static const Rectangle<int> tabR[] = { R (986, 82, 114, 56), R (1109, 82, 84, 56), R (1201, 82, 100, 56), R (1309, 82, 52, 56), R (1369, 82, 66, 56), R (1443, 82, 100, 56) };
    for (int i = 0; i < 6; ++i)
    { auto b = std::make_unique<PVButton> (names[i]); b->setBounds (tabR[i]); b->onClick = [this, i] { ed.setPage (i); }; addAndMakeVisible (*b); tabs.push_back (std::move (b)); }
    master = std::make_unique<KnobControl> (proc, "MASTER", G_MASTER, 60.0f); master->setBounds (R (1802, 56, 100, 112)); addAndMakeVisible (*master);
    scaleBtn.setBounds (R (1690, 96, 96, 40)); scaleBtn.onClick = [this] { juce::PopupMenu m; for (int s : { 75, 100, 125, 150, 200 }) m.addItem (s, juce::String (s) + "%", true, proc.uiScalePercent == s);
                                                                           m.showMenuAsync ({}, [this] (int r) { if (r > 0) ed.setScalePercent (r); }); }; addAndMakeVisible (scaleBtn);

    // ---- helpers
    auto panel = [this] (const juce::String& t, Rectangle<int> r, bool led = true) { auto p = std::make_unique<PanelBg> (t, led); p->setBounds (r); addAndMakeVisible (*p); programGroup.push_back (p.get()); panels.push_back (std::move (p)); };
    auto knob = [this] (const juce::String& cap, int pi, float cx, float cy, float dia = 53.0f, bool bound = false, int off = 0)
    {
        auto k = std::make_unique<KnobControl> (proc, cap, bound ? lp (0, off) : pi, dia); k->setBounds (Rectangle<float> (cx * 0.8f - 42.0f, cy * 0.8f - dia * 0.5f, 84.0f, dia + 30.0f).toNearestInt());
        addAndMakeVisible (*k); programGroup.push_back (k.get()); if (bound) layerKnobs.push_back ({ k.get(), off }); auto* r = k.get(); knobs.push_back (std::move (k)); return r;
    };
    auto pill = [this] (int pi, Rectangle<int> r, bool vert, juce::StringArray labels = {}, std::vector<int> sub = {})
    { auto p = std::make_unique<ChoicePills> (proc, pi, labels, vert, sub); p->setBounds (r); addAndMakeVisible (*p); programGroup.push_back (p.get()); auto* raw = p.get(); pills.push_back (std::move (p)); return raw; };
    auto toggle = [this] (int pi, const juce::String& t, Rectangle<int> r, ToggleSwitch::Look look = ToggleSwitch::Switch)
    { auto s = std::make_unique<ToggleSwitch> (proc, pi, t, look); s->setBounds (r); addAndMakeVisible (*s); programGroup.push_back (s.get()); auto* raw = s.get(); toggles.push_back (std::move (s)); return raw; };

    // ---- LAYER MODIFIER
    panel ("LAYER MODIFIER", R (88, 178, 452, 512));
    for (int l = 0; l < 4; ++l)
    { auto c = std::make_unique<PVButton> (juce::String::charToString ((juce::juce_wchar) ('A' + l))); c->setBounds (R (387 + l * 34, 192, 30, 25)); c->onClick = [this, l] { ed.selectLayer (l); }; addAndMakeVisible (*c); programGroup.push_back (c.get()); layerChips.push_back (std::move (c)); }
    layerList.setBounds (R (104, 240, 417, 177)); addAndMakeVisible (layerList); programGroup.push_back (&layerList);
    static const struct { const char* c; int off; } lk[] = { { "TUNE", L_TUNE }, { "LEVEL", L_LEVEL }, { "PAN", L_PAN }, { "SEND", L_SEND }, { "START", L_START }, { "CUTOFF", L_CUTOFF }, { "RESO", L_RESO }, { "SPREAD", L_SPREAD } };
    for (int i = 0; i < 8; ++i) knob (lk[i].c, 0, 151.0f + (float) (i % 4) * 107.5f, i < 4 ? 468.0f : 593.0f, 53.0f, true, lk[i].off);
    // ---- LCD
    lcd.setBounds (R (562, 178, 876, 512)); addAndMakeVisible (lcd); programGroup.push_back (&lcd);
    lcd.onSubTab = [this] (int i) { static const int map[] = { 0, 2, 0, 4, 3, 0 }; ed.setPage (map[i]); };
    // ---- AMP MODIFIER
    panel ("AMP MODIFIER", R (1460, 178, 454, 246)); env.setBounds (R (1720, 188, 172, 50)); addAndMakeVisible (env); programGroup.push_back (&env);
    static const struct { const char* c; int off; } ak[] = { { "ATTACK", L_ATK }, { "DECAY", L_DEC }, { "SUSTAIN", L_SUS }, { "RELEASE", L_REL } };
    for (int i = 0; i < 4; ++i) knob (ak[i].c, 0, 1526.0f + (float) i * 107.5f, 290.0f, 53.0f, true, ak[i].off);
    // ---- ORBIT XY
    panel ("ORBIT XY", R (1460, 443, 454, 247)); orbit.setBounds (R (1482, 510, 300, 160)); addAndMakeVisible (orbit); programGroup.push_back (&orbit);
    pill (G_ORBIT_MODE, R (1822, 513, 66, 110), true, { "ORBIT", "FREE", "HOLD" });
    // ---- bottom row
    panel ("MASTER FILTER", R (88, 710, 350, 211)); toggle (G_MF_ON, "ON", R (101, 770, 100, 30)); pill (G_MF_TYPE, R (101, 813, 108, 88), true); knob ("FREQ", G_MF_FREQ, 275, 818); knob ("RES", G_MF_RES, 382, 818);
    panel ("DELAY", R (455, 710, 353, 211)); toggle (G_DLY_SYNC, "SYNC", R (468, 770, 110, 30));
    pill (G_DLY_DIV, R (470, 813, 132, 88), true, juce::StringArray (kRateNames, kNumRates), { 2, 12, 4 }); dlyTime = knob ("TIME", G_DLY_DIV, 656, 818); knob ("FDBK", G_DLY_FB, 751, 818);
    panel ("REVERB", R (823, 710, 353, 211)); toggle (G_REV_ON, "ON", R (836, 770, 100, 30)); pill (G_REV_TYPE, R (838, 813, 112, 88), true, { "HALL", "PLATE", "CATH", "SPACE" }, { 0, 2, 3 }); knob ("SIZE", G_REV_SIZE, 1013, 818); knob ("WET", G_REV_WET, 1120, 818);
    panel ("GLITCH", R (1192, 710, 353, 211)); toggle (G_GL_ON, "ON", R (1205, 770, 100, 30)); pill (G_GL_TYPE, R (1207, 813, 112, 88), true, { "STUT", "REV", "TAPE", "CRUSH", "PITCH", "GATE" }, { 0, 1, 3 }); knob ("RATE", G_GL_RATE, 1382, 818); knob ("MIX", G_GL_MIX, 1489, 818);
    panel ("OUTPUT", R (1561, 710, 353, 211)); toggle (G_OUT_LIMIT, "LIMIT", R (1575, 780, 120, 28)); pill (G_OUT_MONO, R (1577, 827, 128, 62), true, { "STEREO", "MONO" }); knob ("VOL", G_OUT_VOL, 1743, 818);
    spectrum.setBounds (R (1784, 779, 121, 114)); addAndMakeVisible (spectrum); programGroup.push_back (&spectrum);
    // delay TIME knob shows the note division when synced, milliseconds otherwise
    syncAtt = std::make_unique<juce::ParameterAttachment> (*proc.param (G_DLY_SYNC), [this] (float v) { dlyTime->attach (v > 0.5f ? G_DLY_DIV : G_DLY_TIME); }, nullptr); syncAtt->sendInitialUpdate();

    // ---- macro strip
    static const float mx[8] = { 144, 369, 617, 863, 1090, 1327, 1543, 1780 };
    for (int i = 0; i < 8; ++i)
    { auto k = std::make_unique<KnobControl> (proc, "", G_M1 + i, 42.0f); k->setBounds (R (mx[i] - 26, 948, 52, 52)); k->setCompact (true); addAndMakeVisible (*k); macroKnobs[(size_t) i] = k.get(); knobs.push_back (std::move (k)); }

    // ---- other pages (page area 70..1530 x 142..737)
    auto pageBounds = Rectangle<int> (70, 142, 1460, 595);
    layers.setBounds (pageBounds); combi.setBounds (pageBounds); fx.setBounds (pageBounds); arp.setBounds (pageBounds); global.setBounds (pageBounds);
    for (juce::Component* c : { (juce::Component*) &layers, (juce::Component*) &combi, (juce::Component*) &fx, (juce::Component*) &arp, (juce::Component*) &global }) addChildComponent (c);
    layers.onLayerPicked = [this] (int l) { ed.selectLayer (l); };

    // ---- keyboard + wheels
    keyboard = std::make_unique<PVKeyboard> (ed.proc.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard);
    keyboard->setAvailableRange (36, 96); keyboard->setOctaveForMiddleC (4); keyboard->setBlackNoteLengthProportion (0.62f); keyboard->setScrollButtonsVisible (false); keyboard->setWantsKeyboardFocus (false);
    keyboard->setBounds (R (212, 1030, 1698, 152)); keyboard->setKeyWidth (keyboard->getWidth() / 36.0f); keyboard->setMidiChannel (1);
    keyboard->setColour (juce::MidiKeyboardComponent::textLabelColourId, Colour (0xff6a5870)); keyboard->setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, Colour (0xff6a5870));
    addAndMakeVisible (*keyboard);
    pb = std::make_unique<WheelSlider> (true); pb->setRange (-1.0, 1.0, 0.0); pb->setValue (0.0); pb->setBounds (R (103, 1040, 36, 136)); pb->setPopupDisplayEnabled (false, false, nullptr);
    pb->onValueChange = [this] { proc.uiMidi (juce::MidiMessage::pitchWheel (1, juce::jlimit (0, 16383, (int) ((pb->getValue() + 1.0) * 0.5 * 16383.0)))); }; addAndMakeVisible (*pb);
    mod = std::make_unique<WheelSlider> (false); mod->setRange (0.0, 1.0, 0.0); mod->setValue (0.0); mod->setBounds (R (150, 1040, 36, 136));
    mod->onValueChange = [this] { proc.uiMidi (juce::MidiMessage::controllerEvent (1, 1, (int) (mod->getValue() * 127.0))); }; addAndMakeVisible (*mod);
}

void PlanetVelEditor::Content::paint (juce::Graphics& g)
{
    const auto& t = theme(); const auto b = getLocalBounds().toFloat();
    // outer graphite hardware frame with faint purple sheen + brushed lines
    g.setColour (Colour (0xff070308)); g.fillAll();
    auto frame = R (36, 26, 1926, 1196).toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (frame.expanded (6.0f), 40.0f);
    g.setGradientFill (juce::ColourGradient (Colour (0xff6c6874), frame.getX(), frame.getY(), Colour (0xff1a1620), frame.getX(), frame.getBottom(), false)); g.fillRoundedRectangle (frame, 36.0f);
    g.setGradientFill (juce::ColourGradient (t.accent2.withAlpha (0.18f), frame.getX(), frame.getY(), juce::Colours::transparentBlack, frame.getRight(), frame.getBottom(), false)); g.fillRoundedRectangle (frame, 36.0f);
    for (float y = frame.getY() + 4.0f; y < frame.getBottom() - 4.0f; y += 2.0f) { g.setColour (juce::Colours::white.withAlpha ((((int) y) * 37 % 11) < 5 ? 0.035f : 0.012f)); g.drawHorizontalLine ((int) y, frame.getX() + 16.0f, frame.getRight() - 16.0f); }
    g.setColour (Colour (0xff0c080e)); g.drawRoundedRectangle (frame.reduced (1.0f), 36.0f, 2.0f);
    // inner plum glass
    auto inner = R (62, 50, 1876, 1148).toFloat();
    g.setGradientFill (juce::ColourGradient (t.bg0, inner.getX(), inner.getY(), t.bg1, inner.getX(), inner.getBottom(), false)); g.fillRoundedRectangle (inner, 28.0f);
    g.setColour (juce::Colours::black.withAlpha (0.8f)); g.drawRoundedRectangle (inner, 28.0f, 3.0f);
    g.setColour (t.accent2.withAlpha (0.2f)); g.drawRoundedRectangle (inner.reduced (2.0f), 26.0f, 1.0f);
    for (auto c : { Point<float> (77, 64), Point<float> (1922, 64), Point<float> (77, 1184), Point<float> (1922, 1184) }) drawScrew (g, c * 0.8f, 9.5f * 0.8f);

    // header
    drawPanel (g, R (88, 62, 1826, 96).toFloat(), {}, false);
    juce::GlyphArrangement ga; ga.addLineOfText (sansFont (62.0f).withExtraKerningFactor (0.03f), "PLANET VEL", 92.0f, 106.0f);
    juce::Path logo; ga.createPath (logo);
    g.setColour (t.accent.withAlpha (0.22f)); g.strokePath (logo, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved)); g.setColour (t.accent.withAlpha (0.35f)); g.strokePath (logo, juce::PathStrokeType (3.0f));
    const auto lb = logo.getBounds();
    g.setGradientFill (juce::ColourGradient (Colour (0xfff6f2fa), 0, lb.getY(), t.accent, 0, lb.getBottom(), false)); g.fillPath (logo);
    g.setColour (t.text); drawSpaced (g, "BY  TRILL  TRON", R (653, 92, 330, 30).toFloat(), juce::Justification::centredLeft, sansFont (15.0f), 0.18f);
    g.setColour (t.dim); drawSpaced (g, u8 ("HYBRID CINEMATIC WORKSTATION  \xc2\xb7  v1.0"), R (653, 118, 480, 22).toFloat(), juce::Justification::centredLeft, monoFont (9.5f), 0.12f);
    // meters (header)
    g.setColour (t.dim); g.setFont (monoFont (9.0f));
    g.drawText (juce::String::formatted ("VOICES %02d  CPU %02d%%", proc.voiceCount.load(), juce::roundToInt (proc.cpuLoad.load() * 100.0f)), R (1560, 62, 220, 30), juce::Justification::centredRight);
    // macro strip panel
    drawPanel (g, R (88, 935, 1826, 73).toFloat(), {}, false);
    static const float mx[8] = { 144, 369, 617, 863, 1090, 1327, 1543, 1780 };
    for (int i = 0; i < 8; ++i)
    {
        const auto nm = proc.macroName (i).toUpperCase(); auto r = R (mx[i] + 38, 952, 160, 44).toFloat();
        g.setColour (t.text); g.setFont (monoFont (9.5f)); g.drawText ("M" + juce::String (i + 1), r.removeFromTop (r.getHeight() * 0.45f), juce::Justification::bottomLeft);
        drawSpaced (g, nm, r, juce::Justification::topLeft, sansFont (12.0f), 0.1f);
    }
    // keyboard housing + wheel labels
    auto kb = R (88, 1027, 1826, 168).toFloat();
    g.setGradientFill (juce::ColourGradient (Colour (0xff1a1320), 0, kb.getY(), Colour (0xff0b060d), 0, kb.getBottom(), false)); g.fillRoundedRectangle (kb, 8.0f); g.setColour (t.border.withAlpha (0.5f)); g.drawRoundedRectangle (kb, 8.0f, 1.0f);
    g.setColour (t.dim); g.setFont (monoFont (8.5f)); g.drawText ("PB", R (100, 1172, 44, 18), juce::Justification::centred); g.drawText ("MOD", R (146, 1172, 44, 18), juce::Justification::centred);
    // orbit readout
    if (ed.page == 0)
    {
        g.setColour (t.accent); g.setFont (monoFont (11.0f)); g.drawText (juce::String (proc.orbitXNow.load(), 2) + " / " + juce::String (proc.orbitYNow.load(), 2), R (1810, 630, 100, 30), juce::Justification::centred);
    }
}

// ================================================================================================== editor
PlanetVelEditor::PlanetVelEditor (PlanetVelProcessor& p) : juce::AudioProcessorEditor (p), proc (p)
{
    proc.loadGlobalSettings();
    setThemeIndex (proc.settings.theme.load()); laf.applyTheme(); setLookAndFeel (&laf);
    content = std::make_unique<Content> (*this); addAndMakeVisible (*content);

    constrainer.setFixedAspectRatio ((double) kBaseW / (double) kBaseH); constrainer.setSizeLimits (960, 600, 3840, 2400);
    setConstrainer (&constrainer); setResizable (true, true);
    int w = juce::jlimit (960, 3840, proc.editorWidth), h = juce::roundToInt ((double) w * kBaseH / kBaseW);
    setSize (w, h);

    proc.addChangeListener (this);
    selectLayer (0); setPage (0); content->global.sync();
    startTimerHz (30);
}
PlanetVelEditor::~PlanetVelEditor()
{
    stopTimer(); proc.removeChangeListener (this);
    proc.editorWidth = getWidth(); proc.editorHeight = getHeight(); proc.saveGlobalSettings();
    setLookAndFeel (nullptr);
}
void PlanetVelEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colours::black); }
void PlanetVelEditor::resized()
{
    // The UI is laid out once in a fixed 1600x1000 coordinate space and scaled with an AffineTransform,
    // so resizing never re-lays-out, never resets parameters and stays vector-sharp at any size.
    const float s = (float) getWidth() / (float) kBaseW;
    content->setTransform (juce::AffineTransform::scale (s)); content->setBounds (0, 0, kBaseW, kBaseH);
    if (! restoring) { proc.editorWidth = getWidth(); proc.editorHeight = getHeight(); proc.uiScalePercent = juce::roundToInt (s * 100.0f); }
}
void PlanetVelEditor::setScalePercent (int pct) { setSize (kBaseW * pct / 100, kBaseH * pct / 100); content->global.sync(); }

void PlanetVelEditor::applyTheme()
{
    laf.applyTheme(); repaint(); for (auto* c : content->getChildren()) c->repaint(); content->lcd.repaint();
}

void PlanetVelEditor::setPage (int pg)
{
    page = pg;
    for (auto* c : content->programGroup) c->setVisible (pg == 0);
    content->combi.setVisible (pg == 1); content->layers.setVisible (pg == 2); content->fx.setVisible (pg == 3); content->arp.setVisible (pg == 4); content->global.setVisible (pg == 5);
    for (int i = 0; i < 6; ++i) content->tabs[(size_t) i]->setActive (i == pg);
    if (pg == 1) content->combi.refresh();
    if (pg == 5) content->global.sync();
    repaint();
}

void PlanetVelEditor::selectLayer (int l)
{
    selectedLayer = juce::jlimit (0, kLayers - 1, l);
    for (auto& [k, off] : content->layerKnobs) k->attach (lp (selectedLayer, off));
    for (int i = 0; i < 4; ++i) content->layerChips[(size_t) i]->setActive (i == selectedLayer);
    content->env.setLayer (selectedLayer); content->layerList.setSelected (selectedLayer); content->layers.setLayer (selectedLayer);
}

void PlanetVelEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // preset / state change: refresh theme, matrix, size
    if (themeIndexRef() != proc.settings.theme.load()) { setThemeIndex (proc.settings.theme.load()); applyTheme(); }
    content->combi.refresh(); content->global.sync();
    const int w = juce::jlimit (960, 3840, proc.editorWidth);
    if (w != getWidth() && proc.editorWidth != 0) { restoring = true; setSize (w, juce::roundToInt ((double) w * kBaseH / kBaseW)); restoring = false; }
    content->repaint();
}

void PlanetVelEditor::timerCallback()
{
    ++frame; const bool anim = proc.settings.animation.load();
    content->lcd.tick (anim);
    if (anim || frame % 3 == 0) { content->spectrum.tick(); content->orbit.repaint(); }
    if (page == 0) { content->layerList.repaint(); content->env.repaint(); }
    content->scaleBtn.setText (juce::String (proc.uiScalePercent) + "%");
    if (frame % 6 == 0) content->repaint (R (88, 62, 1826, 96)), content->repaint (R (88, 935, 1826, 73));
    if (page == 0 && frame % 3 == 0) content->repaint (R (1810, 630, 100, 30));
}

bool PlanetVelEditor::isInterestedInFileDrag (const juce::StringArray& f) { for (auto& s : f) if (juce::File (s).hasFileExtension ("wav;aif;aiff;flac;ogg;mp3")) return true; return false; }
void PlanetVelEditor::filesDropped (const juce::StringArray& f, int, int)
{
    for (auto& s : f)
        if (proc.loadUserSample (juce::File (s)))
        {   // route the dropped audio into the selected layer's granular engine
            const int l = selectedLayer;
            proc.setParamNormalised (lp (l, L_ENGINE), toNormalised (lp (l, L_ENGINE), (float) ENG_GRANULAR), true); proc.setParamNormalised (lp (l, L_TYPE), toNormalised (lp (l, L_TYPE), 4.0f), true);
            proc.setParamNormalised (lp (l, L_MUTE), 0.0f, true); content->global.sync(); repaint(); break;
        }
}
} // namespace PV
