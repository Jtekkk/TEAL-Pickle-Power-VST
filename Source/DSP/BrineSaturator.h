/*
    ==============================================================================

    Pickle Power 🥒⚡
    BrineSaturator.h

    Multi-flavour waveshaping saturation with first-order Antiderivative
    Anti-Aliasing (ADAA). Each "brine" reshapes the transfer curve:

        Dill    - gentle tanh (mostly odd harmonics)
        Kosher  - balanced cubic soft-clip
        Garlic  - asymmetric biased tanh (even-harmonic warmth) + DC block
        Spicy   - aggressive tanh/hard-clip blend (biting odd harmonics)

    ADAA replaces f(u) with the divided difference of its antiderivative F1,
        y = (F1(u[n]) - F1(u[n-1])) / (u[n] - u[n-1])
    (with the analytic midpoint fallback f((u[n]+u[n-1])/2) when the denominator is
    tiny), which strongly suppresses the low-frequency aliases the shaper would
    otherwise fold down — complementing the oversampled core. Computed in double
    precision (the divided difference is cancellation-prone).

    An online level matcher keeps the shaped signal at the input level (so the
    control adds grit, not volume) and the result is crossfaded against the dry
    signal by the Brine amount, so 0 % is transparent.

    Ref: Parker, Zavalishin & Le Bivic, DAFx-16; Faust aanl.lib.

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
            const auto n = (size_t) juce::jmax (1, numChannels);
            dcBlockers.assign (n, dsp::DCBlocker {});
            prevU .assign (n, 0.0);
            prevF1.assign (n, 0.0);
            levelMatcher.prepare (sampleRate);

            driveSmoothed.reset (sampleRate, 0.02);
            mixSmoothed  .reset (sampleRate, 0.02);
            driveSmoothed.setCurrentAndTargetValue (1.0f);
            mixSmoothed  .setCurrentAndTargetValue (0.0f);

            reset();
        }

        void reset()
        {
            for (auto& dc : dcBlockers) dc.reset();
            std::fill (prevU.begin(),  prevU.end(),  0.0);
            for (auto& p : prevF1) p = antideriv (0.0);
            levelMatcher.reset();
        }

        /** @param brine01  saturation amount, 0..1
            @param type     brine flavour */
        void setParameters (float brine01, BrineType newType)
        {
            type = newType;
            brine01 = juce::jlimit (0.0f, 1.0f, brine01);
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
                    float wet = adaa ((double) x * (double) drive, ch);
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
        // First-order ADAA of the current flavour's shaper, per channel.
        float adaa (double u, size_t ch) noexcept
        {
            const double F1u = antideriv (u);
            const double du  = u - prevU[ch];
            double y;
            if (std::abs (du) > 1.0e-4)
                y = (F1u - prevF1[ch]) / du;
            else
                y = (double) shape ((float) (0.5 * (u + prevU[ch])));   // analytic midpoint limit

            prevU[ch]  = u;
            prevF1[ch] = F1u;
            return (float) y;
        }

    public:
        // f(u) — the memoryless shaper (public/static so it can be referenced as the
        // non-anti-aliased ground truth, e.g. in tests).
        static float shapeFor (BrineType type, float u) noexcept
        {
            switch (type)
            {
                case BrineType::Dill:   return std::tanh (u);
                case BrineType::Kosher: return cubicSoftClip (u);
                case BrineType::Garlic: return std::tanh (u + garlicBias) - garlicOffset;
                case BrineType::Spicy:
                {
                    const float soft = std::tanh (u * 1.6f);
                    const float hard = juce::jlimit (-1.0f, 1.0f, u * 1.6f);
                    return 0.82f * soft + 0.18f * hard;
                }
                case BrineType::numTypes:
                default:                return std::tanh (u);
            }
        }

        static float driveFor (float brine01, BrineType type) noexcept
        {
            return 1.0f + std::pow (juce::jlimit (0.0f, 1.0f, brine01), 1.5f) * 29.0f * flavourDrive (type);
        }

    private:
        float shape (float u) const noexcept { return shapeFor (type, u); }

        // F1(u) — first antiderivative of f, in double precision.
        double antideriv (double u) const noexcept
        {
            switch (type)
            {
                case BrineType::Dill:   return logcosh (u);
                case BrineType::Kosher: return cubicAntideriv (u);
                case BrineType::Garlic: return logcosh (u + (double) garlicBias) - (double) garlicOffset * u;
                case BrineType::Spicy:
                    return 0.82 * (1.0 / 1.6) * logcosh (1.6 * u) + 0.18 * clipAntideriv (u);
                case BrineType::numTypes:
                default:                return logcosh (u);
            }
        }

        //==========================================================================
        static double logcosh (double u) noexcept
        {
            const double a = std::abs (u);
            return a + std::log1p (std::exp (-2.0 * a)) - 0.6931471805599453;   // − ln 2
        }

        static float cubicSoftClip (float v) noexcept
        {
            if (v >  1.0f) return  2.0f / 3.0f;
            if (v < -1.0f) return -2.0f / 3.0f;
            return v - (v * v * v) / 3.0f;
        }

        static double cubicAntideriv (double u) noexcept   // ∫ cubicSoftClip
        {
            const double a = std::abs (u);
            if (a <= 1.0)
                return u * u * 0.5 - u * u * u * u / 12.0;
            return (2.0 / 3.0) * a - 0.25;                  // f is odd -> F1 is even
        }

        static double clipAntideriv (double u) noexcept     // ∫ hardclip(1.6u)
        {
            const double a = std::abs (u);
            if (a < 0.625)            return 0.8 * u * u;    // ∫1.6u = 0.8u²
            return a - 0.3125;                               // even
        }

        static float flavourDrive (BrineType t) noexcept
        {
            switch (t)
            {
                case BrineType::Dill:     return 0.70f;
                case BrineType::Kosher:   return 1.00f;
                case BrineType::Garlic:   return 1.10f;
                case BrineType::Spicy:    return 1.40f;
                case BrineType::numTypes:
                default:                  return 1.00f;
            }
        }

        //==========================================================================
        BrineType type = BrineType::Kosher;
        std::vector<dsp::DCBlocker> dcBlockers;
        std::vector<double> prevU, prevF1;     // ADAA state per channel
        dsp::LevelMatcher levelMatcher;

        juce::SmoothedValue<float> driveSmoothed, mixSmoothed;

        static constexpr float garlicBias   = 0.5f;
        inline static const float garlicOffset = std::tanh (0.5f);

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BrineSaturator)
    };
}
