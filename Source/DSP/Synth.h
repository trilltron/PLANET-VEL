// Voice manager + 4-layer renderer.
#pragma once
#include "Engines.h"

namespace PV
{
struct Voice
{
    bool active = false, gate = false, sustained = false;
    int note = -1; float vel = 0; uint64_t age = 0;
    LayerVoice lv[kLayers];
};

class SynthEngine
{
public:
    void prepare (double sampleRate) { sr = sampleRate; allNotesOff (true); }
    void setPolyphony (int n) { maxVoices = juce::jlimit (1, kMaxVoices, n); }

    void noteOn (int note, float vel, const LayerParams (&lp)[kLayers]);
    void noteOff (int note);
    void sustain (bool down);
    void allNotesOff (bool kill);
    int  activeVoices() const noexcept { int n = 0; for (auto& v : voices) n += v.active ? 1 : 0; return n; }

    /** Adds n (<= kSubBlock) samples of every layer to outL/outR[layer]. */
    void render (const LayerParams (&lp)[kLayers], const RenderCtx& ctx, float* const outL[kLayers], float* const outR[kLayers], int n);

private:
    double sr = 44100.0;
    int maxVoices = 24;
    uint64_t counter = 0;
    bool pedal = false;
    std::array<Voice, kMaxVoices> voices;
};
} // namespace PV
