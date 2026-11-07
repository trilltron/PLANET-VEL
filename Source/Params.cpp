#include "Params.h"

namespace PV
{
static juce::String twoDigits (float v, int dp = 0) { return juce::String (v, dp); }

juce::String formatValue (const ParamDef& d, float v)
{
    switch (d.fmt)
    {
        case Fmt::Percent:  return juce::String (juce::roundToInt (v * 100.0f)) + "%";
        case Fmt::Hz:       return v >= 1000.0f ? juce::String (v / 1000.0f, 1) + "k" : juce::String (juce::roundToInt (v));
        case Fmt::Seconds:  return v < 1.0f ? juce::String (juce::roundToInt (v * 1000.0f)) + "ms" : juce::String (v, 1) + "s";
        case Fmt::Ms:       return juce::String (juce::roundToInt (v)) + "ms";
        case Fmt::Db:       return (v >= 0 ? "+" : "") + juce::String (v, 1);
        case Fmt::LinDb:    return v <= 0.0001f ? juce::String ("-inf") : juce::String (juce::roundToInt (juce::Decibels::gainToDecibels (v))) + "dB";
        case Fmt::SqDb:     return v <= 0.01f ? juce::String ("-inf") : juce::String (juce::roundToInt (juce::Decibels::gainToDecibels (v * v))) + "dB";           // layer level is squared in the engine
        case Fmt::MasterDb: return v <= 0.01f ? juce::String ("-inf") : juce::String (juce::Decibels::gainToDecibels (1.5f * v * v), 1) + "dB";
        case Fmt::Semis:    return (v >= 0 ? "+" : "") + juce::String (v, 1);
        case Fmt::Pan:      { int p = juce::roundToInt (v * 50.0f); return p == 0 ? "C" : (p < 0 ? "L" + juce::String (-p) : "R" + juce::String (p)); }
        case Fmt::Int:      return juce::String (juce::roundToInt (v));
        case Fmt::Beats:    return juce::String (v, 2);
        default:            return twoDigits (v, 2);
    }
}

static std::vector<ParamDef> buildDefs()
{
    std::vector<ParamDef> v;
    v.reserve (NUM_PARAMS);

    auto F = [&] (juce::String id, juce::String name, float mn, float mx, float def, Fmt fmt = Fmt::Plain,
                  float centre = 0.0f, bool mod = true, bool bip = false)
    {
        ParamDef d; d.id = id; d.name = name; d.kind = Kind::Float; d.mn = mn; d.mx = mx; d.def = def;
        d.fmt = fmt; d.centre = centre; d.modulatable = mod; d.bipolarUi = bip; v.push_back (d);
    };
    auto C = [&] (juce::String id, juce::String name, juce::StringArray ch, int def)
    {
        ParamDef d; d.id = id; d.name = name; d.kind = Kind::Choice; d.mn = 0; d.mx = (float) (ch.size() - 1);
        d.def = (float) def; d.step = 1; d.choices = ch; d.fmt = Fmt::Int; v.push_back (d);
    };
    auto B = [&] (juce::String id, juce::String name, bool def)
    {
        ParamDef d; d.id = id; d.name = name; d.kind = Kind::Bool; d.mn = 0; d.mx = 1; d.def = def ? 1.f : 0.f; d.step = 1; v.push_back (d);
    };

    juce::StringArray rates; for (auto* r : kRateNames) rates.add (r);

    // ------------------------------------------------------------------ globals
    F ("master", "Master", 0.0f, 1.0f, 0.75f, Fmt::MasterDb, 0.0f, true);
    F ("orbitX", "Orbit X", 0, 1, 0.5f, Fmt::Percent);
    F ("orbitY", "Orbit Y", 0, 1, 0.5f, Fmt::Percent);
    C ("orbitMode", "Orbit Mode", { "Orbit", "Free", "Hold" }, 1);
    F ("orbitRate", "Orbit Rate", 0.0f, 2.0f, 0.15f, Fmt::Plain, 0.4f);
    F ("orbitRadius", "Orbit Radius", 0.0f, 0.5f, 0.25f, Fmt::Percent);
    for (int i = 0; i < kNumMacros; ++i)
        F ("macro" + juce::String (i + 1), "Macro " + juce::String (i + 1) + " " + kMacroNames[i], 0, 1, 0.0f, Fmt::Percent, 0, false);

    B ("mfOn", "Filter On", false);
    C ("mfType", "Filter Type", { "LP", "BP", "HP" }, 0);
    F ("mfFreq", "Filter Freq", 20.0f, 20000.0f, 8000.0f, Fmt::Hz, 1000.0f);
    F ("mfRes", "Filter Res", 0, 1, 0.2f, Fmt::Percent);

    B ("dlyOn", "Delay On", true);
    B ("dlySync", "Delay Sync", true);
    C ("dlyDiv", "Delay Time Div", rates, 12);      // 1/8D
    F ("dlyTime", "Delay Time", 1.0f, 2000.0f, 375.0f, Fmt::Ms, 300.0f);
    F ("dlyFb", "Delay Feedback", 0.0f, 0.95f, 0.44f, Fmt::Percent);
    F ("dlyMix", "Delay Mix", 0, 1, 0.25f, Fmt::Percent);

    B ("revOn", "Reverb On", true);
    C ("revType", "Reverb Type", { "Hall", "Plate", "Cathedral", "Nebula" }, 3);
    F ("revSize", "Reverb Size", 0, 1, 0.7f, Fmt::Percent);
    F ("revWet", "Reverb Wet", 0, 1, 0.35f, Fmt::Percent);

    B ("glOn", "Glitch On", false);
    C ("glType", "Glitch Type", { "Stutter", "Reverse", "TapeStop", "Crush", "PitchJump", "GateChop" }, 0);
    C ("glRate", "Glitch Rate", rates, 5);
    F ("glMix", "Glitch Mix", 0, 1, 0.5f, Fmt::Percent);
    F ("glProb", "Glitch Probability", 0, 1, 0.35f, Fmt::Percent);

    B ("outLimit", "Limiter", true);
    C ("outMono", "Output Mode", { "Stereo", "Mono" }, 0);
    F ("outVol", "Output Volume", -24.0f, 6.0f, 0.0f, Fmt::Db, 0.0f, true, true);

    B ("arpOn", "Arp On", false);
    C ("arpPat", "Arp Pattern", { "Up", "Down", "Up/Down", "Random", "Chord", "As Played", "Converge" }, 0);
    C ("arpRate", "Arp Rate", rates, 4);
    F ("arpOct", "Arp Octaves", 1, 4, 1, Fmt::Int, 0, false);
    F ("arpGate", "Arp Gate", 0.05f, 1.0f, 0.6f, Fmt::Percent, 0, false);

    F ("lfo1Rate", "LFO1 Rate", 0.02f, 20.0f, 0.3f, Fmt::Plain, 1.0f);
    C ("lfo1Shape", "LFO1 Shape", { "Sine", "Triangle", "Saw", "Square", "S&H", "Smooth Random" }, 0);
    F ("lfo2Rate", "LFO2 Rate", 0.02f, 20.0f, 0.7f, Fmt::Plain, 1.0f);
    C ("lfo2Shape", "LFO2 Shape", { "Sine", "Triangle", "Saw", "Square", "S&H", "Smooth Random" }, 1);

    F ("choMix", "Chorus Mix", 0, 1, 0.0f, Fmt::Percent);
    F ("choRate", "Chorus Rate", 0.05f, 5.0f, 0.4f, Fmt::Plain, 0.5f);
    F ("choDepth", "Chorus Depth", 0, 1, 0.5f, Fmt::Percent);
    F ("drive", "Tape Drive", 0, 1, 0.0f, Fmt::Percent);
    F ("width", "Stereo Width", 0, 2, 1.0f, Fmt::Percent);
    F ("eqLo", "EQ Low", -12, 12, 0, Fmt::Db, 0, true, true);
    F ("eqMid", "EQ Mid", -12, 12, 0, Fmt::Db, 0, true, true);
    F ("eqHi", "EQ High", -12, 12, 0, Fmt::Db, 0, true, true);

    jassert ((int) v.size() == G_COUNT);

    // ------------------------------------------------------------------ layers
    for (int l = 0; l < kLayers; ++l)
    {
        const juce::String p = juce::String::charToString ((juce::juce_wchar) ('a' + l)) + "_";
        const juce::String n = juce::String::charToString ((juce::juce_wchar) ('A' + l)) + " ";
        C (p + "engine", n + "Engine", engineNames(), l == 0 ? ENG_WAVETABLE : ENG_ORGANIC);
        B (p + "mute", n + "Mute", l != 0);
        B (p + "solo", n + "Solo", false);
        F (p + "level", n + "Level", 0, 1, 0.8f, Fmt::SqDb);
        F (p + "pan", n + "Pan", -1, 1, 0, Fmt::Pan, 0, true, true);
        F (p + "tune", n + "Tune", -24, 24, 0, Fmt::Semis, 0, true, true);
        F (p + "start", n + "Start", 0, 1, 0.0f, Fmt::Percent);
        F (p + "cutoff", n + "Cutoff", 20, 20000, 12000, Fmt::Hz, 1000);
        F (p + "reso", n + "Resonance", 0, 1, 0.1f, Fmt::Percent);
        C (p + "ftype", n + "Filter Type", { "LP", "BP", "HP", "Notch" }, 0);
        F (p + "drive", n + "Drive", 0, 1, 0, Fmt::Percent);
        F (p + "keytrk", n + "Key Track", 0, 1, 0.3f, Fmt::Percent);
        F (p + "fenv", n + "Env Amount", -1, 1, 0, Fmt::Percent, 0, true, true);
        F (p + "lfo", n + "LFO Amount", 0, 1, 0, Fmt::Percent);
        F (p + "atk", n + "Attack", 0.001f, 10.0f, 0.24f, Fmt::Seconds, 0.4f);
        F (p + "dec", n + "Decay", 0.005f, 10.0f, 0.8f, Fmt::Seconds, 0.6f);
        F (p + "sus", n + "Sustain", 0, 1, 0.8f, Fmt::LinDb);
        F (p + "rel", n + "Release", 0.005f, 20.0f, 2.0f, Fmt::Seconds, 1.0f);
        F (p + "spread", n + "Spread", 0, 1, 0.5f, Fmt::Percent);
        F (p + "send", n + "Reverb Send", 0, 1, 0.5f, Fmt::Percent);
        F (p + "keylo", n + "Key Low", 0, 127, 0, Fmt::Int, 0, false);
        F (p + "keyhi", n + "Key High", 0, 127, 127, Fmt::Int, 0, false);
        F (p + "uni", n + "Unison", 1, 16, 3, Fmt::Int, 0, false);
        F (p + "detune", n + "Detune", 0, 1, 0.25f, Fmt::Percent);
        C (p + "type", n + "Type", { "1", "2", "3", "4", "5", "6", "7", "8" }, 0);
        for (int k = 0; k < 6; ++k)
            F (p + "p" + juce::String (k + 1), n + "Param " + juce::String (k + 1), 0, 1, 0.5f, Fmt::Percent);
    }

    // sensible defaults for the "init" voice
    jassert ((int) v.size() == NUM_PARAMS);
    return v;
}

const std::vector<ParamDef>& defs()
{
    static const std::vector<ParamDef> d = buildDefs();
    return d;
}

int findParamById (const juce::String& id)
{
    static std::map<juce::String, int> m = [] { std::map<juce::String, int> r; int i = 0; for (auto& d : defs()) r[d.id] = i++; return r; }();
    auto it = m.find (id);
    return it == m.end() ? -1 : it->second;
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (auto& d : defs())
    {
        const juce::ParameterID pid { d.id, 1 };
        if (d.kind == Kind::Bool)
            layout.add (std::make_unique<juce::AudioParameterBool> (pid, d.name, d.def > 0.5f));
        else if (d.kind == Kind::Choice)
            layout.add (std::make_unique<juce::AudioParameterChoice> (pid, d.name, d.choices, (int) d.def));
        else
        {
            juce::NormalisableRange<float> r (d.mn, d.mx);
            if (d.centre > d.mn && d.centre < d.mx) r.setSkewForCentre (d.centre);
            auto fmt = d.fmt;
            ParamDef copy = d;
            layout.add (std::make_unique<juce::AudioParameterFloat> (pid, d.name, r, d.def,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction ([copy] (float x, int) { return formatValue (copy, x); })));
            juce::ignoreUnused (fmt);
        }
    }
    return layout;
}

const juce::StringArray& engineNames()
{
    static const juce::StringArray n { "Organic", "Wavetable", "Granular", "Analog", "Spectral" };
    return n;
}

juce::StringArray typeNames (int engine)
{
    switch (engine)
    {
        case ENG_ORGANIC:   return { "Felt Piano", "Choir", "Bells", "Glass", "Kalimba", "Strings", "Music Box", "User Sample" };
        case ENG_WAVETABLE: return { "Analog", "Vocal", "Spectral", "Metallic", "Planetary", "Glass", "Organ", "Digital" };
        case ENG_GRANULAR:  return { "Choir Cloud", "Glass Cloud", "Noise Field", "Bell Cloud", "User Sample", "Choir Cloud", "Glass Cloud", "Noise Field" };
        case ENG_ANALOG:    return { "Saw", "Square", "Triangle", "Sine", "Supersaw", "Noise", "Saw", "Square" };
        case ENG_SPECTRAL:  return { "Noise Bow", "Saw Bow", "Click Strike", "Noise Bow", "Saw Bow", "Click Strike", "Noise Bow", "Saw Bow" };
    }
    return {};
}

juce::String knobLabel (int e, int i)
{
    static const char* t[ENG_COUNT][6] = {
        { "BRIGHT", "RING", "VOWEL", "MOTION", "VOICE", "" },
        { "POSITION", "WARP", "WARP MODE", "", "", "" },
        { "SIZE", "DENSITY", "SPRAY", "PITCH RND", "REVERSE", "SCAN" },
        { "PULSE", "SYNC", "SUB", "FM", "", "" },
        { "STRUCTURE", "DECAY", "BRIGHT", "BLUR", "", "" } };
    return t[juce::jlimit (0, ENG_COUNT - 1, e)][juce::jlimit (0, 5, i)];
}
} // namespace PV
