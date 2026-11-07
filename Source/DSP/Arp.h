// Arpeggiator: 7 patterns, 1-4 octaves, rates 1/1..1/64 incl. triplets & dotted, 32-step editor
// (per-step on/velocity/pitch offset/ratchet 1-8/probability). Host-PPQ locked while playing.
#pragma once
#include "../Common.h"

namespace PV
{
/** One step packed in a lock-free 32 bit word. */
struct ArpStep
{
    bool on = true; int vel = 100, pitch = 0, ratchet = 1, prob = 100;
    uint32_t pack() const { return (on ? 1u : 0u) | ((uint32_t) juce::jlimit (1, 127, vel) << 1) | ((uint32_t) (juce::jlimit (-24, 24, pitch) + 24) << 8)
                                 | ((uint32_t) juce::jlimit (1, 8, ratchet) << 14) | ((uint32_t) juce::jlimit (0, 100, prob) << 18); }
    static ArpStep unpack (uint32_t w) { ArpStep s; s.on = (w & 1u) != 0; s.vel = (int) ((w >> 1) & 127u); s.pitch = (int) ((w >> 8) & 63u) - 24; s.ratchet = (int) ((w >> 14) & 15u); s.prob = (int) ((w >> 18) & 127u); if (s.ratchet < 1) s.ratchet = 1; return s; }
};
using ArpPattern = std::array<std::atomic<uint32_t>, kArpSteps>;

struct ArpParams { bool on = false; int pattern = 0, rate = 4, octaves = 1, length = 16; float gate = 0.6f; };

class Arp
{
public:
    void prepare (double sampleRate) { sr = sampleRate; reset(); }
    void reset();

    /** Consumes note events from `in`, writes arpeggiated notes (and passes everything else) to `out`. */
    void process (juce::MidiBuffer& in, juce::MidiBuffer& out, int numSamples, const ArpParams& p, const ArpPattern& pat,
                  double bpm, bool playing, double ppq, bool ppqValid);

    /** Renders `bars` bars of the pattern for the given input chord into a MIDI sequence (for drag-out to the piano roll). */
    static juce::MidiMessageSequence render (const std::vector<int>& chord, const ArpParams& p, const ArpPattern& pat, int bars, int ppqn = 960);

private:
    struct Ev { int note; int vel; double start, len; };         // start/len as fraction of one step
    int collect (long long idx, const ArpParams& p, const ArpPattern& pat, Ev* out, int maxEv);
    void rebuild (const ArpParams& p);

    double sr = 44100.0;
    int held[64] {}, heldVel[64] {}, nHeld = 0;
    int seq[256] {}, seqLen = 0; bool dirty = true; ArpParams lastP;
    struct Off { int note; double samples; };
    Off offs[256] {}; int nOffs = 0;
    long long lastIdx = -1, freeIdx = 0; double freeToNext = 0;
    Rng rng { 4242 };
    bool wasOn = false;
    friend struct ArpRenderer;
};
} // namespace PV
