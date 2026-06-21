/*
    ==============================================================================

    Pickle Power 🥒⚡
    PickleJarComponent.h

    The glowing jar that frames the dancing pickle: a knurled metal lid, glass
    body with highlights, a wobbling brine fill with a bright meniscus, and rising
    bubbles (BubbleSystem). The dancing pickle is added as a child on top.

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

        void update (float level)
        {
            energy += (juce::jlimit (0.0f, 1.0f, level) - energy) * 0.15f;
            wobblePhase += 0.07f + energy * 0.22f;

            bubbles.update (1.0f / 60.0f, energy, brineArea());
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            auto jar = jarArea();

            // Outer neon glow, pulsing with level.
            for (int i = 5; i >= 1; --i)
            {
                g.setColour (theme::neonGreen.withAlpha (0.04f + energy * 0.05f));
                g.drawRoundedRectangle (jar.expanded ((float) i * 3.0f), theme::cornerRadius + (float) i * 2.0f, 3.0f);
            }

            // Glass body.
            g.setGradientFill (juce::ColourGradient (theme::panelLight.withAlpha (0.9f), jar.getTopLeft(),
                                                     theme::background.withAlpha (0.92f), jar.getBottomLeft(), false));
            g.fillRoundedRectangle (jar, theme::cornerRadius);

            drawBrine (g);

            // Bubbles live inside the glass.
            {
                juce::Graphics::ScopedSaveState save (g);
                g.reduceClipRegion (jar.toNearestInt());
                bubbles.draw (g);

                // Glass highlight streaks.
                g.setColour (juce::Colours::white.withAlpha (0.07f));
                g.fillRoundedRectangle (jar.withWidth (jar.getWidth() * 0.14f)
                                            .translated (jar.getWidth() * 0.10f, 6.0f)
                                            .reduced (0.0f, 8.0f), 6.0f);
                g.setColour (juce::Colours::white.withAlpha (0.04f));
                g.fillRoundedRectangle (jar.withWidth (jar.getWidth() * 0.05f)
                                            .translated (jar.getWidth() * 0.80f, 10.0f)
                                            .reduced (0.0f, 14.0f), 4.0f);
            }

            // Inner edge shade for depth + neon rim.
            g.setColour (theme::background.withAlpha (0.5f));
            g.drawRoundedRectangle (jar.reduced (1.5f), theme::cornerRadius - 1.0f, 3.0f);
            g.setColour (theme::neonGreen.withAlpha (0.75f));
            g.drawRoundedRectangle (jar, theme::cornerRadius, 2.0f);

            drawLid (g);
        }

        void resized() override { bubbles.clear(); }

    private:
        juce::Rectangle<float> jarArea() const
        {
            return getLocalBounds().toFloat().reduced (10.0f).withTrimmedTop (22.0f);
        }

        juce::Rectangle<float> lidArea() const
        {
            auto jar = jarArea();
            return juce::Rectangle<float> (jar.getX() + jar.getWidth() * 0.10f, jar.getY() - 20.0f,
                                           jar.getWidth() * 0.80f, 26.0f);
        }

        juce::Rectangle<float> brineArea() const
        {
            auto jar = jarArea().reduced (4.0f);
            const float fill = 0.55f + energy * 0.22f;
            return jar.withTop (jar.getBottom() - jar.getHeight() * fill);
        }

        void drawLid (juce::Graphics& g)
        {
            auto lid = lidArea();

            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff7ea63c), lid.getTopLeft(),
                                                     theme::pickleDark, lid.getBottomLeft(), false));
            g.fillRoundedRectangle (lid, 6.0f);

            // Knurled vertical ridges.
            {
                juce::Graphics::ScopedSaveState s (g);
                g.reduceClipRegion (lid.getSmallestIntegerContainer());
                const int ridges = (int) (lid.getWidth() / 7.0f);
                for (int i = 0; i < ridges; ++i)
                {
                    const float x = lid.getX() + (float) i / (float) ridges * lid.getWidth();
                    g.setColour (juce::Colours::white.withAlpha (0.08f));
                    g.drawLine (x, lid.getY() + 2.0f, x, lid.getBottom() - 2.0f, 1.5f);
                    g.setColour (theme::pickleDark.withAlpha (0.35f));
                    g.drawLine (x + 2.5f, lid.getY() + 2.0f, x + 2.5f, lid.getBottom() - 2.0f, 1.0f);
                }
            }

            g.setColour (theme::neonLime.withAlpha (0.85f));
            g.drawRoundedRectangle (lid, 6.0f, 1.6f);
        }

        void drawBrine (juce::Graphics& g)
        {
            auto brine = brineArea();

            juce::Path liquid;
            liquid.startNewSubPath (brine.getX(), brine.getY());
            const int steps = 28;
            for (int i = 0; i <= steps; ++i)
            {
                const float t = (float) i / (float) steps;
                const float x = brine.getX() + t * brine.getWidth();
                const float y = brine.getY() + std::sin (wobblePhase + t * 6.5f) * (2.0f + energy * 5.0f);
                liquid.lineTo (x, y);
            }
            liquid.lineTo (brine.getRight(), brine.getBottom());
            liquid.lineTo (brine.getX(), brine.getBottom());
            liquid.closeSubPath();

            g.setGradientFill (juce::ColourGradient (theme::brineLiquid.withAlpha (0.35f), brine.getX(), brine.getY(),
                                                     juce::Colour (0x88a7d23a),            brine.getX(), brine.getBottom(), false));
            g.fillPath (liquid);

            // Bright meniscus line along the surface.
            juce::Path surface;
            surface.startNewSubPath (brine.getX(), brine.getY());
            for (int i = 0; i <= steps; ++i)
            {
                const float t = (float) i / (float) steps;
                const float x = brine.getX() + t * brine.getWidth();
                const float y = brine.getY() + std::sin (wobblePhase + t * 6.5f) * (2.0f + energy * 5.0f);
                surface.lineTo (x, y);
            }
            g.setColour (theme::neonLime.withAlpha (0.55f));
            g.strokePath (surface, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved));
        }

        BubbleSystem bubbles;
        float energy = 0.0f, wobblePhase = 0.0f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PickleJarComponent)
    };
}
