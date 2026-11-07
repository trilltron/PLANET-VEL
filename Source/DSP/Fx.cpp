#include "Fx.h"

namespace PV
{
static inline float softClip (float x) noexcept      // transparent below 0.9, smooth knee to 1.0
{
    const float a = std::abs (x);
    if (a < 0.9f) return x;
    const float y = 0.9f + 0.1f * fastTanh ((a - 0.9f) * 10.0f);
    return x < 0 ? -y : y;
}

// ====================================================================================== reverb
void FdnReverb::prepare (double s)
{
    sr = s;
    static const float ms[N] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.1f, 59.3f, 67.7f, 71.9f };
    for (int i = 0; i < N; ++i)
    {
        len[(size_t) i] = juce::jmax (16, (int) (ms[i] * 0.001 * sr * 3.0));            // up to 3x scale for huge spaces
        line[(size_t) i].assign ((size_t) len[(size_t) i] + 8, 0.0f); wp[(size_t) i] = 0; damp[(size_t) i] = 0;
    }
    static const int ap[4] = { 142, 107, 379, 277 };
    for (int i = 0; i < 4; ++i) { apLen[(size_t) i] = juce::jmax (8, (int) (ap[i] * sr / 44100.0)); apBuf[(size_t) i].assign ((size_t) apLen[(size_t) i], 0.0f); apPos[(size_t) i] = 0; }
    preLen = juce::jmax (1, (int) (0.08 * sr)); pre.assign ((size_t) preLen, 0.0f); prePos = 0;
    setParams (type, size);
}

void FdnReverb::setParams (int t, float sz)
{
    type = t; size = sz;
    static const float scale[4]  = { 0.9f, 0.45f, 1.3f, 3.0f };
    static const float rtMin[4]  = { 0.8f, 0.4f, 2.0f, 6.0f }, rtMax[4] = { 12.0f, 4.0f, 25.0f, 60.0f };
    static const float dampT[4]  = { 0.35f, 0.15f, 0.45f, 0.55f };
    const float rt60 = rtMin[t] * std::pow (rtMax[t] / rtMin[t], sz);
    freeze = (t == 3 && sz >= 0.995f);                                            // Nebula at max = infinite hold
    dampC = dampT[t];
    static const float ms[N] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.1f, 59.3f, 67.7f, 71.9f };
    for (int i = 0; i < N; ++i)
    {
        const int L = juce::jlimit (16, (int) line[(size_t) i].size() - 8, (int) (ms[i] * 0.001 * sr * scale[t]));
        len[(size_t) i] = L;
        g[(size_t) i] = freeze ? 1.0f : std::pow (10.0f, -3.0f * (float) L / (rt60 * (float) sr));
    }
}

void FdnReverb::process (const float* inL, const float* inR, float* outL, float* outR, int n)
{
    const float mod = 4.0f * (float) (sr / 44100.0);
    for (int s = 0; s < n; ++s)
    {
        float x = 0.5f * (inL[s] + inR[s]);
        pre[(size_t) prePos] = x; if (++prePos >= preLen) prePos = 0;
        x = freeze ? 0.0f : pre[(size_t) ((prePos + preLen - juce::jmin (preLen - 1, (int) (0.012 * sr))) % preLen)];
        for (int a = 0; a < 4; ++a)                                              // input diffusion
        {
            auto& b = apBuf[(size_t) a]; const int p = apPos[(size_t) a];
            const float d = b[(size_t) p]; const float y = -0.6f * x + d; b[(size_t) p] = x + 0.6f * y; x = y;
            if (++apPos[(size_t) a] >= apLen[(size_t) a]) apPos[(size_t) a] = 0;
        }
        lfoPh += 0.0000045f * 44100.0f / (float) sr * 100.0f; if (lfoPh > 1) lfoPh -= 1;
        float d[N];
        for (int i = 0; i < N; ++i)
        {
            auto& bf = line[(size_t) i]; const int sz = (int) bf.size();
            float rd = (float) wp[(size_t) i] - (float) len[(size_t) i];
            if (i & 1) rd += mod * std::sin (kTwoPi * (lfoPh + 0.125f * (float) i));
            while (rd < 0) rd += (float) sz; while (rd >= (float) sz) rd -= (float) sz;
            const int i0 = (int) rd; const float fr = rd - (float) i0; const int i1 = i0 + 1 >= sz ? 0 : i0 + 1;
            float v = bf[(size_t) i0] + (bf[(size_t) i1] - bf[(size_t) i0]) * fr;
            damp[(size_t) i] += (v - damp[(size_t) i]) * (1.0f - dampC); v = freeze ? v : damp[(size_t) i];
            d[i] = v * g[(size_t) i];
        }
        // Hadamard mix (orthogonal, 8x8)
        for (int h = 1; h < N; h <<= 1) for (int i = 0; i < N; i += h * 2) for (int j = i; j < i + h; ++j) { const float a = d[j], b = d[j + h]; d[j] = a + b; d[j + h] = a - b; }
        const float sc = 0.35355339f;
        float oL = 0, oR = 0;
        for (int i = 0; i < N; ++i)
        {
            auto& bf = line[(size_t) i]; bf[(size_t) wp[(size_t) i]] = d[i] * sc + x * 0.5f + 1.0e-20f;
            if (++wp[(size_t) i] >= (int) bf.size()) wp[(size_t) i] = 0;
            if (i & 1) oR += (i & 2 ? -1.0f : 1.0f) * d[i] * sc; else oL += (i & 2 ? -1.0f : 1.0f) * d[i] * sc;
        }
        outL[s] = oL; outR[s] = oR;
    }
}

// ====================================================================================== glitch
void Glitcher::prepare (double s)
{
    sr = s; size = (int) (4.0 * sr); bl.assign ((size_t) size, 0.0f); br.assign ((size_t) size, 0.0f); w = 0; active = false; blend = 0; lastIdx = -1; freePos = 0;
}

void Glitcher::process (float* L, float* R, int n, const FxParams& p)
{
    const double beats = kRateBeats[juce::jlimit (0, kNumRates - 1, p.glRate)];
    sliceLen = juce::jmax (64, (int) (beats * 60.0 / p.bpm * sr));
    const double beatsPerSample = p.bpm / 60.0 / sr;
    double ppq = p.ppq;
    const float target = (p.glOn) ? p.glMix : 0.0f;
    const float coef = 1.0f - std::exp (-1.0f / (0.004f * (float) sr));
    auto rd = [&] (const std::vector<float>& b, float pos) -> float
    {
        while (pos < 0) pos += (float) size; while (pos >= (float) size) pos -= (float) size;
        const int i0 = (int) pos; const int i1 = i0 + 1 >= size ? 0 : i0 + 1; const float f = pos - (float) i0;
        return b[(size_t) i0] + (b[(size_t) i1] - b[(size_t) i0]) * f;
    };
    for (int i = 0; i < n; ++i)
    {
        const float dl = L[i], dr = R[i];
        bl[(size_t) w] = dl; br[(size_t) w] = dr;
        // slice boundaries: locked to the host's PPQ while playing, free running otherwise
        bool boundary = false;
        if (p.playing && p.ppqValid) { const long long idx = (long long) std::floor (ppq / beats + 1.0e-9); if (idx != lastIdx) { boundary = true; lastIdx = idx; } ppq += beatsPerSample; }
        else { freePos += 1.0; if (freePos >= sliceLen) { freePos -= sliceLen; boundary = true; } lastIdx = -1; }
        if (boundary)
        {
            cnt = 0; active = false;
            if (p.glOn && rng.next() < p.glProb)
            {
                active = true; type = p.glType;
                switch (type)
                {
                    case 0: capLen = juce::jmax (32, sliceLen >> (int) (rng.next() * 3.0f)); capStart = (float) (w - capLen); readPos = capStart; break;
                    case 1: capLen = sliceLen; readPos = (float) (w - 1); break;
                    case 2: readPos = (float) w; rate = 1.0f; break;
                    case 3: crushStep = 4 + (int) (rng.next() * 12.0f); crushQ = (float) (1 << (3 + (int) (rng.next() * 4.0f))); crushHold = 0; break;
                    case 4: { static const float r[] = { 2.0f, 1.5f, 0.75f, 0.5f }; rate = r[(int) (rng.next() * 3.99f)]; readPos = (float) w - rate * (float) sliceLen; break; }
                    default: break;
                }
            }
        }
        float gl = dl, gr = dr;
        if (active)
        {
            switch (type)
            {
                case 0: gl = rd (bl, readPos); gr = rd (br, readPos); readPos += 1.0f; if (readPos >= capStart + (float) capLen) readPos = capStart; break;
                case 1: gl = rd (bl, readPos); gr = rd (br, readPos); readPos -= 1.0f; break;
                case 2: { const float t = (float) cnt / (float) sliceLen; rate = juce::jmax (0.0f, 1.0f - t * t * 1.05f); readPos += rate; gl = rd (bl, readPos); gr = rd (br, readPos); break; }
                case 3: if (crushHold-- <= 0) { crushHold = crushStep; holdL = std::round (dl * crushQ) / crushQ; holdR = std::round (dr * crushQ) / crushQ; } gl = holdL; gr = holdR; break;
                case 4: gl = rd (bl, readPos); gr = rd (br, readPos); readPos += rate; break;
                default: { const int q = juce::jmax (16, sliceLen / 4); const bool on = ((cnt / (q / 2 + 1)) & 1) == 0; if (! on) gl = gr = 0; break; }
            }
            if (++cnt >= sliceLen) active = false;
        }
        const float tgt = active ? target : 0.0f; blend += (tgt - blend) * coef;
        L[i] = dl + (gl - dl) * blend; R[i] = dr + (gr - dr) * blend;
        if (++w >= size) w = 0;
    }
}

// ====================================================================================== rack
void FxChain::prepare (double s, int)
{
    sr = s;
    choSize = (int) (0.06 * sr) + 8; choL.assign ((size_t) choSize, 0.0f); choR.assign ((size_t) choSize, 0.0f); choW = 0;
    dlSize = (int) (2.6 * sr) + 8; dlL.assign ((size_t) dlSize, 0.0f); dlR.assign ((size_t) dlSize, 0.0f); dlW = 0; dlCur = (float) (0.375 * sr);
    rev.prepare (sr); glitch.prepare (sr);
    revL.assign (8192, 0.0f); revR.assign (8192, 0.0f);
    reset();
}
void FxChain::reset()
{
    mfL.reset(); mfR.reset(); std::fill (choL.begin(), choL.end(), 0.f); std::fill (choR.begin(), choR.end(), 0.f);
    std::fill (dlL.begin(), dlL.end(), 0.f); std::fill (dlR.begin(), dlR.end(), 0.f); fbLpL = fbLpR = 0; limGain = 1;
    for (auto& ch : eq) for (auto& b : ch) { b.z1 = b.z2 = 0; }
    lastEq[0] = lastEq[1] = lastEq[2] = 999;
}

void FxChain::process (float* L, float* R, const float* sendL, const float* sendR, int n, const FxParams& p)
{
    jassert (n <= (int) revL.size());
    // --- master filter + drive + chorus (per 32 sample chunks for coefficient updates)
    for (int base = 0; base < n; base += kSubBlock)
    {
        const int m = juce::jmin (kSubBlock, n - base);
        if (p.mfOn) { mfL.set (p.mfFreq, p.mfRes, sr); mfR.g = mfL.g; mfR.k = mfL.k; mfR.a1 = mfL.a1; mfR.a2 = mfL.a2; mfR.a3 = mfL.a3; }
        const float drv = 1.0f + p.drive * 5.0f, dmix = p.drive, dcomp = 1.0f / (1.0f + p.drive * 0.8f);
        const float choDepthS = (0.0015f + 0.004f * p.choDepth) * (float) sr, choBase = 0.012f * (float) sr;
        for (int i = base; i < base + m; ++i)
        {
            float l = L[i], r = R[i];
            if (p.mfOn) { l = mfL.process (l, p.mfType); r = mfR.process (r, p.mfType); }
            if (p.drive > 0.001f) { l = l * (1 - dmix) + fastTanh (l * drv) * dcomp * dmix; r = r * (1 - dmix) + fastTanh (r * drv) * dcomp * dmix; }
            if (p.choMix > 0.001f && p.quality > 0)
            {
                choL[(size_t) choW] = l; choR[(size_t) choW] = r;
                choPh += p.choRate / (float) sr; if (choPh >= 1.0f) choPh -= 1.0f;
                const float m1 = choBase + choDepthS * std::sin (kTwoPi * choPh), m2 = choBase + choDepthS * std::sin (kTwoPi * (choPh + 0.5f));
                auto rdl = [&] (const std::vector<float>& b, float d) { float pos = (float) choW - d; if (pos < 0) pos += (float) choSize; const int i0 = (int) pos; const int i1 = i0 + 1 >= choSize ? 0 : i0 + 1; const float f = pos - (float) i0; return b[(size_t) i0] + (b[(size_t) i1] - b[(size_t) i0]) * f; };
                const float wl = rdl (choL, m1), wr = rdl (choR, m2);
                l = l * (1 - 0.5f * p.choMix) + wl * p.choMix * 0.7f; r = r * (1 - 0.5f * p.choMix) + wr * p.choMix * 0.7f;
                if (++choW >= choSize) choW = 0;
            }
            L[i] = l; R[i] = r;
        }
    }
    // --- glitch
    glitch.process (L, R, n, p);

    // --- delay (stereo ping-pong, tone-filtered feedback, synced or free)
    if (p.dlyOn && p.dlyMix > 0.001f)
    {
        const float target = juce::jlimit (32.0f, (float) (dlSize - 8), (float) (p.dlySync ? kRateBeats[juce::jlimit (0, kNumRates - 1, p.dlyDiv)] * 60.0 / p.bpm * sr : p.dlyTime * 0.001 * sr));
        const float lpC = std::exp (-kTwoPi * 5500.0f / (float) sr);
        for (int i = 0; i < n; ++i)
        {
            dlCur += (target - dlCur) * 0.0006f;
            float pos = (float) dlW - dlCur; if (pos < 0) pos += (float) dlSize;
            const int i0 = (int) pos; const int i1 = i0 + 1 >= dlSize ? 0 : i0 + 1; const float f = pos - (float) i0;
            const float rl = dlL[(size_t) i0] + (dlL[(size_t) i1] - dlL[(size_t) i0]) * f, rr = dlR[(size_t) i0] + (dlR[(size_t) i1] - dlR[(size_t) i0]) * f;
            fbLpL = lpC * fbLpL + (1 - lpC) * rl; fbLpR = lpC * fbLpR + (1 - lpC) * rr;
            dlL[(size_t) dlW] = fastTanh (L[i] + p.dlyFb * fbLpR) + 1.0e-20f; dlR[(size_t) dlW] = fastTanh (R[i] + p.dlyFb * fbLpL) + 1.0e-20f;
            if (++dlW >= dlSize) dlW = 0;
            L[i] += rl * p.dlyMix; R[i] += rr * p.dlyMix;
        }
    }
    // --- reverb: fed by the layer send bus, added to the main mix
    if (p.revOn && p.revWet > 0.001f)
    {
        rev.setParams (p.revType, p.revSize);
        for (int base = 0; base < n; base += (int) revL.size())
        {
            const int m = juce::jmin ((int) revL.size(), n - base);
            rev.process (sendL + base, sendR + base, revL.data(), revR.data(), m);
            for (int i = 0; i < m; ++i) { L[base + i] += revL[(size_t) i] * p.revWet * 1.4f; R[base + i] += revR[(size_t) i] * p.revWet * 1.4f; }
        }
    }
    // --- EQ
    const float eg[3] = { p.eqLo, p.eqMid, p.eqHi };
    for (int b = 0; b < 3; ++b)
        if (std::abs (eg[b] - lastEq[b]) > 0.001f)
        {
            lastEq[b] = eg[b];
            for (int c = 0; c < 2; ++c) eq[c][b].set (b == 0 ? Biquad::LowShelf : b == 1 ? Biquad::Peak : Biquad::HighShelf, b == 0 ? 120.0f : b == 1 ? 1000.0f : 6500.0f, b == 1 ? 0.8f : 0.7f, eg[b], sr);
        }
    const bool eqOn = std::abs (p.eqLo) + std::abs (p.eqMid) + std::abs (p.eqHi) > 0.01f;
    const float vol = juce::Decibels::decibelsToGain (p.volDb);
    const float rel = std::exp (-1.0f / (0.12f * (float) sr));
    for (int i = 0; i < n; ++i)
    {
        float l = L[i], r = R[i];
        if (eqOn) for (int b = 0; b < 3; ++b) { l = eq[0][b].process (l); r = eq[1][b].process (r); }
        if (std::abs (p.width - 1.0f) > 0.001f) { const float m = 0.5f * (l + r), s = 0.5f * (l - r) * p.width; l = m + s; r = m - s; }
        if (p.mono) l = r = 0.5f * (l + r);
        smMaster += (p.master - smMaster) * 0.002f; smVol += (vol - smVol) * 0.002f;
        const float g = smMaster * smMaster * 1.5f * smVol;                     // squared taper: 0.75 ~ unity-ish
        l *= g; r *= g;
        if (p.limit)
        {
            const float pk = juce::jmax (std::abs (l), std::abs (r)), tg = pk > 0.97f ? 0.97f / pk : 1.0f;
            limGain = tg < limGain ? tg : tg + (limGain - tg) * rel;
            l = softClip (l * limGain); r = softClip (r * limGain);
        }
        L[i] = l; R[i] = r;
    }
    grAmount = 1.0f - limGain;
}
} // namespace PV
