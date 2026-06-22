/*
    ==============================================================================

    Pickle Power 🥒⚡
    FermentationEngine.h   —   the secret sauce

    Instead of a memoryless  out = tanh(in)  shaper, the Fermentation engine keeps
    a running "harmonic age": a slow leaky integrator fed by the signal's energy.
    The longer the plugin is driven, the more the accumulator fills, and the more
    aggressive the saturation becomes — the sound literally matures in the jar and
    relaxes again when the input goes quiet.

    The transfer curve is additionally shaped by:
        • RMS            (overall energy)
        • Crest factor   (peak / RMS)  -> punchy material earns even-harmonic warmth
        • Harmonic age   (how long it has been fermenting)
        • Age control    (sets both the ramp time and the ceiling of the maturity)

    ==============================================================================
*/

#pragma once

#include <juce_dsp/juce_dsp.h>
#include "DSPHelpers.h"
#include "../Utils/Constants.h"

namespace pp
{
    class FermentationEngine
    {
    public:
        FermentationEngine() = default;

        void prepare (double newSampleRate, int numChannels)
        {
            sampleRate = newSampleRate;
            dcBlockers.assign ((size_t) juce::jmax (1, numChannels), dsp::DCBlocker {});
            levelMatcher.prepare (sampleRate);

            rmsCoeff  = (float) std::exp (-1.0 / (sampleRate * 0.080));   // 80 ms RMS
            peakDecay = (float) std::exp (-1.0 / (sampleRate * 0.250));   // 250 ms peak hold

            fermentSmoothed.reset (sampleRate, 0.02);
            fermentSmoothed.setCurrentAndTargetValue (0.0f);

            setParameters (0.0f, 0.0f);
            reset();
        }

        void reset()
        {
            rmsSq = 0.0f;
            peakHold = 0.0f;
            harmonicAge = 0.0f;
            for (auto& dc : dcBlockers)
                dc.reset();
            levelMatcher.reset();
        }

        /** @param ferment01  0..1 amount
            @param maturity01  0..1 derived from the Age control */
        void setParameters (float ferment01, float maturity01)
        {
            fermentSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, ferment01));
            maturity = juce::jlimit (0.0f, 1.0f, maturity01);

            // Older pickle  -> slower ramp (matures over more seconds) and a higher
            // aggression ceiling.
            const float rampSeconds = juce::jmap (maturity, 0.30f, 25.0f);
            ageCoeff = (float) std::exp (-1.0 / (sampleRate * rampSeconds));
            ageCeil  = juce::jmap (maturity, 1.0f, 3.0f);
        }

        void process (juce::dsp::AudioBlock<float>& block)
        {
            const auto numCh = block.getNumChannels();
            const auto numS  = block.getNumSamples();

            for (size_t s = 0; s < numS; ++s)
            {
                // ---- detection (mono-linked) -------------------------------------
                float in = 0.0f;
                for (size_t ch = 0; ch < numCh; ++ch)
                    in = juce::jmax (in, std::abs (block.getChannelPointer (ch)[s]));

                rmsSq = rmsCoeff * rmsSq + (1.0f - rmsCoeff) * in * in;
                const float rms = std::sqrt (rmsSq);

                peakHold = juce::jmax (in, peakHold * peakDecay);
                const float crest = (peakHold + 1.0e-6f) / (rms + 1.0e-6f);

                // ---- maturity accumulation --------------------------------------
                harmonicAge = ageCoeff * harmonicAge + (1.0f - ageCoeff) * rms;

                const float ferment = fermentSmoothed.getNextValue();

                const float drive = juce::jlimit (1.0f, 40.0f,
                    1.0f + ferment * (maturity * 5.0f + harmonicAge * ageCeil * 12.0f));

                const float bias = juce::jlimit (0.0f, 0.4f, (crest - 3.0f) * 0.04f) * ferment;
                const float biasTanh = std::tanh (bias * drive);
                const float mk = levelMatcher.makeup;

                // ---- shaping (loudness-matched, so drive adds grit not volume) ----
                float inMono = 0.0f, outMono = 0.0f;
                for (size_t ch = 0; ch < numCh; ++ch)
                {
                    auto* d = block.getChannelPointer (ch);
                    const float x = d[s];
                    float wet = std::tanh ((x + bias) * drive) - biasTanh;
                    wet = dcBlockers[ch % dcBlockers.size()].process (wet);

                    d[s] = x + ferment * (wet * mk - x);
                    inMono  += std::abs (x);
                    outMono += std::abs (wet);
                }
                if (numCh > 0)
                    levelMatcher.update (inMono / (float) numCh, outMono / (float) numCh);
            }
        }

        /** 0..1-ish readout of how "fermented" the signal currently is — handy for
            driving the UI. */
        float getMaturityReadout() const noexcept
        {
            return juce::jlimit (0.0f, 1.0f, harmonicAge * ageCeil);
        }

    private:
        double sampleRate = 44100.0;

        float rmsCoeff = 0.0f, peakDecay = 0.0f, ageCoeff = 0.0f;
        float rmsSq = 0.0f, peakHold = 0.0f, harmonicAge = 0.0f;

        float maturity = 0.0f, ageCeil = 1.0f;
        juce::SmoothedValue<float> fermentSmoothed;

        std::vector<dsp::DCBlocker> dcBlockers;
        dsp::LevelMatcher levelMatcher;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FermentationEngine)
    };
}
