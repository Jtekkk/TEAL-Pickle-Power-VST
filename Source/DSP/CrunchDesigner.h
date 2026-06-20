/*
    ==============================================================================

    Pickle Power 🥒⚡
    CrunchDesigner.h

    Transient designer. A fast and a slow envelope follower track the signal; the
    difference between them is a transient detector. The Crunch control (-1..+1)
    maps that detector to a gain modulation:

        Crunch > 0   emphasise attacks   -> snappy, "crunchy"
        Crunch < 0   soften attacks      -> rounder, more sustain / glue

    Detection is mono-linked and the resulting gain is applied to every channel
    so the stereo image stays put.

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

            attFast = tc (0.5f);    relFast = tc (60.0f);
            attSlow = tc (18.0f);   relSlow = tc (140.0f);

            amountSmoothed.reset (sampleRate, 0.02);
            amountSmoothed.setCurrentAndTargetValue (0.0f);

            reset();
        }

        void reset()
        {
            fastEnv = slowEnv = 0.0f;
        }

        /** @param crunch  -1..+1 */
        void setParameters (float crunch)
        {
            amountSmoothed.setTargetValue (juce::jlimit (-1.0f, 1.0f, crunch));
        }

        void process (juce::dsp::AudioBlock<float>& block)
        {
            const auto numCh = block.getNumChannels();
            const auto numS  = block.getNumSamples();

            for (size_t s = 0; s < numS; ++s)
            {
                // Mono-linked detection: peak across channels.
                float in = 0.0f;
                for (size_t ch = 0; ch < numCh; ++ch)
                    in = juce::jmax (in, std::abs (block.getChannelPointer (ch)[s]));

                fastEnv = follow (fastEnv, in, attFast, relFast);
                slowEnv = follow (slowEnv, in, attSlow, relSlow);

                const float transient = fastEnv - slowEnv;            // >0 on attacks
                const float shaped    = std::tanh (transient * sensitivity);
                const float crunch    = amountSmoothed.getNextValue();

                const float gainDb = crunch * shaped * maxBoostDb;
                const float gain   = juce::Decibels::decibelsToGain (gainDb);

                for (size_t ch = 0; ch < numCh; ++ch)
                    block.getChannelPointer (ch)[s] *= gain;
            }
        }

    private:
        float tc (float ms) const noexcept
        {
            return (float) std::exp (-1.0 / (sampleRate * (ms * 0.001)));
        }

        static float follow (float env, float in, float att, float rel) noexcept
        {
            const float coeff = (in > env) ? att : rel;
            return coeff * env + (1.0f - coeff) * in;
        }

        double sampleRate = 44100.0;
        float  attFast = 0.0f, relFast = 0.0f, attSlow = 0.0f, relSlow = 0.0f;
        float  fastEnv = 0.0f, slowEnv = 0.0f;

        juce::SmoothedValue<float> amountSmoothed;

        static constexpr float sensitivity = 7.0f;
        static constexpr float maxBoostDb  = 12.0f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrunchDesigner)
    };
}
