#include "Engines.h"

namespace PV
{
namespace
{
inline void panGains (float pan, float& l, float& r)   // constant power, unity at centre
{
    const float a = (juce::jlimit (-1.0f, 1.0f, pan) * 0.5f + 0.5f) * kPi * 0.5f;
    l = std::cos (a) * 1.41421356f; r = std::sin (a) * 1.41421356f;
}
inline float wrap01 (float x) { return x - std::floor (x); }
}

// ================================================================================== ANALOG (VA)
void renderAnalog (LayerVoice& v, const LayerParams& p, const RenderCtx& c, float f, float* L, float* R, int n)
{
    auto& s = v.va;
    const int maxUni = c.quality == 0 ? 3 : (c.quality == 1 ? 9 : 16);
    int uni = juce::jlimit (1, maxUni, p.uni); if (p.type == 4) uni = juce::jmax (uni, juce::jmin (5, maxUni));
    const float sr = (float) c.sr, pw = 0.08f + 0.84f * p.p[0], ratio = 1.0f + p.p[1] * 3.0f, subLvl = p.p[2] * 0.7f, fmAmt = p.p[3] * 0.35f;
    const bool sync = ratio > 1.02f;
    float inc[16], gl[16], gr[16];
    for (int u = 0; u < uni; ++u)
    {
        const float t = uni > 1 ? ((float) u / (float) (uni - 1)) * 2.0f - 1.0f : 0.0f;
        inc[u] = f * semis2ratio (t * p.detune * 0.6f) / sr;
        panGains (t * p.spread, gl[u], gr[u]);
        const float nrm = 1.0f / std::sqrt ((float) uni);
        gl[u] *= nrm * 0.5f; gr[u] *= nrm * 0.5f;
    }
    if (! s.init) { Rng r ((uint32_t) v.note * 31u + 7u); for (int u = 0; u < 16; ++u) { s.ph[u] = r.next(); s.sp[u] = 0; } s.init = true; }
    const int type = p.type == 4 ? 0 : p.type;
    for (int i = 0; i < n; ++i)
    {
        float sl = 0, sR = 0;
        s.fmPh = wrap01 (s.fmPh + f * 2.0f / sr);
        const float fm = fmAmt > 0.001f ? std::sin (kTwoPi * s.fmPh) * fmAmt : 0.0f;
        for (int u = 0; u < uni; ++u)
        {
            float& ph = s.ph[u]; ph += inc[u]; bool wrapped = false; if (ph >= 1.0f) { ph -= 1.0f; wrapped = true; }
            float x, dt = inc[u], q = ph;
            if (sync) { if (wrapped) s.sp[u] = 0; s.sp[u] += inc[u] * ratio; if (s.sp[u] >= 1) s.sp[u] -= 1; q = s.sp[u]; dt = inc[u] * ratio; }
            if (fm != 0.0f) q = wrap01 (q + fm);
            switch (type)
            {
                case 0: x = 2.0f * q - 1.0f - polyBlep (q, dt); break;
                case 1: x = (q < pw ? 1.0f : -1.0f) + polyBlep (q, dt) - polyBlep (wrap01 (q - pw + 1.0f), dt); break;
                case 2: x = 4.0f * std::abs (q - 0.5f) - 1.0f; break;
                case 3: x = std::sin (kTwoPi * q); break;
                default: x = v.org.rng.bi() * 0.7f; break;      // noise
            }
            sl += x * gl[u]; sR += x * gr[u];
        }
        if (subLvl > 0.001f) { s.subPh = wrap01 (s.subPh + f * 0.5f / sr); const float sub = std::sin (kTwoPi * s.subPh) * subLvl * 0.5f; sl += sub; sR += sub; }
        L[i] = sl; R[i] = sR;
    }
}

// ================================================================================= WAVETABLE
void renderWavetable (LayerVoice& v, const LayerParams& p, const RenderCtx& c, float f, float* L, float* R, int n)
{
    auto& lib = Library::get();
    if (! lib.isReady()) { std::fill (L, L + n, 0.f); std::fill (R, R + n, 0.f); return; }
    auto& s = v.wt;
    const int maxUni = c.quality == 0 ? 4 : (c.quality == 1 ? 10 : 16);
    const int uni = juce::jlimit (1, maxUni, p.uni);
    const float sr = (float) c.sr;
    const int fam = juce::jlimit (0, Library::kFamilies - 1, p.type);
    const float fpos = p.p[0] * (float) (Library::kFrames - 1) * 0.9999f;
    const int f0 = juce::jmin (Library::kFrames - 2, (int) fpos); const float fr = fpos - (float) f0;
    const float fMax = f * semis2ratio (p.detune * 0.6f) * 1.02f;
    const float needed = juce::jmax (1.0f, 0.5f * sr / fMax);
    const int mip = juce::jlimit (0, Library::kMips - 1, (int) std::ceil (std::log2 (1023.0f / needed)));
    const float* t0 = lib.table (fam, f0, mip); const float* t1 = lib.table (fam, f0 + 1, mip);
    const int mode = juce::jlimit (0, 5, (int) (p.p[2] * 5.999f)); const float amt = p.p[1];

    float inc[16], gl[16], gr[16];
    for (int u = 0; u < uni; ++u)
    {
        const float t = uni > 1 ? ((float) u / (float) (uni - 1)) * 2.0f - 1.0f : 0.0f;
        inc[u] = f * semis2ratio (t * p.detune * 0.6f) / sr;
        panGains (t * p.spread, gl[u], gr[u]);
        const float nrm = 1.0f / std::sqrt ((float) uni);
        gl[u] *= nrm * 0.55f; gr[u] *= nrm * 0.55f;
    }
    if (! s.init) { Rng r ((uint32_t) v.note * 17u + 3u); for (int u = 0; u < 16; ++u) s.ph[u] = r.next(); s.init = true; }
    const float pwm = 0.5f - 0.45f * amt;
    for (int i = 0; i < n; ++i)
    {
        float sl = 0, sR = 0;
        for (int u = 0; u < uni; ++u)
        {
            float& ph = s.ph[u]; ph += inc[u]; if (ph >= 1.0f) ph -= 1.0f;
            float q = ph;
            switch (mode)
            {
                case 1: q = std::pow (ph, 1.0f + 3.0f * amt); break;                                          // bend
                case 2: q = wrap01 (ph * (1.0f + 7.0f * amt)); break;                                        // sync
                case 3: q = lerpf (ph, ph < 0.5f ? ph * 2.0f : 2.0f - 2.0f * ph, amt); break;               // mirror
                case 4: q = wrap01 (ph + amt * 0.5f * std::sin (kTwoPi * ph * 3.0f)); break;                 // fm
                case 5: q = ph < pwm ? ph * 0.5f / pwm : 0.5f + (ph - pwm) * 0.5f / (1.0f - pwm); break;      // pwm
                default: break;
            }
            const float x = lerpf (readTable (t0, 2047, q), readTable (t1, 2047, q), fr);
            sl += x * gl[u]; sR += x * gr[u];
        }
        L[i] = sl; R[i] = sR;
    }
}

// ================================================================================== ORGANIC
namespace
{
static const float kBell[]   = { 1.0f, 2.0f, 2.76f, 4.07f, 5.40f, 6.85f, 8.93f, 11.3f, 13.34f, 16.0f, 18.64f, 22.f };
static const float kGlass[]  = { 1.0f, 2.32f, 4.25f, 6.63f, 9.38f, 12.5f, 16.0f, 20.0f };
static const float kKalimba[]= { 1.0f, 5.4f, 13.3f, 2.0f, 3.0f };
static const float kBox[]    = { 1.0f, 2.756f, 5.404f, 8.933f, 13.344f, 18.64f };

void setupModal (OrgState& o, int type, float f0, float vel, const float* p, double sr, int maxP)
{
    const float bright = p[0], ring = p[1], motion = p[3];
    const float octs = std::log2 (juce::jmax (20.0f, f0) / 261.63f);
    int np = 0; float ratio[OrgState::MaxP] {}, amp[OrgState::MaxP] {}, T[OrgState::MaxP] {};
    switch (type)
    {
        case 0: // felt / ambient piano: inharmonic string partials, hammer comb, felt low-pass
        {
            np = juce::jmin (12, maxP); const float B = 0.00025f + 0.0005f * std::max (0.0f, octs + 1.0f);
            const float b = juce::jlimit (0.0f, 1.0f, bright * 0.55f + vel * 0.45f);
            const float T0 = (1.2f + ring * 10.0f) * std::exp2 (-octs * 0.75f);
            for (int k = 1; k <= np; ++k)
            {
                ratio[k - 1] = (float) k * std::sqrt (1.0f + B * (float) (k * k));
                amp[k - 1] = std::pow ((float) k, -(1.3f - 0.9f * b)) * std::exp (-(float) k * (0.5f - 0.42f * b)) * (0.55f + 0.45f * std::abs (std::sin (kPi * (float) k / 7.5f)));
                T[k - 1] = T0 / (1.0f + 0.45f * (float) k);
            }
            break;
        }
        case 2: case 3: case 4: case 6:
        {
            const float* R = type == 2 ? kBell : type == 3 ? kGlass : type == 4 ? kKalimba : kBox;
            np = juce::jmin (type == 2 ? 12 : type == 3 ? 8 : type == 4 ? 5 : 6, maxP);
            const float base = type == 2 ? 2.5f + ring * 12.0f : type == 3 ? 3.0f + ring * 16.0f : type == 4 ? 0.5f + ring * 3.5f : 1.0f + ring * 5.0f;
            for (int k = 0; k < np; ++k)
            {
                ratio[k] = R[k];
                amp[k] = (1.0f / (1.0f + (0.9f - 0.7f * bright) * (float) k * (type == 4 ? 1.6f : 1.0f))) * (k == 0 ? 1.0f : (0.45f + 0.55f * vel));
                T[k] = base * std::exp2 (-octs * 0.5f) / (1.0f + (type == 3 ? 0.15f : type == 4 ? 1.1f : 0.45f) * (float) k);
            }
            break;
        }
        default: break;
    }
    o.np = 0;
    for (int k = 0; k < np; ++k)
    {
        const float fk = f0 * ratio[k] * (1.0f + motion * 0.0025f * o.rng.bi());
        if (fk > 0.45f * (float) sr) continue;
        const int i = o.np++;
        const float w = kTwoPi * fk / (float) sr;
        o.c[i] = std::cos (w); o.s[i] = std::sin (w);
        o.r[i] = std::exp (-6.9f / juce::jmax (8.0f, T[k] * (float) sr));
        o.x[i] = amp[k] * 0.35f; o.y[i] = 0.0f;
    }
    o.modal = true;
}

struct Vowel { float f[3]; float a[3]; };
static const Vowel kVowels[5] = { { { 650, 1080, 2650 }, { 1.0f, 0.50f, 0.30f } },    // Ah
                                  { { 400,  800, 2600 }, { 1.0f, 0.45f, 0.20f } },    // Oh
                                  { { 350,  600, 2700 }, { 1.0f, 0.35f, 0.15f } },    // Oo
                                  { { 250, 1000, 2300 }, { 0.8f, 0.25f, 0.08f } },    // Mm
                                  { { 290, 2150, 3100 }, { 1.0f, 0.60f, 0.30f } } };  // Ee
}

void renderOrganic (LayerVoice& v, const LayerParams& p, const RenderCtx& c, float f, bool first, float* L, float* R, int n)
{
    auto& o = v.org; const float sr = (float) c.sr;
    const int type = p.type;
    const int maxP = c.quality == 0 ? 6 : (c.quality == 1 ? 10 : OrgState::MaxP);

    if (type == 0 || type == 2 || type == 3 || type == 4 || type == 6)          // ---- modal (piano, bells, glass, kalimba, music box)
    {
        if (first) setupModal (o, type, f, v.vel, p.p, c.sr, maxP);
        const float gain = 0.55f; const float sprd = p.spread * 0.15f;
        for (int i = 0; i < n; ++i)
        {
            float sum = 0, alt = 0;
            for (int k = 0; k < o.np; ++k)
            {
                const float nx = o.r[k] * (o.x[k] * o.c[k] - o.y[k] * o.s[k]);
                const float ny = o.r[k] * (o.x[k] * o.s[k] + o.y[k] * o.c[k]);
                o.x[k] = nx; o.y[k] = ny; sum += ny; if (k & 1) alt += ny;
            }
            L[i] = (sum - alt * sprd * 2.0f) * gain; R[i] = (sum + alt * sprd * 2.0f) * gain;
        }
        return;
    }
    if (type == 7)                                                             // ---- user sample playback (start offset, reverse, loop)
    {
        const auto* buf = Library::get().userSample();
        if (buf == nullptr || buf->data.empty()) { std::fill (L, L + n, 0.f); std::fill (R, R + n, 0.f); return; }
        const float N = (float) buf->data.size(); const float rate = (f / buf->root) * (float) (buf->rate / c.sr);
        const bool rev = p.p[3] > 0.5f, loop = p.p[2] > 0.5f;
        if (first) o.samplePos = rev ? N - 1.0f - p.start * N : p.start * N;
        for (int i = 0; i < n; ++i)
        {
            float x = 0;
            if (o.samplePos >= 0 && o.samplePos < N - 1)
            { const int ix = (int) o.samplePos; const float fr = o.samplePos - (float) ix; x = buf->data[(size_t) ix] + (buf->data[(size_t) ix + 1] - buf->data[(size_t) ix]) * fr; }
            o.samplePos += rev ? -rate : rate;
            if (loop) { if (o.samplePos >= N - 1) o.samplePos -= N - 1; if (o.samplePos < 0) o.samplePos += N - 1; }
            L[i] = R[i] = x * 0.7f;
        }
        return;
    }
    // ---- choir (type 1) and strings (type 5): real-time oscillators
    const float motion = p.p[3];
    const float detune = type == 1 ? 0.0025f + 0.008f * motion : 0.003f + 0.01f * motion;
    float inc[3];
    for (int k = 0; k < 3; ++k) inc[k] = f * (1.0f + detune * ((float) k - 1.0f)) / sr;
    const float vibRate = 5.0f + 0.5f * (float) (v.note % 5);
    float A[3] = { 0, 0, 0 };
    if (type == 1)
    {
        const float pos = juce::jlimit (0.0f, 0.9999f, p.p[2]) * 4.0f; const int i0 = (int) pos; const float fr = pos - (float) i0;
        const float vt = 0.8f + 0.55f * p.p[4];                         // voice size: bass/male .. female .. boys
        for (int k = 0; k < 3; ++k)
        {
            const float F = lerpf (kVowels[i0].f[k], kVowels[i0 + 1].f[k], fr) * vt; A[k] = lerpf (kVowels[i0].a[k], kVowels[i0 + 1].a[k], fr);
            o.fl[k].setQ (F, 7.0f + 3.0f * (float) k, c.sr); o.fr[k].g = o.fl[k].g; o.fr[k].k = o.fl[k].k; o.fr[k].a1 = o.fl[k].a1; o.fr[k].a2 = o.fl[k].a2; o.fr[k].a3 = o.fl[k].a3;
        }
    }
    const float lpC = type == 5 ? std::exp (-kTwoPi * (400.0f + 9000.0f * p.p[0]) / sr) : 0.0f;
    for (int i = 0; i < n; ++i)
    {
        o.vibPh += vibRate / sr; if (o.vibPh >= 1.0f) o.vibPh -= 1.0f;
        const float vib = 1.0f + std::sin (kTwoPi * o.vibPh) * (0.003f + 0.006f * motion);
        float s[3];
        for (int k = 0; k < 3; ++k)
        {
            const float d = inc[k] * vib; o.ph[k] += d; if (o.ph[k] >= 1.0f) o.ph[k] -= 1.0f;
            s[k] = 2.0f * o.ph[k] - 1.0f - polyBlep (o.ph[k], d);
        }
        float l, r;
        if (type == 1)
        {
            const float br = o.rng.bi() * 0.02f * (0.4f + p.p[0]);
            const float inL = 0.6f * s[0] + 0.5f * s[1] + br, inR = 0.6f * s[2] + 0.5f * s[1] + br;
            l = r = 0;
            for (int k = 0; k < 3; ++k)
            {
                o.fl[k].tick (inL); o.fr[k].tick (inR);
                l += o.fl[k].bp * o.fl[k].k * A[k]; r += o.fr[k].bp * o.fr[k].k * A[k];
            }
            const float w = p.spread * 0.6f; const float m = 0.5f * (l + r);
            l = lerpf (m, l, 0.4f + w) * 1.1f; r = lerpf (m, r, 0.4f + w) * 1.1f;
        }
        else
        {
            const float bow = o.rng.bi() * 0.01f;
            const float l0 = 0.45f * s[0] + 0.3f * s[1] + bow, r0 = 0.45f * s[2] + 0.3f * s[1] + bow;
            o.lp = lpC * o.lp + (1.0f - lpC) * l0; l = o.lp * 0.8f; o.ens = lpC * o.ens + (1.0f - lpC) * r0; r = o.ens * 0.8f;
        }
        L[i] = l; R[i] = r;
    }
}

// ================================================================================= GRANULAR
void renderGranular (LayerVoice& v, const LayerParams& p, const RenderCtx& c, float f, float* L, float* R, int n)
{
    auto& lib = Library::get();
    std::fill (L, L + n, 0.f); std::fill (R, R + n, 0.f);
    if (! lib.isReady()) return;
    int t = p.type; if (t >= 5) t -= 5;
    const Library::Buf* src = t == 4 ? lib.userSample() : lib.source (juce::jlimit (0, Library::kSources - 1, t));
    if (src == nullptr || src->data.empty()) return;
    auto& g = v.gr; const float sr = (float) c.sr, N = (float) src->data.size();
    static float window[1025]; static std::atomic<bool> winInit { false };
    if (! winInit.load()) { for (int i = 0; i <= 1024; ++i) window[i] = 0.5f - 0.5f * std::cos (kTwoPi * (float) i / 1024.0f); winInit.store (true); }

    const int maxGrains = c.quality == 0 ? 6 : (c.quality == 1 ? 12 : 16);
    const float sizeSec = 0.012f * std::pow (35.0f, p.p[0]);               // 12ms .. 420ms
    const float density = 2.0f * std::pow (40.0f, p.p[1]);                  // 2 .. 80 grains / s
    const float spray = p.p[2], pitchRnd = p.p[3] * 7.0f, revProb = p.p[4];
    const float scan = (p.p[5] - 0.5f) * 2.0f;                              // -1 .. +1 buffer lengths per 4 beats, 0 = freeze
    const float scanPerSample = scan / (float) (4.0 * 60.0 / juce::jmax (30.0, c.bpm) * c.sr);
    const float basePitch = (f / src->root) * (float) (src->rate / c.sr);
    const float norm = 0.9f / std::sqrt (juce::jmax (1.0f, density * sizeSec));
    const int interval = juce::jmax (1, (int) (sr / density));

    for (int i = 0; i < n; ++i)
    {
        g.scan += scanPerSample; g.scan -= std::floor (g.scan);
        if (g.spawn <= 0.0f)
        {
            g.spawn += (float) interval * (1.0f + 0.4f * spray * g.rng.bi());
            for (int k = 0; k < maxGrains; ++k)
            {
                auto& gr = g.g[k]; if (gr.active) continue;
                const float startNorm = wrap01 (p.start + g.scan + spray * 0.5f * g.rng.bi());
                float inc = basePitch * semis2ratio (pitchRnd * g.rng.bi());
                const bool rev = g.rng.next() < revProb; if (rev) inc = -inc;
                gr.pos = startNorm * N; gr.inc = inc; gr.age = 0; gr.len = juce::jmax (32, (int) (sizeSec * sr)); gr.active = true;
                const float pan = p.spread * g.rng.bi(); panGains (pan, gr.gl, gr.gr);
                break;
            }
        }
        g.spawn -= 1.0f;
        float sl = 0, sR = 0;
        for (int k = 0; k < maxGrains; ++k)
        {
            auto& gr = g.g[k]; if (! gr.active) continue;
            float pos = gr.pos; while (pos >= N) pos -= N; while (pos < 0) pos += N;
            const int ix = (int) pos; const int ix2 = ix + 1 >= (int) N ? 0 : ix + 1; const float fr = pos - (float) ix;
            const float x = src->data[(size_t) ix] + (src->data[(size_t) ix2] - src->data[(size_t) ix]) * fr;
            const float w = window[juce::jmin (1024, (gr.age * 1024) / gr.len)];
            sl += x * w * gr.gl; sR += x * w * gr.gr; gr.pos = pos + gr.inc;
            if (++gr.age >= gr.len) gr.active = false;
        }
        L[i] = sl * norm * 0.5f; R[i] = sR * norm * 0.5f;
    }
}

// ================================================================================= SPECTRAL
void renderSpectral (LayerVoice& v, const LayerParams& p, const RenderCtx& c, float f, bool first, float* L, float* R, int n)
{
    auto& s = v.sp; const float sr = (float) c.sr;
    static const float bar[12] = { 1.0f, 2.76f, 5.40f, 8.93f, 13.34f, 18.64f, 25.0f, 32.2f, 40.5f, 50.0f, 60.5f, 71.5f };
    const int maxP = c.quality == 0 ? 6 : (c.quality == 1 ? 9 : 12);
    if (first || ! s.init)
    {
        s.np = 0;
        for (int k = 1; k <= maxP; ++k)
        {
            const float r = lerpf ((float) k, bar[k - 1], p.p[0]) * (1.0f + p.p[3] * 0.02f * s.rng.bi());
            s.ratio[s.np] = r; s.amp[s.np] = std::pow ((float) k, -(1.6f - 1.4f * p.p[2])); ++s.np;
        }
        s.init = true; s.lastF = 0; s.clickEnv = 1.0f;
    }
    const float T = 0.08f + p.p[1] * 8.0f;
    if (std::abs (f - s.lastF) > f * 0.0004f)
    {
        for (int k = 0; k < s.np; ++k)
        {
            const float fk = f * s.ratio[k]; if (fk > 0.45f * sr) { s.amp[k] = 0; continue; }
            s.bp[k].setQ (fk, juce::jlimit (6.0f, 3000.0f, kPi * fk * T), c.sr);
        }
        s.lastF = f;
    }
    const float inc = f / sr, clickDec = std::exp (-1.0f / (0.004f * sr));
    for (int i = 0; i < n; ++i)
    {
        float ex;
        if (p.type == 0 || p.type == 3 || p.type == 6) ex = s.rng.bi() * 0.5f;
        else if (p.type == 1 || p.type == 4 || p.type == 7) { s.ph += inc; if (s.ph >= 1) s.ph -= 1; ex = (2.0f * s.ph - 1.0f - polyBlep (s.ph, inc)) * 0.4f; }
        else { s.clickEnv *= clickDec; ex = s.rng.bi() * (s.clickEnv * 3.0f + 0.01f); }
        float y = 0, y2 = 0;
        for (int k = 0; k < s.np; ++k)
        {
            s.bp[k].tick (ex);
            const float o = s.bp[k].bp * s.bp[k].k * s.amp[k]; y += o; if (k & 1) y2 += o;
        }
        const float w = p.spread * 0.5f;
        L[i] = (y - y2 * w) * 0.45f; R[i] = (y + y2 * w) * 0.45f;
    }
}
} // namespace PV
