#pragma once
#include "Parameters.h"
#include <vector>
#include <utility>

struct FactoryPreset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values; // plain (real-world) values; unlisted params use defaults
};

inline const std::vector<FactoryPreset>& getFactoryPresets()
{
    using namespace ParamID;
    static const std::vector<FactoryPreset> presets = {
        { "Stage 73 Classic",   { { model, 0 }, { bell, 0.5f }, { bark, 0.45f }, { drive, 0.15f } } },
        { "Suitcase Vibrato",   { { model, 1 }, { tremOn, 1 }, { tremRate, 4.2f }, { tremDepth, 0.7f },
                                  { bass, 3.0f }, { bark, 0.4f } } },
        { "Headhunter Phase",   { { model, 0 }, { phaserOn, 1 }, { phaserRate, 0.35f }, { phaserDepth, 0.8f },
                                  { bark, 0.6f }, { drive, 0.3f }, { treble, 2.0f } } },
        { "Bark & Drive",       { { model, 0 }, { bark, 0.85f }, { bell, 0.7f }, { drive, 0.6f },
                                  { dynamics, 0.85f }, { treble, 1.5f } } },
        { "Ballad Chorus",      { { model, 0 }, { chorusOn, 1 }, { chorusRate, 0.45f }, { chorusDepth, 0.55f },
                                  { chorusMix, 0.45f }, { bark, 0.3f }, { bell, 0.35f }, { reverbOn, 1 },
                                  { reverbMix, 0.2f } } },
        { "Twin Spring Room",   { { model, 0 }, { reverbOn, 1 }, { reverbMix, 0.35f }, { reverbDecay, 0.65f },
                                  { drive, 0.25f }, { bass, 1.5f } } },
        { "Soft Neo-Soul",      { { model, 1 }, { bell, 0.25f }, { bark, 0.25f }, { treble, -3.0f },
                                  { tremOn, 1 }, { tremRate, 2.5f }, { tremDepth, 0.35f }, { chorusOn, 1 },
                                  { chorusMix, 0.3f }, { release, 0.4f } } },
        { "Bright Bell Tines",  { { model, 0 }, { bell, 0.95f }, { bark, 0.35f }, { treble, 3.0f },
                                  { decay, 1.3f }, { reverbOn, 1 }, { reverbMix, 0.15f } } },
    };
    return presets;
}
