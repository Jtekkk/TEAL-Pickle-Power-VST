/*
    ==============================================================================

    Pickle Power 🥒⚡
    Oversampler.h

    Thin wrapper around juce::dsp::Oversampling. The nonlinear core of the plugin
    (saturation / excitement / fermentation) runs inside the oversampled domain
    to keep aliasing in check, then the signal is decimated back to the host rate.

    ==============================================================================
*/

#pragma once

#include <juce_dsp/juce_dsp.h>
#include "../Utils/Constants.h"

namespace pp
{
    class Oversampler
    {
    public:
        Oversampler() = default;

        /** Builds the oversampling stages for the requested factor. Allocates, so
            call from prepareToPlay (or while processing is suspended), never from
            the audio thread. */
        void prepare (int maxBlockSize, int numChannels, OversampleChoice choice)
        {
            channels = (size_t) juce::jmax (1, numChannels);
            stages   = (size_t) choice;            // 0,1,2,3  ->  1x,2x,4x,8x
            enabled  = (stages > 0);

            if (enabled)
            {
                oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
                    channels, stages,
                    juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
                    /*isMaximumQuality*/ true,
                    /*useIntegerLatency*/ true);

                oversampling->initProcessing ((size_t) juce::jmax (1, maxBlockSize));
                oversampling->reset();
            }
            else
            {
                oversampling.reset();
            }
        }

        void reset()
        {
            if (oversampling != nullptr)
                oversampling->reset();
        }

        /** Upsamples and returns a block at the oversampled rate. When disabled the
            input block is returned unchanged. */
        juce::dsp::AudioBlock<float> processUp (juce::dsp::AudioBlock<float>& input)
        {
            if (enabled && oversampling != nullptr)
                return oversampling->processSamplesUp (input);

            return input;
        }

        /** Downsamples back into the supplied (host-rate) block. No-op when disabled. */
        void processDown (juce::dsp::AudioBlock<float>& output)
        {
            if (enabled && oversampling != nullptr)
                oversampling->processSamplesDown (output);
        }

        /** Integer oversampling factor currently in use (1, 2, 4 or 8). */
        int getFactor() const noexcept { return enabled ? (1 << stages) : 1; }

        /** Latency introduced by the anti-imaging / anti-aliasing filters, in
            host-rate samples. */
        float getLatencySamples() const noexcept
        {
            return (enabled && oversampling != nullptr)
                       ? (float) oversampling->getLatencyInSamples()
                       : 0.0f;
        }

        bool isEnabled() const noexcept { return enabled; }

    private:
        std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
        size_t channels = 2;
        size_t stages   = 0;
        bool   enabled  = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Oversampler)
    };
}
