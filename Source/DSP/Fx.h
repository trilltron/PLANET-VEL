// Cinematic FX rack: master filter -> tape drive -> chorus -> glitch -> ping-pong delay -> (+ send reverb) -> EQ -> width -> output limiter.
// All buffers are allocated in prepare(); process() is allocation free.
#pragma once
#include "Dsp.h"

namespace PV
{
struct FxParams
{
    bool mfOn = false; int mfType = 0; float mfFreq = 20000, mfRes = 0.2f;
    bool dlyOn = true, dlySync = true; int dlyDiv = 12; float dlyTime = 375, dlyFb = 0.4f, dlyMix = 0.25f;
    bool revOn = true; int revType = 3; float revSize = 0.7f, revWet = 0.35f;
    bool glOn = false; int glType = 0, glRate = 5; float glMix = 0.5f, glProb = 0.35f;
    bool limit = true, mono = false; float volDb = 0;
    float choMix = 0, choRate = 0.4f, choDepth = 0.5f, drive = 0, width = 1, eqLo = 0, eqMid = 0, eqHi = 0, master = 0.75f;
    double bpm = 120, ppq = 0; bool playing = false, ppqValid = false;
    int quality = 1;
};

// ------------------------------------------------------------------------------------ reverb (8-line FDN)
class FdnReverb
{
public:
    void prepare (double sr);
    void setParams (int type, float size);
    void process (const float* inL, const float* inR, float* outL, float* outR, int n);   // outputs wet only
private:
    static constexpr int N = 8;
    double sr = 44100;
    std::array<std::vector<float>, N> line; std::array<int, N> len {}, wp {}; std::array<float, N> damp {}, g {};
    std::array<std::vector<float>, 4> apBuf; std::array<int, 4> apLen {}, apPos {};
    std::vector<float> pre; int preLen = 1, prePos = 0;
    float dampC = 0.3f, lfoPh = 0; bool freeze = false;
    int type = 3; float size = 0.7f;
};

// ------------------------------------------------------------------------------------ glitch
class Glitcher
{
public:
    void prepare (double sr);
    void process (float* L, float* R, int n, const FxParams& p);
private:
    double sr = 44100;
    std::vector<float> bl, br; int size = 1, w = 0;
    bool active = false; int type = 0, cnt = 0, sliceLen = 1, capLen = 1; float readPos = 0, capStart = 0, rate = 1; double freePos = 0; long long lastIdx = -1;
    float blend = 0, holdL = 0, holdR = 0; int crushHold = 0, crushStep = 8; float crushQ = 16;
    Rng rng { 777 };
};

// ------------------------------------------------------------------------------------ whole rack
class FxChain
{
public:
    void prepare (double sr, int maxBlock);
    void reset();
    /** main: in/out. send: reverb send bus (in). */
    void process (float* L, float* R, const float* sendL, const float* sendR, int n, const FxParams& p);
    float getGainReduction() const noexcept { return grAmount; }
private:
    double sr = 44100;
    SVF mfL, mfR;
    // chorus
    std::vector<float> choL, choR; int choSize = 1, choW = 0; float choPh = 0;
    // delay
    std::vector<float> dlL, dlR; int dlSize = 1, dlW = 0; float dlCur = 22050, fbLpL = 0, fbLpR = 0;
    FdnReverb rev; Glitcher glitch;
    std::vector<float> revL, revR;
    Biquad eq[2][3]; float lastEq[3] = { 999, 999, 999 };
    float limGain = 1, grAmount = 0, smMaster = 0.75f, smVol = 1.0f;
};
} // namespace PV
