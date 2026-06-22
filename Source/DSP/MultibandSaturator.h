/*
    ==============================================================================

    Pickle Power 🥒⚡  —  PRO
    MultibandSaturator.h

    Three-band saturation. Linkwitz-Riley 4th-order crossovers split the signal
    into Low / Mid / High bands (with the low band all-pass-aligned so the bands
    sum flat when undriven); each band gets its own tanh drive and is blended back
    by its amount. At 0 % per band the stage reconstructs the input.

    Sits inside the oversampled core, so the generated harmonics are anti-aliased.

    ==============================================================================
*/

#pragma once

#include <juce_dsp/juce_dsp.h>

namespace pp
{
    class MultibandSaturator
    {
    public:
        MultibandSaturator() = default;

        void prepare (double sampleRate, int numChannels, int maxBlock)
        {
            juce::dsp::ProcessSpec spec { sampleRate,
                                          (juce::uint32) juce::jmax (1, maxBlock),
                                          (juce::uint32) juce::jmax (1, numChannels) };

            for (auto* f : { &lpLow, &hpLow, &lpHigh, &hpHigh, &apHigh })
                f->prepare (spec);

            lpLow .setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
            hpLow .setType (juce::dsp::LinkwitzRileyFilterType::highpass);
            lpHigh.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
            hpHigh.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
            apHigh.setType (juce::dsp::LinkwitzRileyFilterType::allpass);

            lowSm .reset (sampleRate, 0.02);  lowSm .setCurrentAndTargetValue (0.0f);
            midSm .reset (sampleRate, 0.02);  midSm .setCurrentAndTargetValue (0.0f);
            highSm.reset (sampleRate, 0.02);  highSm.setCurrentAndTargetValue (0.0f);

            setCrossovers (200.0f, 2500.0f, true);
            reset();
        }

        void reset()
        {
            for (auto* f : { &lpLow, &hpLow, &lpHigh, &hpHigh, &apHigh })
                f->reset();
        }

        /** @param low,mid,high   0..1 saturation amount per band
            @param freqLow        low/mid crossover (Hz)
            @param freqHigh       mid/high crossover (Hz) */
        void setParameters (float low, float mid, float high, float freqLow, float freqHigh)
        {
            lowSm .setTargetValue (juce::jlimit (0.0f, 1.0f, low));
            midSm .setTargetValue (juce::jlimit (0.0f, 1.0f, mid));
            highSm.setTargetValue (juce::jlimit (0.0f, 1.0f, high));
            setCrossovers (freqLow, freqHigh, false);
        }

        void process (juce::dsp::AudioBlock<float>& block)
        {
            const auto numCh = block.getNumChannels();
            const auto numS  = block.getNumSamples();

            for (size_t s = 0; s < numS; ++s)
            {
                const float aL = lowSm.getNextValue();
                const float aM = midSm.getNextValue();
                const float aH = highSm.getNextValue();

                for (size_t ch = 0; ch < numCh; ++ch)
                {
                    const int c = (int) ch;
                    auto* d = block.getChannelPointer (ch);
                    const float x = d[s];

                    float low  = lpLow.processSample (c, x);
                    float hi   = hpLow.processSample (c, x);
                    float mid  = lpHigh.processSample (c, hi);
                    float high = hpHigh.processSample (c, hi);
                    low = apHigh.processSample (c, low);   // align phase with the mid/high split

                    d[s] = satBand (low, aL) + satBand (mid, aM) + satBand (high, aH);
                }
            }
        }

    private:
        static float satBand (float b, float amount) noexcept
        {
            if (amount <= 1.0e-4f)
                return b;

            const float drive = 1.0f + amount * 9.0f;
            const float wet   = std::tanh (b * drive) / drive * std::pow (drive, 0.25f);
            return b + amount * (wet - b);
        }

        void setCrossovers (float freqLow, float freqHigh, bool force)
        {
            freqHigh = juce::jmax (freqHigh, freqLow * 1.5f);

            if (force || std::abs (freqLow - curFreqLow) > 0.5f)
            {
                curFreqLow = freqLow;
                lpLow.setCutoffFrequency (freqLow);
                hpLow.setCutoffFrequency (freqLow);
            }
            if (force || std::abs (freqHigh - curFreqHigh) > 0.5f)
            {
                curFreqHigh = freqHigh;
                lpHigh.setCutoffFrequency (freqHigh);
                hpHigh.setCutoffFrequency (freqHigh);
                apHigh.setCutoffFrequency (freqHigh);
            }
        }

        juce::dsp::LinkwitzRileyFilter<float> lpLow, hpLow, lpHigh, hpHigh, apHigh;
        juce::SmoothedValue<float> lowSm, midSm, highSm;
        float curFreqLow = 0.0f, curFreqHigh = 0.0f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultibandSaturator)
    };
}
