/*
    ==============================================================================

    Pickle Power 🥒⚡
    BrineSaturator.h

    Multi-flavour waveshaping saturation. Each "brine" reshapes the transfer
    curve so it generates a different harmonic fingerprint:

        Dill    - gentle, soft odd harmonics (tanh)
        Kosher  - balanced cubic soft-clip
        Garlic  - asymmetric, even-harmonic warmth (biased tanh + DC block)
        Spicy   - aggressive: hard-edged tanh/clip blend, biting odd harmonics

    The Brine amount (0..1) drives the input into the curve; an online loudness
    matcher keeps the shaped signal at roughly the input level so the control adds
    grit rather than volume. The result is crossfaded against the dry signal by the
    same amount, so 0 % is transparent.

    ==============================================================================
*/

#pragma once

#include <juce_dsp/juce_dsp.h>
#include "DSPHelpers.h"
#include "../Utils/Constants.h"

namespace pp
{
    class BrineSaturator
    {
    public:
        BrineSaturator() = default;

        void prepare (double sampleRate, int numChannels)
        {
            dcBlockers.assign ((size_t) juce::jmax (1, numChannels), dsp::DCBlocker {});
            levelMatcher.prepare (sampleRate);

            driveSmoothed.reset (sampleRate, 0.02);
            mixSmoothed  .reset (sampleRate, 0.02);
            driveSmoothed.setCurrentAndTargetValue (1.0f);
            mixSmoothed  .setCurrentAndTargetValue (0.0f);
        }

        void reset()
        {
            for (auto& dc : dcBlockers)
                dc.reset();
            levelMatcher.reset();
        }

        /** @param brine01  saturation amount, 0..1
            @param type     brine flavour */
        void setParameters (float brine01, BrineType newType)
        {
            type = newType;
            brine01 = juce::jlimit (0.0f, 1.0f, brine01);

            // 0..1 -> ~1x .. ~30x drive with a musical curve, scaled per flavour.
            const float drive = 1.0f + std::pow (brine01, 1.5f) * 29.0f * flavourDrive (newType);
            driveSmoothed.setTargetValue (drive);
            mixSmoothed.setTargetValue (brine01);
        }

        void process (juce::dsp::AudioBlock<float>& block)
        {
            const auto numCh = block.getNumChannels();
            const auto numS  = block.getNumSamples();
            if (numCh == 0) return;
            const float inv = 1.0f / (float) numCh;

            for (size_t s = 0; s < numS; ++s)
            {
                const float drive = driveSmoothed.getNextValue();
                const float mix   = mixSmoothed.getNextValue();
                const float mk    = levelMatcher.makeup;

                float inMono = 0.0f, outMono = 0.0f;
                for (size_t ch = 0; ch < numCh; ++ch)
                {
                    auto* d = block.getChannelPointer (ch);
                    const float x = d[s];
                    float wet = shape (x * drive);
                    wet = dcBlockers[ch % dcBlockers.size()].process (wet);

                    d[s] = x + mix * (wet * mk - x);
                    inMono  += std::abs (x);
                    outMono += std::abs (wet);
                }
                levelMatcher.update (inMono * inv, outMono * inv);
            }
        }

    private:
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
                    // Asymmetric bias -> rich even harmonics; DC block cleans up.
                    return std::tanh (driven + garlicBias) - garlicOffset;

                case BrineType::Spicy:
                {
                    // Hard-edged: tanh blended with a hard clip for biting odd
                    // harmonics (band-limited by the oversampled core; no aliasy sin).
                    const float soft = std::tanh (driven * 1.6f);
                    const float hard = juce::jlimit (-1.0f, 1.0f, driven * 1.6f);
                    return 0.82f * soft + 0.18f * hard;
                }

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

        static float flavourDrive (BrineType t) noexcept
        {
            switch (t)
            {
                case BrineType::Dill:     return 0.70f;   // gentle
                case BrineType::Kosher:   return 1.00f;   // classic
                case BrineType::Garlic:   return 1.10f;   // warm + pushed
                case BrineType::Spicy:    return 1.40f;   // aggressive
                case BrineType::numTypes:
                default:                  return 1.00f;
            }
        }

        //==========================================================================
        BrineType type = BrineType::Kosher;
        std::vector<dsp::DCBlocker> dcBlockers;
        dsp::LevelMatcher levelMatcher;

        juce::SmoothedValue<float> driveSmoothed, mixSmoothed;

        static constexpr float garlicBias   = 0.5f;
        inline static const float garlicOffset = std::tanh (0.5f);

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BrineSaturator)
    };
}
