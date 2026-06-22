/*
    ==============================================================================

    Pickle Power 🥒⚡  —  PRO
    DynamicEQ.h

    Two dynamic EQ bands. Each band is a Cytomic TPT (topology-preserving
    transform) state-variable **bell** filter whose gain G is modulated per sample
    by a level detector keyed on the band (a separate constant-0 dB band-pass
    sidechain → envelope → gain computer). The SVF bell models both **boost and
    cut** correctly with the true peaking Q (unlike the cheap peak = x+(G-1)*BP
    identity, which mis-models cuts and needs a gain-dependent Q correction), and
    is well-behaved under per-sample gain modulation and in single precision.

        Range > 0  -> boost the band when it exceeds the threshold (upward)
        Range < 0  -> cut the band when it exceeds the threshold (downward / de-ess)

    Detection is mono-linked. At Range = 0 the stage is exactly transparent.

    Ref: Cytomic "SvfLinearTrapOptimised2"; RBJ Audio-EQ-Cookbook (detection BPF).

    ==============================================================================
*/

#pragma once

#include <juce_dsp/juce_dsp.h>

namespace pp
{
    class DynamicEQ
    {
    public:
        static constexpr int numBands = 2;

        DynamicEQ() = default;

        void prepare (double newSampleRate, int channels)
        {
            sampleRate = newSampleRate;
            numCh = juce::jmax (1, channels);

            attC  = tc (5.0f);
            relC  = tc (90.0f);
            gainC = tc (8.0f);

            for (auto& b : bands)
            {
                b.s1.assign ((size_t) numCh, 0.0f);
                b.s2.assign ((size_t) numCh, 0.0f);
                b.bz1 = b.bz2 = 0.0f;
                b.env = 0.0f;
                b.gainDb = 0.0f;
                updateCoeffs (b);
            }
        }

        void reset()
        {
            for (auto& b : bands)
            {
                std::fill (b.s1.begin(), b.s1.end(), 0.0f);
                std::fill (b.s2.begin(), b.s2.end(), 0.0f);
                b.bz1 = b.bz2 = 0.0f;
                b.env = 0.0f;
                b.gainDb = 0.0f;
            }
        }

        /** Current dynamic gain (dB) applied to band @p i — for UI feedback. */
        float getGainDb (int i) const noexcept { return bands[(size_t) i].gainDb; }

        void setBand (int i, float freqHz, float thresholdDb, float rangeDb)
        {
            auto& b = bands[(size_t) i];
            b.thresholdDb = thresholdDb;
            b.rangeDb = rangeDb;
            if (std::abs (freqHz - b.freq) > 0.5f)
            {
                b.freq = freqHz;
                updateCoeffs (b);
            }
        }

        void process (juce::dsp::AudioBlock<float>& block)
        {
            const auto chs = juce::jmin ((int) block.getNumChannels(), numCh);
            const auto numS = block.getNumSamples();
            if (chs == 0) return;
            const float inv = 1.0f / (float) chs;

            for (size_t s = 0; s < numS; ++s)
            {
                // Mono detection signal from the (current) input.
                float mono = 0.0f;
                for (int ch = 0; ch < chs; ++ch)
                    mono += block.getChannelPointer ((size_t) ch)[s];
                mono *= inv;

                for (auto& b : bands)
                {
                    // --- detection: constant-0 dB band-pass + envelope ---
                    const float bp = b.bb0 * mono + b.bz1;
                    b.bz1 = b.bb1 * mono - b.ba1 * bp + b.bz2;
                    b.bz2 = b.bb2 * mono - b.ba2 * bp;

                    const float rect = std::abs (bp);
                    const float c = (rect > b.env) ? attC : relC;
                    b.env = c * b.env + (1.0f - c) * rect;

                    const float over = juce::Decibels::gainToDecibels (b.env, -100.0f) - b.thresholdDb;
                    const float amt  = juce::jlimit (0.0f, 1.0f, over / 18.0f);
                    b.gainDb = gainC * b.gainDb + (1.0f - gainC) * (b.rangeDb * amt);

                    // --- SVF bell coefficients from the current gain ---
                    const float A  = std::pow (10.0f, b.gainDb / 40.0f);   // A^2 = linear band gain
                    const float k  = 1.0f / (Q * A);
                    const float a1 = 1.0f / (1.0f + b.g * (b.g + k));
                    const float a2 = b.g * a1;
                    const float a3 = b.g * a2;
                    const float m1 = k * (A * A - 1.0f);

                    // --- apply the bell per channel (m0 = 1, m2 = 0) ---
                    for (int ch = 0; ch < chs; ++ch)
                    {
                        auto* d = block.getChannelPointer ((size_t) ch);
                        const float v0 = d[s];
                        const float v3 = v0 - b.s2[(size_t) ch];
                        const float v1 = a1 * b.s1[(size_t) ch] + a2 * v3;
                        const float v2 = b.s2[(size_t) ch] + a2 * b.s1[(size_t) ch] + a3 * v3;
                        b.s1[(size_t) ch] = 2.0f * v1 - b.s1[(size_t) ch];
                        b.s2[(size_t) ch] = 2.0f * v2 - b.s2[(size_t) ch];
                        d[s] = v0 + m1 * v1;
                    }
                }
            }
        }

    private:
        struct Band
        {
            // Detection band-pass (RBJ constant-0 dB, TDF2), single mono state.
            float bb0 = 0, bb1 = 0, bb2 = 0, ba1 = 0, ba2 = 0;
            float bz1 = 0, bz2 = 0;
            // Application SVF bell.
            float g = 0;                       // tan(pi*fc/fs), fixed with freq
            std::vector<float> s1, s2;         // per-channel integrator states
            // Control.
            float env = 0.0f, gainDb = 0.0f;
            float freq = 1000.0f, thresholdDb = -18.0f, rangeDb = 0.0f;
        };

        void updateCoeffs (Band& b)
        {
            const float f  = juce::jlimit (20.0f, (float) (sampleRate * 0.45), b.freq);
            const float w0 = juce::MathConstants<float>::twoPi * f / (float) sampleRate;

            // Detection: RBJ constant-0 dB band-pass.
            const float alpha = std::sin (w0) / (2.0f * Q);
            const float a0 = 1.0f + alpha;
            b.bb0 = alpha / a0;
            b.bb1 = 0.0f;
            b.bb2 = -alpha / a0;
            b.ba1 = (-2.0f * std::cos (w0)) / a0;
            b.ba2 = (1.0f - alpha) / a0;

            // Application: TPT SVF prewarped frequency.
            b.g = std::tan (juce::MathConstants<float>::pi * f / (float) sampleRate);
        }

        float tc (float ms) const noexcept
        {
            return (float) std::exp (-1.0 / (sampleRate * (ms * 0.001)));
        }

        double sampleRate = 44100.0;
        int    numCh = 2;
        float  attC = 0, relC = 0, gainC = 0;
        Band   bands[numBands];

        static constexpr float Q = 1.4f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DynamicEQ)
    };
}
