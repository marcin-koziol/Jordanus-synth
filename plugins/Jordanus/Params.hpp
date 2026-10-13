/*
 * Jordanus - shared parameter definitions used by both the DSP plugin
 * side and the UI, so ranges/units/labels can't drift between the two.
 *
 * The two performance gestures (flageolet harmonics, sub "low end") are NOT
 * parameters: they come live from MIDI CCs (see kParamHarmCC/kParamSubCC),
 * and are echoed back to the UI through the two output-only meter params at
 * the end of the table.
 */

#pragma once

#include <cstdint>

namespace jordanus {

enum Params : uint32_t {
    kParamBloom = 0,
    kParamHarmCC,
    kParamUpHarm,
    kParamDownHarm,
    kParamBaseUnderHarm,

    kParamSubLevel,
    kParamSubDeep,
    kParamSubMorph,
    kParamSubCC,
    kParamBaseUnderSub,
    kParamBaseSub,

    kParamDrive,
    kParamWave,
    kParamTilt,
    kParamDetune,
    kParamFlagShape,
    kParamSubShape,
    kParamVoices,
    kParamSpread,
    kParamAsym,
    kParamCab,

    kParamBendRange,
    kParamGlide,

    kParamAttack,
    kParamDecay,
    kParamSustain,
    kParamRelease,
    kParamVolume,

    // MIDI learn handshake with the UI, never automated, never in a preset:
    // the UI arms kParamLearn (0 = off, 1 = harmonics, 2 = sub); the DSP
    // binds the next incoming CC itself, disarms, and reports the CC number
    // in kParamLearnedCC (-1 while nothing new has been learned), which the
    // UI then writes into kParamHarmCC/kParamSubCC so the host stores it.
    kParamLearn,

    // output-only (UI's LCD meters + learn result) - not user controls.
    // Must stay last before kParamCount.
    kParamHarmView,
    kParamSubView,
    kParamLearnedCC,

    kParamCount
};

inline constexpr bool isOutputParam(uint32_t index) noexcept
{
    return index == kParamHarmView || index == kParamSubView || index == kParamLearnedCC;
}

// params that belong in a preset: everything except the learn handshake and outputs
inline constexpr bool isPresetParam(uint32_t index) noexcept
{
    return index != kParamLearn && !isOutputParam(index);
}

enum class ParamShape { Linear, Logarithmic, Integer };

struct ParamInfo
{
    const char* name;
    const char* symbol;
    const char* unit;
    float min;
    float max;
    float def;
    ParamShape shape;
};

inline const ParamInfo& getParamInfo(uint32_t index) noexcept
{
    static constexpr ParamInfo table[kParamCount] = {
        { "Bloom",          "bloom",       "s",  0.01f,   1.5f,    0.08f,  ParamShape::Logarithmic },
        { "Harmonics CC",   "harm_cc",     "",   0.0f,    119.0f,  1.0f,   ParamShape::Integer },
        { "Up Flageolet",   "up_harm",     "",   2.0f,    8.0f,    3.0f,   ParamShape::Integer }, // harmonic number
        { "Down Flageolet", "down_harm",   "",   2.0f,    8.0f,    2.0f,   ParamShape::Integer },
        { "Base Under Harm", "base_harm",  "",   0.0f,    1.0f,    0.05f,  ParamShape::Linear }, // base left under a full flageolet

        { "Sub Level",      "sub_level",   "",   0.0f,    1.0f,    0.6f,   ParamShape::Linear },
        { "Sub -2 Oct",     "sub_deep",    "",   0.0f,    1.0f,    0.6f,   ParamShape::Linear },
        { "Sub Morph",      "sub_morph",   "s",  0.005f,  1.0f,    0.15f,  ParamShape::Logarithmic },
        { "Sub CC",         "sub_cc",      "",   0.0f,    119.0f,  2.0f,   ParamShape::Integer },
        { "Base Under Sub", "base_sub_duck", "", 0.0f,    1.0f,    1.0f,   ParamShape::Linear }, // base left with SUB fully down
        { "Base Sub",       "base_sub",    "",   0.0f,    1.0f,    0.0f,   ParamShape::Linear }, // fixed -1 oct under the base

        { "Drive",          "drive",       "",   0.0f,    1.0f,    0.06f,  ParamShape::Linear },
        { "Wave",           "wave",        "",   0.0f,    1.0f,    0.1f,   ParamShape::Linear }, // sine .. saw (0.5) .. square
        { "Body Tilt",      "tilt",        "",   0.5f,    2.0f,    0.8f,   ParamShape::Linear },
        { "Body Detune",    "detune",      "ct", 0.0f,    25.0f,   7.0f,   ParamShape::Linear },
        { "Flageolet Wave", "flag_shape",  "",   0.0f,    1.0f,    0.6f,   ParamShape::Linear }, // sine .. saw (0.5) .. square
        { "Sub Wave",       "sub_shape",   "",   0.0f,    1.0f,    0.0f,   ParamShape::Linear },
        { "Voices",         "voices",      "",   1.0f,    7.0f,    1.0f,   ParamShape::Integer }, // unison copies of the base oscillator
        { "Spread",         "spread",      "ct", 0.0f,    50.0f,   12.0f,  ParamShape::Linear },
        { "Asymmetry",      "asym",        "",   0.0f,    1.0f,    0.1f,   ParamShape::Linear },
        { "Cabinet",        "cab",         "Hz", 2000.0f, 8000.0f, 4200.0f, ParamShape::Logarithmic },

        { "Bend Range",     "bend_range",  "st", 1.0f,    24.0f,   12.0f,  ParamShape::Integer },
        { "Glide",          "glide",       "s",  0.0f,    1.0f,    0.0f,   ParamShape::Linear },

        { "Attack",         "attack",      "s",  0.001f,  1.0f,    0.004f, ParamShape::Logarithmic },
        { "Decay",          "decay",       "s",  0.005f,  5.0f,    0.25f,  ParamShape::Logarithmic },
        { "Sustain",        "sustain",     "",   0.0f,    1.0f,    0.7f,   ParamShape::Linear },
        { "Release",        "release",     "s",  0.01f,   2.0f,    0.18f,  ParamShape::Logarithmic },
        { "Volume",         "volume",      "",   0.0f,    1.0f,    0.8f,   ParamShape::Linear },

        { "MIDI Learn",     "midi_learn",  "",   0.0f,    2.0f,    0.0f,   ParamShape::Integer },

        { "Harmonics View", "harm_view",   "",   0.0f,    1.0f,    0.5f,   ParamShape::Linear }, // bipolar, 0.5 = centre
        { "Sub View",       "sub_view",    "",   0.0f,    1.0f,    0.5f,   ParamShape::Linear }, // bipolar, 0.5 = centre
        { "Learned CC",     "learned_cc",  "",  -1.0f,  127.0f,   -1.0f,   ParamShape::Integer },
    };
    return table[index];
}

} // namespace jordanus
