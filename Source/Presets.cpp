#include "Presets.h"
#include <set>

namespace PV
{
const char* modSrcName (int s)
{
    static const char* n[] = { "None", "M1 Tide", "M2 Morph", "M3 Space", "M4 Grit", "M5 Warp", "M6 Air", "M7 Drift", "M8 Glitch", "Mod Wheel", "LFO 1", "LFO 2", "Aftertouch" };
    return n[juce::jlimit (0, SRC_COUNT - 1, s)];
}

static const juce::NormalisableRange<float>& rangeOf (int i)
{
    static std::vector<juce::NormalisableRange<float>> r = []
    {
        std::vector<juce::NormalisableRange<float>> v;
        for (auto& d : defs()) { juce::NormalisableRange<float> x (d.mn, d.mx); if (d.kind == Kind::Float && d.centre > d.mn && d.centre < d.mx) x.setSkewForCentre (d.centre); v.push_back (x); }
        return v;
    }();
    return r[(size_t) i];
}

float toNormalised (int i, float real)
{
    const auto& d = defs()[(size_t) i];
    if (d.kind == Kind::Choice) return d.mx > 0 ? juce::jlimit (0.0f, 1.0f, real / d.mx) : 0.0f;
    if (d.kind == Kind::Bool) return real > 0.5f ? 1.0f : 0.0f;
    return rangeOf (i).convertTo0to1 (juce::jlimit (d.mn, d.mx, real));
}
float toReal (int i, float n)
{
    const auto& d = defs()[(size_t) i];
    if (d.kind == Kind::Choice) return std::round (n * d.mx);
    if (d.kind == Kind::Bool) return n > 0.5f ? 1.0f : 0.0f;
    return rangeOf (i).convertFrom0to1 (juce::jlimit (0.0f, 1.0f, n));
}

// ================================================================================================ Preset
Preset::Preset()
{
    norm.resize ((size_t) NUM_PARAMS);
    for (int i = 0; i < NUM_PARAMS; ++i) norm[(size_t) i] = toNormalised (i, defs()[(size_t) i].def);
    for (int i = 0; i < kNumMacros; ++i) macroNames[(size_t) i] = kMacroNames[i];
    for (auto& s : arp) s = ArpStep().pack();
}
Preset Preset::makeInit() { Preset p; p.name = "Init"; p.category = "User"; p.updateEngineMask(); return p; }

void Preset::updateEngineMask()
{
    engineMask = 0;
    for (int l = 0; l < kLayers; ++l)
        if (norm[(size_t) lp (l, L_MUTE)] < 0.5f) engineMask |= 1 << (int) toReal (lp (l, L_ENGINE), norm[(size_t) lp (l, L_ENGINE)]);
}

juce::String Preset::toJson() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("format", "PLANETVEL"); o->setProperty ("version", 1);
    o->setProperty ("name", name); o->setProperty ("category", category); o->setProperty ("author", author); o->setProperty ("tags", tags);
    auto* pr = new juce::DynamicObject();
    for (int i = 0; i < NUM_PARAMS; ++i) pr->setProperty (defs()[(size_t) i].id, (double) norm[(size_t) i]);
    o->setProperty ("params", juce::var (pr));
    juce::Array<juce::var> macros, rs, steps;
    for (auto& m : macroNames) macros.add (m);
    for (auto& r : routes) if (r.src != SRC_NONE && r.dest >= 0)
    { auto* ro = new juce::DynamicObject(); ro->setProperty ("src", modSrcName (r.src)); ro->setProperty ("dest", defs()[(size_t) r.dest].id); ro->setProperty ("amount", (double) r.amount); rs.add (juce::var (ro)); }
    for (auto w : arp) { auto s = ArpStep::unpack (w); auto* so = new juce::DynamicObject(); so->setProperty ("on", s.on); so->setProperty ("vel", s.vel); so->setProperty ("pitch", s.pitch); so->setProperty ("ratchet", s.ratchet); so->setProperty ("prob", s.prob); steps.add (juce::var (so)); }
    o->setProperty ("macroNames", macros); o->setProperty ("modMatrix", rs);
    auto* ao = new juce::DynamicObject(); ao->setProperty ("length", arpLength); ao->setProperty ("steps", steps); o->setProperty ("arp", juce::var (ao));
    return juce::JSON::toString (juce::var (o), false);
}

bool Preset::fromJson (const juce::String& text, Preset& out)
{
    const auto v = juce::JSON::parse (text);
    auto* o = v.getDynamicObject();
    if (o == nullptr || o->getProperty ("format").toString() != "PLANETVEL") return false;
    Preset p;
    p.name = o->getProperty ("name").toString(); p.category = o->getProperty ("category").toString(); p.author = o->getProperty ("author").toString(); p.tags = o->getProperty ("tags").toString();
    if (auto* pr = o->getProperty ("params").getDynamicObject())
        for (auto& kv : pr->getProperties()) { const int i = findParamById (kv.name.toString()); if (i >= 0) p.norm[(size_t) i] = juce::jlimit (0.0f, 1.0f, (float) (double) kv.value); }
    if (auto* m = o->getProperty ("macroNames").getArray()) for (int i = 0; i < juce::jmin (kNumMacros, m->size()); ++i) p.macroNames[(size_t) i] = (*m)[i].toString();
    if (auto* rs = o->getProperty ("modMatrix").getArray())
    {
        int k = 0;
        for (auto& r : *rs)
        {
            if (k >= kRouteSlots) break;
            Route ro; const auto sn = r["src"].toString();
            for (int s = 1; s < SRC_COUNT; ++s) if (sn == modSrcName (s)) ro.src = s;
            ro.dest = findParamById (r["dest"].toString()); ro.amount = (float) (double) r["amount"];
            if (ro.src != SRC_NONE && ro.dest >= 0) p.routes[(size_t) k++] = ro;
        }
    }
    if (auto* ao = o->getProperty ("arp").getDynamicObject())
    {
        p.arpLength = juce::jlimit (1, kArpSteps, (int) ao->getProperty ("length"));
        if (auto* st = ao->getProperty ("steps").getArray())
            for (int i = 0; i < juce::jmin (kArpSteps, st->size()); ++i)
            { ArpStep s; const auto e = (*st)[i]; s.on = (bool) e["on"]; s.vel = (int) e["vel"]; s.pitch = (int) e["pitch"]; s.ratchet = (int) e["ratchet"]; s.prob = (int) e["prob"]; p.arp[(size_t) i] = s.pack(); }
    }
    p.updateEngineMask(); out = std::move (p); return true;
}

// ================================================================================================ factory
namespace
{
struct Maker
{
    Preset p; Rng r; int nextRoute = 0;
    Maker (const juce::String& name, const juce::String& cat, const juce::String& tags_)
    {
        p.name = name; p.category = cat; p.tags = tags_; p.factory = true;
        r = Rng ((uint32_t) name.hashCode() ^ 0x9E3779B9u);
        for (int i = 0; i < 4; ++i) r.next();
    }
    float rnd (float a, float b) { return a + (b - a) * r.next(); }
    int   pick (std::initializer_list<int> l) { return *(l.begin() + (int) (r.next() * (float) l.size()) % (int) l.size()); }
    bool  chance (float pr) { return r.next() < pr; }
    void  g (int idx, float real) { p.norm[(size_t) idx] = toNormalised (idx, real); }
    void  l (int layer, int i, float real) { g (lp (layer, i), real); }
    void  layer (int layer, int engine, int type, float level, float pan = 0.0f)
    { l (layer, L_MUTE, 0); l (layer, L_ENGINE, (float) engine); l (layer, L_TYPE, (float) type); l (layer, L_LEVEL, level); l (layer, L_PAN, pan); }
    void  mute (int layer) { l (layer, L_MUTE, 1); }
    void  env (int layer, float a, float d, float s, float rel) { l (layer, L_ATK, a); l (layer, L_DEC, d); l (layer, L_SUS, s); l (layer, L_REL, rel); }
    void  filt (int layer, float cutoff, float reso, float keytrk = 0.4f, float fenv = 0.0f) { l (layer, L_CUTOFF, cutoff); l (layer, L_RESO, reso); l (layer, L_KEYTRK, keytrk); l (layer, L_FENV, fenv); }
    void  par (int layer, float a, float b, float c, float d, float e = 0.5f, float f = 0.5f)
    { l (layer, L_P1, a); l (layer, L_P2, b); l (layer, L_P3, c); l (layer, L_P4, d); l (layer, L_P5, e); l (layer, L_P6, f); }
    void  uni (int layer, int n, float detune, float spread) { l (layer, L_UNI, (float) n); l (layer, L_DETUNE, detune); l (layer, L_SPREAD, spread); }
    void  route (int src, int dest, float amt) { if (nextRoute < kRouteSlots) p.routes[(size_t) nextRoute++] = { src, dest, amt }; }
    bool  on (int layer) const { return p.norm[(size_t) lp (layer, L_MUTE)] < 0.5f; }
    int   engineOf (int layer) const { return (int) toReal (lp (layer, L_ENGINE), p.norm[(size_t) lp (layer, L_ENGINE)]); }
    int   typeOf (int layer) const { return (int) toReal (lp (layer, L_TYPE), p.norm[(size_t) lp (layer, L_TYPE)]); }

    void fx (int revType, float revSize, float revWet, float dlyMix, int dlyDiv, float dlyFb)
    { g (G_REV_TYPE, (float) revType); g (G_REV_SIZE, revSize); g (G_REV_WET, revWet); g (G_DLY_MIX, dlyMix); g (G_DLY_DIV, (float) dlyDiv); g (G_DLY_FB, dlyFb); }
    void orbit (bool animated, float rate, float radius, float x = 0.5f, float y = 0.5f)
    { g (G_ORBIT_MODE, animated ? 0.0f : 2.0f); g (G_ORBIT_RATE, rate); g (G_ORBIT_RADIUS, radius); g (G_ORBIT_X, x); g (G_ORBIT_Y, y); }

    /** all 8 macros are always mapped; strength scales the amounts */
    void standardMacros (float k = 1.0f)
    {
        // M1 TIDE: filter opening + orbit sweep
        for (int L = 0; L < 2; ++L) if (on (L)) route (SRC_M1, lp (L, L_CUTOFF), 0.30f * k);
        route (SRC_M1, G_ORBIT_X, 0.35f * k);
        // M2 MORPH: the engine's main timbre control on every layer
        for (int L = 0; L < kLayers; ++L) if (on (L)) route (SRC_M2, morphParam (L), 0.40f * k);
        route (SRC_M2, G_ORBIT_Y, 0.30f * k);
        // M3 SPACE
        route (SRC_M3, G_REV_WET, 0.35f * k); route (SRC_M3, G_DLY_MIX, 0.25f * k); route (SRC_M3, G_REV_SIZE, 0.20f * k);
        // M4 GRIT
        route (SRC_M4, G_DRIVE, 0.55f * k); route (SRC_M4, lp (0, L_DRIVE), 0.40f * k);
        // M5 WARP
        for (int L = 0; L < kLayers; ++L) if (on (L)) route (SRC_M5, warpParam (L), 0.45f * k);
        // M6 AIR
        for (int L = 0; L < kLayers; ++L) if (on (L)) route (SRC_M6, lp (L, L_CUTOFF), 0.25f * k);
        route (SRC_M6, G_EQ_HI, 0.45f * k);
        // M7 DRIFT
        for (int L = 0; L < 2; ++L) if (on (L)) { route (SRC_M7, lp (L, L_DETUNE), 0.40f * k); route (SRC_M7, lp (L, L_SPREAD), 0.25f * k); }
        route (SRC_M7, G_CHO_MIX, 0.35f * k);
        // M8 GLITCH (glitch is always "on" but silent until mix is raised)
        route (SRC_M8, G_GL_MIX, 0.8f); route (SRC_M8, G_GL_PROB, 0.5f);
        for (int i = 0; i < kNumMacros; ++i) g (G_M1 + i, i == 7 ? 0.0f : rnd (0.0f, 0.4f));
    }
    int morphParam (int L) const
    {
        switch (engineOf (L)) { case ENG_ORGANIC: return lp (L, typeOf (L) == 1 ? L_P3 : L_P1); case ENG_GRANULAR: return lp (L, L_START); default: return lp (L, L_P1); }
    }
    int warpParam (int L) const
    {
        switch (engineOf (L)) { case ENG_ORGANIC: return lp (L, L_P4); case ENG_GRANULAR: return lp (L, L_P4); case ENG_SPECTRAL: return lp (L, L_P4); default: return lp (L, L_P2); }
    }
    void arpPattern (float density, bool ratchets)
    {
        for (int i = 0; i < kArpSteps; ++i)
        {
            ArpStep s; s.on = i % 4 == 0 || chance (density); s.vel = (i % 4 == 0 ? 110 : (int) rnd (60, 100));
            s.pitch = chance (0.18f) ? pick ({ -12, 12, 7, 5, 0 }) : 0; s.ratchet = (ratchets && chance (0.15f)) ? pick ({ 2, 2, 3, 4 }) : 1; s.prob = chance (0.2f) ? (int) rnd (55, 90) : 100;
            p.arp[(size_t) i] = s.pack();
        }
        p.arpLength = pick ({ 8, 12, 16, 16, 16, 24, 32 });
    }
    Preset finish() { p.updateEngineMask(); return p; }
};

struct NameBank
{
    juce::StringArray adj { "Crimson","Violet","Red","Ion","Pulsar","Dust","Slow","Velvet","Neon","Lunar","Solar","Magenta","Amber","Distant","Frozen","Hollow","Silent","Faded","Burning","Drifting",
                            "Glass","Ember","Twilight","Orbiting","Cosmic","Alien","Midnight","Rusted","Opal","Quartz","Static","Phantom","Sapphire","Coral","Ashen","Gilded","Lucid","Nebular","Umbral","Auroral" };
    juce::StringArray pads { "Veil","Bloom","Drift","Haze","Tide","Canopy","Horizon","Mist","Orbit","Atmosphere","Aurora","Shroud","Dawn","Glow","Lullaby","Wash","Cloud","Silk","Swell","Current" };
    juce::StringArray leads { "Lead","Signal","Line","Beam","Runner","Pulse","Wire","Flare","Flight","Voice" };
    juce::StringArray arps { "Arp","Sequencer","Steps","Pulse","Motion","Cascade","Ladder","Engine","Pattern","Rain" };
    juce::StringArray glitch { "Glitch","Transmission","Stutter","Packet","Fragment","Burst","Rain","Comet","Static","Corrupt" };
    juce::StringArray plucks { "Pluck","Seeds","Spark","Droplet","Petal","Crater","Thread","Chime","Ripple","Fall" };
    juce::StringArray bells { "Bells","Chimes","Mallets","Music Box","Celeste","Temple","Satellite","Carillon","Gong","Crystals" };
    juce::StringArray pianos { "Piano","Keys","Felt","Hammers","Upright","Grand","Memory","Room","Nocturne","Rain Piano" };
    juce::StringArray choirs { "Choir","Voices","Ahh","Ensemble","Chorus","Hymn","Requiem","Vowels","Anthem","Chant" };
    juce::StringArray strings { "Strings","Cello","Ensemble","Bow","Violins","Ether","Section","Swell" };
    juce::StringArray textures { "Drone","Texture","Storm","Field","Abyss","Hum","Rift","Static Sea","Cavern","Wind" };
    juce::StringArray bass { "Sub","Bass","Low","Gravity","Rumble","Pulse Bass","Depth" };
    juce::StringArray fxs { "Impact","Riser","Downlifter","Hit","Entry","Braam","Sweep","Swell","Boom","Collapse" };
    juce::StringArray sig { "Transmission","Pulse Choir","Lullaby","Signal","Hymn","Beacon","Relay","Pilgrim","Oracle","Memory" };
};

juce::String uniqueName (std::set<juce::String>& used, const juce::StringArray& fixed, int& fixedIdx, Rng& r, const juce::StringArray& adj, const juce::StringArray& nouns)
{
    if (fixedIdx < fixed.size()) { auto n = fixed[fixedIdx++]; used.insert (n); return n; }
    for (int tries = 0; tries < 400; ++tries)
    {
        auto n = adj[(int) (r.next() * (float) adj.size()) % adj.size()] + " " + nouns[(int) (r.next() * (float) nouns.size()) % nouns.size()];
        if (tries > 150) n += " " + juce::String::charToString ((juce::juce_wchar) ('A' + (int) (r.next() * 26.0f) % 26));
        if (used.insert (n).second) return n;
    }
    juce::String n = "Vel " + juce::String ((int) used.size()); used.insert (n); return n;
}

// ---- category generators ----------------------------------------------------------------------------------------------
void makePad (Maker& m, bool dark)
{
    const int fam = m.pick ({ 0, 2, 4, 5, 3, 1 });
    m.layer (0, ENG_WAVETABLE, fam, 0.8f); m.par (0, m.rnd (0.1f, 0.9f), m.rnd (0.0f, 0.5f), m.pick ({ 0, 0, 1, 2 }) / 5.0f + 0.05f, 0.5f);
    m.uni (0, m.pick ({ 4, 5, 7, 9 }), m.rnd (0.15f, 0.45f), m.rnd (0.6f, 1.0f)); m.env (0, m.rnd (0.6f, 3.5f), 1.5f, 0.85f, m.rnd (3.0f, 8.0f)); m.filt (0, dark ? m.rnd (700, 2000) : m.rnd (1800, 6000), m.rnd (0.08f, 0.25f), 0.45f, m.rnd (0.0f, 0.3f));
    m.l (0, L_LFO, m.rnd (0.0f, 0.25f));
    if (m.chance (0.6f)) { m.layer (1, ENG_ORGANIC, 5, 0.45f, m.rnd (-0.4f, 0.4f)); m.par (1, m.rnd (0.3f, 0.7f), 0.5f, 0.5f, m.rnd (0.2f, 0.7f)); m.l (1, L_TUNE, m.pick ({ 0, -12, 0 })); }
    else { m.layer (1, ENG_ANALOG, 4, 0.4f, m.rnd (-0.3f, 0.3f)); m.uni (1, 7, 0.4f, 0.9f); m.l (1, L_TUNE, -12); m.par (1, 0.5f, 0.0f, 0.3f, 0.0f); }
    m.env (1, m.rnd (1.0f, 4.0f), 1.0f, 0.9f, m.rnd (3.0f, 8.0f)); m.filt (1, m.rnd (900, 2500), 0.12f, 0.4f);
    m.layer (2, ENG_GRANULAR, m.pick ({ 0, 1, 2 }), m.rnd (0.25f, 0.45f), 0.0f); m.par (2, m.rnd (0.45f, 0.75f), m.rnd (0.4f, 0.7f), m.rnd (0.2f, 0.6f), m.rnd (0.0f, 0.15f), m.rnd (0.0f, 0.3f), m.rnd (0.45f, 0.55f)); m.l (2, L_START, m.rnd (0.0f, 1.0f));
    m.env (2, m.rnd (2.0f, 5.0f), 1.0f, 1.0f, m.rnd (4.0f, 9.0f)); m.filt (2, m.rnd (2500, 8000), 0.1f, 0.3f);
    if (m.chance (0.55f)) { m.layer (3, ENG_SPECTRAL, m.pick ({ 0, 1 }), 0.22f, m.rnd (-0.5f, 0.5f)); m.par (3, m.rnd (0.05f, 0.45f), m.rnd (0.5f, 0.8f), m.rnd (0.3f, 0.6f), m.rnd (0.2f, 0.6f)); m.l (3, L_TUNE, 12); m.env (3, m.rnd (1.0f, 3.0f), 1.0f, 1.0f, m.rnd (3.0f, 7.0f)); }
    m.orbit (m.chance (0.7f), m.rnd (0.04f, 0.25f), m.rnd (0.15f, 0.35f)); m.fx (m.pick ({ 0, 2, 3, 3 }), m.rnd (0.6f, 0.92f), m.rnd (0.35f, 0.6f), m.rnd (0.08f, 0.25f), m.pick ({ 2, 12, 3 }), m.rnd (0.35f, 0.55f));
    m.g (G_CHO_MIX, m.rnd (0.15f, 0.45f)); m.g (G_EQ_HI, m.rnd (-1.0f, 2.0f));
    m.standardMacros(); m.route (SRC_MODWHEEL, lp (0, L_CUTOFF), 0.4f); m.route (SRC_MODWHEEL, lp (1, L_CUTOFF), 0.4f);
}
void makeLead (Maker& m)
{
    const bool wt = m.chance (0.5f);
    if (wt) { m.layer (0, ENG_WAVETABLE, m.pick ({ 0, 3, 7, 4 }), 0.75f); m.par (0, m.rnd (0.1f, 0.8f), m.rnd (0.2f, 0.7f), m.pick ({ 1, 2, 4, 5 }) / 5.0f + 0.05f, 0.5f); }
    else { m.layer (0, ENG_ANALOG, m.pick ({ 0, 1, 4 }), 0.75f); m.par (0, m.rnd (0.3f, 0.7f), m.rnd (0.0f, 0.6f), m.rnd (0.0f, 0.4f), m.rnd (0.0f, 0.3f)); }
    m.uni (0, m.pick ({ 2, 3, 5, 7 }), m.rnd (0.1f, 0.35f), m.rnd (0.3f, 0.8f)); m.env (0, m.rnd (0.003f, 0.05f), 0.4f, m.rnd (0.6f, 0.9f), m.rnd (0.25f, 0.9f)); m.filt (0, m.rnd (2500, 9000), m.rnd (0.1f, 0.35f), 0.6f, m.rnd (0.1f, 0.4f));
    m.layer (1, ENG_ANALOG, 1, 0.35f, m.rnd (-0.5f, 0.5f)); m.uni (1, 3, 0.3f, 0.7f); m.l (1, L_TUNE, m.pick ({ -12, 7, 12 })); m.env (1, 0.01f, 0.4f, 0.7f, 0.5f); m.filt (1, 3000, 0.15f, 0.6f);
    if (m.chance (0.6f)) { m.layer (2, ENG_ORGANIC, m.pick ({ 2, 3 }), 0.2f); m.par (2, 0.6f, 0.4f, 0.5f, 0.3f); m.l (2, L_TUNE, 12); m.env (2, 0.003f, 0.3f, 0.5f, 1.2f); }
    m.orbit (false, 0.1f, 0.2f); m.fx (m.pick ({ 1, 0 }), m.rnd (0.3f, 0.55f), m.rnd (0.15f, 0.3f), m.rnd (0.25f, 0.4f), m.pick ({ 12, 3, 13 }), m.rnd (0.35f, 0.5f));
    m.standardMacros(); m.route (SRC_MODWHEEL, lp (0, L_P1), 0.5f); m.route (SRC_MODWHEEL, lp (0, L_LFO), 0.5f); m.l (0, L_LFO, 0.1f); m.g (G_LFO1_RATE, m.rnd (3.5f, 6.0f));
}
void makeArp (Maker& m, bool glitchy)
{
    m.g (G_ARP_ON, 1); m.g (G_ARP_PAT, (float) m.pick ({ 0, 2, 3, 6, 0, 2 })); m.g (G_ARP_RATE, (float) m.pick ({ 4, 3, 8, 9, 4, 4 })); m.g (G_ARP_OCT, (float) m.pick ({ 1, 2, 2, 3 })); m.g (G_ARP_GATE, m.rnd (0.3f, 0.7f)); m.arpPattern (m.rnd (0.5f, 0.85f), glitchy || m.chance (0.4f));
    if (m.chance (0.5f)) { m.layer (0, ENG_WAVETABLE, m.pick ({ 0, 3, 5, 7 }), 0.75f); m.par (0, m.rnd (0.1f, 0.8f), m.rnd (0.0f, 0.5f), m.pick ({ 0, 4 }) / 5.0f + 0.05f, 0.5f); }
    else { m.layer (0, ENG_ANALOG, m.pick ({ 0, 1 }), 0.75f); m.par (0, m.rnd (0.3f, 0.7f), 0.0f, m.rnd (0.0f, 0.3f), 0.0f); }
    m.uni (0, m.pick ({ 1, 2, 3 }), m.rnd (0.1f, 0.3f), 0.5f); m.env (0, 0.002f, m.rnd (0.12f, 0.45f), 0.0f, m.rnd (0.15f, 0.6f)); m.filt (0, m.rnd (1500, 6500), m.rnd (0.1f, 0.35f), 0.6f, m.rnd (0.2f, 0.6f));
    m.layer (1, ENG_ORGANIC, m.pick ({ 2, 3, 4, 6 }), 0.4f, m.rnd (-0.5f, 0.5f)); m.par (1, 0.6f, m.rnd (0.1f, 0.4f), 0.5f, 0.3f); m.l (1, L_TUNE, m.pick ({ 0, 12 })); m.env (1, 0.002f, 0.4f, 1.0f, 0.5f);
    if (m.chance (0.4f)) { m.layer (2, ENG_SPECTRAL, 2, 0.2f); m.par (2, m.rnd (0.5f, 1.0f), 0.2f, 0.6f, 0.2f); m.env (2, 0.002f, 0.3f, 1.0f, 0.5f); }
    m.orbit (false, 0.1f, 0.2f, m.rnd (0.3f, 0.7f), m.rnd (0.3f, 0.6f));
    m.fx (m.pick ({ 0, 1, 3 }), m.rnd (0.45f, 0.8f), m.rnd (0.25f, 0.45f), m.rnd (0.3f, 0.5f), m.pick ({ 12, 3, 13, 2 }), m.rnd (0.4f, 0.6f));
    if (glitchy) { m.g (G_GL_MIX, m.rnd (0.5f, 0.9f)); m.g (G_GL_PROB, m.rnd (0.25f, 0.55f)); m.g (G_GL_TYPE, (float) m.pick ({ 0, 1, 3, 4, 5 })); m.g (G_GL_RATE, (float) m.pick ({ 4, 5, 3 })); }
    m.standardMacros(); m.route (SRC_MODWHEEL, lp (0, L_CUTOFF), 0.45f); m.route (SRC_MODWHEEL, G_DLY_MIX, 0.3f);
    if (glitchy) m.g (G_GL_ON, 1);
}
void makeGlitchPluck (Maker& m)
{
    m.g (G_GL_MIX, m.rnd (0.6f, 1.0f)); m.g (G_GL_PROB, m.rnd (0.3f, 0.7f)); m.g (G_GL_TYPE, (float) m.pick ({ 0, 1, 2, 3, 4, 5, 0, 3 })); m.g (G_GL_RATE, (float) m.pick ({ 3, 4, 5, 8, 9 }));
    m.layer (0, m.pick ({ ENG_ANALOG, ENG_WAVETABLE }), m.pick ({ 0, 1, 7 }), 0.75f); m.par (0, m.rnd (0.2f, 0.7f), m.rnd (0.2f, 0.7f), 0.2f, m.rnd (0.0f, 0.4f)); m.uni (0, 2, 0.2f, 0.5f);
    m.env (0, 0.002f, m.rnd (0.1f, 0.35f), 0.0f, m.rnd (0.1f, 0.4f)); m.filt (0, m.rnd (1200, 6000), m.rnd (0.15f, 0.4f), 0.6f, m.rnd (0.3f, 0.7f));
    m.layer (1, ENG_ORGANIC, m.pick ({ 4, 6, 3 }), 0.5f, m.rnd (-0.4f, 0.4f)); m.par (1, 0.55f, 0.25f, 0.5f, 0.4f); m.env (1, 0.002f, 0.3f, 1.0f, 0.4f);
    m.layer (2, ENG_GRANULAR, 3, 0.2f); m.par (2, 0.25f, 0.7f, 0.5f, 0.3f, 0.5f, 0.6f); m.env (2, 0.01f, 0.4f, 0.6f, 0.8f);
    m.orbit (false, 0.1f, 0.2f); m.fx (m.pick ({ 1, 0 }), m.rnd (0.35f, 0.6f), m.rnd (0.2f, 0.4f), m.rnd (0.3f, 0.5f), m.pick ({ 12, 3, 4 }), m.rnd (0.4f, 0.55f));
    m.g (G_GL_ON, 1); m.g (G_MF_ON, 1); m.g (G_MF_FREQ, m.rnd (3000, 12000)); m.g (G_DRIVE, m.rnd (0.0f, 0.3f));
    m.standardMacros (0.9f); m.route (SRC_MODWHEEL, G_GL_MIX, 0.2f); m.route (SRC_MODWHEEL, G_MF_FREQ, -0.4f);
}
void makePluck (Maker& m)
{
    const bool org = m.chance (0.6f);
    if (org) { m.layer (0, ENG_ORGANIC, m.pick ({ 4, 3, 6, 2 }), 0.8f); m.par (0, m.rnd (0.4f, 0.8f), m.rnd (0.2f, 0.6f), 0.5f, m.rnd (0.1f, 0.5f)); m.env (0, 0.002f, 0.5f, 1.0f, m.rnd (0.4f, 1.5f)); }
    else { m.layer (0, ENG_WAVETABLE, m.pick ({ 0, 5, 3, 7 }), 0.75f); m.par (0, m.rnd (0.1f, 0.7f), m.rnd (0.0f, 0.5f), 0.3f, 0.5f); m.env (0, 0.002f, m.rnd (0.2f, 0.9f), 0.0f, m.rnd (0.3f, 1.2f)); m.uni (0, 2, 0.2f, 0.5f); m.filt (0, m.rnd (1500, 7000), 0.2f, 0.6f, m.rnd (0.2f, 0.6f)); }
    m.layer (1, ENG_SPECTRAL, 2, 0.3f, m.rnd (-0.4f, 0.4f)); m.par (1, m.rnd (0.3f, 0.9f), m.rnd (0.1f, 0.4f), m.rnd (0.4f, 0.8f), 0.2f); m.env (1, 0.002f, 0.3f, 1.0f, 0.5f);
    m.layer (2, ENG_GRANULAR, m.pick ({ 1, 3 }), 0.15f); m.par (2, 0.3f, 0.5f, 0.4f, 0.1f, 0.2f, 0.5f); m.env (2, 0.02f, 0.5f, 1.0f, 1.0f);
    m.orbit (false, 0.1f, 0.2f); m.fx (m.pick ({ 0, 1, 3 }), m.rnd (0.4f, 0.8f), m.rnd (0.25f, 0.5f), m.rnd (0.15f, 0.35f), m.pick ({ 12, 3 }), m.rnd (0.35f, 0.5f));
    m.standardMacros(); m.route (SRC_MODWHEEL, lp (0, L_P1), 0.4f); m.route (SRC_MODWHEEL, G_REV_WET, 0.2f);
}
void makeBell (Maker& m)
{
    m.layer (0, ENG_ORGANIC, m.pick ({ 2, 3, 6, 2 }), 0.8f); m.par (0, m.rnd (0.4f, 0.85f), m.rnd (0.45f, 0.9f), 0.5f, m.rnd (0.1f, 0.5f)); m.env (0, 0.002f, 1.0f, 1.0f, m.rnd (1.5f, 5.0f)); m.uni (0, 1, 0.1f, m.rnd (0.3f, 0.8f));
    m.layer (1, ENG_ORGANIC, m.pick ({ 3, 4, 6 }), 0.35f, m.rnd (-0.5f, 0.5f)); m.par (1, 0.6f, 0.6f, 0.5f, 0.4f); m.l (1, L_TUNE, m.pick ({ 12, 7, 19 })); m.env (1, 0.002f, 1.0f, 1.0f, 3.0f);
    if (m.chance (0.7f)) { m.layer (2, ENG_GRANULAR, 3, 0.3f); m.par (2, m.rnd (0.3f, 0.6f), m.rnd (0.3f, 0.6f), m.rnd (0.3f, 0.6f), 0.2f, 0.1f, m.rnd (0.45f, 0.55f)); m.env (2, m.rnd (0.3f, 1.5f), 1.0f, 1.0f, 4.0f); }
    if (m.chance (0.4f)) { m.layer (3, ENG_SPECTRAL, 2, 0.25f); m.par (3, m.rnd (0.5f, 1.0f), m.rnd (0.4f, 0.8f), 0.6f, 0.4f); m.env (3, 0.002f, 1.0f, 1.0f, 3.0f); }
    m.orbit (m.chance (0.3f), 0.08f, 0.2f); m.fx (m.pick ({ 0, 3, 2 }), m.rnd (0.65f, 0.95f), m.rnd (0.4f, 0.65f), m.rnd (0.2f, 0.35f), m.pick ({ 12, 3, 13 }), m.rnd (0.4f, 0.55f));
    m.standardMacros(); m.route (SRC_MODWHEEL, lp (0, L_P1), 0.4f); m.route (SRC_MODWHEEL, G_REV_WET, 0.25f);
}
void makePiano (Maker& m)
{
    m.layer (0, ENG_ORGANIC, 0, 0.85f); m.par (0, m.rnd (0.25f, 0.55f), m.rnd (0.55f, 0.95f), 0.5f, m.rnd (0.05f, 0.35f)); m.env (0, 0.002f, 1.0f, 1.0f, m.rnd (0.8f, 3.0f)); m.l (0, L_SPREAD, m.rnd (0.3f, 0.7f)); m.filt (0, m.rnd (3500, 9000), 0.05f, 0.3f);
    if (m.chance (0.7f)) { m.layer (1, ENG_ORGANIC, 5, 0.22f, m.rnd (-0.3f, 0.3f)); m.par (1, 0.3f, 0.5f, 0.5f, 0.5f); m.env (1, m.rnd (1.2f, 3.0f), 1.0f, 1.0f, m.rnd (3.0f, 6.0f)); m.filt (1, 1800, 0.1f, 0.3f); }
    else { m.layer (1, ENG_GRANULAR, 0, 0.2f); m.par (1, 0.6f, 0.5f, 0.4f, 0.0f, 0.1f, 0.5f); m.env (1, 2.0f, 1.0f, 1.0f, 5.0f); }
    if (m.chance (0.6f)) { m.layer (2, ENG_GRANULAR, 1, 0.16f); m.par (2, 0.55f, 0.45f, 0.5f, 0.05f, 0.2f, 0.52f); m.env (2, 2.5f, 1.0f, 1.0f, 6.0f); m.l (2, L_TUNE, 12); }
    if (m.chance (0.3f)) { m.layer (3, ENG_WAVETABLE, 5, 0.12f); m.par (3, 0.3f, 0.2f, 0.0f, 0.5f); m.env (3, 1.0f, 1.0f, 1.0f, 4.0f); m.l (3, L_TUNE, 12); }
    m.orbit (false, 0.05f, 0.15f); m.fx (m.pick ({ 0, 2, 0 }), m.rnd (0.55f, 0.9f), m.rnd (0.3f, 0.55f), m.rnd (0.05f, 0.2f), 12, 0.4f);
    m.g (G_DRIVE, m.rnd (0.05f, 0.3f)); m.g (G_CHO_MIX, m.rnd (0.0f, 0.2f)); m.g (G_EQ_HI, m.rnd (-3.0f, 0.0f));
    m.standardMacros(); m.route (SRC_MODWHEEL, lp (0, L_P1), 0.4f); m.route (SRC_MODWHEEL, lp (1, L_LEVEL), 0.3f);
}
void makeChoir (Maker& m)
{
    const float vow = m.rnd (0.0f, 1.0f);
    m.layer (0, ENG_ORGANIC, 1, 0.8f, -0.15f); m.par (0, m.rnd (0.4f, 0.8f), 0.5f, vow, m.rnd (0.25f, 0.7f), m.rnd (0.45f, 0.65f)); m.uni (0, 1, 0.2f, m.rnd (0.5f, 0.9f)); m.env (0, m.rnd (0.25f, 1.8f), 1.0f, 0.9f, m.rnd (1.5f, 5.0f)); m.filt (0, m.rnd (3000, 9000), 0.05f, 0.3f);
    m.layer (1, ENG_ORGANIC, 1, 0.55f, 0.15f); m.par (1, 0.5f, 0.5f, juce::jlimit (0.0f, 1.0f, vow + m.rnd (-0.15f, 0.15f)), 0.5f, m.rnd (0.0f, 0.2f)); m.l (1, L_TUNE, -12); m.env (1, m.rnd (0.4f, 2.0f), 1.0f, 0.9f, m.rnd (2.0f, 5.0f)); m.filt (1, 2500, 0.05f, 0.3f);
    m.layer (2, ENG_ORGANIC, 1, 0.28f, 0.0f); m.par (2, 0.7f, 0.5f, 0.0f, 0.3f, 1.0f); m.l (2, L_TUNE, 12); m.env (2, m.rnd (0.5f, 2.0f), 1.0f, 0.9f, 3.5f); m.filt (2, 7000, 0.05f, 0.3f);
    if (m.chance (0.6f)) { m.layer (3, m.chance (0.5f) ? ENG_GRANULAR : ENG_ORGANIC, m.chance (0.5f) ? 0 : 5, 0.3f); m.par (3, 0.6f, 0.5f, 0.4f, 0.1f, 0.2f, 0.5f); m.env (3, 2.0f, 1.0f, 1.0f, 5.0f); }
    m.orbit (m.chance (0.4f), 0.06f, 0.2f); m.fx (m.pick ({ 2, 0, 3 }), m.rnd (0.7f, 0.95f), m.rnd (0.4f, 0.65f), m.rnd (0.05f, 0.2f), 12, 0.4f); m.g (G_CHO_MIX, m.rnd (0.1f, 0.35f));
    m.standardMacros(); m.route (SRC_MODWHEEL, lp (0, L_P3), 0.6f); m.route (SRC_MODWHEEL, lp (1, L_P3), 0.6f);
}
void makeStrings (Maker& m)
{
    m.layer (0, ENG_ORGANIC, 5, 0.8f, -0.2f); m.par (0, m.rnd (0.35f, 0.75f), 0.5f, 0.5f, m.rnd (0.3f, 0.8f)); m.env (0, m.rnd (0.15f, 1.0f), 1.0f, 0.9f, m.rnd (0.8f, 3.0f));
    m.layer (1, ENG_ORGANIC, 5, 0.5f, 0.2f); m.par (1, m.rnd (0.2f, 0.5f), 0.5f, 0.5f, 0.6f); m.l (1, L_TUNE, -12); m.env (1, m.rnd (0.3f, 1.2f), 1.0f, 0.9f, 2.5f);
    m.layer (2, ENG_ORGANIC, 1, 0.22f); m.par (2, 0.5f, 0.5f, 0.55f, 0.4f, 0.5f); m.env (2, 0.8f, 1.0f, 0.9f, 3.0f);
    if (m.chance (0.4f)) { m.layer (3, ENG_WAVETABLE, 6, 0.2f); m.par (3, m.rnd (0.1f, 0.5f), 0.0f, 0.0f, 0.5f); m.env (3, 0.6f, 1.0f, 0.9f, 3.0f); }
    m.orbit (false, 0.05f, 0.2f); m.fx (m.pick ({ 0, 2 }), m.rnd (0.5f, 0.8f), m.rnd (0.3f, 0.5f), m.rnd (0.05f, 0.2f), 12, 0.4f); m.g (G_CHO_MIX, m.rnd (0.15f, 0.4f));
    m.standardMacros(); m.route (SRC_MODWHEEL, lp (0, L_P1), 0.45f); m.route (SRC_MODWHEEL, lp (0, L_CUTOFF), 0.3f);
}
void makeTexture (Maker& m)
{
    m.layer (0, ENG_GRANULAR, m.pick ({ 0, 1, 2, 3 }), 0.8f); m.par (0, m.rnd (0.4f, 0.9f), m.rnd (0.3f, 0.75f), m.rnd (0.3f, 0.8f), m.rnd (0.0f, 0.3f), m.rnd (0.0f, 0.5f), m.rnd (0.3f, 0.7f)); m.l (0, L_START, m.rnd (0, 1)); m.env (0, m.rnd (2.0f, 7.0f), 1.0f, 1.0f, m.rnd (5.0f, 12.0f)); m.filt (0, m.rnd (1500, 7000), 0.1f, 0.3f); m.l (0, L_SPREAD, 0.9f);
    m.layer (1, ENG_SPECTRAL, m.pick ({ 0, 1 }), 0.5f, m.rnd (-0.5f, 0.5f)); m.par (1, m.rnd (0.3f, 1.0f), m.rnd (0.5f, 1.0f), m.rnd (0.2f, 0.6f), m.rnd (0.3f, 0.9f)); m.env (1, m.rnd (2.0f, 6.0f), 1.0f, 1.0f, m.rnd (5.0f, 10.0f)); m.l (1, L_TUNE, m.pick ({ 0, -12, 12 }));
    m.layer (2, ENG_ANALOG, m.pick ({ 3, 5, 4 }), 0.3f); m.uni (2, 5, 0.5f, 1.0f); m.l (2, L_TUNE, -12); m.env (2, 4.0f, 1.0f, 1.0f, 8.0f); m.filt (2, m.rnd (300, 1200), 0.2f, 0.2f); m.par (2, 0.5f, 0.0f, 0.4f, 0.0f);
    m.layer (3, ENG_WAVETABLE, m.pick ({ 2, 3, 4 }), 0.35f); m.par (3, m.rnd (0.1f, 0.9f), m.rnd (0.2f, 0.7f), m.pick ({ 2, 4 }) / 5.0f + 0.05f, 0.5f); m.uni (3, 6, 0.4f, 1.0f); m.env (3, 3.0f, 1.0f, 1.0f, 8.0f); m.filt (3, m.rnd (800, 3500), 0.15f, 0.3f);
    m.orbit (true, m.rnd (0.03f, 0.15f), m.rnd (0.25f, 0.45f)); m.fx (3, m.rnd (0.75f, 0.98f), m.rnd (0.5f, 0.75f), m.rnd (0.1f, 0.3f), 2, 0.5f); m.g (G_CHO_MIX, 0.3f); m.g (G_LFO1_RATE, m.rnd (0.05f, 0.4f));
    m.l (0, L_LFO, m.rnd (0.1f, 0.5f)); m.l (3, L_LFO, m.rnd (0.1f, 0.5f));
    m.standardMacros(); m.route (SRC_MODWHEEL, lp (0, L_P3), 0.5f); m.route (SRC_MODWHEEL, lp (1, L_P1), 0.4f);
}
void makeBass (Maker& m)
{
    m.layer (0, ENG_ANALOG, m.pick ({ 0, 1, 4 }), 0.8f); m.par (0, m.rnd (0.3f, 0.6f), 0.0f, m.rnd (0.2f, 0.6f), m.rnd (0.0f, 0.2f)); m.uni (0, m.pick ({ 1, 2, 3 }), m.rnd (0.05f, 0.2f), 0.3f);
    m.env (0, 0.004f, m.rnd (0.2f, 0.6f), m.rnd (0.5f, 0.85f), m.rnd (0.15f, 0.5f)); m.filt (0, m.rnd (250, 1100), m.rnd (0.2f, 0.45f), 0.5f, m.rnd (0.25f, 0.6f)); m.l (0, L_DRIVE, m.rnd (0.1f, 0.5f));
    m.layer (1, ENG_ANALOG, 3, 0.7f); m.l (1, L_TUNE, -12); m.uni (1, 1, 0, 0); m.env (1, 0.004f, 0.3f, 1.0f, 0.3f); m.filt (1, 500, 0.0f, 0.3f);
    if (m.chance (0.5f)) { m.layer (2, ENG_WAVETABLE, m.pick ({ 0, 3, 4 }), 0.3f); m.par (2, m.rnd (0.1f, 0.6f), m.rnd (0.1f, 0.5f), 0.1f, 0.5f); m.l (2, L_TUNE, 12); m.env (2, 0.004f, 0.4f, 0.7f, 0.4f); m.filt (2, 900, 0.2f, 0.5f, 0.3f); }
    m.orbit (false, 0.05f, 0.1f); m.fx (m.pick ({ 1, 0 }), 0.35f, m.rnd (0.05f, 0.15f), m.rnd (0.0f, 0.1f), 12, 0.3f); m.g (G_DRIVE, m.rnd (0.1f, 0.4f));
    m.standardMacros (0.8f); m.route (SRC_MODWHEEL, lp (0, L_CUTOFF), 0.45f); m.route (SRC_MODWHEEL, lp (0, L_DRIVE), 0.3f);
}
void makeFx (Maker& m, int kind)    // 0 impact, 1 riser, 2 downlifter, 3 braam/hit
{
    const bool rise = kind == 1, fall = kind == 2;
    m.layer (0, ENG_SPECTRAL, m.pick ({ 2, 0, 1 }), 0.8f); m.par (0, m.rnd (0.4f, 1.0f), m.rnd (0.6f, 1.0f), m.rnd (0.3f, 0.7f), m.rnd (0.3f, 0.9f));
    m.layer (1, ENG_GRANULAR, 2, 0.5f); m.par (1, m.rnd (0.5f, 0.9f), m.rnd (0.5f, 0.9f), m.rnd (0.5f, 1.0f), m.rnd (0.2f, 0.6f), m.rnd (0.0f, 0.5f), m.rnd (0.2f, 0.8f));
    m.layer (2, ENG_ANALOG, kind == 3 ? 4 : 5, 0.35f); m.uni (2, 7, 0.5f, 1.0f); m.l (2, L_TUNE, kind == 3 ? -24 : 0); m.par (2, 0.5f, 0.0f, 0.5f, 0.0f);
    for (int L = 0; L < 3; ++L)
    {
        if (rise) { m.env (L, m.rnd (4.0f, 8.0f), 1.0f, 1.0f, m.rnd (0.4f, 1.5f)); m.filt (L, m.rnd (300, 900), 0.25f, 0.1f, 0.9f); }
        else if (fall) { m.env (L, 0.01f, m.rnd (2.0f, 5.0f), 0.3f, m.rnd (2.0f, 6.0f)); m.filt (L, m.rnd (6000, 12000), 0.25f, 0.1f, -0.7f); }
        else { m.env (L, 0.002f, m.rnd (0.8f, 2.5f), 0.0f, m.rnd (3.0f, 9.0f)); m.filt (L, m.rnd (3000, 9000), 0.2f, 0.3f, m.rnd (-0.3f, 0.2f)); }
    }
    m.orbit (true, m.rnd (0.05f, 0.3f), m.rnd (0.2f, 0.45f)); m.fx (3, m.rnd (0.8f, 0.98f), m.rnd (0.5f, 0.8f), m.rnd (0.1f, 0.3f), 2, 0.5f); m.g (G_DRIVE, m.rnd (0.1f, 0.5f));
    m.standardMacros(); m.route (SRC_MODWHEEL, G_REV_WET, 0.3f); m.route (SRC_MODWHEEL, lp (0, L_CUTOFF), 0.5f);
}
void makeSignature (Maker& m, int variant)
{
    // hybrid: choir + granular + planetary wavetable + spectral, always all four engines
    m.layer (0, ENG_ORGANIC, 1, 0.75f, -0.2f); m.par (0, 0.6f, 0.5f, m.rnd (0.0f, 0.4f), 0.5f, 0.55f); m.env (0, m.rnd (0.5f, 2.0f), 1.0f, 0.9f, m.rnd (3.0f, 6.0f)); m.filt (0, 6000, 0.08f, 0.3f);
    m.layer (1, ENG_WAVETABLE, 4, 0.55f, 0.2f); m.par (1, m.rnd (0.1f, 0.9f), m.rnd (0.1f, 0.5f), 0.05f, 0.5f); m.uni (1, 7, 0.35f, 0.9f); m.env (1, m.rnd (1.0f, 3.0f), 1.0f, 0.9f, 5.0f); m.filt (1, m.rnd (1500, 4000), 0.15f, 0.4f, 0.2f);
    m.layer (2, ENG_GRANULAR, variant % 2 == 0 ? 0 : 3, 0.4f); m.par (2, m.rnd (0.4f, 0.7f), m.rnd (0.4f, 0.7f), m.rnd (0.3f, 0.6f), 0.1f, 0.3f, m.rnd (0.4f, 0.6f)); m.l (2, L_START, m.rnd (0, 1)); m.env (2, 2.5f, 1.0f, 1.0f, 6.0f);
    m.layer (3, ENG_SPECTRAL, 1, 0.3f); m.par (3, m.rnd (0.3f, 0.9f), 0.7f, 0.5f, m.rnd (0.3f, 0.8f)); m.l (3, L_TUNE, 12); m.env (3, 1.5f, 1.0f, 1.0f, 5.0f);
    m.orbit (true, m.rnd (0.05f, 0.2f), m.rnd (0.25f, 0.45f)); m.fx (3, m.rnd (0.8f, 0.97f), m.rnd (0.5f, 0.7f), m.rnd (0.15f, 0.3f), 12, 0.5f); m.g (G_CHO_MIX, 0.3f);
    if (variant % 3 == 0) { m.g (G_ARP_ON, 1); m.g (G_ARP_PAT, 2); m.g (G_ARP_RATE, 8); m.g (G_ARP_OCT, 2); m.arpPattern (0.7f, true); for (int L = 0; L < 4; ++L) if (L != 3) m.l (L, L_DEC, 0.5f); }
    m.standardMacros (1.2f); m.route (SRC_MODWHEEL, lp (0, L_P3), 0.5f); m.route (SRC_MODWHEEL, G_REV_WET, 0.2f);
}
} // namespace

namespace PresetBank
{
juce::StringArray categories()
{
    return { "Pads", "Synths & Leads", "Arps", "Glitch Plucks", "Plucks", "Bells & Mallets", "Ambient Pianos", "Choirs",
             "Strings & Organic", "Textures & Drones", "Bass", "Cinematic FX", "Signature" };
}

const std::vector<Preset>& factory()
{
    static const std::vector<Preset> bank = []
    {
        std::vector<Preset> out; out.reserve (640);
        NameBank nb; std::set<juce::String> used; Rng nameRng (31337);
        struct Cat { const char* name; int count; juce::StringArray fixed; const juce::StringArray* nouns; const char* tag; };
        const Cat cats[] = {
            { "Pads", 90, { "Red Nebula", "Violet Horizon", "Pulsar Bloom II", "Ion Veil", "Crimson Atmosphere", "Slow Planet", "Velvet Orbit", "Dust of Vel" }, &nb.pads, "pad ambient evolving" },
            { "Synths & Leads", 60, { "Solar Wind Lead", "Magenta Signal", "Retro Orbit", "Plasma Line", "Neon Meridian" }, &nb.leads, "lead synth" },
            { "Arps", 60, { "Orbit Sequencer", "Satellite Pulse", "Crimson Arp Engine", "Starfield Motion", "Gravity Steps" }, &nb.arps, "arp sequence" },
            { "Glitch Plucks", 45, { "Broken Transmission", "Data Rain", "Signal 47 Glitch", "Chopped Comet", "Stutter Moon" }, &nb.glitch, "glitch pluck stutter" },
            { "Plucks", 45, { "Glass Seeds", "Lunar Pluck", "Kalimba Drift", "Soft Crater" }, &nb.plucks, "pluck" },
            { "Bells & Mallets", 45, { "Ice Temple Bells", "Celeste Dust", "Ring of Vel", "Music Box Planet", "Bowed Satellite" }, &nb.bells, "bell mallet crystal" },
            { "Ambient Pianos", 50, { "Felt Horizon", "Midnight Upright", "Tape Memory Piano", "Frozen Grand", "Distant Room Keys", "Rain on Hammers" }, &nb.pianos, "piano felt ambient" },
            { "Choirs", 55, { "Cathedral of Vel", "Red Sky Choir", "Boys of the Moon", "Soft Ahh Ensemble", "Xeno Choir Reborn", "Vowel Drift", "Requiem Orbit" }, &nb.choirs, "choir vocal vowel" },
            { "Strings & Organic", 30, { "Bowed Ether", "Cello Dust", "Airy Ensemble" }, &nb.strings, "strings organic" },
            { "Textures & Drones", 50, { "Core of the Planet", "Magnetic Storm", "Deep Field", "Ocean Under Ice", "Event Horizon Drone" }, &nb.textures, "texture drone granular" },
            { "Bass", 30, { "Dark Matter Sub II", "Gravity Bass", "Velvet Low" }, &nb.bass, "bass sub" },
            { "Cinematic FX", 30, { "Impact Crater", "Atmosphere Entry", "Riser to Vel", "Downlifter Void", "Braam Planet" }, &nb.fxs, "cinematic fx hit riser" },
            { "Signature", 30, { "Vel Transmission", "Red Pulse Choir", "Alien Lullaby", "Signal From Vel", "Gravity Hymn" }, &nb.sig, "signature hybrid planet" } };

        for (const auto& c : cats)
        {
            int fi = 0;
            for (int i = 0; i < c.count; ++i)
            {
                const auto name = uniqueName (used, c.fixed, fi, nameRng, nb.adj, *c.nouns);
                Maker m (name, c.name, c.tag);
                const juce::String cat (c.name);
                m.g (G_GL_ON, 1.0f); m.g (G_GL_MIX, 0.0f);               // glitch is armed but silent until M8 / the glitchy presets raise its mix
                if (cat == "Pads") makePad (m, i % 5 == 3);
                else if (cat == "Synths & Leads") makeLead (m);
                else if (cat == "Arps") makeArp (m, i % 4 == 3);
                else if (cat == "Glitch Plucks") makeGlitchPluck (m);
                else if (cat == "Plucks") makePluck (m);
                else if (cat == "Bells & Mallets") makeBell (m);
                else if (cat == "Ambient Pianos") makePiano (m);
                else if (cat == "Choirs") makeChoir (m);
                else if (cat == "Strings & Organic") makeStrings (m);
                else if (cat == "Textures & Drones") makeTexture (m);
                else if (cat == "Bass") makeBass (m);
                else if (cat == "Cinematic FX") makeFx (m, name.containsIgnoreCase ("riser") ? 1 : name.containsIgnoreCase ("downlifter") ? 2 : name.containsIgnoreCase ("braam") || name.containsIgnoreCase ("hit") ? 3 : (i % 4));
                else makeSignature (m, i);
                out.push_back (m.finish());
            }
        }
#if __has_include("FactoryCalibration.inc")
        #include "FactoryCalibration.inc"
        for (size_t i = 0; i < out.size() && i < sizeof (kFactoryCal) / sizeof (kFactoryCal[0]); ++i)
        { out[i].norm[(size_t) G_OUT_VOL] = toNormalised (G_OUT_VOL, kFactoryCal[i][0]); out[i].norm[(size_t) G_MASTER] = toNormalised (G_MASTER, kFactoryCal[i][1]); }
#endif
        return out;
    }();
    return bank;
}

juce::File userDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Trill Tron").getChildFile ("Planet Vel").getChildFile ("Presets").getChildFile ("User");
}
std::vector<Preset> loadUser()
{
    std::vector<Preset> v;
    for (auto& f : userDirectory().findChildFiles (juce::File::findFiles, true, "*.pvpreset"))
    { Preset p; if (Preset::fromJson (f.loadFileAsString(), p)) { p.factory = false; v.push_back (std::move (p)); } }
    return v;
}
bool saveUser (const Preset& p, juce::File* written)
{
    auto dir = userDirectory(); if (! dir.createDirectory()) return false;
    auto f = dir.getChildFile (juce::File::createLegalFileName (p.name) + ".pvpreset");
    const bool ok = f.replaceWithText (p.toJson()); if (written) *written = f; return ok;
}
} // namespace PresetBank
} // namespace PV
