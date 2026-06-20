/*
    ==============================================================================

    Pickle Power 🥒⚡
    DancingPickleComponent.h

    The pickle is not decorative — it reacts to the audio. The editor pushes the
    current output level (and clip / nuclear flags) every frame via update(); the
    pickle squashes on transients, bobs and grins harder with level, glows red
    when the limiter is clamping, and spins into a full routine in Nuclear mode.

    ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Utils/Theme.h"

namespace pp
{
    class DancingPickleComponent : public juce::Component
    {
    public:
        DancingPickleComponent() = default;

        /** Called once per UI frame by the editor.
            @param level      0..1 output level
            @param clipping   limiter is clamping / output hot
            @param nuclear    nuclear mode engaged */
        void update (float level, bool isClipping, bool isNuclear)
        {
            level = juce::jlimit (0.0f, 1.0f, level);

            // Rising-edge transient detection -> bounce impulse.
            const float delta = level - prevLevel;
            prevLevel = level;
            if (delta > 0.12f)
                squashImpulse = juce::jmin (0.5f, squashImpulse + delta * 1.4f);

            energy += (level - energy) * 0.18f;
            squashImpulse *= 0.82f;

            animPhase += 0.06f + energy * 0.35f;
            if (animPhase > juce::MathConstants<float>::twoPi * 64.0f)
                animPhase -= juce::MathConstants<float>::twoPi * 64.0f;

            armPhase += 0.10f + energy * 0.6f;

            if (isNuclear) spin += 0.04f + energy * 0.10f;
            else           spin *= 0.90f;

            clipping = isClipping;
            nuclear  = isNuclear;

            repaint();
        }

        void reset()
        {
            energy = squashImpulse = animPhase = armPhase = spin = prevLevel = 0.0f;
            clipping = nuclear = false;
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            auto area = getLocalBounds().toFloat().reduced (8.0f);
            const auto cx = area.getCentreX();
            const auto cy = area.getCentreY();

            const float bob    = std::sin (animPhase) * (3.0f + energy * 14.0f);
            const float squash = squashImpulse + std::sin (animPhase * 2.0f) * energy * 0.12f;

            juce::Graphics::ScopedSaveState save (g);
            g.addTransform (juce::AffineTransform::translation (0.0f, bob));
            if (std::abs (spin) > 0.001f)
                g.addTransform (juce::AffineTransform::rotation (std::sin (spin) * 0.5f, cx, cy));

            const float bw = area.getWidth()  * 0.55f * (1.0f - squash * 0.30f);
            const float bh = area.getHeight() * 0.82f * (1.0f + squash * 0.30f);
            auto body = juce::Rectangle<float> (bw, bh).withCentre ({ cx, cy });

            drawArmsAndLegs (g, body);
            drawBody (g, body);
            drawFace (g, body);

            if (nuclear)
                drawNuclearGlow (g, body);
        }

    private:
        //==========================================================================
        void drawBody (juce::Graphics& g, juce::Rectangle<float> body)
        {
            juce::Path p;
            p.addRoundedRectangle (body.getX(), body.getY(), body.getWidth(), body.getHeight(),
                                   body.getWidth() * 0.45f);

            g.setGradientFill (juce::ColourGradient (theme::pickleLight, body.getTopLeft(),
                                                     theme::pickleDark,  body.getBottomRight(), false));
            g.fillPath (p);

            // Pickle bumps.
            juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (p);
            g.setColour (theme::pickleBump.withAlpha (0.55f));
            const float positions[][2] = {
                { 0.30f, 0.18f }, { 0.66f, 0.27f }, { 0.40f, 0.42f },
                { 0.70f, 0.55f }, { 0.32f, 0.66f }, { 0.60f, 0.78f }, { 0.45f, 0.88f }
            };
            for (auto& pos : positions)
            {
                const float r = body.getWidth() * 0.07f;
                g.fillEllipse (body.getX() + pos[0] * body.getWidth()  - r,
                               body.getY() + pos[1] * body.getHeight() - r, r * 2.0f, r * 2.0f);
            }

            // Soft highlight stripe.
            auto hl = body.withWidth (body.getWidth() * 0.22f)
                          .withX (body.getX() + body.getWidth() * 0.16f)
                          .reduced (0.0f, body.getHeight() * 0.10f);
            g.setColour (theme::pickleLight.withAlpha (0.35f));
            g.fillRoundedRectangle (hl, hl.getWidth() * 0.5f);

            // Neon outline.
            g.setColour (nuclear ? theme::nuclear : theme::neonGreen);
            g.strokePath (p, juce::PathStrokeType (2.5f));
        }

        void drawFace (juce::Graphics& g, juce::Rectangle<float> body)
        {
            const float eyeY = body.getY() + body.getHeight() * 0.30f;
            const float eyeDX = body.getWidth() * 0.18f;
            const float eyeR  = body.getWidth() * 0.11f;
            const float cx = body.getCentreX();

            const float pupilOffset = std::sin (animPhase) * eyeR * 0.25f;

            for (int s : { -1, 1 })
            {
                const float ex = cx + s * eyeDX;

                if (clipping)
                {
                    g.setColour (theme::danger.withAlpha (0.45f));
                    g.fillEllipse (ex - eyeR * 1.5f, eyeY - eyeR * 1.5f, eyeR * 3.0f, eyeR * 3.0f);
                }

                g.setColour (juce::Colours::white);
                g.fillEllipse (ex - eyeR, eyeY - eyeR, eyeR * 2.0f, eyeR * 2.0f);

                g.setColour (clipping ? theme::danger : juce::Colours::black);
                const float pr = eyeR * 0.5f;
                g.fillEllipse (ex - pr + pupilOffset, eyeY - pr, pr * 2.0f, pr * 2.0f);
            }

            // Mouth: a grin that opens up with energy.
            const float mouthY = body.getY() + body.getHeight() * 0.52f;
            const float mouthW = body.getWidth() * 0.34f;
            const float open   = 0.2f + energy * 0.8f;
            juce::Path mouth;
            mouth.startNewSubPath (cx - mouthW * 0.5f, mouthY);
            mouth.quadraticTo (cx, mouthY + mouthW * open, cx + mouthW * 0.5f, mouthY);
            if (open > 0.6f)
                mouth.quadraticTo (cx, mouthY + mouthW * open * 0.4f, cx - mouthW * 0.5f, mouthY);

            g.setColour (theme::pickleDark.darker (0.5f));
            g.fillPath (mouth);
            g.setColour (theme::neonLime.withAlpha (0.8f));
            g.strokePath (mouth, juce::PathStrokeType (2.0f));

            // Nuclear shades.
            if (nuclear)
            {
                g.setColour (juce::Colours::black);
                for (int s : { -1, 1 })
                {
                    const float ex = cx + s * eyeDX;
                    g.fillRoundedRectangle (ex - eyeR * 1.4f, eyeY - eyeR, eyeR * 2.8f, eyeR * 1.8f, 3.0f);
                }
                g.fillRect (cx - eyeDX * 0.3f, eyeY - eyeR * 0.2f, eyeDX * 0.6f, 3.0f);
            }
        }

        void drawArmsAndLegs (juce::Graphics& g, juce::Rectangle<float> body)
        {
            g.setColour (nuclear ? theme::nuclear : theme::pickleBody);
            const auto stroke = juce::PathStrokeType (5.0f, juce::PathStrokeType::curved,
                                                            juce::PathStrokeType::rounded);

            const float swing = std::sin (armPhase) * (0.4f + energy * 1.0f);

            // Arms.
            for (int s : { -1, 1 })
            {
                const float sx = body.getCentreX() + s * body.getWidth() * 0.45f;
                const float sy = body.getY() + body.getHeight() * 0.38f;
                juce::Path arm;
                arm.startNewSubPath (sx, sy);
                arm.lineTo (sx + s * body.getWidth() * 0.30f,
                            sy - std::sin (armPhase + (s > 0 ? 0.0f : juce::MathConstants<float>::pi))
                                 * body.getHeight() * 0.18f * (0.5f + energy));
                g.strokePath (arm, stroke);
            }

            // Legs.
            for (int s : { -1, 1 })
            {
                const float sx = body.getCentreX() + s * body.getWidth() * 0.22f;
                const float sy = body.getBottom() - body.getHeight() * 0.02f;
                juce::Path leg;
                leg.startNewSubPath (sx, sy);
                leg.lineTo (sx + s * body.getWidth() * 0.12f + swing * body.getWidth() * 0.10f,
                            sy + body.getHeight() * 0.16f);
                g.strokePath (leg, stroke);
            }
        }

        void drawNuclearGlow (juce::Graphics& g, juce::Rectangle<float> body)
        {
            for (int i = 3; i >= 1; --i)
            {
                g.setColour (theme::nuclear.withAlpha (0.10f));
                auto glow = body.expanded ((float) i * 6.0f);
                g.drawRoundedRectangle (glow, glow.getWidth() * 0.45f, 4.0f);
            }
        }

        //==========================================================================
        float energy = 0.0f, squashImpulse = 0.0f, animPhase = 0.0f, armPhase = 0.0f;
        float spin = 0.0f, prevLevel = 0.0f;
        bool  clipping = false, nuclear = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DancingPickleComponent)
    };
}
