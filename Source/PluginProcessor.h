#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Parameters.h"
#include "dsp/RhodesEngine.h"

class BasementalRhodesProcessor : public juce::AudioProcessor
{
public:
    BasementalRhodesProcessor();
    ~BasementalRhodesProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;   // on-screen keyboard (standalone / auditioning)

    int getActiveVoiceCount() const { return activeVoices.load(); }

private:
    void updateEngineParams();
    void handleMidi (const juce::MidiMessage& m);

    brh::RhodesEngine engine;
    int currentProgram = 0;
    std::atomic<int> activeVoices { 0 };

    // cached raw parameter pointers (lock-free reads on the audio thread)
    std::atomic<float>* pModel {}; std::atomic<float>* pBell {}; std::atomic<float>* pBark {};
    std::atomic<float>* pDecay {}; std::atomic<float>* pRelease {}; std::atomic<float>* pDynamics {};
    std::atomic<float>* pMech {}; std::atomic<float>* pTune {};
    std::atomic<float>* pDrive {}; std::atomic<float>* pBass {}; std::atomic<float>* pTreble {};
    std::atomic<float>* pCab {}; std::atomic<float>* pVolume {};
    std::atomic<float>* pTremOn {}; std::atomic<float>* pTremRate {}; std::atomic<float>* pTremDepth {};
    std::atomic<float>* pPhOn {}; std::atomic<float>* pPhRate {}; std::atomic<float>* pPhDepth {}; std::atomic<float>* pPhFb {};
    std::atomic<float>* pChOn {}; std::atomic<float>* pChRate {}; std::atomic<float>* pChDepth {}; std::atomic<float>* pChMix {};
    std::atomic<float>* pRvOn {}; std::atomic<float>* pRvMix {}; std::atomic<float>* pRvDecay {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BasementalRhodesProcessor)
};
