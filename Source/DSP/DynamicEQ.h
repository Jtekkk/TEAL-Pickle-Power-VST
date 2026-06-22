/*
    ==============================================================================

    Pickle Power 🥒⚡  —  PRO
    DynamicEQ.h

    Two dynamic EQ bands. Each band uses the peaking-from-bandpass identity

        peak(x) = x + (G - 1) * BP(x)

    where BP is a constant-0 dB bandpass, so the band gain G can change per sample
    with just a multiply-add (no biquad coefficient recompute). G is driven by the
    level inside the band relative to a threshold:

        Range > 0  -> boost the band when it exceeds the threshold (upward)
        Range < 0  -> cut the band when it exceeds the threshold (downward / de-ess)

    Detection is mono-linked so the stereo image is preserved. At Range = 0 the
    stage is transparent.

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

            attC = tc (5.0f);
            relC = tc (90.0f);
            gainC = tc (8.0f);

            for (auto& b : bands)
            {
                b.z1.assign ((size_t) numCh, 0.0f);
                b.z2.assign ((size_t) numCh, 0.0f);
                b.env = 0.0f;
                b.gainDb = 0.0f;
                updateCoeffs (b);
            }
        }

        void reset()
        {
            for (auto& b : bands)
            {
                std::fill (b.z1.begin(), b.z1.end(), 0.0f);
                std::fill (b.z2.begin(), b.z2.end(), 0.0f);
                b.env = 0.0f;
                b.gainDb = 0.0f;
            }
        }

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

            for (size_t s = 0; s < numS; ++s)
            {
                for (auto& b : bands)
                {
                    float det = 0.0f;
                    float bp[8] = { 0 };

                    for (int ch = 0; ch < chs; ++ch)
                    {
                        const float x = block.getChannelPointer ((size_t) ch)[s];
                        const float y = b.b0 * x + b.z1[(size_t) ch];
                        b.z1[(size_t) ch] = b.b1 * x - b.a1 * y + b.z2[(size_t) ch];
                        b.z2[(size_t) ch] = b.b2 * x - b.a2 * y;
                        bp[ch] = y;
                        det = juce::jmax (det, std::abs (y));
                    }

                    const float c = (det > b.env) ? attC : relC;
                    b.env = c * b.env + (1.0f - c) * det;

                    const float over = juce::Decibels::gainToDecibels (b.env, -100.0f) - b.thresholdDb;
                    const float amt  = juce::jlimit (0.0f, 1.0f, over / 18.0f);
                    const float target = b.rangeDb * amt;
                    b.gainDb = gainC * b.gainDb + (1.0f - gainC) * target;

                    const float g = juce::Decibels::decibelsToGain (b.gainDb) - 1.0f;
                    for (int ch = 0; ch < chs; ++ch)
                        block.getChannelPointer ((size_t) ch)[s] += g * bp[ch];
                }
            }
        }

    private:
        struct Band
        {
            float b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
            std::vector<float> z1, z2;
            float env = 0.0f, gainDb = 0.0f;
            float freq = 1000.0f, thresholdDb = -18.0f, rangeDb = 0.0f;
        };

        void updateCoeffs (Band& b)
        {
            // RBJ constant-0 dB-peak band-pass.
            const float w0 = juce::MathConstants<float>::twoPi
                             * juce::jlimit (20.0f, (float) (sampleRate * 0.45), b.freq) / (float) sampleRate;
            const float alpha = std::sin (w0) / (2.0f * Q);
            const float a0 = 1.0f + alpha;

            b.b0 = alpha / a0;
            b.b1 = 0.0f;
            b.b2 = -alpha / a0;
            b.a1 = (-2.0f * std::cos (w0)) / a0;
            b.a2 = (1.0f - alpha) / a0;
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
