#pragma once
// Minimal VST 2.4 ABI definitions (struct layouts / opcodes of the public plugin interface), written for PLANET VEL.
// No Steinberg SDK code is used. Only the pieces ZIL needs are declared.
#include <cstdint>

namespace pvvst2
{
using intptr = std::intptr_t;
struct AEffect;
typedef intptr (*HostCb)(AEffect*, int32_t opcode, int32_t index, intptr value, void* ptr, float opt);

struct AEffect
{
    int32_t magic;
    intptr (*dispatcher)(AEffect*, int32_t, int32_t, intptr, void*, float);
    void (*process)(AEffect*, float**, float**, int32_t);
    void (*setParameter)(AEffect*, int32_t, float);
    float (*getParameter)(AEffect*, int32_t);
    int32_t numPrograms, numParams, numInputs, numOutputs, flags;
    intptr resvd1, resvd2;
    int32_t initialDelay, realQualities, offQualities;
    float ioRatio;
    void* object;
    void* user;
    int32_t uniqueID, version;
    void (*processReplacing)(AEffect*, float**, float**, int32_t);
    void (*processDoubleReplacing)(AEffect*, double**, double**, int32_t);
    char future[56];
};
struct ERect { int16_t top, left, bottom, right; };
struct VstEvent { int32_t type, byteSize, deltaFrames, flags; char data[16]; };
struct VstMidiEvent { int32_t type, byteSize, deltaFrames, flags, noteLength, noteOffset; char midiData[4]; char detune, noteOffVelocity, reserved1, reserved2; };
struct VstEvents { int32_t numEvents; intptr reserved; VstEvent* events[2]; };
struct VstTimeInfo
{
    double samplePos, sampleRate, nanoSeconds, ppqPos, tempo, barStartPos, cycleStartPos, cycleEndPos;
    int32_t timeSigNumerator, timeSigDenominator, smpteOffset, smpteFrameRate, samplesToNextClock, flags;
};
struct VstPinProperties { char label[64]; int32_t flags, arrangementType; char shortLabel[6]; char future[48]; };
}

#if defined(_WIN32)
 #define PV_VST2_EXPORT extern "C" __declspec(dllexport)
#else
 #define PV_VST2_EXPORT extern "C" __attribute__((visibility("default")))
#endif
PV_VST2_EXPORT pvvst2::AEffect* VSTPluginMain(pvvst2::HostCb host);
