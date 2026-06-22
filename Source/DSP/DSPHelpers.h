/*
    ==============================================================================

    Pickle Power 🥒⚡
    DSPHelpers.h

    Small shared DSP utilities.

    ==============================================================================
*/

#pragma once

#include <juce_dsp/juce_dsp.h>

namespace pp::dsp
{
    //==============================================================================
    /** One-pole DC blocker (removes the offset introduced by asymmetric shapers). */
    struct DCBlocker
    {
        float x1 = 0.0f, y1 = 0.0f;
        static constexpr float R = 0.9975f;

        float process (float x) noexcept
        {
            const float y = x - x1 + R * y1;
            x1 = x;
            y1 = y;
            return y;
        }

        void reset() noexcept { x1 = y1 = 0.0f; }
    };

    //==============================================================================
    /** Online loudness matcher for waveshapers. Tracks the running RMS of the
        input vs the (pre-makeup) shaped signal and exposes a smoothed makeup gain
        so the stage stays roughly loudness-neutral as drive increases — the
        control then changes character rather than level. Mono-linked.

        Usage per sample: read `makeup`, apply it, then call update() with the
        mono input / shaped magnitudes (a one-sample lag, which is inaudible). */
    struct LevelMatcher
    {
        float makeup = 1.0f;

        void prepare (double sampleRate, float ms = 150.0f)
        {
            coeff = (float) std::exp (-1.0 / (sampleRate * (ms * 0.001)));
            reset();
        }

        void reset() noexcept { inSq = outSq = 0.0f; makeup = 1.0f; }

        void update (float inMono, float outMono) noexcept
        {
            inSq  = coeff * inSq  + (1.0f - coeff) * inMono  * inMono;
            outSq = coeff * outSq + (1.0f - coeff) * outMono * outMono;
            if (outSq > 1.0e-9f)
                makeup = juce::jlimit (0.25f, 4.0f, std::sqrt (inSq / outSq));
        }

    private:
        float coeff = 0.0f, inSq = 0.0f, outSq = 0.0f;
    };
}
