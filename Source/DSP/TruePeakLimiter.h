/*
    ==============================================================================

    Pickle Power 🥒⚡
    TruePeakLimiter.h

    Look-ahead brick-wall limiter with **inter-sample (true-peak) detection**.
    The gain reduction is computed from a 4× upsampled estimate of the waveform
    (cubic Catmull-Rom interpolation of the recent segment), so peaks that occur
    *between* samples — which a sample-peak limiter misses and which can sit ≥3 dB
    above the sample peak — drive the gain too. The audio is delayed through a
    look-ahead ring so the gain can ramp down before the peak arrives, and a final
    hard clamp guarantees the output samples never exceed the ceiling. Gain is
    mono-linked across channels so the stereo image is preserved.

    The true-peak estimate is exactly that — an estimate (not the full ITU-R
    BS.1770 FIR); combined with the conservative −1 dBTP default ceiling and the
    hard clamp it keeps the output peak-safe in practice.

    ==============================================================================
*/

#pragma once

#include <juce_dsp/juce_dsp.h>

namespace pp
{
    class TruePeakLimiter
    {
    public:
        TruePeakLimiter() = default;

        void prepare (double newSampleRate, int numChannels)
        {
            sampleRate = newSampleRate;
            const auto numCh = (size_t) juce::jmax (1, numChannels);

            lookahead = juce::jmax (1, (int) std::round (sampleRate * 0.0015));   // 1.5 ms

            delay.assign (numCh, std::vector<float> ((size_t) lookahead, 0.0f));
            hA.assign (numCh, 0.0f);
            hB.assign (numCh, 0.0f);
            hC.assign (numCh, 0.0f);
            writeIdx = 0;

            attackCoeff  = 1.0f - std::exp (-1.0f / ((float) lookahead * 0.30f));
            releaseCoeff = 1.0f - std::exp (-1.0f / ((float) (sampleRate * 0.100)));

            gain = 1.0f;
            setCeiling (-1.0f);   // −1 dBTP
        }

        void reset()
        {
            for (auto& ch : delay)
                std::fill (ch.begin(), ch.end(), 0.0f);
            std::fill (hA.begin(), hA.end(), 0.0f);
            std::fill (hB.begin(), hB.end(), 0.0f);
            std::fill (hC.begin(), hC.end(), 0.0f);
            writeIdx = 0;
            gain = 1.0f;
        }

        void setCeiling (float dbTP)
        {
            ceiling = juce::Decibels::decibelsToGain (dbTP);
        }

        void process (juce::dsp::AudioBlock<float>& block)
        {
            const auto numCh = block.getNumChannels();
            const auto numS  = block.getNumSamples();

            for (size_t s = 0; s < numS; ++s)
            {
                // ---- true-peak (inter-sample) detection, mono-linked ----
                float truePeak = 0.0f;
                for (size_t ch = 0; ch < numCh; ++ch)
                {
                    const float d = block.getChannelPointer (ch)[s];          // x[n]
                    const float a = hA[ch], b = hB[ch], c = hC[ch];           // x[n-3..n-1]

                    // Estimate the peak of the segment between b (x[n-2]) and c (x[n-1])
                    // via Catmull-Rom through (a,b,c,d); cheap 4× inter-sample estimate.
                    float tp = juce::jmax (std::abs (b), std::abs (c));
                    tp = juce::jmax (tp, std::abs (catmull (a, b, c, d, 0.25f)));
                    tp = juce::jmax (tp, std::abs (catmull (a, b, c, d, 0.50f)));
                    tp = juce::jmax (tp, std::abs (catmull (a, b, c, d, 0.75f)));
                    truePeak = juce::jmax (truePeak, tp);

                    hA[ch] = b; hB[ch] = c; hC[ch] = d;
                }

                const float desired = (truePeak > ceiling) ? (ceiling / truePeak) : 1.0f;
                const float coeff = (desired < gain) ? attackCoeff : releaseCoeff;   // fast down, slow up
                gain += (desired - gain) * coeff;

                for (size_t ch = 0; ch < numCh; ++ch)
                {
                    auto* d = block.getChannelPointer (ch);
                    auto& line = delay[ch % delay.size()];

                    const float delayed = line[(size_t) writeIdx];
                    line[(size_t) writeIdx] = d[s];
                    d[s] = juce::jlimit (-ceiling, ceiling, delayed * gain);
                }

                if (++writeIdx >= lookahead)
                    writeIdx = 0;
            }
        }

        float getLatencySamples() const noexcept { return (float) lookahead; }

        /** Current gain reduction in dB (>= 0). */
        float getGainReductionDb() const noexcept
        {
            return -juce::Decibels::gainToDecibels (gain, -60.0f);
        }

    private:
        static float catmull (float p0, float p1, float p2, float p3, float t) noexcept
        {
            const float t2 = t * t, t3 = t2 * t;
            return 0.5f * ((2.0f * p1)
                         + (-p0 + p2) * t
                         + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2
                         + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
        }

        double sampleRate = 44100.0;
        int    lookahead  = 1;
        int    writeIdx   = 0;

        float  attackCoeff = 0.5f, releaseCoeff = 0.01f;
        float  gain = 1.0f, ceiling = 1.0f;

        std::vector<std::vector<float>> delay;
        std::vector<float> hA, hB, hC;   // per-channel input history for interpolation

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TruePeakLimiter)
    };
}
