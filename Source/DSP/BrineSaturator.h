/*
    ==============================================================================

    Pickle Power 🥒⚡
    BrineSaturator.h

    Multi-flavour waveshaping saturation. Each "brine" reshapes the transfer
    curve so it generates a different harmonic fingerprint:

        Dill    - gentle, mostly odd harmonics (tanh)
        Kosher  - balanced cubic soft-clip
        Garlic  - asymmetric, even-harmonic warmth (biased tanh + DC block)
        Spicy   - aggressive arctan drive, biting highs

    The Brine amount (0..1) drives the input into the curve and crossfades the
    shaped signal back against the dry signal so 0 % is bit-transparent.

    ==============================================================================
*/

#pragma once

#include <juce_dsp/juce_dsp.h>
#include "../Utils/Constants.h"

namespace pp
{
    class BrineSaturator
    {
    public:
        BrineSaturator() = default;

        void prepare (double sampleRate, int numChannels)
        {
            dcBlockers.assign ((size_t) juce::jmax (1, numChannels), DCBlocker {});

            driveSmoothed.reset (sampleRate, 0.02);
            mixSmoothed  .reset (sampleRate, 0.02);
            driveSmoothed.setCurrentAndTargetValue (1.0f);
            mixSmoothed  .setCurrentAndTargetValue (0.0f);
        }

        void reset()
        {
            for (auto& dc : dcBlockers)
                dc.reset();
        }

        /** @param brine01  saturation amount, 0..1
            @param type     brine flavour */
        void setParameters (float brine01, BrineType newType)
        {
            type = newType;
            brine01 = juce::jlimit (0.0f, 1.0f, brine01);

            // 0..1  ->  ~1x .. ~30x drive (≈ +29 dB) with a musical curve.
            const float drive = 1.0f + std::pow (brine01, 1.5f) * 29.0f;
            driveSmoothed.setTargetValue (drive);
            mixSmoothed.setTargetValue (brine01);
        }

        void process (juce::dsp::AudioBlock<float>& block)
        {
            const auto numCh = block.getNumChannels();
            const auto numS  = block.getNumSamples();

            for (size_t s = 0; s < numS; ++s)
            {
                const float drive = driveSmoothed.getNextValue();
                const float mix   = mixSmoothed.getNextValue();
                const float comp  = 1.0f / std::sqrt (juce::jmax (1.0f, drive));

                for (size_t ch = 0; ch < numCh; ++ch)
                {
                    auto* d   = block.getChannelPointer (ch);
                    const float x = d[s];
                    float wet = shape (x * drive) * comp;
                    wet = dcBlockers[ch % dcBlockers.size()].process (wet);
                    d[s] = x + mix * (wet - x);
                }
            }
        }

    private:
        //==========================================================================
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

        //==========================================================================
        float shape (float driven) const noexcept
        {
            switch (type)
            {
                case BrineType::Dill:
                    return std::tanh (driven);

                case BrineType::Kosher:
                    return cubicSoftClip (driven);

                case BrineType::Garlic:
                    // Asymmetric bias generates even harmonics; DC block cleans up.
                    return std::tanh (driven + garlicBias) - garlicOffset;

                case BrineType::Spicy:
                    return juce::jlimit (-1.0f, 1.0f,
                        std::atan (driven * 1.6f) * (2.0f / juce::MathConstants<float>::pi) * 1.15f);

                case BrineType::numTypes:
                default:
                    return std::tanh (driven);
            }
        }

        static float cubicSoftClip (float v) noexcept
        {
            if (v >  1.0f) return  2.0f / 3.0f;
            if (v < -1.0f) return -2.0f / 3.0f;
            return v - (v * v * v) / 3.0f;
        }

        //==========================================================================
        BrineType type = BrineType::Kosher;
        std::vector<DCBlocker> dcBlockers;

        juce::SmoothedValue<float> driveSmoothed, mixSmoothed;

        static constexpr float garlicBias   = 0.35f;
        inline static const float garlicOffset = std::tanh (0.35f);

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BrineSaturator)
    };
}
