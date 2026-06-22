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
        void setupToggle (juce::ToggleButton&, juce::Label&, const juce::String& paramID,
                          std::unique_ptr<APVTS::ButtonAttachment>&, const juce::String& labelText);
        void setPage (bool pro);
        void layoutGrid (juce::Rectangle<int> area, const std::vector<Knob*>& knobs, int cols, int rows);

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

        // PRO page
        juce::TextButton tabMain { "MAIN" }, tabPro { "PRO" };
        bool showingPro = false;

        juce::ComboBox stereoModeBox;
        juce::Label    stereoModeLabel;
        std::unique_ptr<APVTS::ComboBoxAttachment> stereoModeAttachment;

        juce::ToggleButton autoGainButton, multibandButton;
        juce::Label        autoGainLabel,  multibandLabel;
        std::unique_ptr<APVTS::ButtonAttachment> autoGainAttachment, multibandAttachment;

        Knob mbLow, mbMid, mbHigh, mbFreqLow, mbFreqHigh;

        juce::Rectangle<int> panelArea;
        bool lastNuclear = false;

        // Env-gated preview mode (PP_PICKLE_DEMO=1): synthesises a pulsing level so
        // the pickle / jar / meter animate without an audio device. No effect on the
        // audio path; useful for demos and screenshots.
        bool  demoMode  = false;
        float demoPhase = 0.0f;
        bool  startOnProPage = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PicklePowerEditor)
    };
}
