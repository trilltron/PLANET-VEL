// PLANET VEL - parameter table.
// !! The ORDER of this table is the host-visible parameter index (FL "Browse parameters",
// !! automation clips, Link to controller). Only ever APPEND new entries at the END of the
// !! relevant enum so that old projects/automation never shift.
#pragma once
#include "Common.h"

namespace PV
{
enum GlobalParam : int
{
    G_MASTER = 0,
    G_ORBIT_X, G_ORBIT_Y, G_ORBIT_MODE, G_ORBIT_RATE, G_ORBIT_RADIUS,
    G_M1, G_M2, G_M3, G_M4, G_M5, G_M6, G_M7, G_M8,
    G_MF_ON, G_MF_TYPE, G_MF_FREQ, G_MF_RES,
    G_DLY_ON, G_DLY_SYNC, G_DLY_DIV, G_DLY_TIME, G_DLY_FB, G_DLY_MIX,
    G_REV_ON, G_REV_TYPE, G_REV_SIZE, G_REV_WET,
    G_GL_ON, G_GL_TYPE, G_GL_RATE, G_GL_MIX, G_GL_PROB,
    G_OUT_LIMIT, G_OUT_MONO, G_OUT_VOL,
    G_ARP_ON, G_ARP_PAT, G_ARP_RATE, G_ARP_OCT, G_ARP_GATE,
    G_LFO1_RATE, G_LFO1_SHAPE, G_LFO2_RATE, G_LFO2_SHAPE,
    G_CHO_MIX, G_CHO_RATE, G_CHO_DEPTH, G_DRIVE, G_WIDTH, G_EQ_LO, G_EQ_MID, G_EQ_HI,
    G_COUNT
};

enum LayerParam : int
{
    L_ENGINE = 0, L_MUTE, L_SOLO, L_LEVEL, L_PAN, L_TUNE, L_START,
    L_CUTOFF, L_RESO, L_FTYPE, L_DRIVE, L_KEYTRK, L_FENV, L_LFO,
    L_ATK, L_DEC, L_SUS, L_REL,
    L_SPREAD, L_SEND, L_KEYLO, L_KEYHI, L_UNI, L_DETUNE, L_TYPE,
    L_P1, L_P2, L_P3, L_P4, L_P5, L_P6,
    L_COUNT
};

constexpr int NUM_PARAMS = G_COUNT + kLayers * L_COUNT;
constexpr int lp (int layer, int i) { return G_COUNT + layer * L_COUNT + i; }

enum Engine { ENG_ORGANIC = 0, ENG_WAVETABLE, ENG_GRANULAR, ENG_ANALOG, ENG_SPECTRAL, ENG_COUNT };

enum class Kind { Float, Choice, Bool };
enum class Fmt  { Plain, Percent, Hz, Seconds, Db, LinDb, SqDb, MasterDb, Semis, Pan, Int, Beats, Ms };

struct ParamDef
{
    juce::String id, name;
    Kind  kind = Kind::Float;
    float mn = 0, mx = 1, def = 0, centre = 0, step = 0;
    Fmt   fmt = Fmt::Plain;
    juce::StringArray choices;
    bool  modulatable = false;     // may be a mod-matrix destination
    bool  bipolarUi   = false;
};

const std::vector<ParamDef>& defs();
juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
juce::String formatValue (const ParamDef& d, float v);
int findParamById (const juce::String& id);          // -1 if not found

// Names shown in the UI for engine specific controls
const juce::StringArray& engineNames();
juce::StringArray typeNames (int engine);
juce::String      knobLabel (int engine, int pIndex);   // "" = unused
} // namespace PV
