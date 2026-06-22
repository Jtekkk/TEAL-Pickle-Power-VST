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

    //==============================================================================
    /** ITU-R BS.1770 K-weighting filter (per channel): a +4 dB head high-shelf
        (~1.68 kHz) followed by an RLB high-pass (~38 Hz). Designed via RBJ at the
        runtime sample rate (a faithful approximation of the spec curve — for an
        internal loudness match that is all that's needed; perceptually-weighted
        energy tracks the ear far better than flat RMS). */
    struct KWeighting
    {
        void prepare (double sampleRate, int numChannels)
        {
            designShelf (sampleRate, 1681.97, 4.0);
            designHighPass (sampleRate, 38.0, 0.5);
            ch.assign ((size_t) juce::jmax (1, numChannels), ChState {});
        }

        void reset() { for (auto& c : ch) c = ChState {}; }

        float process (int channel, float x) noexcept
        {
            auto& st = ch[(size_t) channel % ch.size()];
            const float y1 = sb0 * x  + st.s1;  st.s1 = sb1 * x  - sa1 * y1 + st.s2;  st.s2 = sb2 * x  - sa2 * y1;
            const float y2 = hb0 * y1 + st.h1;  st.h1 = hb1 * y1 - ha1 * y2 + st.h2;  st.h2 = hb2 * y1 - ha2 * y2;
            return y2;
        }

    private:
        struct ChState { float s1 = 0, s2 = 0, h1 = 0, h2 = 0; };

        void designShelf (double fs, double fc, double dB)   // RBJ high-shelf, slope S = 1
        {
            const double A  = std::pow (10.0, dB / 40.0);
            const double w0 = juce::MathConstants<double>::twoPi * fc / fs;
            const double cw = std::cos (w0);
            const double alpha = std::sin (w0) * 0.5 * std::sqrt ((A + 1.0 / A) * (1.0 / 1.0 - 1.0) + 2.0);
            const double tsa = 2.0 * std::sqrt (A) * alpha;
            const double a0 =        (A + 1.0) - (A - 1.0) * cw + tsa;
            sb0 = (float) ( A * ((A + 1.0) + (A - 1.0) * cw + tsa) / a0);
            sb1 = (float) (-2.0 * A * ((A - 1.0) + (A + 1.0) * cw) / a0);
            sb2 = (float) ( A * ((A + 1.0) + (A - 1.0) * cw - tsa) / a0);
            sa1 = (float) ( 2.0 * ((A - 1.0) - (A + 1.0) * cw) / a0);
            sa2 = (float) (((A + 1.0) - (A - 1.0) * cw - tsa) / a0);
        }

        void designHighPass (double fs, double fc, double Q)
        {
            const double w0 = juce::MathConstants<double>::twoPi * fc / fs;
            const double cw = std::cos (w0);
            const double alpha = std::sin (w0) / (2.0 * Q);
            const double a0 = 1.0 + alpha;
            hb0 = (float) (((1.0 + cw) * 0.5) / a0);
            hb1 = (float) ((-(1.0 + cw)) / a0);
            hb2 = (float) (((1.0 + cw) * 0.5) / a0);
            ha1 = (float) ((-2.0 * cw) / a0);
            ha2 = (float) ((1.0 - alpha) / a0);
        }

        float sb0 = 1, sb1 = 0, sb2 = 0, sa1 = 0, sa2 = 0;
        float hb0 = 1, hb1 = 0, hb2 = 0, ha1 = 0, ha2 = 0;
        std::vector<ChState> ch;
    };
}
