/*
 * Jordanus - factory presets, seeded into the user's preset
 * directory (see ui/PresetStore.hpp) the first time the UI runs with an
 * empty preset library. Same mechanism as the other Sideous plugins: each
 * preset only lists the symbols it changes; everything else keeps its
 * Params.hpp default.
 */

#pragma once

#include "PresetStore.hpp"

#include <cstring>
#include <utility>
#include <vector>

namespace jordanus {
namespace ui {

struct FactoryPreset
{
    const char* name;
    std::vector<std::pair<const char*, float>> overrides; // symbol -> value
};

inline const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> presets = {
        // the defaults: tuned against the reference recordings
        { "Rude Lead", {} },
        // quicker bloom into the flageolets, a touch more grit, brighter cabinet
        { "Scream Harmonics", {
            { "bloom", 0.12f }, { "drive", 0.2f }, { "cab", 4800.0f } } },
        // slow feedback swell, darker cabinet, a little glide between notes
        { "Feedback Swell", {
            { "bloom", 0.7f }, { "cab", 3400.0f }, { "glide", 0.04f }, { "release", 0.4f } } },
        // sub-heavy: deeper -2 oct share, louder sub, clean and round lead
        { "Low End Monster", {
            { "sub_level", 0.85f }, { "sub_deep", 0.9f }, { "drive", 0.0f }, { "asym", 0.05f }, { "cab", 3000.0f } } },
        // hollow, clarinet-ish: odd-only square body, pure-sine flageolets
        { "Hollow Square", {
            { "wave", 1.0f }, { "tilt", 1.5f }, { "flag_shape", 0.0f } } },
        // wide detune for a thick, chorused body; square sub for a buzzy low end
        { "Fat Wobble", {
            { "detune", 16.0f }, { "wave", 0.2f }, { "tilt", 0.8f }, { "sub_shape", 0.7f }, { "drive", 0.15f } } },
        // 7-voice saw unison with a guitar-like envelope: hard attack, lower sustain
        { "Supersaw Lead", {
            { "wave", 0.5f }, { "voices", 7.0f }, { "spread", 20.0f }, { "drive", 0.12f },
            { "decay", 0.5f }, { "sustain", 0.5f } } },
        // plucked guitar envelope, 4th-harmonic (two octaves) flageolet up
        { "Guitar Pluck", {
            { "decay", 0.4f }, { "sustain", 0.35f }, { "release", 0.3f }, { "up_harm", 4.0f }, { "drive", 0.1f } } },
        // two-octave bend range for dive-bomb style bends
        { "Dive Bomb", {
            { "bend_range", 24.0f }, { "drive", 0.3f }, { "asym", 0.3f } } },
    };
    return presets;
}

inline void seedFactoryPresets()
{
    for (const FactoryPreset& fp : factoryPresets())
    {
        float values[kParamCount];
        for (uint32_t i = 0; i < kParamCount; ++i)
            values[i] = getParamInfo(i).def;

        for (const auto& kv : fp.overrides)
        {
            for (uint32_t i = 0; i < kParamCount; ++i)
            {
                if (isPresetParam(i) && std::strcmp(getParamInfo(i).symbol, kv.first) == 0)
                {
                    values[i] = kv.second;
                    break;
                }
            }
        }

        savePreset(fp.name, values);
    }
}

} // namespace ui
} // namespace jordanus
