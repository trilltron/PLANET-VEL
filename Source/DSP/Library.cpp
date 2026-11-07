#include "Library.h"
#include "Dsp.h"
#include <complex>

namespace PV
{
Library& Library::get() { static Library l; return l; }

void Library::ensureStarted()
{
    bool expected = false;
    if (started.compare_exchange_strong (expected, true)) startThread (juce::Thread::Priority::low);
}
void Library::waitUntilReady() { ensureStarted(); while (! isReady()) juce::Thread::sleep (5); }

void Library::run()
{
    buildWavetables();
    buildSources();
    ready.store (true, std::memory_order_release);
}

// ---------------------------------------------------------------------------------- wavetables
namespace
{
float hash01 (int a, int b) { uint32_t h = (uint32_t) a * 374761393u + (uint32_t) b * 668265263u; h = (h ^ (h >> 13)) * 1274126177u; return (float) ((h ^ (h >> 16)) & 0xFFFFFF) / 16777216.0f; }
float smoothNoise (float x, int seed)
{
    const int i = (int) std::floor (x); const float f = x - (float) i; const float s = f * f * (3 - 2 * f);
    return lerpf (hash01 (i, seed), hash01 (i + 1, seed), s);
}
float gauss (float x, float c, float w) { const float d = (x - c) / w; return std::exp (-d * d); }

// harmonic amplitude of table family `fam` at harmonic h (1-based) and morph position m in [0,1]
float harmonicAmp (int fam, int h, float m, float& phase)
{
    const float fh = (float) h; phase = 0.0f;
    switch (fam)
    {
        case 0: // ANALOG: saw -> square -> narrow pulse
        {
            if (m < 0.5f) { const float t = m * 2.0f; return (1.0f / fh) * ((h & 1) ? 1.0f : 1.0f - t); }
            const float pw = lerpf (0.5f, 0.12f, (m - 0.5f) * 2.0f);
            return std::abs (std::sin (kPi * fh * pw)) / fh;
        }
        case 1: // VOCAL: formant sweep A-E-I-O-U (assuming f0 ~ 130Hz)
        {
            static const float F[5][3] = { { 650, 1080, 2650 }, { 400, 1700, 2600 }, { 290, 2150, 3100 }, { 400, 800, 2600 }, { 350, 600, 2700 } };
            const float pos = m * 4.0f; const int i0 = juce::jmin (3, (int) pos); const float f = pos - (float) i0;
            float a = 0;
            for (int k = 0; k < 3; ++k) a += gauss (fh * 130.0f, lerpf (F[i0][k], F[i0 + 1][k], f), 90.0f + 60.0f * (float) k) * (1.0f - 0.3f * (float) k);
            return (a + 0.02f) / std::pow (fh, 0.4f);
        }
        case 2: // SPECTRAL: smoothly evolving random spectral envelope
        {
            phase = hash01 (h, 7) * kTwoPi;
            return (1.0f / std::pow (fh, 0.75f)) * (0.1f + 0.9f * smoothNoise (fh * 0.35f + m * 9.0f, 3));
        }
        case 3: // METALLIC: energy clustered on stretched bell partials
        {
            static const float R[] = { 1, 2.76f, 5.40f, 8.93f, 13.34f, 18.64f, 24.8f, 32.0f };
            float a = 0;
            for (int k = 0; k < 8; ++k) a += gauss (fh, R[k] * lerpf (1.0f, 1.9f, m), 0.8f + 0.3f * (float) k) / (1.0f + 0.35f * (float) k);
            return a * 0.8f + (h == 1 ? 0.5f : 0.0f);
        }
        case 4: // PLANETARY: stacked fifths / seconds that bloom upward
        {
            static const int S[] = { 1, 2, 3, 4, 6, 8, 9, 12, 16, 18, 24, 27, 32, 36, 48, 54, 64 };
            float a = 0;
            for (int s : S) if (h == s) a = 1.0f / std::pow ((float) s, lerpf (0.9f, 0.25f, m));
            phase = std::sin (fh * 0.7f + m * 6.0f) * 1.2f;
            return a + 0.02f / fh;
        }
        case 5: // GLASS: odd harmonics with a travelling sparkle peak
            return ((h & 1) ? 1.0f : 0.05f) / fh * (0.4f + 2.2f * gauss (fh, 3.0f + m * 40.0f, 3.5f + m * 4.0f));
        case 6: // ORGAN: drawbar morph
        {
            static const int D[] = { 1, 2, 3, 4, 6, 8 };
            float a = 0;
            for (int k = 0; k < 6; ++k) if (h == D[k]) a = 0.35f + 0.65f * std::sin (kPi * (m * 2.0f + (float) k * 0.31f) ) * std::sin (kPi * (m * 2.0f + (float) k * 0.31f));
            return a;
        }
        default: // DIGITAL: comb-notched saw
            return (1.0f / fh) * (0.5f + 0.5f * std::cos (fh * kPi * (0.02f + m * 0.35f)));
    }
}
} // namespace

void Library::buildWavetables()
{
    wt.assign ((size_t) kFamilies * kFrames * kMips * (kTableSize + 1), 0.0f);
    juce::dsp::FFT fft (11);
    std::vector<std::complex<float>> spec ((size_t) kTableSize), out ((size_t) kTableSize);

    for (int fam = 0; fam < kFamilies; ++fam)
        for (int fr = 0; fr < kFrames; ++fr)
        {
            const float m = (float) fr / (float) (kFrames - 1);
            float scale = 1.0f;
            for (int mip = 0; mip < kMips; ++mip)
            {
                // mip m keeps harmonics up to 1023>>m so playback never aliases above Nyquist
                const int cap = juce::jmin (1023, 1023 >> mip);
                std::fill (spec.begin(), spec.end(), std::complex<float>());
                for (int h = 1; h <= cap; ++h)
                {
                    float ph; const float a = harmonicAmp (fam, h, m, ph);
                    const float th = ph - kPi * 0.5f;                         // sin(x+ph) = cos(x+ph-pi/2)
                    const std::complex<float> c (0.5f * a * std::cos (th), 0.5f * a * std::sin (th));
                    spec[(size_t) h] = c;
                    spec[(size_t) (kTableSize - h)] = std::conj (c);          // Hermitian -> real signal
                }
                fft.perform (spec.data(), out.data(), true);
                float* dst = wt.data() + (((size_t) fam * kFrames + (size_t) fr) * kMips + (size_t) mip) * (kTableSize + 1);
                for (int i = 0; i < kTableSize; ++i) dst[i] = out[(size_t) i].real();
                if (mip == 0)           // one normalisation factor per frame, shared by all its mips
                {
                    float peak = 1.0e-9f; for (int i = 0; i < kTableSize; ++i) peak = juce::jmax (peak, std::abs (dst[i]));
                    scale = 0.9f / peak;
                }
                for (int i = 0; i < kTableSize; ++i) dst[i] *= scale;
                dst[kTableSize] = dst[0];
            }
        }
}

// ------------------------------------------------------------------------------- granular sources
void Library::buildSources()
{
    const double sr = kSourceRate; const int N = (int) (sr * 8.0);
    Rng rng (1234);
    auto fadeLoop = [&] (std::vector<float>& d)             // equal-power crossfade of the tail into the head
    {
        const int x = (int) (sr * 0.5);
        for (int i = 0; i < x; ++i) { const float t = (float) i / (float) x; d[(size_t) i] = d[(size_t) i] * std::sin (t * kPi * 0.5f) + d[(size_t) (N - x + i)] * std::cos (t * kPi * 0.5f); }
        d.resize ((size_t) (N - x));
    };
    auto normalise = [&] (std::vector<float>& d) { float p = 1e-6f; for (float s : d) p = juce::jmax (p, std::abs (s)); for (auto& s : d) s *= 0.8f / p; };

    // 0: choir cloud - 4 detuned glottal saws through slowly morphing formants
    {
        std::vector<float> d ((size_t) N, 0.0f);
        float ph[4] = { 0, 0.3f, 0.6f, 0.9f }; SVF f[3];
        static const float V[3][3] = { { 650, 1080, 2650 }, { 400, 800, 2600 }, { 290, 2150, 3100 } };
        for (int i = 0; i < N; ++i)
        {
            const float t = (float) i / (float) N; const float vowel = 1.0f + std::sin (t * kTwoPi * 2.0f);   // 0..2
            const int v0 = juce::jmin (1, (int) vowel); const float vf = vowel - (float) v0;
            if ((i & 31) == 0) for (int k = 0; k < 3; ++k) f[k].setQ (lerpf (V[v0][k], V[v0 + 1][k], vf), 9.0f + 3.0f * (float) k, sr);
            float s = 0;
            for (int o = 0; o < 4; ++o)
            {
                const float inc = (kSourceRoot * (1.0f + 0.003f * ((float) o - 1.5f)) * (1.0f + 0.004f * std::sin (t * 40.0f + (float) o))) / (float) sr;
                ph[o] += inc; if (ph[o] >= 1) ph[o] -= 1;
                s += (2 * ph[o] - 1) - polyBlep (ph[o], inc);
            }
            float y = 0; for (int k = 0; k < 3; ++k) { f[k].tick (s * 0.25f); y += f[k].bp * f[k].k * (1.0f - 0.25f * (float) k); }
            d[(size_t) i] = y + rng.bi() * 0.004f;
        }
        fadeLoop (d); normalise (d); sources[0].data = std::move (d);
    }
    // 1: glass cloud - slowly beating inharmonic glass partials
    {
        std::vector<float> d ((size_t) N, 0.0f); static const float R[] = { 1, 2.32f, 4.25f, 6.63f, 9.38f, 12.5f, 2.0f, 3.0f };
        for (int k = 0; k < 8; ++k)
        {
            const float f0 = kSourceRoot * R[k]; if (f0 > 15000) continue;
            const float fm = 0.12f + 0.07f * (float) k, a = 1.0f / (1.0f + 0.5f * (float) k);
            for (int i = 0; i < N; ++i)
            {
                const float t = (float) i / (float) sr;
                d[(size_t) i] += a * std::sin (kTwoPi * f0 * t) * (0.55f + 0.45f * std::sin (kTwoPi * fm * t + (float) k));
            }
        }
        fadeLoop (d); normalise (d); sources[1].data = std::move (d);
    }
    // 2: noise field - band-passed noise around the root with slow movement
    {
        std::vector<float> d ((size_t) N, 0.0f); SVF a, b;
        for (int i = 0; i < N; ++i)
        {
            const float t = (float) i / (float) sr;
            if ((i & 63) == 0) { a.setQ (kSourceRoot * (1.0f + 0.5f * std::sin (t * 0.9f)), 14.0f, sr); b.setQ (kSourceRoot * 2.01f * (1.0f + 0.3f * std::sin (t * 1.3f)), 10.0f, sr); }
            const float n = rng.bi(); a.tick (n); b.tick (n);
            d[(size_t) i] = a.bp * a.k * 1.5f + b.bp * b.k * 0.7f;
        }
        fadeLoop (d); normalise (d); sources[2].data = std::move (d);
    }
    // 3: bell cloud - randomly struck inharmonic bells
    {
        std::vector<float> d ((size_t) N, 0.0f); static const float R[] = { 1, 2.76f, 5.40f, 8.93f, 13.34f };
        for (int strike = 0; strike < 18; ++strike)
        {
            const int s0 = (int) (rng.next() * (float) N); const float pitch = std::exp2 ((float) (int) (rng.next() * 5.0f) * 0.2f * 0.0f + std::floor (rng.next() * 3.0f) * 0.5f);
            for (int k = 0; k < 5; ++k)
            {
                const float f0 = kSourceRoot * pitch * R[k]; if (f0 > 14000) continue; const float dec = 3.0f / (1.0f + (float) k);
                for (int i = 0; i < (int) (sr * 3.0); ++i) { const float t = (float) i / (float) sr; d[(size_t) ((s0 + i) % N)] += std::sin (kTwoPi * f0 * t) * std::exp (-t / dec * 2.0f) * 0.4f / (1.0f + (float) k); }
            }
        }
        fadeLoop (d); normalise (d); sources[3].data = std::move (d);
    }
    for (auto& s : sources) { s.rate = sr; s.root = kSourceRoot; }
}

// ------------------------------------------------------------------------------------ user sample
bool Library::loadUserSample (const juce::File& f)
{
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
    if (r == nullptr || r->lengthInSamples < 64 || r->lengthInSamples > (juce::int64) r->sampleRate * 120) return false;
    const int numCh = r->numChannels > 1 ? 2 : 1, numSmp = (int) r->lengthInSamples;
    juce::AudioBuffer<float> tmp (numCh, numSmp);
    r->read (&tmp, 0, numSmp, (juce::int64) 0, true, true);
    auto b = std::make_unique<Buf>();
    b->rate = r->sampleRate; b->root = kSourceRoot;
    b->data.resize ((size_t) tmp.getNumSamples());
    for (int i = 0; i < tmp.getNumSamples(); ++i)
    {
        float s = tmp.getSample (0, i); if (tmp.getNumChannels() > 1) s = 0.5f * (s + tmp.getSample (1, i));
        b->data[(size_t) i] = s;
    }
    const juce::ScopedLock sl (userLock);
    userPtr.store (b.get(), std::memory_order_release);
    userKeep.push_back (std::move (b));
    while (userKeep.size() > 6) userKeep.erase (userKeep.begin());       // old ones are long out of use by now
    return true;
}
} // namespace PV
