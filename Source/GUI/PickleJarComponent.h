/*
    ==============================================================================

    Pickle Power 🥒⚡
    PickleJarComponent.h

    The glowing jar that frames the dancing pickle: glass body, lid, a wobbling
    brine fill and rising bubbles (BubbleSystem). The dancing pickle is added as a
    child component on top of the jar by the editor.

    ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Utils/Theme.h"
#include "BubbleSystem.h"

namespace pp
{
    class PickleJarComponent : public juce::Component
    {
    public:
        PickleJarComponent() { setInterceptsMouseClicks (false, false); }

        /** Advance animation. @param level 0..1 audio level. */
        void update (float level)
        {
            energy += (juce::jlimit (0.0f, 1.0f, level) - energy) * 0.15f;
            wobblePhase += 0.08f + energy * 0.2f;

            bubbles.update (1.0f / 60.0f, energy, brineArea());
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            auto jar = jarArea();

            // Outer neon glow.
            for (int i = 4; i >= 1; --i)
            {
                g.setColour (theme::neonGreen.withAlpha (0.05f + energy * 0.04f));
                g.drawRoundedRectangle (jar.expanded ((float) i * 3.0f), theme::cornerRadius + i * 2.0f, 3.0f);
            }

            // Glass body.
            g.setGradientFill (juce::ColourGradient (theme::panel.withAlpha (0.85f), jar.getTopLeft(),
                                                     theme::background.withAlpha (0.85f), jar.getBottomLeft(), false));
            g.fillRoundedRectangle (jar, theme::cornerRadius);

            // Brine fill with a wobbling surface.
            drawBrine (g);

            // Bubbles live in the brine.
            {
                juce::Graphics::ScopedSaveState save (g);
                g.reduceClipRegion (jar.toNearestInt());
                bubbles.draw (g);
            }

            // Glass reflections.
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.fillRoundedRectangle (jar.withWidth (jar.getWidth() * 0.18f).translated (jar.getWidth() * 0.12f, 0.0f), 6.0f);

            // Rim.
            g.setColour (theme::neonGreen.withAlpha (0.7f));
            g.drawRoundedRectangle (jar, theme::cornerRadius, 2.0f);

            // Lid.
            auto lid = lidArea();
            g.setGradientFill (juce::ColourGradient (theme::neonGreen.darker (0.6f), lid.getTopLeft(),
                                                     theme::pickleDark, lid.getBottomLeft(), false));
            g.fillRoundedRectangle (lid, 6.0f);
            g.setColour (theme::neonLime.withAlpha (0.8f));
            g.drawRoundedRectangle (lid, 6.0f, 1.5f);
        }

        void resized() override { bubbles.clear(); }

    private:
        juce::Rectangle<float> jarArea() const
        {
            return getLocalBounds().toFloat().reduced (10.0f).withTrimmedTop (18.0f);
        }

        juce::Rectangle<float> lidArea() const
        {
            auto jar = jarArea();
            return juce::Rectangle<float> (jar.getX() + jar.getWidth() * 0.12f, jar.getY() - 16.0f,
                                           jar.getWidth() * 0.76f, 22.0f);
        }

        juce::Rectangle<float> brineArea() const
        {
            auto jar = jarArea().reduced (4.0f);
            const float fill = 0.55f + energy * 0.25f;
            return jar.withTop (jar.getBottom() - jar.getHeight() * fill);
        }

        void drawBrine (juce::Graphics& g)
        {
            auto brine = brineArea();

            juce::Path liquid;
            liquid.startNewSubPath (brine.getX(), brine.getY());
            const int steps = 24;
            for (int i = 0; i <= steps; ++i)
            {
                const float t = (float) i / (float) steps;
                const float x = brine.getX() + t * brine.getWidth();
                const float y = brine.getY() + std::sin (wobblePhase + t * 6.0f) * (2.0f + energy * 4.0f);
                liquid.lineTo (x, y);
            }
            liquid.lineTo (brine.getRight(), brine.getBottom());
            liquid.lineTo (brine.getX(), brine.getBottom());
            liquid.closeSubPath();

            g.setColour (theme::brineLiquid);
            g.fillPath (liquid);
            g.setColour (theme::neonLime.withAlpha (0.4f));
            g.strokePath (liquid, juce::PathStrokeType (1.5f));
        }

        BubbleSystem bubbles;
        float energy = 0.0f, wobblePhase = 0.0f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PickleJarComponent)
    };
}
