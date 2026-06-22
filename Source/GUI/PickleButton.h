/*
    ==============================================================================

    Pickle Power 🥒⚡
    PickleButton.h

    A tiny pickle-shaped button (sits next to the title). Click it to play the
    embedded jingle. Squashes when pressed, glows on hover.

    ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Utils/Theme.h"

namespace pp
{
    class PickleButton : public juce::Button
    {
    public:
        PickleButton() : juce::Button ("pickle") {}

        void paintButton (juce::Graphics& g, bool highlighted, bool down) override
        {
            auto area = getLocalBounds().toFloat().reduced (2.0f);

            if (highlighted || down)
            {
                g.setColour (theme::neonGreen.withAlpha (down ? 0.40f : 0.22f));
                g.fillEllipse (area.expanded (1.0f));
            }

            const float squash = down ? 0.12f : 0.0f;
            auto body = area.reduced (area.getWidth() * 0.20f, area.getHeight() * 0.04f)
                            .reduced (0.0f, area.getHeight() * squash);

            juce::Path p;
            p.addRoundedRectangle (body, body.getWidth() * 0.45f);

            g.setGradientFill (juce::ColourGradient (theme::pickleLight, body.getTopLeft(),
                                                     theme::pickleDark,  body.getBottomRight(), false));
            g.fillPath (p);
            g.setColour (theme::neonGreen.withAlpha (0.9f));
            g.strokePath (p, juce::PathStrokeType (1.4f));

            // A couple of warts + a highlight so it reads as a pickle.
            g.setColour (theme::pickleBump.withAlpha (0.7f));
            const float r = body.getWidth() * 0.12f;
            g.fillEllipse (body.getCentreX() - r * 1.4f, body.getY() + body.getHeight() * 0.30f, r, r);
            g.fillEllipse (body.getCentreX() + r * 0.4f, body.getY() + body.getHeight() * 0.55f, r, r);
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.fillRoundedRectangle (body.getX() + body.getWidth() * 0.22f, body.getY() + body.getHeight() * 0.12f,
                                    body.getWidth() * 0.16f, body.getHeight() * 0.5f, body.getWidth() * 0.08f);
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PickleButton)
    };
}
