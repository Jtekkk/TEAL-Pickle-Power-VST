/*
    ==============================================================================

    Pickle Power 🥒⚡
    TruePeakLimiter.h

    Look-ahead brick-wall limiter. The gain reduction is computed from the signal
    a short look-ahead ahead of the audio (delayed through a small ring buffer),
    so the gain can ramp down smoothly *before* a transient arrives. A final hard
    clamp at the ceiling guarantees the output never exceeds it even if the smooth
    ramp can't quite keep up. When the whole nonlinear core runs oversampled the
    inter-sample (true) peaks are well represented at this stage.

    Gain is mono-linked across channels so the stereo image is preserved.

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
            writeIdx = 0;

            attackCoeff  = 1.0f - std::exp (-1.0f / ((float) lookahead * 0.30f));
            releaseCoeff = 1.0f - std::exp (-1.0f / ((float) (sampleRate * 0.100)));

            gain = 1.0f;
            setCeiling (-1.0f);
        }

        void reset()
        {
            for (auto& ch : delay)
                std::fill (ch.begin(), ch.end(), 0.0f);
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
                float inMax = 0.0f;
                for (size_t ch = 0; ch < numCh; ++ch)
                    inMax = juce::jmax (inMax, std::abs (block.getChannelPointer (ch)[s]));

                const float desired = (inMax > ceiling) ? (ceiling / inMax) : 1.0f;

                // Fast attack (move down), slow release (move up).
                const float coeff = (desired < gain) ? attackCoeff : releaseCoeff;
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
        double sampleRate = 44100.0;
        int    lookahead  = 1;
        int    writeIdx   = 0;

        float  attackCoeff = 0.5f, releaseCoeff = 0.01f;
        float  gain = 1.0f, ceiling = 1.0f;

        std::vector<std::vector<float>> delay;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TruePeakLimiter)
    };
}
