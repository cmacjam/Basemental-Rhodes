#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "BasementalLookAndFeel.h"

class BasementalRhodesEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit BasementalRhodesEditor (BasementalRhodesProcessor&);
    ~BasementalRhodesEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int kBaseW = 1040, kBaseH = 640;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };
    struct Switch
    {
        juce::ToggleButton button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
    };
    struct Section { juce::String title; juce::Rectangle<int> area; };

    class Content : public juce::Component
    {
    public:
        explicit Content (BasementalRhodesEditor& e) : owner (e) {}
        void paint (juce::Graphics& g) override { owner.paintContent (g); }
    private:
        BasementalRhodesEditor& owner;
    };

    void paintContent (juce::Graphics&);
    void layoutContent();
    void timerCallback() override;
    Knob& addKnob (const char* paramId, const juce::String& name);
    Switch& addSwitch (const char* paramId, const juce::String& name);
    void setModel (int index);

    BasementalRhodesProcessor& proc;
    BasementalLookAndFeel lnf;
    Content content { *this };

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Switch>> switches;
    std::map<juce::String, Knob*> knobById;
    std::map<juce::String, Switch*> switchById;

    juce::TextButton stageButton { "STAGE" }, suitcaseButton { "SUITCASE" };
    juce::ComboBox presetBox;
    juce::MidiKeyboardComponent keyboard;
    juce::TooltipWindow tooltips { this, 600 };

    std::vector<Section> sections;
    int voiceCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BasementalRhodesEditor)
};
