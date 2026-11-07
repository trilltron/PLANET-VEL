// The five layer engines (Organic / Wavetable / Granular / Analog / Spectral) and per-layer voice state.
// All render functions are allocation free and lock free; they write (overwrite) n <= kSubBlock samples.
#pragma once
#include "Dsp.h"
#include "Library.h"
#include "../Params.h"

namespace PV
{
struct LayerParams            // snapshot of one layer's (modulated) parameters, refreshed every kSubBlock samples
{
    int   engine = ENG_WAVETABLE, type = 0, ftype = 0, uni = 1, keylo = 0, keyhi = 127;
    bool  enabled = false;
    float level = 0.8f, pan = 0, tune = 0, start = 0, cutoff = 12000, reso = 0.1f, drive = 0, keytrk = 0.3f, fenv = 0, lfo = 0;
    float spread = 0.5f, send = 0.5f, detune = 0.25f, p[6] = { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
    Env::Coefs env;
    float orbitW = 1.0f;      // orbit blend weight
};

struct RenderCtx
{
    double sr = 44100.0, bpm = 120.0;
    int    quality = 1;            // 0 ECO, 1 NORMAL, 2 HQ
    float  lfo1 = 0, lfo2 = 0, bend = 0;
    const Tuning* tuning = nullptr;
};

struct VAState   { float ph[16] {}, sp[16] {}, subPh = 0, fmPh = 0; bool init = false; };
struct WTState   { float ph[16] {}; bool init = false; };
struct OrgState
{
    static constexpr int MaxP = 14;
    float x[MaxP] {}, y[MaxP] {}, c[MaxP] {}, s[MaxP] {}, r[MaxP] {}; int np = 0; bool modal = false;
    float ph[3] {}, vibPh = 0, lp = 0, samplePos = 0, ens = 0; SVF fl[3], fr[3];
    Rng rng;
};
struct Grain { float pos = 0, inc = 0, gl = 0, gr = 0; int age = 0, len = 0; bool active = false; };
struct GrainState { Grain g[16]; float spawn = 0, scan = 0; Rng rng; };
struct SpecState
{
    SVF bp[12]; float ratio[12] {}, amp[12] {}, lastF = 0, ph = 0, clickEnv = 1.0f; Rng rng; int np = 0; bool init = false;
};

struct LayerVoice
{
    Env   env;
    SVF   fl, fr;
    float gL = 0, gR = 0;             // ramped output gains
    float velGain = 1, vel = 1; bool fresh = true;
    int   note = 60;
    VAState va; WTState wt; OrgState org; GrainState gr; SpecState sp;

    void start (int n, float v, uint32_t seed)
    {
        note = n; vel = v; velGain = 0.25f + 0.75f * std::pow (v, 0.8f);
        va = {}; wt = {}; org = {}; gr = {}; sp = {};
        org.rng = Rng (seed * 7u + 1u); gr.rng = Rng (seed * 13u + 5u); sp.rng = Rng (seed * 29u + 3u);
        fl.reset(); fr.reset(); gL = gR = -1.0f; fresh = true;      // -1 => "snap on first block"
        env.kill(); env.on();
    }
};

// engine entry points -----------------------------------------------------------------------------
void renderOrganic  (LayerVoice&, const LayerParams&, const RenderCtx&, float freq, bool firstBlock, float* L, float* R, int n);
void renderWavetable(LayerVoice&, const LayerParams&, const RenderCtx&, float freq, float* L, float* R, int n);
void renderGranular (LayerVoice&, const LayerParams&, const RenderCtx&, float freq, float* L, float* R, int n);
void renderAnalog   (LayerVoice&, const LayerParams&, const RenderCtx&, float freq, float* L, float* R, int n);
void renderSpectral (LayerVoice&, const LayerParams&, const RenderCtx&, float freq, bool firstBlock, float* L, float* R, int n);
} // namespace PV
