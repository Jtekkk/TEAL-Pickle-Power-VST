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
#include "DSPHelpers.h"

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

            matchLow.prepare (sampleRate);
            matchMid.prepare (sampleRate);
            matchHigh.prepare (sampleRate);

            setCrossovers (200.0f, 2500.0f, true);
            reset();
        }

        void reset()
        {
            for (auto* f : { &lpLow, &hpLow, &lpHigh, &hpHigh, &apHigh })
                f->reset();
            matchLow.reset();
            matchMid.reset();
            matchHigh.reset();
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
            if (numCh == 0) return;
            const float inv = 1.0f / (float) numCh;

            for (size_t s = 0; s < numS; ++s)
            {
                const float aL = lowSm.getNextValue();
                const float aM = midSm.getNextValue();
                const float aH = highSm.getNextValue();
                const float mkL = matchLow.makeup, mkM = matchMid.makeup, mkH = matchHigh.makeup;

                float inL = 0, outL = 0, inM = 0, outM = 0, inH = 0, outH = 0;

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

                    const float wL = satRaw (low,  aL);
                    const float wM = satRaw (mid,  aM);
                    const float wH = satRaw (high, aH);

                    d[s] = (low  + aL * (wL * mkL - low))
                         + (mid  + aM * (wM * mkM - mid))
                         + (high + aH * (wH * mkH - high));

                    inL += std::abs (low);  outL += std::abs (wL);
                    inM += std::abs (mid);  outM += std::abs (wM);
                    inH += std::abs (high); outH += std::abs (wH);
                }

                matchLow .update (inL * inv, outL * inv);
                matchMid .update (inM * inv, outM * inv);
                matchHigh.update (inH * inv, outH * inv);
            }
        }

    private:
        // Raw per-band saturation (pre makeup); loudness restored by the matchers.
        static float satRaw (float b, float amount) noexcept
        {
            const float drive = 1.0f + amount * 9.0f;
            return std::tanh (b * drive);
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
        dsp::LevelMatcher matchLow, matchMid, matchHigh;
        float curFreqLow = 0.0f, curFreqHigh = 0.0f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultibandSaturator)
    };
}
