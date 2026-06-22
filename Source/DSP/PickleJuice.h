/*
    ==============================================================================

    Pickle Power 🥒⚡
    PickleJuice.h

    The one-knob "make it better" macro. A single 0..1 control simultaneously
    pushes a glue compressor and a touch of saturation; an online level matcher
    provides the make-up gain so the stage stays loudness-neutral (the knob glues
    and thickens rather than changing volume). At 0 % it is transparent (dry/wet
    crossfaded by the knob). Compression gain and make-up are mono-linked so the
    stereo image is preserved.

    ==============================================================================
*/

#pragma once

#include <juce_dsp/juce_dsp.h>
#include "DSPHelpers.h"

namespace pp
{
    class PickleJuice
    {
    public:
        PickleJuice() = default;

        void prepare (double newSampleRate, int /*numChannels*/)
        {
            sampleRate = newSampleRate;

            attCoeff = tc (12.0f);
            relCoeff = tc (120.0f);

            juiceSmoothed.reset (sampleRate, 0.03);
            juiceSmoothed.setCurrentAndTargetValue (0.0f);

            levelMatcher.prepare (sampleRate);
            reset();
        }

        void reset()
        {
            env = 0.0f;
            levelMatcher.reset();
        }

        /** @param juice01  0..1 */
        void setParameters (float juice01)
        {
            juiceSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, juice01));
        }

        void process (juce::dsp::AudioBlock<float>& block)
        {
            const auto numCh = block.getNumChannels();
            const auto numS  = block.getNumSamples();
            if (numCh == 0) return;
            const float inv = 1.0f / (float) numCh;

            for (size_t s = 0; s < numS; ++s)
            {
                const float juice = juiceSmoothed.getNextValue();

                // ---- detection (mono-linked peak envelope) ----------------------
                float in = 0.0f;
                for (size_t ch = 0; ch < numCh; ++ch)
                    in = juce::jmax (in, std::abs (block.getChannelPointer (ch)[s]));

                const float coeff = (in > env) ? attCoeff : relCoeff;
                env = coeff * env + (1.0f - coeff) * in;

                // ---- compressor curve -------------------------------------------
                const float thrDb   = juce::jmap (juice, 0.0f, 1.0f, 0.0f, -24.0f);
                const float ratio   = juce::jmap (juice, 0.0f, 1.0f, 1.0f,   4.0f);
                const float levelDb = juce::Decibels::gainToDecibels (env, -100.0f);

                const float overDb = levelDb - thrDb;
                const float grDb   = overDb > 0.0f ? overDb * (1.0f - 1.0f / ratio) : 0.0f;
                const float gain   = juce::Decibels::decibelsToGain (-grDb);

                const float satDrive = 1.0f + juice * 2.0f;
                const float mk = levelMatcher.makeup;

                float inMono = 0.0f, outMono = 0.0f;
                for (size_t ch = 0; ch < numCh; ++ch)
                {
                    auto* d = block.getChannelPointer (ch);
                    const float x   = d[s];
                    const float sat = std::tanh (x * gain * satDrive);   // compress + saturate

                    d[s] = x + juice * (sat * mk - x);
                    inMono  += std::abs (x);
                    outMono += std::abs (sat);
                }
                levelMatcher.update (inMono * inv, outMono * inv);
            }
        }

    private:
        float tc (float ms) const noexcept
        {
            return (float) std::exp (-1.0 / (sampleRate * (ms * 0.001)));
        }

        double sampleRate = 44100.0;
        float  attCoeff = 0.0f, relCoeff = 0.0f, env = 0.0f;

        juce::SmoothedValue<float> juiceSmoothed;
        dsp::LevelMatcher levelMatcher;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PickleJuice)
    };
}
