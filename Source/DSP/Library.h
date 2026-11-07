// Shared, lazily-built read-only content: wavetables (8 families x 26 frames = 208 tables, 10 mip levels),
// procedurally rendered granular source material, and the user-dropped granular sample.
#pragma once
#include "../Common.h"

namespace PV
{
class Library
{
public:
    static constexpr int kTableSize = 2048;
    static constexpr int kFamilies  = 8;
    static constexpr int kFrames    = 26;
    static constexpr int kMips      = 10;
    static constexpr int kSources   = 4;         // built-in granular sources
    static constexpr float kSourceRoot = 261.6256f;
    static constexpr double kSourceRate = 44100.0;

    static Library& get();
    /** Builds the tables once (~230 ms, synchronous, idempotent). Called from prepareToPlay - never from the constructor, so plugin
        scanners stay instant. No thread is used: a background thread alive at DLL unload can deadlock on Windows. */
    void ensureStarted();
    bool isReady() const noexcept { return ready.load (std::memory_order_acquire); }
    void waitUntilReady();

    /** table pointer with kTableSize+1 samples (guard point). */
    const float* table (int family, int frame, int mip) const noexcept
    { return wt.data() + (((size_t) family * kFrames + (size_t) frame) * kMips + (size_t) mip) * (kTableSize + 1); }

    struct Buf { std::vector<float> data; double rate = 44100.0; float root = 261.6256f; };
    const Buf* source (int i) const noexcept { return sources[(size_t) i].data.empty() ? nullptr : &sources[(size_t) i]; }

    // user sample (message thread sets, audio thread reads the raw pointer)
    const Buf* userSample() const noexcept { return userPtr.load (std::memory_order_acquire); }
    bool loadUserSample (const juce::File& f);

private:
    Library() = default;
    void buildWavetables();
    void buildSources();

    std::vector<float> wt;
    std::array<Buf, kSources> sources;
    std::atomic<bool> ready { false };
    juce::CriticalSection buildLock;
    std::atomic<const Buf*> userPtr { nullptr };
    std::vector<std::unique_ptr<Buf>> userKeep;       // retired buffers kept alive for a few loads
    juce::CriticalSection userLock;
};
} // namespace PV
