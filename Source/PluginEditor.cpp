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

        setupKnob (brine,     id::brine,         "BRINE");
        setupKnob (crunchAtk, id::crunchAttack,  "ATTACK");
        setupKnob (crunchSus, id::crunchSustain, "SUSTAIN");
        setupKnob (snapLow,   id::snapLow,       "LOW AIR");
        setupKnob (snapMid,   id::snapMid,       "MID AIR");
        setupKnob (snapHigh,  id::snapHigh,      "HIGH AIR");
        setupKnob (ferment,   id::fermentation,  "FERMENT");
        setupKnob (age,       id::age,           "AGE");
        setupKnob (juice,     id::pickleJuice,   "PICKLE JUICE");
        setupKnob (width,     id::width,         "WIDTH");
        setupKnob (mix,       id::mix,           "MIX");
        setupKnob (output,    id::output,        "OUTPUT");

        setupCombo (brineTypeBox, brineTypeLabel, brineTypeChoices(),
                    id::brineType, brineTypeAttachment, "FLAVOUR");
        setupCombo (oversamplingBox, oversamplingLabel, oversamplingChoices(),
                    id::oversampling, oversamplingAttachment, "OVERSAMPLING");

        // Factory preset selector (not an APVTS parameter).
        auto& pm = processorRef.getPresetManager();
        presetBox.addItemList (pm.getNames(), 1);
        presetBox.setSelectedItemIndex (pm.getCurrent(), juce::dontSendNotification);
        presetBox.setJustificationType (juce::Justification::centred);
        presetBox.onChange = [this]
        {
            processorRef.getPresetManager().apply (presetBox.getSelectedItemIndex());
        };
        addAndMakeVisible (presetBox);

        presetLabel.setText ("PRESET", juce::dontSendNotification);
        presetLabel.setJustificationType (juce::Justification::centredRight);
        presetLabel.setColour (juce::Label::textColourId, theme::textDim);
        presetLabel.setFont (NeonLookAndFeel::pickleFont (11.0f, true));
        addAndMakeVisible (presetLabel);

        bypassButton.setClickingTogglesState (true);
        bypassButton.getProperties().set ("invertOnState", true);   // on == bypassed == dim
        addAndMakeVisible (bypassButton);
        bypassAttachment = std::make_unique<APVTS::ButtonAttachment> (
            processorRef.getAPVTS(), id::bypass, bypassButton);

        // ---- PRO page controls ----
        setupCombo (stereoModeBox, stereoModeLabel, stereoModeChoices(),
                    id::stereoMode, stereoModeAttachment, "STEREO");
        setupToggle (autoGainButton,  autoGainLabel,  id::autoGain,  autoGainAttachment,  "AUTO GAIN");
        setupToggle (multibandButton, multibandLabel, id::multiband, multibandAttachment, "MULTIBAND");
        setupKnob (mbLow,      id::mbLow,      "MB LOW");
        setupKnob (mbMid,      id::mbMid,      "MB MID");
        setupKnob (mbHigh,     id::mbHigh,     "MB HIGH");
        setupKnob (mbFreqLow,  id::mbFreqLow,  "X-LOW");
        setupKnob (mbFreqHigh, id::mbFreqHigh, "X-HIGH");

        // ---- MAIN / PRO tabs ----
        for (auto* t : { &tabMain, &tabPro })
        {
            t->setClickingTogglesState (true);
            t->setRadioGroupId (100);
            t->setColour (juce::TextButton::buttonColourId,   theme::panel);
            t->setColour (juce::TextButton::buttonOnColourId, theme::neonGreen.withAlpha (0.30f));
            t->setColour (juce::TextButton::textColourOffId,  theme::textDim);
            t->setColour (juce::TextButton::textColourOnId,   theme::textBright);
            addAndMakeVisible (*t);
        }
        tabMain.onClick = [this] { setPage (false); };
        tabPro .onClick = [this] { setPage (true); };

        const auto demoVal = juce::SystemStats::getEnvironmentVariable ("PP_PICKLE_DEMO", "0");
        demoMode = demoVal != "0";
        if (demoMode)
        {
            const int demoPreset = 2;   // "Punchy Snare" — shows off the new controls
            processorRef.getPresetManager().apply (demoPreset);
            presetBox.setSelectedItemIndex (demoPreset, juce::dontSendNotification);
            startOnProPage = (demoVal == "2");   // PP_PICKLE_DEMO=2 opens the PRO page
        }

        setSize (meta::editorWidth, meta::editorHeight);
        setResizable (false, false);

        setPage (startOnProPage);
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

    void PicklePowerEditor::setupToggle (juce::ToggleButton& b, juce::Label& l,
                                         const juce::String& paramID,
                                         std::unique_ptr<APVTS::ButtonAttachment>& attachment,
                                         const juce::String& labelText)
    {
        b.setClickingTogglesState (true);
        addAndMakeVisible (b);

        l.setText (labelText, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        l.setColour (juce::Label::textColourId, theme::neonLime);
        l.setFont (NeonLookAndFeel::pickleFont (11.0f, true));
        addAndMakeVisible (l);

        attachment = std::make_unique<APVTS::ButtonAttachment> (
            processorRef.getAPVTS(), paramID, b);
    }

    void PicklePowerEditor::setPage (bool pro)
    {
        showingPro = pro;
        const bool m = ! pro;

        const std::array<Knob*, 12> mainKnobs {
            &brine, &crunchAtk, &crunchSus, &snapLow, &snapMid, &snapHigh,
            &ferment, &age, &juice, &width, &mix, &output
        };
        for (auto* k : mainKnobs) { k->slider.setVisible (m); k->label.setVisible (m); }
        brineTypeBox.setVisible (m);    brineTypeLabel.setVisible (m);
        oversamplingBox.setVisible (m); oversamplingLabel.setVisible (m);

        const std::array<Knob*, 5> proKnobs { &mbLow, &mbMid, &mbHigh, &mbFreqLow, &mbFreqHigh };
        for (auto* k : proKnobs) { k->slider.setVisible (pro); k->label.setVisible (pro); }
        stereoModeBox.setVisible (pro);   stereoModeLabel.setVisible (pro);
        autoGainButton.setVisible (pro);  autoGainLabel.setVisible (pro);
        multibandButton.setVisible (pro); multibandLabel.setVisible (pro);

        tabMain.setToggleState (m,   juce::dontSendNotification);
        tabPro .setToggleState (pro, juce::dontSendNotification);
        repaint();
    }

    void PicklePowerEditor::layoutGrid (juce::Rectangle<int> area, const std::vector<Knob*>& knobs,
                                        int cols, int rows)
    {
        const int cw = area.getWidth()  / cols;
        const int chh = area.getHeight() / rows;

        for (int i = 0; i < (int) knobs.size(); ++i)
        {
            const int r = i / cols, c = i % cols;
            auto cell = juce::Rectangle<int> (area.getX() + c * cw, area.getY() + r * chh, cw, chh).reduced (4);
            knobs[(size_t) i]->label.setBounds (cell.removeFromTop (16));
            knobs[(size_t) i]->slider.setBounds (cell);
        }
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
        g.setFont (NeonLookAndFeel::pickleFont (26.0f, true));
        g.drawText ("PICKLE POWER", 18, 8, 330, 28, juce::Justification::centredLeft);

        g.setColour (theme::textDim);
        g.setFont (NeonLookAndFeel::pickleFont (11.0f, true));
        g.drawText (nuclear ? "* * *  NUCLEAR PICKLE MODE  * * *  v" + juce::String (meta::version)
                            : "BRINE - CRUNCH - SNAP - FERMENT   v" + juce::String (meta::version),
                    20, 35, 340, 14, juce::Justification::centredLeft);
    }

    void PicklePowerEditor::resized()
    {
        auto area = getLocalBounds();

        auto header = area.removeFromTop (52);
        bypassButton.setBounds (header.removeFromRight (54).reduced (10));
        auto presetArea = header.removeFromRight (200).reduced (4, 12);
        presetLabel.setBounds (presetArea.removeFromLeft (46));
        presetBox.setBounds (presetArea);
        auto tabArea = header.removeFromRight (124).reduced (8, 13);
        tabMain.setBounds (tabArea.removeFromLeft (tabArea.getWidth() / 2).reduced (2, 0));
        tabPro .setBounds (tabArea.reduced (2, 0));

        auto content = area.reduced (16, 8);

        // Left: jar with the dancing pickle inside, plus the output meter.
        auto left = content.removeFromLeft (288);
        jar.setBounds (left);
        pickle.setBounds (left.reduced (32).withTrimmedTop (16));

        content.removeFromLeft (8);
        meter.setBounds (content.removeFromLeft (34));
        content.removeFromLeft (16);

        // Right: control panel (MAIN and PRO pages share the area; visibility toggles).
        panelArea = content;
        const auto right = content.reduced (12);

        // ---- MAIN page ----
        {
            auto r = right;
            auto combos = r.removeFromTop (52);
            auto comboL = combos.removeFromLeft (combos.getWidth() / 2).reduced (6, 2);
            brineTypeLabel.setBounds (comboL.removeFromTop (16));
            brineTypeBox.setBounds (comboL.removeFromTop (28));
            auto comboR = combos.reduced (6, 2);
            oversamplingLabel.setBounds (comboR.removeFromTop (16));
            oversamplingBox.setBounds (comboR.removeFromTop (28));

            r.removeFromTop (8);
            layoutGrid (r, { &brine, &crunchAtk, &crunchSus, &snapLow, &snapMid, &snapHigh,
                             &ferment, &age, &juice, &width, &mix, &output }, 3, 4);
        }

        // ---- PRO page ----
        {
            auto r = right;
            auto top = r.removeFromTop (66);
            auto c1 = top.removeFromLeft (top.getWidth() / 3).reduced (6, 2);
            stereoModeLabel.setBounds (c1.removeFromTop (16));
            stereoModeBox.setBounds (c1.removeFromTop (28));
            auto c2 = top.removeFromLeft (top.getWidth() / 2).reduced (6, 2);
            autoGainLabel.setBounds (c2.removeFromTop (16));
            autoGainButton.setBounds (c2.removeFromTop (32).withSizeKeepingCentre (46, 28));
            auto c3 = top.reduced (6, 2);
            multibandLabel.setBounds (c3.removeFromTop (16));
            multibandButton.setBounds (c3.removeFromTop (32).withSizeKeepingCentre (46, 28));

            r.removeFromTop (8);
            layoutGrid (r, { &mbLow, &mbMid, &mbHigh, &mbFreqLow, &mbFreqHigh }, 3, 2);
        }
    }

    //==============================================================================
    void PicklePowerEditor::timerCallback()
    {
        float rms  = processorRef.getMeterRms();
        float peak = processorRef.getMeterPeak();
        float gr   = processorRef.getGainReductionDb();
        const bool nuclear = processorRef.isNuclear();

        if (demoMode)
        {
            demoPhase += 0.06f;
            const float beat = std::pow (0.5f + 0.5f * std::sin (demoPhase * 3.0f), 4.0f);
            rms  = 0.25f + 0.50f * beat;
            peak = 0.40f + 0.55f * beat;
            gr   = beat * 4.0f;
        }

        const bool clip = peak > 0.92f || gr > 1.0f;

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
