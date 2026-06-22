/*
    ==============================================================================

    Pickle Power 🥒⚡
    NeonLookAndFeel.h

    Custom LookAndFeel giving the plugin its neon-green, glowing-glass look:
    rotary knobs with a glowing value arc, styled combo box and a power-style
    bypass toggle.

    ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Utils/Theme.h"

namespace pp
{
    class NeonLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        NeonLookAndFeel()
        {
            setColour (juce::Slider::textBoxTextColourId,    theme::textBright);
            setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            setColour (juce::Label::textColourId,            theme::textBright);

            setColour (juce::ComboBox::backgroundColourId,   theme::panel);
            setColour (juce::ComboBox::textColourId,         theme::textBright);
            setColour (juce::ComboBox::outlineColourId,      theme::neonGreen.withAlpha (0.5f));
            setColour (juce::ComboBox::arrowColourId,        theme::neonGreen);

            setColour (juce::PopupMenu::backgroundColourId,        theme::panel);
            setColour (juce::PopupMenu::textColourId,              theme::textBright);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, theme::neonGreen.withAlpha (0.3f));
            setColour (juce::PopupMenu::highlightedTextColourId,   theme::textBright);
        }

        //==========================================================================
        static juce::Font pickleFont (float height, bool bold = false)
        {
            return juce::Font (juce::FontOptions {}
                                   .withHeight (height)
                                   .withStyle (bold ? "Bold" : "Regular"));
        }

        juce::Font getLabelFont (juce::Label&) override        { return pickleFont (14.0f); }
        juce::Font getComboBoxFont (juce::ComboBox&) override  { return pickleFont (15.0f, true); }
        juce::Font getPopupMenuFont() override                 { return pickleFont (15.0f); }

        //==========================================================================
        void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                               float pos, float startAngle, float endAngle,
                               juce::Slider& slider) override
        {
            auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (6.0f);
            const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
            const auto centre = bounds.getCentre();
            const auto angle  = startAngle + pos * (endAngle - startAngle);

            const auto stroke = juce::PathStrokeType (4.0f, juce::PathStrokeType::curved,
                                                            juce::PathStrokeType::rounded);

            // Background track.
            juce::Path track;
            track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                                 startAngle, endAngle, true);
            g.setColour (theme::panelLight);
            g.strokePath (track, stroke);

            const bool active = slider.isEnabled();
            const auto accent = active ? theme::neonGreen : theme::textDim;

            // Value arc with a soft outer glow.
            juce::Path value;
            value.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                                 startAngle, angle, true);
            g.setColour (accent.withAlpha (0.22f));
            g.strokePath (value, juce::PathStrokeType (11.0f, juce::PathStrokeType::curved,
                                                              juce::PathStrokeType::rounded));
            g.setColour (accent);
            g.strokePath (value, stroke);

            // Knob body.
            const auto knobR = radius * 0.62f;
            auto knob = juce::Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (centre);
            g.setGradientFill (juce::ColourGradient (theme::panelLight, knob.getTopLeft(),
                                                     theme::background,  knob.getBottomRight(), false));
            g.fillEllipse (knob);
            g.setColour (accent.withAlpha (0.6f));
            g.drawEllipse (knob, 1.5f);

            // Pointer.
            juce::Path pointer;
            const float pw = 3.5f;
            pointer.addRoundedRectangle (-pw * 0.5f, -knobR + 3.0f, pw, knobR * 0.55f, pw * 0.5f);
            pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
            g.setColour (accent.brighter (0.4f));
            g.fillPath (pointer);
        }

        //==========================================================================
        void drawComboBox (juce::Graphics& g, int width, int height, bool,
                           int, int, int, int, juce::ComboBox& box) override
        {
            auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (1.0f);

            g.setColour (findColour (juce::ComboBox::backgroundColourId));
            g.fillRoundedRectangle (bounds, 6.0f);

            g.setColour (box.findColour (juce::ComboBox::outlineColourId));
            g.drawRoundedRectangle (bounds, 6.0f, 1.4f);

            juce::Path arrow;
            const auto ax = (float) width - 18.0f;
            const auto ay = (float) height * 0.5f;
            arrow.startNewSubPath (ax, ay - 2.5f);
            arrow.lineTo (ax + 6.0f, ay + 3.5f);
            arrow.lineTo (ax + 12.0f, ay - 2.5f);
            g.setColour (box.findColour (juce::ComboBox::arrowColourId));
            g.strokePath (arrow, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                             juce::PathStrokeType::rounded));
        }

        //==========================================================================
        void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                               bool shouldDrawButtonAsHighlighted, bool) override
        {
            auto bounds = button.getLocalBounds().toFloat().reduced (2.0f);

            // "lit" = the control is engaged. Bypass inverts (on == bypassed == dim);
            // set the "invertOnState" property on such buttons.
            const bool on = button.getToggleState();
            const bool invert = (bool) button.getProperties().getWithDefault ("invertOnState", false);
            const bool lit = invert ? ! on : on;
            const auto accent = lit ? theme::neonGreen : theme::textDim;

            g.setColour (theme::panel);
            g.fillRoundedRectangle (bounds, 6.0f);
            g.setColour (accent.withAlpha (shouldDrawButtonAsHighlighted ? 0.9f : 0.6f));
            g.drawRoundedRectangle (bounds, 6.0f, 1.4f);

            // Power glyph.
            auto c = bounds.getCentre();
            const auto r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.26f;
            juce::Path ring;
            ring.addCentredArc (c.x, c.y, r, r, 0.0f,
                                juce::degreesToRadians (35.0f),
                                juce::degreesToRadians (325.0f), true);
            g.setColour (accent);
            g.strokePath (ring, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved,
                                                            juce::PathStrokeType::rounded));
            g.drawLine (c.x, c.y - r - 2.0f, c.x, c.y - 1.0f, 2.2f);
        }
    };
}
