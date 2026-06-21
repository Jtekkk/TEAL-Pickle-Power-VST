/*
    ==============================================================================

    Pickle Power 🥒⚡
    SnapExciter.h

    Three-band harmonic exciter. Two one-pole crossovers split each channel into
    Low / Mid / High bands; each band gets its own harmonic generator and is added
    back on top of the dry signal:

        Low Air  (~ < 800 Hz)    warm even-harmonic body  (biased shaper)
        Mid Air  (~ 0.8-5 kHz)   presence / bite          (odd harmonics)
        High Air (~ > 5 kHz)     sparkle / sheen          (bright asymmetric)

    Each band amount is 0..1. Only the *generated* harmonics are added (the
    residue after removing the input), so 0 % is transparent.

    ==============================================================================
*/

#pragma once

#include <juce_dsp/juce_dsp.h>

namespace pp
{
    class SnapExciter
    {
    public:
        SnapExciter() = default;

        void prepare (double newSampleRate, int numChannels)
        {
            sampleRate = newSampleRate;
            states.assign ((size_t) juce::jmax (1, numChannels), State {});

            aLow  = onePole (800.0f);
            aHigh = onePole (5000.0f);

            lowSm .reset (sampleRate, 0.02);  lowSm .setCurrentAndTargetValue (0.0f);
            midSm .reset (sampleRate, 0.02);  midSm .setCurrentAndTargetValue (0.0f);
            highSm.reset (sampleRate, 0.02);  highSm.setCurrentAndTargetValue (0.0f);
        }

        void reset()
        {
            for (auto& st : states)
                st = State {};
        }

        /** @param low,mid,high   0..1 amount per air band */
        void setParameters (float low, float mid, float high)
        {
            lowSm .setTargetValue (juce::jlimit (0.0f, 1.0f, low));
            midSm .setTargetValue (juce::jlimit (0.0f, 1.0f, mid));
            highSm.setTargetValue (juce::jlimit (0.0f, 1.0f, high));
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
                    auto& st = states[ch % states.size()];
                    auto* d  = block.getChannelPointer (ch);
                    const float x = d[s];

                    st.lp1 += aLow  * (x - st.lp1);     // < 800 Hz
                    st.lp2 += aHigh * (x - st.lp2);     // < 5 kHz
                    const float low  = st.lp1;
                    const float mid  = st.lp2 - st.lp1;
                    const float high = x - st.lp2;

                    const float add = aL * harm (low,  2.5f, 0.25f) * 1.2f    // warm body
                                    + aM * harm (mid,  3.5f, 0.0f)  * 1.5f    // presence
                                    + aH * harm (high, 5.0f, 0.0f)  * 2.0f;   // air

                    d[s] = x + add;
                }
            }
        }

    private:
        struct State { float lp1 = 0.0f, lp2 = 0.0f; };

        // Generated-harmonic residue: shaped band minus the (level-matched) input.
        static float harm (float b, float drive, float bias) noexcept
        {
            const float s = std::tanh (b * drive + bias) - std::tanh (bias);
            return s / drive - b;
        }

        float onePole (float hz) const noexcept
        {
            const float fc = juce::jlimit (20.0f, (float) (sampleRate * 0.45), hz);
            return 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * fc / (float) sampleRate);
        }

        double sampleRate = 44100.0;
        float  aLow = 0.1f, aHigh = 0.5f;
        std::vector<State> states;

        juce::SmoothedValue<float> lowSm, midSm, highSm;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SnapExciter)
    };
}
