/*
    ==============================================================================

    Pickle Power 🥒⚡
    CrunchDesigner.h

    Transient designer with independent Attack and Sustain control (SPL-style).

    Two pairs of envelope followers run on the mono-linked detection signal:

      • Attack pair  - a fast and a slow *attack* follower. Their positive
        difference spikes at note onsets, giving an attack/transient detector.
      • Sustain pair - a fast-release and a slow-release follower. The slow one
        lags above the fast one during the decay/body, giving a sustain detector.

    Attack (-1..+1) and Sustain (-1..+1) scale dB gain applied around those two
    regions; the gain is mono-linked so the stereo image is preserved. At 0/0 the
    stage is transparent.

    ==============================================================================
*/

#pragma once

#include <juce_dsp/juce_dsp.h>

namespace pp
{
    class CrunchDesigner
    {
    public:
        CrunchDesigner() = default;

        void prepare (double newSampleRate, int /*numChannels*/)
        {
            sampleRate = newSampleRate;

            aFastC = tc (0.3f);   aSlowC = tc (8.0f);    attRelC = tc (90.0f);
            sAttC  = tc (5.0f);   sFastRelC = tc (45.0f); sSlowRelC = tc (320.0f);

            attackSmoothed.reset (sampleRate, 0.02);
            sustainSmoothed.reset (sampleRate, 0.02);
            attackSmoothed.setCurrentAndTargetValue (0.0f);
            sustainSmoothed.setCurrentAndTargetValue (0.0f);

            reset();
        }

        void reset()
        {
            aFast = aSlow = sFast = sSlow = 0.0f;
        }

        /** @param attack   -1..+1
            @param sustain  -1..+1 */
        void setParameters (float attack, float sustain)
        {
            attackSmoothed.setTargetValue (juce::jlimit (-1.0f, 1.0f, attack));
            sustainSmoothed.setTargetValue (juce::jlimit (-1.0f, 1.0f, sustain));
        }

        void process (juce::dsp::AudioBlock<float>& block)
        {
            const auto numCh = block.getNumChannels();
            const auto numS  = block.getNumSamples();

            for (size_t s = 0; s < numS; ++s)
            {
                float in = 0.0f;
                for (size_t ch = 0; ch < numCh; ++ch)
                    in = juce::jmax (in, std::abs (block.getChannelPointer (ch)[s]));

                // Attack detector (fast vs slow attack).
                aFast = follow (aFast, in, aFastC, attRelC);
                aSlow = follow (aSlow, in, aSlowC, attRelC);
                const float attackTransient = juce::jmax (0.0f, aFast - aSlow);

                // Sustain detector (slow release lags above fast release on the tail).
                sFast = follow (sFast, in, sAttC, sFastRelC);
                sSlow = follow (sSlow, in, sAttC, sSlowRelC);
                const float sustainBody = juce::jmax (0.0f, sSlow - sFast);

                const float attack  = attackSmoothed.getNextValue();
                const float sustain = sustainSmoothed.getNextValue();

                const float gainDb = attack  * std::tanh (attackTransient * sens) * maxAttDb
                                   + sustain * std::tanh (sustainBody     * sens) * maxSusDb;
                const float gain = juce::Decibels::decibelsToGain (gainDb);

                for (size_t ch = 0; ch < numCh; ++ch)
                    block.getChannelPointer (ch)[s] *= gain;
            }
        }

    private:
        float tc (float ms) const noexcept
        {
            return (float) std::exp (-1.0 / (sampleRate * (ms * 0.001)));
        }

        static float follow (float env, float in, float attC, float relC) noexcept
        {
            const float c = (in > env) ? attC : relC;
            return c * env + (1.0f - c) * in;
        }

        double sampleRate = 44100.0;
        float  aFastC = 0, aSlowC = 0, attRelC = 0, sAttC = 0, sFastRelC = 0, sSlowRelC = 0;
        float  aFast = 0, aSlow = 0, sFast = 0, sSlow = 0;

        juce::SmoothedValue<float> attackSmoothed, sustainSmoothed;

        static constexpr float sens     = 8.0f;
        static constexpr float maxAttDb = 15.0f;   // SPL Transient Designer attack range
        static constexpr float maxSusDb = 24.0f;   // SPL Transient Designer sustain range

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrunchDesigner)
    };
}
