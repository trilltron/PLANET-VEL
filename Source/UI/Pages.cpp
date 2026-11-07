#include "Pages.h"

namespace PV
{
using juce::Rectangle; using juce::Colour;

static void place (juce::Component& c, juce::Component& parent, int x, int y, int w, int h) { parent.addAndMakeVisible (c); c.setBounds (x, y, w, h); }

// ============================================================================================================ LAYERS
LayersPage::LayersPage (PlanetVelProcessor& p) : proc (p)
{
    auto panel = [this] (const juce::String& t, int x, int y, int w, int h) { auto b = std::make_unique<PanelBg> (t); place (*b, *this, x, y, w, h); panels.push_back (std::move (b)); };
    panel ("SOURCE", 10, 62, 700, 250); panel (u8 ("UNISON \xc2\xb7 START"), 720, 62, 340, 250); panel ("OUTPUT", 1070, 62, 380, 250);
    panel ("FILTER", 10, 325, 700, 262); panel ("AMP ENVELOPE", 720, 325, 340, 262); panel (u8 ("MODULATION \xc2\xb7 LFO 1 / 2"), 1070, 325, 380, 262);

    layerTabsOwner = std::make_unique<SettingPills> (juce::StringArray { "LAYER A", "LAYER B", "LAYER C", "LAYER D" }, [this] (int l) { if (onLayerPicked) onLayerPicked (l); });
    place (*layerTabsOwner, *this, 10, 12, 420, 34); layerTabs = layerTabsOwner.get();
    mute = std::make_unique<ToggleSwitch> (proc, lp (0, L_MUTE), "MUTE", ToggleSwitch::Letter); solo = std::make_unique<ToggleSwitch> (proc, lp (0, L_SOLO), "SOLO", ToggleSwitch::Letter);
    place (*mute, *this, 445, 12, 64, 34); place (*solo, *this, 515, 12, 64, 34);
    enginePills = std::make_unique<ChoicePills> (proc, lp (0, L_ENGINE), engineNames(), false); place (*enginePills, *this, 600, 12, 560, 34);
    typePills = std::make_unique<ChoicePills> (proc, lp (0, L_TYPE), juce::StringArray(), false); place (*typePills, *this, 26, 96, 668, 28);
    ftypePills = std::make_unique<ChoicePills> (proc, lp (0, L_FTYPE), juce::StringArray(), false); place (*ftypePills, *this, 26, 360, 330, 26);

    auto knob = [this] (const juce::String& cap, int off, int x, int y, float dia = 54.0f, bool layerBound = true, int absParam = -1)
    {
        auto k = std::make_unique<KnobControl> (proc, cap, layerBound ? lp (0, off) : absParam, dia); place (*k, *this, x, y, 84, (int) dia + 30);
        if (layerBound) layerKnobs.push_back ({ k.get(), off }); auto* raw = k.get(); knobs.push_back (std::move (k)); return raw;
    };
    for (int i = 0; i < 6; ++i) pKnobs[(size_t) i] = knob ("P" + juce::String (i + 1), L_P1 + i, 30 + i * 110, 140);
    knob ("UNISON", L_UNI, 740, 110); knob ("DETUNE", L_DETUNE, 835, 110); knob ("SPREAD", L_SPREAD, 930, 110); knob ("START", L_START, 740, 200); 
    knob ("LEVEL", L_LEVEL, 1085, 100); knob ("PAN", L_PAN, 1175, 100); knob ("TUNE", L_TUNE, 1265, 100); knob ("SEND", L_SEND, 1355, 100);
    knob ("KEY LOW", L_KEYLO, 1085, 195); knob ("KEY HIGH", L_KEYHI, 1175, 195);
    knob ("CUTOFF", L_CUTOFF, 26, 410); knob ("RESO", L_RESO, 126, 410); knob ("DRIVE", L_DRIVE, 226, 410); knob ("KEYTRK", L_KEYTRK, 326, 410); knob ("ENV AMT", L_FENV, 426, 410); knob ("LFO AMT", L_LFO, 526, 410);
    knob ("ATTACK", L_ATK, 735, 500, 46); knob ("DECAY", L_DEC, 810, 500, 46); knob ("SUSTAIN", L_SUS, 885, 500, 46); knob ("RELEASE", L_REL, 960, 500, 46);
    env = std::make_unique<EnvView> (proc); place (*env, *this, 740, 362, 310, 120);
    lfo1 = new KnobControl (proc, "LFO1 RATE", G_LFO1_RATE, 46.0f); place (*lfo1, *this, 1085, 365, 84, 76); knobs.emplace_back (lfo1);
    lfo2 = new KnobControl (proc, "LFO2 RATE", G_LFO2_RATE, 46.0f); place (*lfo2, *this, 1085, 480, 84, 76); knobs.emplace_back (lfo2);
    lfo1Shape = std::make_unique<ChoicePills> (proc, G_LFO1_SHAPE, juce::StringArray(), false); place (*lfo1Shape, *this, 1180, 380, 255, 24);
    lfo2Shape = std::make_unique<ChoicePills> (proc, G_LFO2_SHAPE, juce::StringArray(), false); place (*lfo2Shape, *this, 1180, 495, 255, 24);
    setLayer (0); startTimerHz (10);
}

void LayersPage::setLayer (int l)
{
    layer = l; layerTabs->setValue (l);
    for (auto& [k, off] : layerKnobs) k->attach (lp (l, off));
    mute->attach (lp (l, L_MUTE)); solo->attach (lp (l, L_SOLO)); enginePills->attach (lp (l, L_ENGINE)); typePills->attach (lp (l, L_TYPE)); ftypePills->attach (lp (l, L_FTYPE));
    env->setLayer (l); shownEngine = -1; refreshEngineUi();
}
void LayersPage::timerCallback() { refreshEngineUi(); env->repaint(); }
void LayersPage::refreshEngineUi()
{
    const int e = juce::jlimit (0, ENG_COUNT - 1, (int) proc.getRaw (lp (layer, L_ENGINE)));
    if (e == shownEngine) return; shownEngine = e;
    typePills->setLabels (typeNames (e));
    for (int i = 0; i < 6; ++i)
    {
        const auto lab = knobLabel (e, i);
        pKnobs[(size_t) i]->setVisible (lab.isNotEmpty()); pKnobs[(size_t) i]->setCaption (lab);
    }
    // the START knob doubles as the grain position / sample start offset; uni/detune only matter for oscillator engines
}

// ============================================================================================================ MOD MATRIX
ModMatrixView::Row::Row (PlanetVelProcessor& p, int s) : proc (p), slot (s)
{
    addAndMakeVisible (num); num.setText (juce::String (s + 1), juce::dontSendNotification); num.setFont (monoFont (11.0f)); num.setColour (juce::Label::textColourId, theme().dim); num.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (src); addAndMakeVisible (dest); addAndMakeVisible (amount);
    amount.setSliderStyle (juce::Slider::LinearHorizontal); amount.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 18); amount.setRange (-1.0, 1.0, 0.001); amount.setDoubleClickReturnValue (true, 0.0);
    amount.getProperties().set ("bipolar", true); amount.setColour (juce::Slider::textBoxTextColourId, theme().lcdText); amount.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    amount.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; };
    amount.onValueChange = [this] { proc.routes[(size_t) slot].amount.store ((float) amount.getValue()); };
    src.onClick = [this]
    {
        juce::PopupMenu m; for (int s = 0; s < SRC_COUNT; ++s) m.addItem (s + 1, modSrcName (s), true, proc.routes[(size_t) slot].src.load() == s);
        m.showMenuAsync ({}, [this] (int r) { if (r > 0) { proc.routes[(size_t) slot].src.store (r - 1); refresh(); } });
    };
    dest.onClick = [this]
    {
        juce::PopupMenu m, g; g.addSectionHeader ("GLOBAL"); juce::PopupMenu sub[kLayers];
        m.addItem (1, "None");
        juce::PopupMenu glob;
        for (int i = 0; i < NUM_PARAMS; ++i)
        {
            const auto& d = defs()[(size_t) i]; if (! d.modulatable || d.kind != Kind::Float) continue;
            if (i < G_COUNT) glob.addItem (1000 + i, d.name, true, proc.routes[(size_t) slot].dest.load() == i);
            else sub[(i - G_COUNT) / L_COUNT].addItem (1000 + i, d.name.fromFirstOccurrenceOf (" ", false, false), true, proc.routes[(size_t) slot].dest.load() == i);
        }
        m.addSubMenu ("Global", glob); for (int l = 0; l < kLayers; ++l) m.addSubMenu ("Layer " + juce::String::charToString ((juce::juce_wchar) ('A' + l)), sub[l]);
        m.showMenuAsync ({}, [this] (int r) { if (r == 1) proc.routes[(size_t) slot].dest.store (-1); else if (r >= 1000) proc.routes[(size_t) slot].dest.store (r - 1000); refresh(); });
    };
    refresh();
}
void ModMatrixView::Row::refresh()
{
    const int s = proc.routes[(size_t) slot].src.load(), d = proc.routes[(size_t) slot].dest.load();
    src.setText (modSrcName (s)); dest.setText (d >= 0 ? defs()[(size_t) d].name : "-"); src.setActive (s != SRC_NONE); dest.setActive (d >= 0);
    amount.setValue (proc.routes[(size_t) slot].amount.load(), juce::dontSendNotification);
}
void ModMatrixView::Row::resized() { auto r = getLocalBounds().reduced (2, 1); num.setBounds (r.removeFromLeft (26)); src.setBounds (r.removeFromLeft (150).reduced (2)); dest.setBounds (r.removeFromLeft (270).reduced (2)); amount.setBounds (r.reduced (4, 0)); }

ModMatrixView::ModMatrixView (PlanetVelProcessor& p) : proc (p)
{
    addAndMakeVisible (vp); vp.setViewedComponent (&holder, false); vp.setScrollBarsShown (true, false);
    for (int i = 0; i < kRouteSlots; ++i) { rows.push_back (std::make_unique<Row> (proc, i)); holder.addAndMakeVisible (*rows.back()); }
}
void ModMatrixView::resized() { vp.setBounds (getLocalBounds()); holder.setSize (getWidth() - 14, kRouteSlots * 26); for (int i = 0; i < kRouteSlots; ++i) rows[(size_t) i]->setBounds (0, i * 26, holder.getWidth(), 26); }
void ModMatrixView::refresh() { for (auto& r : rows) r->refresh(); }

// ============================================================================================================ COMBI
CombiPage::CombiPage (PlanetVelProcessor& p) : proc (p), matrix (p)
{
    place (mixBg, *this, 10, 10, 640, 577); place (modBg, *this, 660, 10, 790, 577); place (labels, *this, 0, 0, 660, 590);
    static const int mixParams[] = { L_LEVEL, L_PAN, L_SEND, L_TUNE, L_KEYLO, L_KEYHI }; static const char* caps[] = { "LEVEL", "PAN", "SEND", "TUNE", "KEY LO", "KEY HI" };
    for (int l = 0; l < kLayers; ++l)
    {
        const int y = 52 + l * 130;
        auto m = std::make_unique<ToggleSwitch> (proc, lp (l, L_MUTE), "MUTE", ToggleSwitch::Letter); place (*m, *this, 78, y + 18, 60, 28); toggles.push_back (std::move (m));
        auto s = std::make_unique<ToggleSwitch> (proc, lp (l, L_SOLO), "SOLO", ToggleSwitch::Letter); place (*s, *this, 78, y + 52, 60, 28); toggles.push_back (std::move (s));
        for (int k = 0; k < 6; ++k) { auto kc = std::make_unique<KnobControl> (proc, caps[k], lp (l, mixParams[k]), 44.0f); place (*kc, *this, 156 + k * 80, y + 8, 76, 86); knobs.push_back (std::move (kc)); }
    }
    place (matrix, *this, 672, 50, 766, 527);
}
void CombiPage::resized() {}
void CombiPage::Labels::paint (juce::Graphics& g)
{
    const auto& t = theme();
    for (int l = 0; l < kLayers; ++l)
    {
        const int y = 52 + l * 130; const int eng = juce::jlimit (0, ENG_COUNT - 1, (int) proc.getRaw (lp (l, L_ENGINE)));
        g.setColour (juce::Colours::black.withAlpha (0.25f)); g.fillRoundedRectangle (20.0f, (float) y, 620.0f, 118.0f, 7.0f);
        g.setColour (t.accent); g.setFont (sansFont (34.0f)); g.drawText (juce::String::charToString ((juce::juce_wchar) ('A' + l)), 28, y + 8, 40, 44, juce::Justification::centred);
        g.setColour (t.dim); g.setFont (monoFont (10.0f)); g.drawText (engineNames()[eng].toUpperCase(), 24, y + 84, 120, 16, juce::Justification::centred);
        g.drawText (typeNames (eng)[juce::jlimit (0, 7, (int) proc.getRaw (lp (l, L_TYPE)))], 24, y + 98, 120, 14, juce::Justification::centred);
    }
}

// ============================================================================================================ FX
FxPage::FxPage (PlanetVelProcessor& p) : proc (p)
{
    auto panel = [this] (const juce::String& t, int x, int y, int w, int h) { auto b = std::make_unique<PanelBg> (t); place (*b, *this, x, y, w, h); panels.push_back (std::move (b)); };
    auto knob = [this] (const juce::String& c, int pi, int x, int y, float d = 52.0f) { auto k = std::make_unique<KnobControl> (proc, c, pi, d); place (*k, *this, x, y, 84, (int) d + 30); auto* r = k.get(); knobs.push_back (std::move (k)); return r; };
    auto pill = [this] (int pi, int x, int y, int w, int h, bool vert, std::vector<int> sub = {}) { auto c = std::make_unique<ChoicePills> (proc, pi, juce::StringArray(), vert, sub); place (*c, *this, x, y, w, h); pills.push_back (std::move (c)); };
    panel ("CHORUS / ENSEMBLE", 10, 10, 330, 180); knob ("MIX", G_CHO_MIX, 25, 62); knob ("RATE", G_CHO_RATE, 125, 62); knob ("DEPTH", G_CHO_DEPTH, 225, 62);
    panel (u8 ("TAPE \xc2\xb7 WIDTH"), 350, 10, 250, 180); knob ("DRIVE", G_DRIVE, 365, 62); knob ("WIDTH", G_WIDTH, 465, 62);
    panel ("MASTER EQ", 610, 10, 330, 180); knob ("LOW", G_EQ_LO, 625, 62); knob ("MID", G_EQ_MID, 725, 62); knob ("HIGH", G_EQ_HI, 825, 62);
    panel (u8 ("ORBIT  \xc2\xb7  LAYER BLEND"), 950, 10, 500, 180); pill (G_ORBIT_MODE, 965, 56, 130, 28, false); knob ("RATE", G_ORBIT_RATE, 1105, 56); knob ("RADIUS", G_ORBIT_RADIUS, 1185, 56); knob ("X", G_ORBIT_X, 1265, 56); knob ("Y", G_ORBIT_Y, 1345, 56);
    panel ("REVERB", 10, 200, 440, 190); pill (G_REV_TYPE, 25, 245, 400, 28, false); knob ("SIZE", G_REV_SIZE, 40, 290); knob ("WET", G_REV_WET, 150, 290);
    { auto t = std::make_unique<ToggleSwitch> (proc, G_REV_ON, "ON"); place (*t, *this, 300, 300, 100, 24); toggles.push_back (std::move (t)); }
    panel ("DELAY", 460, 200, 440, 190);
    { auto t = std::make_unique<ToggleSwitch> (proc, G_DLY_ON, "ON"); place (*t, *this, 475, 245, 100, 24); toggles.push_back (std::move (t)); auto s = std::make_unique<ToggleSwitch> (proc, G_DLY_SYNC, "SYNC"); place (*s, *this, 600, 245, 100, 24); toggles.push_back (std::move (s)); }
    dlyTime = knob ("TIME", G_DLY_DIV, 480, 290); knob ("FEEDBACK", G_DLY_FB, 590, 290); knob ("MIX", G_DLY_MIX, 700, 290);
    panel ("GLITCH", 910, 200, 540, 190);
    { auto t = std::make_unique<ToggleSwitch> (proc, G_GL_ON, "ON"); place (*t, *this, 925, 245, 90, 24); toggles.push_back (std::move (t)); }
    pill (G_GL_TYPE, 1030, 244, 405, 26, false); knob ("RATE", G_GL_RATE, 930, 290); knob ("PROB", G_GL_PROB, 1040, 290); knob ("MIX", G_GL_MIX, 1150, 290);
    panel ("MASTER FILTER", 10, 400, 440, 187); pill (G_MF_TYPE, 25, 445, 200, 28, false); { auto t = std::make_unique<ToggleSwitch> (proc, G_MF_ON, "ON"); place (*t, *this, 250, 446, 100, 24); toggles.push_back (std::move (t)); } knob ("FREQ", G_MF_FREQ, 40, 490, 50); knob ("RES", G_MF_RES, 150, 490, 50);
    panel ("OUTPUT", 460, 400, 440, 187); { auto t = std::make_unique<ToggleSwitch> (proc, G_OUT_LIMIT, "LIMITER"); place (*t, *this, 475, 445, 130, 24); toggles.push_back (std::move (t)); } pill (G_OUT_MONO, 640, 444, 200, 28, false);
    knob ("VOLUME", G_OUT_VOL, 480, 490, 50); knob ("MASTER", G_MASTER, 590, 490, 50);
    panel ("LAYER OUTPUTS", 910, 400, 540, 187);
    auto note = new juce::Label ({}, "Layers A-D can be routed to their own stereo outputs.\nIn FL Studio: click the plugin's output selector in the\nwrapper settings and enable \"Layer A\" ... \"Layer D\", then\nroute each to a mixer track. Enabled layers bypass the\nmain FX rack and the reverb send.");
    note->setFont (monoFont (11.5f)); note->setColour (juce::Label::textColourId, theme().dim); place (*note, *this, 925, 440, 510, 130);
}
void FxPage::resized() {}

// ============================================================================================================ ARP
void ArpStepEditor::paint (juce::Graphics& g)
{
    const auto& t = theme(); auto b = getLocalBounds().toFloat();
    g.setColour (Colour (0xff0b0510)); g.fillRoundedRectangle (b, 6.0f); g.setColour (t.border.withAlpha (0.7f)); g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 1.0f);
    const int len = proc.arpLength.load(); const float cw = b.getWidth() / (float) kArpSteps, topH = 34.0f;
    static const char* ln[] = { "VELOCITY", "PITCH", "RATCHET", "PROBABILITY" };
    g.setColour (t.dim); g.setFont (monoFont (10.0f)); g.drawText (ln[lane], 8, (int) topH + 4, 200, 14, juce::Justification::left);
    for (int i = 0; i < kArpSteps; ++i)
    {
        const auto s = ArpStep::unpack (proc.arpSteps[(size_t) i].load()); const float x = (float) i * cw; const bool inside = i < len;
        const auto cell = Rectangle<float> (x + 2.0f, 6.0f, cw - 4.0f, topH - 12.0f);
        g.setColour (s.on && inside ? t.accent : t.panel.darker (0.2f)); g.fillRoundedRectangle (cell, 3.0f); if (s.on && inside) { g.setColour (t.accent.withAlpha (0.25f)); g.fillRoundedRectangle (cell.expanded (2.0f), 4.0f); }
        g.setColour (t.dim.withAlpha (i % 4 == 0 ? 0.9f : 0.5f)); g.setFont (monoFont (8.5f)); g.drawText (juce::String (i + 1), (int) x, 0, (int) cw, 7, juce::Justification::centred);
        const float top = topH + 22.0f, h = b.getHeight() - top - 6.0f; float f = 0;
        switch (lane) { case 0: f = (float) s.vel / 127.0f; break; case 1: f = (float) (s.pitch + 24) / 48.0f; break; case 2: f = (float) (s.ratchet - 1) / 7.0f + 0.04f; break; default: f = (float) s.prob / 100.0f; }
        const auto bar = Rectangle<float> (x + 3.0f, top + h * (1.0f - f), cw - 6.0f, h * f);
        g.setColour ((s.on && inside ? t.accent2 : t.dim.withAlpha (0.35f)).withAlpha (inside ? 0.95f : 0.3f)); g.fillRoundedRectangle (bar, 2.0f);
        if (lane == 1) { g.setColour (t.hot.withAlpha (0.4f)); g.drawHorizontalLine ((int) (top + h * 0.5f), x, x + cw); }
        if (lane == 2 || lane == 1) { g.setColour (juce::Colours::white.withAlpha (inside ? 0.8f : 0.3f)); g.setFont (monoFont (9.0f)); g.drawText (lane == 2 ? juce::String (s.ratchet) : juce::String (s.pitch), (int) x, (int) top - 12, (int) cw, 12, juce::Justification::centred); }
    }
    g.setColour (t.accent.withAlpha (0.6f)); g.drawVerticalLine ((int) (len * cw), topH, b.getBottom());
}
void ArpStepEditor::edit (const juce::MouseEvent& e, bool first)
{
    const float cw = (float) getWidth() / (float) kArpSteps; const int i = juce::jlimit (0, kArpSteps - 1, (int) ((float) e.x / cw));
    auto s = ArpStep::unpack (proc.arpSteps[(size_t) i].load());
    const float topH = 34.0f, top = topH + 22.0f, h = (float) getHeight() - top - 6.0f;
    if (e.y < (int) topH) { if (first || i != lastStep) { if (first) s.on = ! s.on; else return; } }
    else
    {
        const float f = juce::jlimit (0.0f, 1.0f, 1.0f - ((float) e.y - top) / h);
        switch (lane) { case 0: s.vel = juce::jlimit (1, 127, juce::roundToInt (f * 127.0f)); break; case 1: s.pitch = juce::roundToInt (f * 48.0f) - 24; break;
                        case 2: s.ratchet = 1 + juce::roundToInt (f * 7.0f); break; default: s.prob = juce::roundToInt (f * 100.0f); }
    }
    lastStep = i; proc.arpSteps[(size_t) i].store (s.pack()); repaint();
}
void ArpStepEditor::mouseDown (const juce::MouseEvent& e) { lastStep = -1; edit (e, true); }
void ArpStepEditor::mouseDrag (const juce::MouseEvent& e) { edit (e, false); }

ArpPage::ArpPage (PlanetVelProcessor& p) : proc (p), editor (p)
{
    place (ctrlBg, *this, 10, 10, 330, 577); place (seqBg, *this, 350, 10, 1100, 577);
    on = std::make_unique<ToggleSwitch> (proc, G_ARP_ON, "ARP ON"); place (*on, *this, 28, 52, 150, 26);
    pattern = std::make_unique<ChoicePills> (proc, G_ARP_PAT, juce::StringArray(), true); place (*pattern, *this, 28, 96, 130, 230);
    octaves = std::make_unique<ChoicePills> (proc, -1, juce::StringArray { "1 OCT", "2 OCT", "3 OCT", "4 OCT" }, true);
    rate = std::make_unique<KnobControl> (proc, "RATE", G_ARP_RATE, 56.0f); place (*rate, *this, 190, 96, 100, 90);
    gate = std::make_unique<KnobControl> (proc, "GATE", G_ARP_GATE, 56.0f); place (*gate, *this, 190, 200, 100, 90);
    auto* oct = new KnobControl (proc, "OCTAVES", G_ARP_OCT, 56.0f); place (*oct, *this, 190, 304, 100, 90); knobsKeep.emplace_back (oct);
    { auto* l = new juce::Label ({}, "PATTERN LENGTH (steps)"); l->setFont (sansFont (11.0f)); l->setColour (juce::Label::textColourId, theme().text); place (*l, *this, 28, 356, 262, 18); keep.emplace_back (l); }
    length = std::make_unique<SettingPills> (juce::StringArray { "8", "12", "16", "24", "32" }, [this] (int i) { static const int v[] = { 8, 12, 16, 24, 32 }; proc.arpLength.store (v[i]); editor.repaint(); });
    place (*length, *this, 28, 380, 262, 26);
    place (dragMidi, *this, 28, 470, 262, 34); place (randomize, *this, 28, 516, 262, 34);
    randomize.onClick = [this]
    {
        auto& r = juce::Random::getSystemRandom();
        for (int i = 0; i < kArpSteps; ++i) { ArpStep s; s.on = i % 4 == 0 || r.nextFloat() < 0.65f; s.vel = (int) (55 + r.nextFloat() * 70.0f); s.pitch = r.nextFloat() < 0.2f ? (r.nextBool() ? 12 : -12) : 0; s.ratchet = r.nextFloat() < 0.15f ? 2 + r.nextInt (3) : 1; s.prob = r.nextFloat() < 0.2f ? 60 + r.nextInt (30) : 100; proc.arpSteps[(size_t) i].store (s.pack()); }
        editor.repaint();
    };
    lane = std::make_unique<SettingPills> (juce::StringArray { "VELOCITY", "PITCH", "RATCHET", "PROBABILITY" }, [this] (int l) { editor.setLane (l); });
    place (*lane, *this, 366, 50, 480, 26); place (editor, *this, 366, 86, 1068, 480);
    dragMidi.addMouseListener (this, false);
}
void ArpPage::resized() {}

juce::File ArpPage::writeMidiFile() const
{
    ArpParams ap; ap.on = true; ap.pattern = (int) proc.getRaw (G_ARP_PAT); ap.rate = (int) proc.getRaw (G_ARP_RATE); ap.octaves = (int) std::round (proc.getRaw (G_ARP_OCT)); ap.gate = proc.getRaw (G_ARP_GATE); ap.length = proc.arpLength.load();
    auto seq = Arp::render ({ 60, 63, 67, 70 }, ap, proc.arpSteps, 2, 960);      // 2 bars, C minor 7 chord - transpose in the piano roll
    juce::MidiFile mf; mf.setTicksPerQuarterNote (960); mf.addTrack (seq);
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("PlanetVel"); dir.createDirectory();
    auto f = dir.getChildFile (juce::File::createLegalFileName (proc.currentName() + " - Arp") + ".mid"); f.deleteFile();
    if (auto os = f.createOutputStream())
        mf.writeTo (*os);
    return f;
}
void ArpPage::mouseDrag (const juce::MouseEvent& e)
{
    if (e.eventComponent != &dragMidi || dragging || e.getDistanceFromDragStart() < 8) return;
    dragging = true; auto f = writeMidiFile();
    juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this, [this] { dragging = false; });
}

// ============================================================================================================ GLOBAL
GlobalPage::GlobalPage (PlanetVelProcessor& p, std::function<void (int)> sc, std::function<void()> tc) : proc (p), setScale (std::move (sc)), themeChanged (std::move (tc))
{
    place (engineBg, *this, 10, 10, 710, 280); place (viewBg, *this, 730, 10, 720, 280); place (libBg, *this, 10, 300, 710, 287); place (aboutBg, *this, 730, 300, 720, 287); place (info, *this, 0, 0, 1460, 595);
    auto lab = [this] (const juce::String& t, int x, int y) { auto* l = new juce::Label ({}, t); l->setFont (sansFont (11.0f)); l->setColour (juce::Label::textColourId, theme().text); place (*l, *this, x, y, 200, 18); labels.emplace_back (l); };
    lab ("QUALITY  (ECO = older laptops)", 28, 52); quality = std::make_unique<SettingPills> (juce::StringArray { "ECO", "NORMAL", "HQ" }, [this] (int i) { proc.settings.quality = i; }); place (*quality, *this, 28, 74, 330, 28);
    lab ("POLYPHONY  (voices, 4 layers each)", 28, 120); poly = std::make_unique<SettingPills> (juce::StringArray { "8", "16", "24", "32", "48", "64" }, [this] (int i) { static const int v[] = { 8, 16, 24, 32, 48, 64 }; proc.settings.polyphony = v[i]; }); place (*poly, *this, 28, 142, 420, 28);
    lab ("PITCH BEND RANGE  (semitones)", 28, 188); pbRange = std::make_unique<SettingPills> (juce::StringArray { "2", "5", "7", "12", "24" }, [this] (int i) { static const int v[] = { 2, 5, 7, 12, 24 }; proc.settings.pitchBendRange = v[i]; }); place (*pbRange, *this, 28, 210, 330, 28);
    lab ("MPE  (reserved - not active in v1.0)", 400, 188); mpe = std::make_unique<SettingPills> (juce::StringArray { "OFF", "ON" }, [this] (int i) { proc.settings.mpe = i == 1; }); place (*mpe, *this, 400, 210, 160, 28);
    lab ("THEME", 748, 52); themePills = std::make_unique<SettingPills> (juce::StringArray { "CRIMSON", "VIOLET NIGHT", "ECLIPSE" }, [this] (int i) { proc.settings.theme = i; setThemeIndex (i); if (themeChanged) themeChanged(); }); place (*themePills, *this, 748, 74, 460, 28);
    lab ("UI SCALE", 748, 120); scalePills = std::make_unique<SettingPills> (juce::StringArray { "75%", "100%", "125%", "150%", "200%" }, [this] (int i) { static const int v[] = { 75, 100, 125, 150, 200 }; if (setScale) setScale (v[i]); }); place (*scalePills, *this, 748, 142, 460, 28);
    lab ("ANIMATIONS  (orb, meters)", 748, 188); animation = std::make_unique<SettingPills> (juce::StringArray { "ON", "OFF" }, [this] (int i) { proc.settings.animation = i == 0; }); place (*animation, *this, 748, 210, 200, 28);
    lab ("RENDERER: software (JUCE) / Direct2D on Windows 10+ - chosen automatically", 748, 252);
    place (lib, *this, 28, 344, 330, 32); place (sample, *this, 28, 388, 330, 32); place (tuning, *this, 28, 432, 330, 32); place (resetTuning, *this, 372, 432, 200, 32);
    
    lib.onClick = [this] { chooser = std::make_unique<juce::FileChooser> ("Select sample library folder", juce::File (proc.settings.libraryPath)); chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this] (const juce::FileChooser& c) { if (c.getResult() != juce::File()) { proc.settings.libraryPath = c.getResult().getFullPathName(); proc.saveGlobalSettings(); repaint(); } }); };
    sample.onClick = [this] { chooser = std::make_unique<juce::FileChooser> ("Load audio for the Granular / Organic 'User Sample' source", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3"); chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& c) { if (c.getResult().existsAsFile()) { proc.loadUserSample (c.getResult()); repaint(); } }); };
    tuning.onClick = [this] { chooser = std::make_unique<juce::FileChooser> ("Load Scala tuning", juce::File(), "*.scl"); chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& c) { if (c.getResult().existsAsFile()) { proc.loadTuning (c.getResult()); repaint(); } }); };
    resetTuning.onClick = [this] { proc.resetTuning(); repaint(); };
    sync();
}
void GlobalPage::sync()
{
    info.repaint();
    quality->setValue (proc.settings.quality.load()); const int pv = proc.settings.polyphony.load(); poly->setValue (pv <= 8 ? 0 : pv <= 16 ? 1 : pv <= 24 ? 2 : pv <= 32 ? 3 : pv <= 48 ? 4 : 5);
    const int pb = proc.settings.pitchBendRange.load(); pbRange->setValue (pb <= 2 ? 0 : pb <= 5 ? 1 : pb <= 7 ? 2 : pb <= 12 ? 3 : 4); themePills->setValue (proc.settings.theme.load());
    const int sc = proc.uiScalePercent; scalePills->setValue (sc <= 75 ? 0 : sc <= 100 ? 1 : sc <= 125 ? 2 : sc <= 150 ? 3 : 4); animation->setValue (proc.settings.animation.load() ? 0 : 1); mpe->setValue (proc.settings.mpe.load() ? 1 : 0);
    repaint();
}
void GlobalPage::resized() {}
void GlobalPage::Info::paint (juce::Graphics& g)
{
    const auto& t = theme(); g.setColour (t.dim); g.setFont (monoFont (10.5f));
    g.drawText ("Library:", 28, 482, 70, 18, juce::Justification::left); g.drawText ("Sample:", 28, 506, 70, 18, juce::Justification::left); g.drawText ("Tuning:", 28, 530, 70, 18, juce::Justification::left);
    g.setColour (t.lcdText);
    auto shorten = [] (const juce::String& s) { return s.isEmpty() ? juce::String ("-") : (s.length() > 62 ? "..." + s.substring (s.length() - 59) : s); };
    g.drawText (shorten (proc.settings.libraryPath), 100, 482, 600, 18, juce::Justification::left); g.drawText (shorten (juce::File (proc.settings.userSamplePath).getFileName()), 100, 506, 600, 18, juce::Justification::left);
    g.drawText (proc.settings.tuningFile.isEmpty() ? juce::String ("12-TET (440 Hz)") : shorten (juce::File (proc.settings.tuningFile).getFileName()), 100, 530, 600, 18, juce::Justification::left);
    g.setColour (t.accent); g.setFont (sansFont (30.0f)); g.drawText ("PLANET VEL", 760, 340, 680, 40, juce::Justification::left);
    g.setColour (t.text); g.setFont (monoFont (12.0f)); g.drawText ("by TRILL TRON  -  Hybrid Cinematic Workstation  -  v1.0.0", 760, 384, 680, 18, juce::Justification::left);
    g.setColour (t.dim); g.setFont (monoFont (11.0f));
    g.drawFittedText (juce::String ("\"PLANET VEL - Signals From a Red World.\"\n\n") + "5 engines x 4 layers  -  620 factory presets  -  8 macros\nVST3 / AU / Standalone  -  sample rate " + juce::String (proc.getSampleRate(), 0) + " Hz\nSister product of ZIL - Xeno-Synth Transmitter", { 760, 414, 680, 150 }, juce::Justification::topLeft, 8);
}
} // namespace PV
