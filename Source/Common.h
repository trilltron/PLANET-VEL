// PLANET VEL by Trill Tron - common includes and tiny helpers
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_events/juce_events.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <atomic>
#include <cmath>
#include <map>
#include <memory>
#include <vector>

namespace PV
{
constexpr int   kLayers     = 4;
constexpr int   kMaxVoices  = 64;
constexpr int   kRouteSlots = 32;
constexpr int   kArpSteps   = 32;
constexpr int   kSubBlock   = 32;      // control-rate granularity of the synth core
constexpr int   kNumMacros  = 8;
constexpr float kPi         = 3.14159265358979323846f;
constexpr float kTwoPi      = 6.28318530717958647692f;

/** UTF-8 literal -> juce::String (juce::String(const char*) assumes ASCII). */
inline juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

inline float fastTanh (float x) noexcept
{
    x = juce::jlimit (-3.0f, 3.0f, x);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}
inline float lerpf (float a, float b, float t) noexcept { return a + (b - a) * t; }
inline float semis2ratio (float s) noexcept { return std::exp2 (s * (1.0f / 12.0f)); }

/** Tiny allocation-free LCG random generator (audio thread safe). */
struct Rng
{
    uint32_t s = 22222u;
    Rng() = default;
    explicit Rng (uint32_t seed) : s (seed ? seed : 1u) {}
    inline uint32_t u32() noexcept { s = s * 1664525u + 1013904223u; return s; }
    inline float next() noexcept { return (float) (u32() >> 8) * (1.0f / 16777216.0f); }   // [0,1)
    inline float bi() noexcept   { return next() * 2.0f - 1.0f; }                          // [-1,1)
};

static const char* const kMacroNames[kNumMacros] = { "TIDE", "MORPH", "SPACE", "GRIT", "WARP", "AIR", "DRIFT", "GLITCH" };
static const char* const kRateNames[] = { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64",
                                          "1/4T", "1/8T", "1/16T", "1/32T", "1/4D", "1/8D", "1/16D", "1/32D" };
static const float kRateBeats[] = { 4.f, 2.f, 1.f, .5f, .25f, .125f, .0625f,
                                    2.f/3.f, 1.f/3.f, 1.f/6.f, 1.f/12.f, 1.5f, .75f, .375f, .1875f };
constexpr int kNumRates = 15;
} // namespace PV
