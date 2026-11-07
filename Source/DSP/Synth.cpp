#include "Synth.h"

namespace PV
{
void SynthEngine::noteOn (int note, float vel, const LayerParams (&lp)[kLayers])
{
    // re-trigger of a sounding note: release the old one first
    for (auto& v : voices) if (v.active && v.gate && v.note == note) { v.gate = false; for (auto& l : v.lv) l.env.off(); }

    Voice* slot = nullptr; int used = 0;
    for (auto& v : voices) { if (v.active) ++used; else if (slot == nullptr) slot = &v; }
    if (used >= maxVoices || slot == nullptr)           // steal: quietest released voice, else the oldest
    {
        Voice* best = nullptr; float bestScore = 1.0e9f;
        for (auto& v : voices)
        {
            if (! v.active) continue;
            float e = 0; for (auto& l : v.lv) e = juce::jmax (e, l.env.v);
            const float score = (v.gate ? 1000.0f : 0.0f) + e * 10.0f - (float) (counter - v.age) * 1.0e-6f;
            if (score < bestScore) { bestScore = score; best = &v; }
        }
        slot = best;
    }
    if (slot == nullptr) return;
    auto& v = *slot;
    v.active = true; v.gate = true; v.sustained = false; v.note = note; v.vel = vel; v.age = ++counter;
    for (int l = 0; l < kLayers; ++l)
    {
        auto& lv = v.lv[l];
        if (lp[l].enabled && note >= lp[l].keylo && note <= lp[l].keyhi) lv.start (note, vel, (uint32_t) (counter * 2654435761u + (uint32_t) l));
        else lv.env.kill();
    }
}

void SynthEngine::noteOff (int note)
{
    for (auto& v : voices)
        if (v.active && v.gate && v.note == note)
        {
            if (pedal) { v.sustained = true; continue; }
            v.gate = false; for (auto& l : v.lv) l.env.off();
        }
}

void SynthEngine::sustain (bool down)
{
    pedal = down;
    if (! down)
        for (auto& v : voices) if (v.active && v.sustained) { v.sustained = false; v.gate = false; for (auto& l : v.lv) l.env.off(); }
}

void SynthEngine::allNotesOff (bool kill)
{
    for (auto& v : voices)
    {
        if (! v.active) continue;
        v.gate = false; v.sustained = false;
        for (auto& l : v.lv) { if (kill) l.env.kill(); else l.env.off(); }
        if (kill) v.active = false;
    }
}

void SynthEngine::render (const LayerParams (&lp)[kLayers], const RenderCtx& ctx, float* const outL[kLayers], float* const outR[kLayers], int n)
{
    alignas (16) float tl[kSubBlock], tr[kSubBlock];
    const float invN = 1.0f / (float) n;

    for (auto& v : voices)
    {
        if (! v.active) continue;
        bool any = false;
        for (int l = 0; l < kLayers; ++l)
        {
            auto& lv = v.lv[l]; const auto& p = lp[l];
            if (! lv.env.active()) continue;
            any = true;
            const float gain = p.level * p.orbitW * lv.velGain;
            if (gain < 1.0e-5f && lv.gL <= 1.0e-5f)          // silent (muted / orbited away): just keep the envelope running
            { for (int i = 0; i < n; ++i) lv.env.tick (p.env); lv.fresh = false; continue; }

            const float freq = ctx.tuning->hz[(size_t) juce::jlimit (0, 127, lv.note)] * semis2ratio (p.tune + ctx.bend);
            const bool first = lv.fresh;
            switch (p.engine)
            {
                case ENG_ORGANIC:    renderOrganic   (lv, p, ctx, freq, first, tl, tr, n); break;
                case ENG_WAVETABLE:  renderWavetable (lv, p, ctx, freq, tl, tr, n);        break;
                case ENG_GRANULAR:   renderGranular  (lv, p, ctx, freq, tl, tr, n);        break;
                case ENG_ANALOG:     renderAnalog    (lv, p, ctx, freq, tl, tr, n);        break;
                default:             renderSpectral  (lv, p, ctx, freq, first, tl, tr, n); break;
            }
            lv.fresh = false;

            // filter (coefficients once per sub-block; env/keytrack/LFO as octave offsets)
            const float oct = p.keytrk * (float) (lv.note - 60) / 12.0f + p.fenv * lv.env.v * 5.0f + p.lfo * ctx.lfo1 * 3.0f;
            lv.fl.set (p.cutoff * std::exp2 (oct), p.reso, ctx.sr);
            lv.fr.g = lv.fl.g; lv.fr.k = lv.fl.k; lv.fr.a1 = lv.fl.a1; lv.fr.a2 = lv.fl.a2; lv.fr.a3 = lv.fl.a3;
            const float preDrive = 1.0f + p.drive * 5.0f, postDrive = 1.0f / (1.0f + p.drive * 1.2f);

            float pl, pr; { const float a = (juce::jlimit (-1.0f, 1.0f, p.pan) * 0.5f + 0.5f) * kPi * 0.5f; pl = std::cos (a) * 1.41421356f; pr = std::sin (a) * 1.41421356f; }
            const float tL = gain * pl, tR = gain * pr;
            if (lv.gL < 0) { lv.gL = tL; lv.gR = tR; }             // first block: no ramp from 0 needed (env starts at 0)
            const float dL = (tL - lv.gL) * invN, dR = (tR - lv.gR) * invN;
            float* oL = outL[l]; float* oR = outR[l];
            float gl = lv.gL, gr = lv.gR;
            for (int i = 0; i < n; ++i)
            {
                float xl = tl[i], xr = tr[i];
                if (p.drive > 0.001f) { xl = fastTanh (xl * preDrive) * postDrive; xr = fastTanh (xr * preDrive) * postDrive; }
                xl = lv.fl.process (xl, p.ftype); xr = lv.fr.process (xr, p.ftype);
                const float e = lv.env.tick (p.env);
                gl += dL; gr += dR;
                oL[i] += xl * e * gl; oR[i] += xr * e * gr;
            }
            lv.gL = tL; lv.gR = tR;
        }
        if (! any) v.active = false;
    }
}
} // namespace PV
