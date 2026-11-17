#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace ParamID
{
    // instrument
    inline constexpr const char* model      = "model";       // 0 = Stage, 1 = Suitcase
    inline constexpr const char* bell       = "bell";
    inline constexpr const char* bark       = "bark";
    inline constexpr const char* decay      = "decay";
    inline constexpr const char* release    = "release";
    inline constexpr const char* dynamics   = "dynamics";
    inline constexpr const char* mechNoise  = "mechNoise";
    inline constexpr const char* tune       = "tune";
    // amp
    inline constexpr const char* drive      = "drive";
    inline constexpr const char* bass       = "bass";
    inline constexpr const char* treble     = "treble";
    inline constexpr const char* cabinet    = "cabinet";
    inline constexpr const char* volume     = "volume";
    // vibrato / tremolo
    inline constexpr const char* tremOn     = "tremOn";
    inline constexpr const char* tremRate   = "tremRate";
    inline constexpr const char* tremDepth  = "tremDepth";
    // phaser
    inline constexpr const char* phaserOn   = "phaserOn";
    inline constexpr const char* phaserRate = "phaserRate";
    inline constexpr const char* phaserDepth= "phaserDepth";
    inline constexpr const char* phaserFb   = "phaserFb";
    // chorus
    inline constexpr const char* chorusOn   = "chorusOn";
    inline constexpr const char* chorusRate = "chorusRate";
    inline constexpr const char* chorusDepth= "chorusDepth";
    inline constexpr const char* chorusMix  = "chorusMix";
    // spring reverb
    inline constexpr const char* reverbOn   = "reverbOn";
    inline constexpr const char* reverbMix  = "reverbMix";
    inline constexpr const char* reverbDecay= "reverbDecay";
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = [] (float v, int) { return String (juce::roundToInt (v * 100.0f)) + " %"; };
    auto hz  = [] (float v, int) { return String (v, 2) + " Hz"; };
    auto db  = [] (float v, int) { return String (v, 1) + " dB"; };

    auto addF = [&] (const char* id, const char* name, NormalisableRange<float> r, float def,
                     std::function<String (float, int)> fmt)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
                        AudioParameterFloatAttributes().withStringFromValueFunction (fmt)));
    };
    auto addB = [&] (const char* id, const char* name, bool def)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::model, 1 }, "Model",
                                                        StringArray { "Stage", "Suitcase" }, 0));
    addF (ParamID::bell,      "Bell",       { 0.0f, 1.0f }, 0.5f, pct);
    addF (ParamID::bark,      "Bark",       { 0.0f, 1.0f }, 0.45f, pct);
    addF (ParamID::decay,     "Decay",      { 0.25f, 2.0f, 0.0f, 0.6f }, 1.0f,
          [] (float v, int) { return String (v, 2) + "x"; });
    addF (ParamID::release,   "Release",    { 0.0f, 1.0f }, 0.25f, pct);
    addF (ParamID::dynamics,  "Dynamics",   { 0.0f, 1.0f }, 0.7f, pct);
    addF (ParamID::mechNoise, "Mech Noise", { 0.0f, 1.0f }, 0.3f, pct);
    addF (ParamID::tune,      "Tune",       { 430.0f, 450.0f, 0.01f }, 440.0f,
          [] (float v, int) { return "A = " + String (v, 1) + " Hz"; });

    addF (ParamID::drive,     "Drive",      { 0.0f, 1.0f }, 0.15f, pct);
    addF (ParamID::bass,      "Bass",       { -12.0f, 12.0f, 0.1f }, 0.0f, db);
    addF (ParamID::treble,    "Treble",     { -12.0f, 12.0f, 0.1f }, 0.0f, db);
    addB (ParamID::cabinet,   "Cabinet",    true);
    addF (ParamID::volume,    "Volume",     { -36.0f, 12.0f, 0.1f }, 0.0f, db);

    addB (ParamID::tremOn,    "Vibrato On", false);
    addF (ParamID::tremRate,  "Vibrato Rate",  { 0.5f, 10.0f, 0.0f, 0.5f }, 4.0f, hz);
    addF (ParamID::tremDepth, "Vibrato Depth", { 0.0f, 1.0f }, 0.6f, pct);

    addB (ParamID::phaserOn,    "Phaser On", false);
    addF (ParamID::phaserRate,  "Phaser Rate",     { 0.05f, 6.0f, 0.0f, 0.4f }, 0.4f, hz);
    addF (ParamID::phaserDepth, "Phaser Depth",    { 0.0f, 1.0f }, 0.7f, pct);
    addF (ParamID::phaserFb,    "Phaser Feedback", { 0.0f, 0.85f }, 0.3f, pct);

    addB (ParamID::chorusOn,    "Chorus On", false);
    addF (ParamID::chorusRate,  "Chorus Rate",  { 0.05f, 5.0f, 0.0f, 0.4f }, 0.6f, hz);
    addF (ParamID::chorusDepth, "Chorus Depth", { 0.0f, 1.0f }, 0.5f, pct);
    addF (ParamID::chorusMix,   "Chorus Mix",   { 0.0f, 1.0f }, 0.5f, pct);

    addB (ParamID::reverbOn,    "Spring On", false);
    addF (ParamID::reverbMix,   "Spring Mix",   { 0.0f, 1.0f }, 0.25f, pct);
    addF (ParamID::reverbDecay, "Spring Decay", { 0.0f, 1.0f }, 0.5f, pct);

    return layout;
}
