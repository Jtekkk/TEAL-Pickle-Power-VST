/*
    ==============================================================================

    Pickle Power 🥒⚡
    PluginEditor.h

    ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "GUI/NeonLookAndFeel.h"
#include "GUI/DancingPickleComponent.h"
#include "GUI/PickleJarComponent.h"
#include "GUI/PickleMeter.h"

namespace pp
{
    class PicklePowerEditor : public juce::AudioProcessorEditor,
                              private juce::Timer
    {
    public:
        explicit PicklePowerEditor (PicklePowerProcessor&);
        ~PicklePowerEditor() override;

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        using APVTS = juce::AudioProcessorValueTreeState;

        struct Knob
        {
            juce::Slider slider;
            juce::Label  label;
            std::unique_ptr<APVTS::SliderAttachment> attachment;
        };

        void timerCallback() override;
        void setupKnob (Knob&, const juce::String& paramID, const juce::String& displayName);
        void setupCombo (juce::ComboBox&, juce::Label&, const juce::StringArray& items,
                         const juce::String& paramID, std::unique_ptr<APVTS::ComboBoxAttachment>&,
                         const juce::String& labelText);

        PicklePowerProcessor& processorRef;
        NeonLookAndFeel lookAndFeel;

        PickleJarComponent     jar;
        DancingPickleComponent pickle;
        PickleMeter            meter;

        Knob brine, crunchAtk, crunchSus, snapLow, snapMid, snapHigh,
             ferment, age, juice, width, mix, output;

        juce::ComboBox brineTypeBox, oversamplingBox;
        juce::Label    brineTypeLabel, oversamplingLabel;
        std::unique_ptr<APVTS::ComboBoxAttachment> brineTypeAttachment, oversamplingAttachment;

        juce::ComboBox presetBox;
        juce::Label    presetLabel;

        juce::ToggleButton bypassButton;
        std::unique_ptr<APVTS::ButtonAttachment> bypassAttachment;

        juce::Rectangle<int> panelArea;
        bool lastNuclear = false;

        // Env-gated preview mode (PP_PICKLE_DEMO=1): synthesises a pulsing level so
        // the pickle / jar / meter animate without an audio device. No effect on the
        // audio path; useful for demos and screenshots.
        bool  demoMode  = false;
        float demoPhase = 0.0f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PicklePowerEditor)
    };
}
