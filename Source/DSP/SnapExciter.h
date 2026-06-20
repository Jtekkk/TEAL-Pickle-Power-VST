/*
    ==============================================================================

    Pickle Power 🥒⚡
    SnapExciter.h

    High-frequency exciter — the "snap"/air of the pickle. A one-pole crossover
    isolates the high band, a gentle asymmetric shaper generates new upper
    harmonics, and the excited band is filtered again and added back on top of
    the dry signal. Snap (0..1) sets how much sparkle is blended in.

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

            setCrossover (5500.0f);

            amountSmoothed.reset (sampleRate, 0.02);
            amountSmoothed.setCurrentAndTargetValue (0.0f);
        }

        void reset()
        {
            for (auto& st : states)
                st = State {};
        }

        /** @param snap01  0..1 air amount */
        void setParameters (float snap01)
        {
            amountSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, snap01));
        }

        void process (juce::dsp::AudioBlock<float>& block)
        {
            const auto numCh = block.getNumChannels();
            const auto numS  = block.getNumSamples();

            for (size_t s = 0; s < numS; ++s)
            {
                const float amount = amountSmoothed.getNextValue();

                for (size_t ch = 0; ch < numCh; ++ch)
                {
                    auto& st  = states[ch % states.size()];
                    auto* d   = block.getChannelPointer (ch);
                    const float x = d[s];

                    // Crossover: high band = signal - low-pass.
                    st.lp += lpCoeff * (x - st.lp);
                    const float high = x - st.lp;

                    // Asymmetric harmonic generation (even + odd).
                    float ex = std::tanh (high * 3.0f);
                    ex = ex * 0.7f + (ex * ex - 0.5f) * 0.3f;

                    // Keep only the airy part of the generated harmonics.
                    st.lpHarm += lpCoeff * (ex - st.lpHarm);
                    const float airy = ex - st.lpHarm;

                    d[s] = x + amount * airy * makeup;
                }
            }
        }

    private:
        struct State
        {
            float lp = 0.0f;        // crossover low-pass state
            float lpHarm = 0.0f;    // post-shaper low-pass state
        };

        void setCrossover (float hz)
        {
            const float fc = juce::jlimit (1000.0f, (float) (sampleRate * 0.45), hz);
            lpCoeff = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * fc / (float) sampleRate);
        }

        double sampleRate = 44100.0;
        float  lpCoeff = 0.1f;
        std::vector<State> states;

        juce::SmoothedValue<float> amountSmoothed;

        static constexpr float makeup = 0.9f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SnapExciter)
    };
}
