/*
    ==============================================================================

    Pickle Power 🥒⚡
    PickleMeter.h

    Vertical output meter: ballistic RMS bar with a green→yellow→red gradient,
    a peak-hold cap, dB tick marks and a gain-reduction zone drawn from the top.

    ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Utils/Theme.h"

namespace pp
{
    class PickleMeter : public juce::Component
    {
    public:
        PickleMeter() = default;

        /** @param rms   0..1 linear RMS
            @param peak  0..1 linear peak
            @param grDb  gain reduction in dB (>= 0) */
        void update (float rms, float peak, float grDb)
        {
            displayRms = juce::jmax (rms, displayRms * 0.85f);
            displayPeak = (peak >= displayPeak) ? peak : displayPeak * 0.96f;
            displayGr += (grDb - displayGr) * 0.3f;
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat();

            auto labelArea = bounds.removeFromBottom (14.0f);
            g.setColour (theme::textDim);
            g.setFont (juce::Font (juce::FontOptions {}.withHeight (9.5f).withStyle ("Bold")));
            g.drawText ("OUT", labelArea, juce::Justification::centred);

            g.setColour (theme::panel);
            g.fillRoundedRectangle (bounds, 4.0f);

            auto bar = bounds.reduced (3.0f);

            // Gradient column (full height) revealed by the level bar.
            juce::ColourGradient grad (theme::neonGreen, bar.getBottomLeft(),
                                       theme::danger,     bar.getTopLeft(), false);
            grad.addColour (0.55, theme::neonYellow);
            grad.addColour (0.80, juce::Colour (0xffff9d3a));

            const float h = bar.getHeight() * juce::jlimit (0.0f, 1.0f, toNorm (displayRms));
            auto fill = bar.withTop (bar.getBottom() - h);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (fill, 2.0f);

            // dB tick marks.
            g.setFont (juce::Font (juce::FontOptions {}.withHeight (8.0f)));
            for (float db : { 0.0f, -6.0f, -12.0f, -24.0f, -48.0f })
            {
                const float y = bar.getBottom() - bar.getHeight() * juce::jlimit (0.0f, 1.0f, dbToNorm (db));
                g.setColour (theme::textDim.withAlpha (0.5f));
                g.drawLine (bar.getX(), y, bar.getRight(), y, 0.5f);
            }

            // Peak-hold cap.
            const float py = bar.getBottom() - bar.getHeight() * juce::jlimit (0.0f, 1.0f, toNorm (displayPeak));
            g.setColour (displayPeak > 0.94f ? theme::danger : theme::textBright);
            g.fillRect (bar.getX(), py - 1.0f, bar.getWidth(), 2.0f);

            // Gain reduction from the top.
            if (displayGr > 0.05f)
            {
                const float grH = bar.getHeight() * juce::jlimit (0.0f, 1.0f, displayGr / 12.0f);
                g.setColour (theme::danger.withAlpha (0.5f));
                g.fillRect (bar.getX(), bar.getY(), bar.getWidth(), grH);
            }

            g.setColour (theme::neonGreen.withAlpha (0.4f));
            g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
        }

    private:
        static float dbToNorm (float db) { return juce::jmap (db, -48.0f, 0.0f, 0.0f, 1.0f); }
        static float toNorm (float lin)  { return dbToNorm (juce::Decibels::gainToDecibels (lin, -48.0f)); }

        float displayRms = 0.0f, displayPeak = 0.0f, displayGr = 0.0f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PickleMeter)
    };
}
