#include "Arp.h"

namespace PV
{
void Arp::reset() { nHeld = 0; seqLen = 0; nOffs = 0; lastIdx = -1; freeIdx = 0; freeToNext = 0; dirty = true; wasOn = false; }

void Arp::rebuild (const ArpParams& p)
{
    // sorted (or as-played) base list expanded over octaves
    int base[64]; const int n = nHeld;
    for (int i = 0; i < n; ++i) base[i] = held[i];
    if (p.pattern != 5) std::sort (base, base + n);
    int tmp[256]; int m = 0;
    for (int o = 0; o < juce::jlimit (1, 4, p.octaves); ++o)
        for (int i = 0; i < n && m < 256; ++i) tmp[m++] = juce::jlimit (0, 127, base[i] + 12 * o);
    seqLen = 0;
    if (p.pattern == 6)                                                         // converge: outside -> in
        for (int lo = 0, hi = m - 1; lo <= hi; ++lo, --hi) { seq[seqLen++] = tmp[lo]; if (hi != lo) seq[seqLen++] = tmp[hi]; }
    else for (int i = 0; i < m; ++i) seq[seqLen++] = tmp[i];
    lastP = p; dirty = false;
}

int Arp::collect (long long idx, const ArpParams& p, const ArpPattern& pat, Ev* out, int maxEv)
{
    if (seqLen == 0) return 0;
    const int len = juce::jlimit (1, kArpSteps, p.length);
    const ArpStep st = ArpStep::unpack (pat[(size_t) (idx % len)].load (std::memory_order_relaxed));
    if (! st.on) return 0;
    if (st.prob < 100 && rng.next() * 100.0f >= (float) st.prob) return 0;
    int notes[256]; int nn = 0;
    switch (p.pattern)
    {
        case 0: case 5: case 6: notes[nn++] = seq[(int) (idx % seqLen)]; break;                                   // up / as played / converge
        case 1: notes[nn++] = seq[seqLen - 1 - (int) (idx % seqLen)]; break;                                       // down
        case 2: { const int per = juce::jmax (1, seqLen * 2 - 2); const int k = (int) (idx % per); notes[nn++] = seq[k < seqLen ? k : per - k]; break; }
        case 3: notes[nn++] = seq[(int) (rng.next() * (float) seqLen) % seqLen]; break;                            // random
        default: for (int i = 0; i < seqLen && nn < 16; ++i) notes[nn++] = seq[i]; break;                          // chord
    }
    int count = 0; const int r = st.ratchet;
    for (int k = 0; k < r; ++k)
        for (int i = 0; i < nn && count < maxEv; ++i)
            out[count++] = { juce::jlimit (0, 127, notes[i] + st.pitch), st.vel, (double) k / r, (double) p.gate / r };
    return count;
}

void Arp::process (juce::MidiBuffer& in, juce::MidiBuffer& out, int numSamples, const ArpParams& p, const ArpPattern& pat,
                   double bpm, bool playing, double ppq, bool ppqValid)
{
    // --- 1. split input: notes feed the held list (when arp is on), the rest passes through
    juce::MidiBuffer pass;
    if (! p.on)
    {
        if (wasOn)       // arp just switched off: release whatever it was holding
        { for (int i = 0; i < nOffs; ++i) out.addEvent (juce::MidiMessage::noteOff (1, offs[i].note), 0); nOffs = 0; nHeld = 0; wasOn = false; }
        out.swapWith (in); return;
    }
    wasOn = true;
    for (const auto meta : in)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())
        {
            if (nHeld < 64) { held[nHeld] = m.getNoteNumber(); heldVel[nHeld] = m.getVelocity(); ++nHeld; dirty = true; }
            if (nHeld == 1) { freeToNext = 0; }                                // first note: start right away when free-running
        }
        else if (m.isNoteOff())
        {
            for (int i = 0; i < nHeld; ++i) if (held[i] == m.getNoteNumber()) { for (int j = i; j < nHeld - 1; ++j) { held[j] = held[j + 1]; heldVel[j] = heldVel[j + 1]; } --nHeld; dirty = true; break; }
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff()) { nHeld = 0; dirty = true; pass.addEvent (m, meta.samplePosition); }
        else pass.addEvent (m, meta.samplePosition);
    }
    if (dirty || lastP.pattern != p.pattern || lastP.octaves != p.octaves) rebuild (p);

    // --- 2. pending note-offs scheduled in earlier blocks
    int w = 0;
    for (int i = 0; i < nOffs; ++i)
    {
        if (offs[i].samples < numSamples) out.addEvent (juce::MidiMessage::noteOff (1, offs[i].note), juce::jmax (0, (int) offs[i].samples));
        else { offs[w] = offs[i]; offs[w].samples -= numSamples; ++w; }
    }
    nOffs = w;

    // --- 3. step triggers inside this block
    const double beats = kRateBeats[juce::jlimit (0, kNumRates - 1, p.rate)];
    const double stepSamples = beats * 60.0 / juce::jmax (20.0, bpm) * sr;
    const bool locked = playing && ppqValid;
    double toNext; long long idx;
    if (locked)
    {
        const double pos = ppq / beats; idx = (long long) std::ceil (pos - 1.0e-7);
        toNext = (idx - pos) * stepSamples; if (toNext < 0) toNext = 0;
    }
    else { toNext = freeToNext; idx = freeIdx; lastIdx = -1; }

    while (toNext < numSamples)
    {
        if (! locked || idx > lastIdx)
        {
            if (nHeld > 0)
            {
                Ev ev[256]; const int ne = collect (idx, p, pat, ev, 256);
                const int hv = heldVel[0];
                for (int e = 0; e < ne; ++e)
                {
                    const int pos = juce::jlimit (0, numSamples - 1, (int) (toNext + ev[e].start * stepSamples));
                    const int vel = juce::jlimit (1, 127, (ev[e].vel + hv) / 2);
                    out.addEvent (juce::MidiMessage::noteOn (1, ev[e].note, (juce::uint8) vel), pos);
                    if (nOffs < 256) offs[nOffs++] = { ev[e].note, (toNext + ev[e].start * stepSamples + juce::jmax (4.0, ev[e].len * stepSamples)) - numSamples };
                    // a note-off landing inside this very block is emitted right away
                    if (offs[nOffs - 1].samples < 0) { out.addEvent (juce::MidiMessage::noteOff (1, ev[e].note), juce::jlimit (0, numSamples - 1, pos + juce::jmax (4, (int) (ev[e].len * stepSamples)))); --nOffs; }
                }
            }
            lastIdx = idx;
        }
        ++idx; toNext += stepSamples;
    }
    if (! locked) { freeToNext = toNext - numSamples; freeIdx = idx; }
    // pending offs that were scheduled with "samples" relative to this block start: they already had numSamples subtracted
    out.addEvents (pass, 0, numSamples, 0);
}

juce::MidiMessageSequence Arp::render (const std::vector<int>& chord, const ArpParams& p, const ArpPattern& pat, int bars, int ppqn)
{
    Arp a; a.prepare (44100.0);
    for (int n : chord) { a.held[a.nHeld] = n; a.heldVel[a.nHeld] = 100; ++a.nHeld; }
    a.rebuild (p);
    juce::MidiMessageSequence seq;
    const double beats = kRateBeats[juce::jlimit (0, kNumRates - 1, p.rate)];
    const long long steps = (long long) std::floor (bars * 4.0 / beats + 1.0e-6);
    for (long long i = 0; i < steps; ++i)
    {
        Ev ev[256]; const int ne = a.collect (i, p, pat, ev, 256);
        for (int e = 0; e < ne; ++e)
        {
            const double t0 = (i + ev[e].start) * beats * ppqn, t1 = t0 + juce::jmax (4.0, ev[e].len * beats * ppqn);
            seq.addEvent (juce::MidiMessage::noteOn (1, ev[e].note, (juce::uint8) ev[e].vel), t0);
            seq.addEvent (juce::MidiMessage::noteOff (1, ev[e].note), t1);
        }
    }
    seq.updateMatchedPairs(); return seq;
}
} // namespace PV
