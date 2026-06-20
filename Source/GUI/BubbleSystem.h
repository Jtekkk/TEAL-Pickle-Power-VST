/*
    ==============================================================================

    Pickle Power 🥒⚡
    BubbleSystem.h

    A tiny particle system of brine bubbles that rise inside the jar. Spawn rate
    and rise speed scale with the audio level, so the jar fizzes harder when the
    signal is hot.

    ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Utils/Theme.h"

namespace pp
{
    class BubbleSystem
    {
    public:
        BubbleSystem() { bubbles.reserve (64); }

        /** @param dt          seconds since last update
            @param intensity   0..1 audio level
            @param area        region (in local coords) the bubbles live in */
        void update (float dt, float intensity, juce::Rectangle<float> area)
        {
            spawnAccum += dt * (3.0f + intensity * 40.0f);
            while (spawnAccum >= 1.0f && bubbles.size() < 80)
            {
                spawnAccum -= 1.0f;
                spawn (intensity, area);
            }

            for (auto& b : bubbles)
            {
                b.y    -= b.speed * dt * (1.0f + intensity * 1.5f);
                b.x    += std::sin (b.y * 0.05f + b.wobblePhase) * b.wobble * dt;
                b.life -= dt;
            }

            bubbles.erase (std::remove_if (bubbles.begin(), bubbles.end(),
                                           [area] (const Bubble& b)
                                           {
                                               return b.life <= 0.0f || b.y < area.getY() - 10.0f;
                                           }),
                           bubbles.end());
        }

        void draw (juce::Graphics& g) const
        {
            for (const auto& b : bubbles)
            {
                const float a = juce::jlimit (0.0f, 1.0f, b.life) * 0.7f;
                g.setColour (theme::bubble.withAlpha (a));
                g.fillEllipse (b.x - b.size, b.y - b.size, b.size * 2.0f, b.size * 2.0f);
                g.setColour (theme::neonLime.withAlpha (a * 0.5f));
                g.drawEllipse (b.x - b.size, b.y - b.size, b.size * 2.0f, b.size * 2.0f, 0.8f);
            }
        }

        void clear() { bubbles.clear(); spawnAccum = 0.0f; }

    private:
        struct Bubble
        {
            float x = 0, y = 0, size = 2, speed = 20, wobble = 0, wobblePhase = 0, life = 1;
        };

        void spawn (float intensity, juce::Rectangle<float> area)
        {
            Bubble b;
            b.x = area.getX() + rng.nextFloat() * area.getWidth();
            b.y = area.getBottom() - rng.nextFloat() * 8.0f;
            b.size = 1.2f + rng.nextFloat() * (2.0f + intensity * 3.0f);
            b.speed = 18.0f + rng.nextFloat() * 35.0f;
            b.wobble = 4.0f + rng.nextFloat() * 12.0f;
            b.wobblePhase = rng.nextFloat() * juce::MathConstants<float>::twoPi;
            b.life = 1.6f + rng.nextFloat() * 1.5f;
            bubbles.push_back (b);
        }

        std::vector<Bubble> bubbles;
        juce::Random rng;
        float spawnAccum = 0.0f;
    };
}
