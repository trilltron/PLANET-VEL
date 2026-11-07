// Preset model, .pvpreset (JSON) I/O and the factory bank.
#pragma once
#include "Params.h"
#include "DSP/Arp.h"

namespace PV
{
enum ModSrc { SRC_NONE = 0, SRC_M1, SRC_M2, SRC_M3, SRC_M4, SRC_M5, SRC_M6, SRC_M7, SRC_M8, SRC_MODWHEEL, SRC_LFO1, SRC_LFO2, SRC_AFTERTOUCH, SRC_COUNT };
const char* modSrcName (int s);

struct Route { int src = SRC_NONE, dest = -1; float amount = 0; };

struct Preset
{
    juce::String name { "Init" }, category { "User" }, author { "Trill Tron" }, tags;
    std::vector<float> norm;                              // NUM_PARAMS normalised values
    std::array<Route, kRouteSlots> routes {};
    std::array<juce::String, kNumMacros> macroNames;
    std::array<uint32_t, kArpSteps> arp {};
    int arpLength = 16;
    int engineMask = 0;                                   // bit per Engine used by an unmuted layer
    bool factory = false;

    Preset();
    static Preset makeInit();
    void updateEngineMask();
    juce::String toJson() const;
    static bool fromJson (const juce::String& text, Preset& out);
};

float toNormalised (int paramIndex, float realValue);
float toReal (int paramIndex, float normalised);

namespace PresetBank
{
const std::vector<Preset>& factory();                 // 620 presets, built once, deterministic
juce::File userDirectory();
std::vector<Preset> loadUser();
bool saveUser (const Preset&, juce::File* written = nullptr);
juce::StringArray categories();
}
} // namespace PV
