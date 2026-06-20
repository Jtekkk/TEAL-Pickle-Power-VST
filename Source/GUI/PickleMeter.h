/*
    ==============================================================================

    Pickle Power 🥒⚡
    PickleMeter.h

    Vertical output meter with a peak hold and a gain-reduction indicator drawn
    from the top. The editor feeds it RMS / peak / GR every frame via update();
    the bar uses ballistic decay so it reads musically.

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

            if (peak >= displayPeak) displayPeak = peak;
            else                     displayPeak *= 0.97f;

            displayGr += (grDb - displayGr) * 0.3f;
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat().reduced (2.0f);

            g.setColour (theme::panel);
            g.fillRoundedRectangle (bounds, 4.0f);

            auto barArea = bounds.reduced (3.0f);

            // RMS bar (bottom-up) with a green->yellow->red gradient.
            const float h = barArea.getHeight() * juce::jlimit (0.0f, 1.0f, toNorm (displayRms));
            auto bar = barArea.withTop (barArea.getBottom() - h);

            juce::ColourGradient grad (theme::neonGreen, barArea.getBottomLeft(),
                                       theme::danger,     barArea.getTopLeft(), false);
            grad.addColour (0.6, theme::neonYellow);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (bar, 2.0f);

            // Peak cap.
            const float py = barArea.getBottom() - barArea.getHeight() * juce::jlimit (0.0f, 1.0f, toNorm (displayPeak));
            g.setColour (theme::textBright);
            g.fillRect (barArea.getX(), py - 1.0f, barArea.getWidth(), 2.0f);

            // Gain reduction from the top.
            if (displayGr > 0.05f)
            {
                const float grH = barArea.getHeight() * juce::jlimit (0.0f, 1.0f, displayGr / 12.0f);
                g.setColour (theme::danger.withAlpha (0.55f));
                g.fillRect (barArea.getX(), barArea.getY(), barArea.getWidth(), grH);
            }

            g.setColour (theme::neonGreen.withAlpha (0.4f));
            g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
        }

    private:
        // Map linear amplitude to a 0..1 meter position over a ~ -48..0 dB range.
        static float toNorm (float lin)
        {
            const float db = juce::Decibels::gainToDecibels (lin, -48.0f);
            return juce::jmap (db, -48.0f, 0.0f, 0.0f, 1.0f);
        }

        float displayRms = 0.0f, displayPeak = 0.0f, displayGr = 0.0f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PickleMeter)
    };
}
