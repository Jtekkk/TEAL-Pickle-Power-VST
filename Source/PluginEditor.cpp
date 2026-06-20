/*
    ==============================================================================

    Pickle Power 🥒⚡
    PluginEditor.cpp

    ==============================================================================
*/

#include "PluginEditor.h"
#include "Utils/Theme.h"

namespace pp
{
    PicklePowerEditor::PicklePowerEditor (PicklePowerProcessor& p)
        : AudioProcessorEditor (&p), processorRef (p)
    {
        setLookAndFeel (&lookAndFeel);

        addAndMakeVisible (jar);
        addAndMakeVisible (pickle);
        addAndMakeVisible (meter);

        setupKnob (brine,   id::brine,        "BRINE");
        setupKnob (crunch,  id::crunch,       "CRUNCH");
        setupKnob (snap,    id::snap,         "SNAP");
        setupKnob (ferment, id::fermentation, "FERMENT");
        setupKnob (age,     id::age,          "AGE");
        setupKnob (juice,   id::pickleJuice,  "PICKLE JUICE");
        setupKnob (width,   id::width,        "WIDTH");
        setupKnob (mix,     id::mix,          "MIX");
        setupKnob (output,  id::output,       "OUTPUT");

        setupCombo (brineTypeBox, brineTypeLabel, brineTypeChoices(),
                    id::brineType, brineTypeAttachment, "FLAVOUR");
        setupCombo (oversamplingBox, oversamplingLabel, oversamplingChoices(),
                    id::oversampling, oversamplingAttachment, "OVERSAMPLING");

        bypassButton.setClickingTogglesState (true);
        addAndMakeVisible (bypassButton);
        bypassAttachment = std::make_unique<APVTS::ButtonAttachment> (
            processorRef.getAPVTS(), id::bypass, bypassButton);

        setSize (meta::editorWidth, meta::editorHeight);
        setResizable (false, false);

        startTimerHz (60);
    }

    PicklePowerEditor::~PicklePowerEditor()
    {
        stopTimer();
        setLookAndFeel (nullptr);
    }

    //==============================================================================
    void PicklePowerEditor::setupKnob (Knob& k, const juce::String& paramID, const juce::String& displayName)
    {
        k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 18);
        k.slider.setColour (juce::Slider::textBoxTextColourId, theme::textBright);
        k.slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (k.slider);

        k.label.setText (displayName, juce::dontSendNotification);
        k.label.setJustificationType (juce::Justification::centred);
        k.label.setColour (juce::Label::textColourId, theme::neonLime);
        k.label.setFont (NeonLookAndFeel::pickleFont (12.5f, true));
        addAndMakeVisible (k.label);

        k.attachment = std::make_unique<APVTS::SliderAttachment> (
            processorRef.getAPVTS(), paramID, k.slider);
    }

    void PicklePowerEditor::setupCombo (juce::ComboBox& box, juce::Label& label,
                                        const juce::StringArray& items, const juce::String& paramID,
                                        std::unique_ptr<APVTS::ComboBoxAttachment>& attachment,
                                        const juce::String& labelText)
    {
        box.addItemList (items, 1);
        box.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (box);

        label.setText (labelText, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, theme::textDim);
        label.setFont (NeonLookAndFeel::pickleFont (11.0f, true));
        addAndMakeVisible (label);

        attachment = std::make_unique<APVTS::ComboBoxAttachment> (
            processorRef.getAPVTS(), paramID, box);
    }

    //==============================================================================
    void PicklePowerEditor::paint (juce::Graphics& g)
    {
        g.setGradientFill (juce::ColourGradient (theme::panel, 0.0f, 0.0f,
                                                 theme::background, 0.0f, (float) getHeight(), false));
        g.fillAll();

        // Right-hand control panel backdrop.
        if (! panelArea.isEmpty())
        {
            g.setColour (theme::panel.withAlpha (0.6f));
            g.fillRoundedRectangle (panelArea.toFloat(), theme::cornerRadius);
            g.setColour (theme::neonGreen.withAlpha (0.18f));
            g.drawRoundedRectangle (panelArea.toFloat(), theme::cornerRadius, 1.0f);
        }

        // Title.
        const bool nuclear = processorRef.isNuclear();
        g.setColour (nuclear ? theme::nuclear : theme::neonGreen);
        g.setFont (NeonLookAndFeel::pickleFont (28.0f, true));
        g.drawText ("PICKLE POWER", 18, 8, 460, 30, juce::Justification::centredLeft);

        g.setColour (theme::textDim);
        g.setFont (NeonLookAndFeel::pickleFont (11.5f, true));
        g.drawText (nuclear ? "* * *  NUCLEAR PICKLE MODE  * * *"
                            : "BRINE  -  CRUNCH  -  SNAP  -  FERMENT",
                    20, 36, 460, 14, juce::Justification::centredLeft);

        g.setColour (theme::textDim);
        g.setFont (NeonLookAndFeel::pickleFont (10.0f));
        g.drawText ("v" + juce::String (meta::version), getWidth() - 80, 12, 64, 14,
                    juce::Justification::centredRight);
    }

    void PicklePowerEditor::resized()
    {
        auto area = getLocalBounds();

        auto header = area.removeFromTop (52);
        bypassButton.setBounds (header.removeFromRight (54).reduced (10));

        auto content = area.reduced (16, 8);

        // Left: jar with the dancing pickle inside, plus the output meter.
        auto left = content.removeFromLeft (288);
        jar.setBounds (left);
        pickle.setBounds (left.reduced (44).withTrimmedTop (10));

        content.removeFromLeft (8);
        meter.setBounds (content.removeFromLeft (24));
        content.removeFromLeft (16);

        // Right: control panel.
        panelArea = content;
        auto right = content.reduced (12);

        auto combos = right.removeFromTop (52);
        auto comboL = combos.removeFromLeft (combos.getWidth() / 2).reduced (6, 2);
        brineTypeLabel.setBounds (comboL.removeFromTop (16));
        brineTypeBox.setBounds (comboL.removeFromTop (28));
        auto comboR = combos.reduced (6, 2);
        oversamplingLabel.setBounds (comboR.removeFromTop (16));
        oversamplingBox.setBounds (comboR.removeFromTop (28));

        right.removeFromTop (8);

        const std::array<Knob*, 9> knobs {
            &brine, &crunch, &snap, &ferment, &age, &juice, &width, &mix, &output
        };

        const int cols = 3, rows = 3;
        const int cw = right.getWidth()  / cols;
        const int chh = right.getHeight() / rows;

        for (int i = 0; i < (int) knobs.size(); ++i)
        {
            const int r = i / cols, c = i % cols;
            auto cell = juce::Rectangle<int> (right.getX() + c * cw,
                                              right.getY() + r * chh, cw, chh).reduced (4);
            knobs[(size_t) i]->label.setBounds (cell.removeFromTop (16));
            knobs[(size_t) i]->slider.setBounds (cell);
        }
    }

    //==============================================================================
    void PicklePowerEditor::timerCallback()
    {
        const float rms  = processorRef.getMeterRms();
        const float peak = processorRef.getMeterPeak();
        const float gr   = processorRef.getGainReductionDb();
        const bool  nuclear = processorRef.isNuclear();
        const bool  clip = peak > 0.92f || gr > 1.0f;

        jar.update (rms);
        pickle.update (peak, clip, nuclear);
        meter.update (rms, peak, gr);

        if (nuclear != lastNuclear)
        {
            lastNuclear = nuclear;
            repaint();
        }
    }
}
