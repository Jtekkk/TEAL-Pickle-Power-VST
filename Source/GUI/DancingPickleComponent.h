/*
    ==============================================================================

    Pickle Power 🥒⚡
    DancingPickleComponent.h

    The star of the show. A fully procedural, audio-reactive cartoon pickle:

        • organic warty body with layered shading, rim light and neon outline
        • expressive face (blinking eyes with catch-lights, eyebrows, a mouth
          that opens with energy, a tongue when it really gets going)
        • cartoon gloved hands and sneakers that swing / shuffle to the beat
        • squash-and-stretch + a little hop on transients
        • headbang lean with sustained energy
        • Nuclear mode: shades, devil-horns, glow and spin

    The editor pushes the current level / clip / nuclear flags via update().

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

        /** @param level     0..1 output level
            @param clip      output is hot / limiter clamping
            @param nuke       nuclear mode engaged */
        void update (float level, bool clip, bool nuke)
        {
            level = juce::jlimit (0.0f, 1.0f, level);

            const float delta = level - prevLevel;
            prevLevel = level;
            if (delta > 0.10f)
                squashImpulse = juce::jmin (0.55f, squashImpulse + delta * 1.6f);

            energy += (level - energy) * 0.18f;
            squashImpulse *= 0.84f;

            animPhase += 0.05f + energy * 0.40f;
            armPhase  += 0.09f + energy * 0.70f;
            if (animPhase > juce::MathConstants<float>::twoPi * 128.0f) animPhase -= juce::MathConstants<float>::twoPi * 128.0f;
            if (armPhase  > juce::MathConstants<float>::twoPi * 128.0f) armPhase  -= juce::MathConstants<float>::twoPi * 128.0f;

            if (nuke) spin += 0.035f + energy * 0.10f;
            else      spin *= 0.90f;

            clipping = clip;
            nuclear  = nuke;
            ++frame;

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
            auto stage = getLocalBounds().toFloat().reduced (6.0f);
            const float cx = stage.getCentreX();

            const float bob  = std::sin (animPhase) * (2.0f + energy * 14.0f);
            const float hop  = squashImpulse * 22.0f;
            const float yOff = bob - hop;

            const float squash = squashImpulse * 0.55f + std::sin (animPhase * 2.0f) * energy * 0.10f;
            const float lean   = std::sin (animPhase * 1.3f) * (0.05f + energy * 0.20f);

            const float bw = stage.getWidth()  * 0.46f * (1.0f - squash * 0.22f);
            const float bh = stage.getHeight() * 0.70f * (1.0f + squash * 0.22f);
            const float by = stage.getCentreY() - stage.getHeight() * 0.03f;
            auto body = juce::Rectangle<float> (bw, bh).withCentre ({ cx, by });

            // Ground shadow (stays put, shrinks as the pickle hops up).
            drawGroundShadow (g, cx, stage.getBottom() - 4.0f, bw, yOff);

            juce::Graphics::ScopedSaveState save (g);
            g.addTransform (juce::AffineTransform::translation (0.0f, yOff));
            if (std::abs (spin) > 0.001f)
                g.addTransform (juce::AffineTransform::rotation (std::sin (spin) * 0.45f, cx, by));
            else
                g.addTransform (juce::AffineTransform::rotation (lean * 0.35f, cx, by));

            drawGlow (g, body);
            drawLegs (g, body);
            drawArms (g, body);
            drawBody (g, body, lean * bw);
            drawFace (g, body);
        }

    private:
        //==========================================================================
        juce::Colour neon()    const { return nuclear ? theme::nuclear : theme::neonGreen; }
        juce::Colour armCol()  const { return nuclear ? theme::pickleDark : theme::pickleBody.darker (0.1f); }

        juce::Path buildBody (juce::Rectangle<float> r, float bend) const
        {
            const float cx = r.getCentreX();
            const float hw = r.getWidth() * 0.5f;
            const float top = r.getY(), bot = r.getBottom(), h = r.getHeight();
            const float topX = cx + bend;

            juce::Path p;
            p.startNewSubPath (topX, top);
            p.cubicTo (topX + hw * 1.04f, top + h * 0.06f,
                       cx + hw * 1.00f,   top + h * 0.42f,
                       cx + hw * 0.86f,   bot - h * 0.12f);
            p.quadraticTo (cx + hw * 0.42f, bot, cx, bot);
            p.quadraticTo (cx - hw * 0.42f, bot, cx - hw * 0.86f, bot - h * 0.12f);
            p.cubicTo (cx - hw * 1.00f,   top + h * 0.42f,
                       topX - hw * 1.04f, top + h * 0.06f,
                       topX, top);
            p.closeSubPath();
            return p;
        }

        void drawBody (juce::Graphics& g, juce::Rectangle<float> body, float bend)
        {
            auto p = buildBody (body, bend);

            g.setGradientFill (juce::ColourGradient (theme::pickleLight, body.getX(), body.getY(),
                                                     theme::pickleDark,  body.getX(), body.getBottom(), false));
            g.fillPath (p);

            {
                juce::Graphics::ScopedSaveState s (g);
                g.reduceClipRegion (p);

                // Roundness shading: bright left edge, shadowed right edge.
                juce::ColourGradient round (juce::Colours::white.withAlpha (0.18f), body.getX(), body.getCentreY(),
                                            theme::pickleDark.withAlpha (0.6f),   body.getRight(), body.getCentreY(), false);
                round.addColour (0.45, juce::Colours::transparentBlack);
                g.setGradientFill (round);
                g.fillRect (body.expanded (4.0f));

                drawWarts (g, body);

                // Specular streak (upper-left).
                auto streak = juce::Rectangle<float> (body.getWidth() * 0.15f, body.getHeight() * 0.46f)
                                  .withCentre ({ body.getX() + body.getWidth() * 0.32f,
                                                 body.getY() + body.getHeight() * 0.30f });
                g.setColour (juce::Colours::white.withAlpha (0.16f));
                g.fillEllipse (streak);
            }

            // Neon outline + glow.
            const auto c = neon();
            for (int i = 3; i >= 1; --i)
            {
                g.setColour (c.withAlpha (0.10f));
                g.strokePath (p, juce::PathStrokeType ((float) i * 3.0f));
            }
            g.setColour (c.brighter (0.1f));
            g.strokePath (p, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        void drawWarts (juce::Graphics& g, juce::Rectangle<float> body)
        {
            static const float pts[][2] = {
                { 0.34f, 0.14f }, { 0.64f, 0.20f }, { 0.44f, 0.33f }, { 0.70f, 0.44f },
                { 0.32f, 0.52f }, { 0.58f, 0.62f }, { 0.40f, 0.72f }, { 0.66f, 0.80f }, { 0.50f, 0.90f }
            };
            const float r = body.getWidth() * 0.055f;

            for (auto& pt : pts)
            {
                const float bx = body.getX() + pt[0] * body.getWidth();
                const float by = body.getY() + pt[1] * body.getHeight();

                g.setColour (theme::pickleDark.withAlpha (0.45f));
                g.fillEllipse (bx - r + 1.5f, by - r + 2.0f, r * 2.0f, r * 2.0f);

                juce::ColourGradient rg (theme::pickleBump.brighter (0.5f), bx - r * 0.3f, by - r * 0.3f,
                                         theme::pickleDark, bx + r * 0.7f, by + r * 0.7f, true);
                g.setGradientFill (rg);
                g.fillEllipse (bx - r, by - r, r * 2.0f, r * 2.0f);

                g.setColour (juce::Colours::white.withAlpha (0.5f));
                g.fillEllipse (bx - r * 0.35f, by - r * 0.5f, r * 0.55f, r * 0.55f);
            }
        }

        void drawFace (juce::Graphics& g, juce::Rectangle<float> body)
        {
            const float cx = body.getCentreX();
            const float fy = body.getY() + body.getHeight() * 0.25f;
            const float dx = body.getWidth() * 0.21f;
            const float eyeR = body.getWidth() * 0.14f;
            const bool  blink = (frame % 260) < 7 && ! nuclear;

            // Eyebrows.
            g.setColour (theme::pickleDark.darker (0.4f));
            for (int s : { -1, 1 })
            {
                const float ex = cx + (float) s * dx;
                const float browY = fy - eyeR * 1.55f;
                const float inner = nuclear ? eyeR * 0.7f : 0.0f;   // angry tilt in nuclear
                juce::Path brow;
                brow.startNewSubPath (ex - eyeR * 0.8f, browY + (s < 0 ? inner : 0.0f));
                brow.lineTo (ex + eyeR * 0.8f, browY + (s > 0 ? inner : 0.0f) - (nuclear ? 0.0f : eyeR * 0.2f));
                g.strokePath (brow, juce::PathStrokeType (eyeR * 0.42f, juce::PathStrokeType::curved,
                                                                        juce::PathStrokeType::rounded));
            }

            // Eyes.
            const float pdx = std::sin (animPhase) * eyeR * 0.18f;
            for (int s : { -1, 1 })
            {
                const float ex = cx + (float) s * dx;

                if (clipping)
                {
                    g.setColour (theme::danger.withAlpha (0.45f));
                    g.fillEllipse (ex - eyeR * 1.6f, fy - eyeR * 1.6f, eyeR * 3.2f, eyeR * 3.2f);
                }

                if (blink)
                {
                    g.setColour (juce::Colours::black);
                    juce::Path lid;
                    lid.startNewSubPath (ex - eyeR, fy);
                    lid.quadraticTo (ex, fy + eyeR * 0.4f, ex + eyeR, fy);
                    g.strokePath (lid, juce::PathStrokeType (2.5f));
                    continue;
                }

                g.setColour (juce::Colours::white);
                g.fillEllipse (ex - eyeR, fy - eyeR * 1.1f, eyeR * 2.0f, eyeR * 2.2f);

                const float pr = eyeR * 0.56f;
                g.setColour (clipping ? theme::danger : juce::Colour (0xff10140c));
                g.fillEllipse (ex - pr + pdx, fy - pr + eyeR * 0.15f, pr * 2.0f, pr * 2.0f);

                g.setColour (juce::Colours::white.withAlpha (0.9f));
                g.fillEllipse (ex - pr * 0.3f + pdx, fy - pr * 0.6f, pr * 0.55f, pr * 0.55f);
            }

            // Mouth.
            drawMouth (g, body, cx);

            // Nuclear shades.
            if (nuclear)
                drawShades (g, cx, fy, dx, eyeR);
        }

        void drawMouth (juce::Graphics& g, juce::Rectangle<float> body, float cx)
        {
            const float my = body.getY() + body.getHeight() * 0.50f;
            const float mw = body.getWidth() * 0.40f;
            float open = juce::jlimit (0.0f, 1.0f, energy * 1.3f + squashImpulse * 0.9f);
            if (nuclear) open = juce::jmax (open, 0.7f);

            if (open < 0.14f)
            {
                juce::Path smile;
                smile.startNewSubPath (cx - mw * 0.5f, my);
                smile.quadraticTo (cx, my + mw * (0.35f + open), cx + mw * 0.5f, my);
                g.setColour (theme::pickleDark.darker (0.4f));
                g.strokePath (smile, juce::PathStrokeType (3.2f, juce::PathStrokeType::curved,
                                                                 juce::PathStrokeType::rounded));
                return;
            }

            const float mh = mw * open * 0.85f;
            auto mr = juce::Rectangle<float> (mw, mh).withCentre ({ cx, my + mh * 0.25f });

            juce::Path mouth;
            mouth.startNewSubPath (mr.getX(), mr.getY());
            mouth.quadraticTo (cx, mr.getY() - mh * 0.25f, mr.getRight(), mr.getY());
            mouth.quadraticTo (cx, mr.getBottom() + mh * 0.20f, mr.getX(), mr.getY());
            mouth.closeSubPath();

            g.setColour (juce::Colour (0xff20120f));
            g.fillPath (mouth);

            {   // tongue
                juce::Graphics::ScopedSaveState s (g);
                g.reduceClipRegion (mouth);
                g.setColour (juce::Colour (0xffd2554f));
                g.fillEllipse (cx - mw * 0.28f, mr.getBottom() - mh * 0.55f, mw * 0.56f, mh * 0.8f);
                g.setColour (juce::Colours::white);   // top teeth
                g.fillRect (mr.getX(), mr.getY() - 1.0f, mr.getWidth(), mh * 0.16f);
            }

            g.setColour (theme::neonLime.withAlpha (0.85f));
            g.strokePath (mouth, juce::PathStrokeType (2.2f));
        }

        void drawShades (juce::Graphics& g, float cx, float fy, float dx, float eyeR)
        {
            const float lw = eyeR * 2.5f, lh = eyeR * 2.0f;
            g.setColour (juce::Colour (0xff0b0b0b));
            for (int s : { -1, 1 })
            {
                auto lens = juce::Rectangle<float> (lw, lh).withCentre ({ cx + (float) s * dx, fy });
                g.fillRoundedRectangle (lens, eyeR * 0.4f);
                g.setColour (theme::nuclear.withAlpha (0.8f));
                g.drawRoundedRectangle (lens, eyeR * 0.4f, 1.5f);
                g.setColour (juce::Colour (0xff0b0b0b));
            }
            g.setColour (juce::Colour (0xff0b0b0b));
            g.fillRect (cx - dx, fy - 2.0f, 2.0f * dx, 4.0f);
            g.setColour (juce::Colours::white.withAlpha (0.5f));   // glint
            g.drawLine (cx - dx - lw * 0.4f, fy + lh * 0.3f, cx - dx + lw * 0.1f, fy - lh * 0.3f, 2.0f);
        }

        void drawArms (juce::Graphics& g, juce::Rectangle<float> body)
        {
            const float shoulderY = body.getY() + body.getHeight() * 0.36f;
            const auto stroke = juce::PathStrokeType (body.getWidth() * 0.11f,
                                                      juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
            const float raise = energy;

            for (int s : { -1, 1 })
            {
                const float sx = body.getCentreX() + (float) s * body.getWidth() * 0.40f;
                const float swing = std::sin (armPhase + (s > 0 ? 0.0f : juce::MathConstants<float>::pi))
                                    * (0.3f + energy * 0.8f);

                const float handX = sx + (float) s * body.getWidth() * (0.20f + raise * 0.12f);
                const float handY = shoulderY + body.getHeight() * (0.20f - raise * 0.42f)
                                    - swing * body.getHeight() * 0.12f;

                juce::Path arm;
                arm.startNewSubPath (sx, shoulderY);
                arm.cubicTo (sx + (float) s * body.getWidth() * 0.12f, shoulderY + body.getHeight() * 0.05f,
                             handX - (float) s * body.getWidth() * 0.05f, handY + body.getHeight() * 0.05f,
                             handX, handY);
                g.setColour (armCol());
                g.strokePath (arm, stroke);

                const float hr = body.getWidth() * 0.115f;
                if (nuclear && s > 0)
                {
                    // Devil-horns hand.
                    g.setColour (juce::Colours::white);
                    g.fillEllipse (handX - hr, handY - hr * 0.6f, hr * 2.0f, hr * 1.6f);
                    for (float fxo : { -0.55f, 0.55f })
                        g.fillRoundedRectangle (handX + fxo * hr - hr * 0.22f, handY - hr * 2.1f,
                                                hr * 0.44f, hr * 1.8f, hr * 0.22f);
                }
                else
                {
                    g.setColour (juce::Colours::white);
                    g.fillEllipse (handX - hr, handY - hr, hr * 2.0f, hr * 2.0f);
                }
                g.setColour (armCol().darker (0.3f));
                g.drawEllipse (handX - hr, handY - hr, hr * 2.0f, hr * 2.0f, 1.4f);
            }
        }

        void drawLegs (juce::Graphics& g, juce::Rectangle<float> body)
        {
            const float hipY = body.getBottom() - body.getHeight() * 0.04f;
            const auto stroke = juce::PathStrokeType (body.getWidth() * 0.10f,
                                                      juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

            for (int s : { -1, 1 })
            {
                const float hx = body.getCentreX() + (float) s * body.getWidth() * 0.18f;
                const float step = std::sin (armPhase + (s > 0 ? juce::MathConstants<float>::pi : 0.0f))
                                   * (0.2f + energy * 0.7f);
                const float footX = hx + (float) s * body.getWidth() * 0.04f + step * body.getWidth() * 0.12f;
                const float footY = hipY + body.getHeight() * 0.15f - juce::jmax (0.0f, step) * body.getHeight() * 0.05f;

                juce::Path leg;
                leg.startNewSubPath (hx, hipY);
                leg.quadraticTo (hx + (float) s * body.getWidth() * 0.02f, (hipY + footY) * 0.5f, footX, footY);
                g.setColour (armCol());
                g.strokePath (leg, stroke);

                auto shoe = juce::Rectangle<float> (body.getWidth() * 0.22f, body.getHeight() * 0.055f)
                                .withCentre ({ footX + (float) s * body.getWidth() * 0.05f, footY });
                g.setColour (juce::Colours::white);
                g.fillRoundedRectangle (shoe, shoe.getHeight() * 0.5f);
                g.setColour (neon().withAlpha (0.8f));
                g.drawLine (shoe.getX() + 2.0f, shoe.getCentreY(), shoe.getRight() - 2.0f, shoe.getCentreY(), 1.4f);
            }
        }

        void drawGlow (juce::Graphics& g, juce::Rectangle<float> body)
        {
            const float a = 0.05f + energy * 0.12f + (nuclear ? 0.08f : 0.0f);
            const auto c = neon();
            for (int i = 4; i >= 1; --i)
            {
                g.setColour (c.withAlpha (a * (float) i / 4.0f));
                auto halo = body.expanded ((float) i * 7.0f);
                g.fillRoundedRectangle (halo, halo.getWidth() * 0.5f);
            }
        }

        void drawGroundShadow (juce::Graphics& g, float cx, float groundY, float bw, float yOff)
        {
            const float scale = juce::jlimit (0.45f, 1.0f, 1.0f + yOff * 0.012f);
            const float sw = bw * 1.15f * scale;
            auto sh = juce::Rectangle<float> (sw, sw * 0.16f).withCentre ({ cx, groundY });
            juce::ColourGradient sg (juce::Colours::black.withAlpha (0.40f), sh.getCentreX(), sh.getCentreY(),
                                     juce::Colours::transparentBlack, sh.getRight(), sh.getCentreY(), true);
            g.setGradientFill (sg);
            g.fillEllipse (sh);
        }

        //==========================================================================
        float energy = 0.0f, squashImpulse = 0.0f, animPhase = 0.0f, armPhase = 0.0f;
        float spin = 0.0f, prevLevel = 0.0f;
        int   frame = 0;
        bool  clipping = false, nuclear = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DancingPickleComponent)
    };
}
