// Small DSP building blocks (header only, allocation free)
#pragma once
#include "../Common.h"

namespace PV
{
/** Zavalishin TPT state-variable filter. */
struct SVF
{
    float g = 0.1f, k = 1.0f, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
    float lo = 0, bp = 0, hp = 0;

    void set (float cutoffHz, float res01, double sr) noexcept
    {
        const float f = juce::jlimit (15.0f, (float) sr * 0.47f, cutoffHz);
        g = std::tan (kPi * f / (float) sr);
        k = 2.0f - 1.96f * juce::jlimit (0.0f, 1.0f, res01);          // res 0 -> Q .5, res 1 -> Q ~25
        a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    void setQ (float cutoffHz, float q, double sr) noexcept
    {
        const float f = juce::jlimit (15.0f, (float) sr * 0.47f, cutoffHz);
        g = std::tan (kPi * f / (float) sr);
        k = 1.0f / juce::jmax (0.3f, q);
        a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    inline void tick (float x) noexcept
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
        bp = v1; lo = v2; hp = x - k * v1 - v2;
    }
    inline float process (float x, int type) noexcept
    {
        tick (x);
        switch (type) { case 0: return lo; case 1: return bp * k; case 2: return hp; default: return lo + hp; }
    }
    void reset() noexcept { ic1 = ic2 = lo = bp = hp = 0; }
};

/** Exponential-ish ADSR with per-block coefficient setup. */
struct Env
{
    enum State { Idle, Atk, Dec, Sus, Rel };
    State s = Idle;
    float v = 0.0f;

    struct Coefs { float atkInc = 0.01f, decC = 0.999f, sus = 0.8f, relC = 0.999f; };

    static Coefs make (float atk, float dec, float sus, float rel, double sr) noexcept
    {
        Coefs c;
        c.atkInc = 1.0f / juce::jmax (1.0f, atk * (float) sr);
        c.decC   = std::exp (-4.6f / juce::jmax (1.0f, dec * (float) sr));
        c.relC   = std::exp (-6.9f / juce::jmax (1.0f, rel * (float) sr));
        c.sus    = sus;
        return c;
    }
    void on() noexcept  { s = Atk; }
    void off() noexcept { if (s != Idle) s = Rel; }
    void kill() noexcept { s = Idle; v = 0; }
    bool active() const noexcept { return s != Idle; }

    inline float tick (const Coefs& c) noexcept
    {
        switch (s)
        {
            case Atk: v += c.atkInc; if (v >= 1.0f) { v = 1.0f; s = Dec; } break;
            case Dec: v = c.sus + (v - c.sus) * c.decC; if (v - c.sus < 1.0e-4f) { v = c.sus; s = Sus; } break;
            case Sus: v = c.sus; break;
            case Rel: v *= c.relC; if (v < 1.0e-4f) { v = 0; s = Idle; } break;
            default: break;
        }
        return v;
    }
};

inline float polyBlep (float t, float dt) noexcept
{
    if (t < dt)         { t /= dt;           return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt)  { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

inline float readTable (const float* t, int mask, float phase01) noexcept
{
    const float x = phase01 * (float) (mask + 1);
    const int i = (int) x;
    const float f = x - (float) i;
    return t[i & mask] + (t[(i + 1) & mask] - t[i & mask]) * f;
}

/** MIDI note -> Hz table; can be replaced by a .scl/.tun tuning. */
struct Tuning
{
    std::array<float, 128> hz {};
    Tuning() { reset(); }
    void reset() { for (int n = 0; n < 128; ++n) hz[(size_t) n] = 440.0f * std::exp2 (((float) n - 69.0f) / 12.0f); }
    /** Loads a Scala .scl file; 1/1 is mapped to MIDI note 60 (C4 = 261.6256 Hz). */
    bool loadScl (const juce::File& f)
    {
        juce::StringArray lines; lines.addLines (f.loadFileAsString());
        std::vector<double> cents; int expected = -1; bool gotCount = false;
        for (auto l : lines)
        {
            l = l.trim();
            if (l.startsWith ("!") ) continue;
            if (expected < 0 && ! gotCount) { gotCount = true; continue; }       // description line
            if (expected < 0) { expected = l.getIntValue(); continue; }
            if ((int) cents.size() >= expected) break;
            auto tok = l.upToFirstOccurrenceOf (" ", false, false).trim();
            if (tok.contains (".")) cents.push_back (tok.getDoubleValue());
            else if (tok.contains ("/")) cents.push_back (1200.0 * std::log2 (tok.upToFirstOccurrenceOf ("/", false, false).getDoubleValue()
                                                                              / juce::jmax (1.0, tok.fromFirstOccurrenceOf ("/", false, false).getDoubleValue())));
            else cents.push_back (1200.0 * std::log2 (juce::jmax (1.0, tok.getDoubleValue())));
        }
        if (cents.empty() || (int) cents.size() != expected) return false;
        const double period = cents.back(); const int n = (int) cents.size();
        for (int note = 0; note < 128; ++note)
        {
            const int rel = note - 60; const int oct = (int) std::floor ((double) rel / n); const int deg = rel - oct * n;
            const double c = oct * period + (deg == 0 ? 0.0 : cents[(size_t) deg - 1]);
            hz[(size_t) note] = (float) (261.6256 * std::pow (2.0, c / 1200.0));
        }
        return true;
    }
};

/** RBJ biquad (used by EQ). */
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    enum Type { LowShelf, Peak, HighShelf };
    void set (Type t, float f, float q, float gainDb, double sr) noexcept
    {
        const float A = std::pow (10.0f, gainDb / 40.0f), w = kTwoPi * f / (float) sr, cs = std::cos (w), sn = std::sin (w);
        const float al = sn / (2.0f * q);
        float a0 = 1, B0, B1, B2, A1, A2;
        if (t == Peak) { B0 = 1 + al * A; B1 = -2 * cs; B2 = 1 - al * A; a0 = 1 + al / A; A1 = -2 * cs; A2 = 1 - al / A; }
        else
        {
            const float sq = 2.0f * std::sqrt (A) * al;
            if (t == LowShelf) { B0 = A * ((A + 1) - (A - 1) * cs + sq); B1 = 2 * A * ((A - 1) - (A + 1) * cs); B2 = A * ((A + 1) - (A - 1) * cs - sq);
                                 a0 = (A + 1) + (A - 1) * cs + sq; A1 = -2 * ((A - 1) + (A + 1) * cs); A2 = (A + 1) + (A - 1) * cs - sq; }
            else               { B0 = A * ((A + 1) + (A - 1) * cs + sq); B1 = -2 * A * ((A - 1) + (A + 1) * cs); B2 = A * ((A + 1) + (A - 1) * cs - sq);
                                 a0 = (A + 1) - (A - 1) * cs + sq; A1 = 2 * ((A - 1) - (A + 1) * cs); A2 = (A + 1) - (A - 1) * cs - sq; }
        }
        b0 = B0 / a0; b1 = B1 / a0; b2 = B2 / a0; a1 = A1 / a0; a2 = A2 / a0;
    }
    inline float process (float x) noexcept
    {
        const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y;
    }
};
} // namespace PV
