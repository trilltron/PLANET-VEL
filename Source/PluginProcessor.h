#pragma once
#include "Common.h"
#include "Params.h"
#include "Presets.h"
#include "DSP/Synth.h"
#include "DSP/Fx.h"
#include "DSP/Arp.h"

namespace PV
{
struct Settings
{
    std::atomic<int>  quality { 1 };          // 0 ECO / 1 NORMAL / 2 HQ
    std::atomic<int>  polyphony { 24 };
    std::atomic<int>  pitchBendRange { 2 };
    std::atomic<int>  theme { 0 };            // 0 Crimson / 1 Violet Night / 2 Eclipse
    std::atomic<bool> animation { true };
    std::atomic<bool> mpe { false };
    juce::String libraryPath, tuningFile, userSamplePath;     // message-thread only
};

class PlanetVelProcessor : public juce::AudioProcessor, public juce::ChangeBroadcaster, private juce::AsyncUpdater
{
public:
    PlanetVelProcessor();
    ~PlanetVelProcessor() override;

    // ---- AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Planet Vel"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 20.0; }
    int  getNumPrograms() override { return (int) PresetBank::factory().size(); }
    int  getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int i) override;
    const juce::String getProgramName (int i) override { return i >= 0 && i < getNumPrograms() ? PresetBank::factory()[(size_t) i].name : juce::String(); }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- parameters
    juce::AudioProcessorValueTreeState apvts;
    juce::RangedAudioParameter* param (int i) const { return params[(size_t) i]; }
    float getRaw (int i) const { return raw[(size_t) i]->load(); }
    void  setParamNormalised (int i, float n, bool gesture = false);

    // ---- presets
    void loadPreset (const Preset& p, int programIndex = -1);
    Preset captureCurrent() const;                              // current state as a Preset (for saving / A-B)
    juce::String currentName() const { const juce::ScopedLock l (metaLock); return meta.name; }
    juce::String currentCategory() const { const juce::ScopedLock l (metaLock); return meta.category; }
    juce::String macroName (int i) const { const juce::ScopedLock l (metaLock); return meta.macroNames[(size_t) i]; }
    void saveUserPreset (const juce::String& name);
    void toggleAB();
    int  abSlot() const { return abCurrent; }
    bool isFavorite (const juce::String& n) const { return favorites.contains (n); }
    void toggleFavorite (const juce::String& n);
    void loadGlobalSettings();                                  // message thread, called by the editor
    void saveGlobalSettings();

    // ---- modulation matrix (lock free slots)
    struct RouteSlot { std::atomic<int> src { 0 }, dest { -1 }; std::atomic<float> amount { 0.0f }; };
    std::array<RouteSlot, kRouteSlots> routes;
    ArpPattern arpSteps;
    std::atomic<int> arpLength { 16 };

    juce::MidiKeyboardState keyboardState;                      // shared with the on-screen keyboard: lights keys for incoming MIDI
    // ---- UI -> audio MIDI (wheels)
    void uiMidi (const juce::MidiMessage& m);
    // ---- MIDI learn
    void startMidiLearn (int paramIndex) { learnTarget.store (paramIndex); }
    void clearMidiMap (int paramIndex) { for (auto& c : ccMap) if (c.load() == paramIndex) c.store (-1); }
    int  midiLearnTarget() const { return learnTarget.load(); }
    int  ccFor (int paramIndex) const { for (int c = 0; c < 128; ++c) if (ccMap[(size_t) c].load() == paramIndex) return c; return -1; }

    // ---- settings / library
    Settings settings;
    bool loadUserSample (const juce::File& f);
    bool loadTuning (const juce::File& f);
    void resetTuning() { tuning.reset(); settings.tuningFile = {}; }
    int  editorWidth = 1600, editorHeight = 1000;               // persisted per instance
    int  uiScalePercent = 100;

    // ---- meters (read by the UI)
    std::atomic<float> peakL { 0 }, peakR { 0 }, cpuLoad { 0 }, orbitXNow { 0.5f }, orbitYNow { 0.5f };
    std::atomic<int>   voiceCount { 0 };
    std::array<float, 2048> scope {}; std::atomic<int> scopePos { 0 };
    float layerActivity (int l) const { return layerMeter[(size_t) l].load(); }

    double getBpmForUi() const { return lastBpm.load(); }
    double transportBpm = 120.0; bool transportPlaying = false;

private:
    static BusesProperties makeBuses();
    void handleAsyncUpdate() override;
    void processChunk (float* mainL, float* mainR, float* const* auxL, float* const* auxR, const bool* auxOn, int n, juce::MidiBuffer& midi, double ppq, bool ppqValid);
    void buildLayerParams (const float* snap, LayerParams (&out)[kLayers]);
    void applyMods (const float* base, float* out);

    std::array<juce::RangedAudioParameter*, NUM_PARAMS> params {};
    std::array<std::atomic<float>*, NUM_PARAMS> raw {};
    std::array<juce::NormalisableRange<float>, NUM_PARAMS> ranges;

    SynthEngine synth; FxChain fx; Arp arp; Tuning tuning;
    double sr = 44100.0; int maxBlock = 512;
    static constexpr int kChunk = 4096;
    std::array<std::vector<float>, kLayers> layL, layR;
    std::vector<float> sendL, sendR;
    juce::MidiBuffer arpOut, chunkMidi;
    float base[NUM_PARAMS] {}, snap[NUM_PARAMS] {};
    LayerParams curLP[kLayers];
    float macroSrc[SRC_COUNT] {};

    // modulation state
    struct Lfo
    {
        float ph = 0, cur = 0, nxt = 0, val = 0; Rng rng;
        float run (float rateHz, int shape, double dt)
        {
            ph += (float) (rateHz * dt); bool wrap = false; while (ph >= 1.0f) { ph -= 1.0f; wrap = true; }
            if (wrap) { cur = nxt; nxt = rng.bi(); }
            switch (shape)
            {
                case 0: val = std::sin (kTwoPi * ph); break;
                case 1: val = 4.0f * std::abs (ph - 0.5f) - 1.0f; break;
                case 2: val = 2.0f * ph - 1.0f; break;
                case 3: val = ph < 0.5f ? 1.0f : -1.0f; break;
                case 4: val = cur; break;
                default: { const float t = ph * ph * (3.0f - 2.0f * ph); val = cur + (nxt - cur) * t; break; }
            }
            return val;
        }
    } lfo1, lfo2;
    float orbitPhase = 0;
    float bendSemis = 0; std::atomic<float> modWheel { 0 }, aftertouch { 0 };
    std::atomic<double> lastBpm { 120.0 };
    std::array<std::atomic<float>, kLayers> layerMeter {};

    // UI midi fifo (3 bytes each, packed in an int)
    static constexpr int kFifoSize = 256;
    juce::AbstractFifo uiFifo { kFifoSize }; std::array<int, kFifoSize> uiFifoData {};
    // cc learn + deferred parameter writes from the audio thread
    std::array<std::atomic<int>, 128> ccMap;
    std::atomic<int> learnTarget { -1 };
    juce::AbstractFifo ccFifo { 128 }; std::array<std::pair<int, float>, 128> ccFifoData {};

    // preset / meta
    mutable juce::CriticalSection metaLock;
    Preset meta;
    int currentProgram = 0;
    std::array<std::vector<float>, 2> abState; int abCurrent = 0;
    juce::StringArray favorites;
    bool globalLoaded = false;
    juce::uint32 lastCpuTick = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlanetVelProcessor)
};
} // namespace PV
