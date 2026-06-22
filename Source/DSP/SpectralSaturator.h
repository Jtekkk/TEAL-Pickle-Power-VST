/*
    ==============================================================================

    Pickle Power 🥒⚡  —  PRO
    SpectralSaturator.h

    FFT/STFT magnitude saturation. The signal is analysed in overlapping Hann
    windows (512-pt FFT, 75 % overlap); each bin's magnitude is soft-saturated
    toward the frame's peak magnitude (phase preserved), then reconstructed with
    overlap-add. This densifies / glues the spectrum.

        Amount  0..1   how hard the spectrum is saturated
        Tilt   -1..+1  weights saturation toward lows (-) or highs (+)

    Runs at host rate. Adds (fftSize - hop) samples of latency; report it via
    getLatencySamples(). At Amount = 0 it reconstructs the input (transparent).

    ==============================================================================
*/

#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cstring>

namespace pp
{
    class SpectralSaturator
    {
    public:
        static constexpr int fftOrder = 10;
        static constexpr int fftSize  = 1 << fftOrder;   // 1024 (finer bins for tonal material)
        static constexpr int hop      = fftSize / 4;     // 256 (75 % overlap)
        static constexpr int latency  = fftSize - hop;   // 768

        SpectralSaturator() : fft (fftOrder)
        {
            double sumSq = 0.0;
            for (int n = 0; n < fftSize; ++n)
            {
                window[(size_t) n] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi
                                                             * (float) n / (float) fftSize);
                sumSq += (double) window[(size_t) n] * window[(size_t) n];
            }
            scale = (float) ((double) hop / sumSq);   // overlap-add normalisation (analysis*synthesis Hann)
        }

        void prepare (double /*sampleRate*/, int channels)
        {
            states.assign ((size_t) juce::jmax (1, channels), State {});
            reset();
        }

        void reset()
        {
            for (auto& st : states)
            {
                std::fill (st.inFifo.begin(),  st.inFifo.end(),  0.0f);
                std::fill (st.outFifo.begin(), st.outFifo.end(), 0.0f);
                std::fill (st.accum.begin(),   st.accum.end(),   0.0f);
                st.rover = latency;
            }
            amountCur = amountTarget;
            tiltCur   = tiltTarget;
        }

        /** @param amount 0..1   @param tilt -1..+1 */
        void setParameters (float amount, float tilt)
        {
            amountTarget = juce::jlimit (0.0f, 1.0f, amount);
            tiltTarget   = juce::jlimit (-1.0f, 1.0f, tilt);
        }

        float getLatencySamples() const noexcept { return (float) latency; }

        void process (juce::dsp::AudioBlock<float>& block)
        {
            amountCur += (amountTarget - amountCur) * 0.2f;
            tiltCur   += (tiltTarget   - tiltCur)   * 0.2f;

            const int chs = juce::jmin ((int) block.getNumChannels(), (int) states.size());
            const auto numS = block.getNumSamples();

            for (int ch = 0; ch < chs; ++ch)
            {
                auto& st = states[(size_t) ch];
                auto* d  = block.getChannelPointer ((size_t) ch);

                for (size_t s = 0; s < numS; ++s)
                {
                    st.inFifo[(size_t) st.rover] = d[s];
                    d[s] = st.outFifo[(size_t) (st.rover - latency)];

                    if (++st.rover >= fftSize)
                    {
                        st.rover = latency;
                        processFrame (st);
                    }
                }
            }
        }

    private:
        struct State
        {
            std::array<float, fftSize>     inFifo  {};
            std::array<float, fftSize>     outFifo {};
            std::array<float, 2 * fftSize> accum   {};
            int rover = latency;
        };

        void processFrame (State& st)
        {
            for (int k = 0; k < fftSize; ++k)
                work[(size_t) k] = st.inFifo[(size_t) k] * window[(size_t) k];

            fft.performRealOnlyForwardTransform (work.data());

            const int nBins = fftSize / 2;
            float ref = 1.0e-9f;
            for (int bin = 0; bin <= nBins; ++bin)
            {
                const float re = work[(size_t) (2 * bin)];
                const float im = work[(size_t) (2 * bin + 1)];
                mags[(size_t) bin] = std::sqrt (re * re + im * im);
                ref = juce::jmax (ref, mags[(size_t) bin]);
            }

            for (int bin = 0; bin <= nBins; ++bin)
            {
                const float norm   = (float) bin / (float) nBins;
                const float weight = juce::jmax (0.1f, 1.0f + tiltCur * (norm - 0.5f) * 2.0f);
                const float drive  = 1.0f + amountCur * 6.0f * weight;

                const float ratio  = mags[(size_t) bin] / ref;
                const float sat    = std::tanh (ratio * drive) / std::tanh (drive);
                float newMag = ref * sat;
                newMag = mags[(size_t) bin] + amountCur * (newMag - mags[(size_t) bin]);

                const float sc = (mags[(size_t) bin] > 1.0e-9f) ? newMag / mags[(size_t) bin] : 0.0f;
                work[(size_t) (2 * bin)]     *= sc;
                work[(size_t) (2 * bin + 1)] *= sc;
            }

            fft.performRealOnlyInverseTransform (work.data());

            for (int k = 0; k < fftSize; ++k)
                st.accum[(size_t) k] += window[(size_t) k] * work[(size_t) k] * scale;

            for (int k = 0; k < hop; ++k)
                st.outFifo[(size_t) k] = st.accum[(size_t) k];

            std::memmove (st.accum.data(), st.accum.data() + hop, (size_t) fftSize * sizeof (float));

            for (int k = 0; k < latency; ++k)
                st.inFifo[(size_t) k] = st.inFifo[(size_t) (k + hop)];
        }

        juce::dsp::FFT fft;
        std::array<float, fftSize>     window {};
        std::array<float, 2 * fftSize> work   {};
        std::array<float, fftSize / 2 + 1> mags {};
        float scale = 1.0f;

        std::vector<State> states;
        float amountTarget = 0.0f, amountCur = 0.0f;
        float tiltTarget = 0.0f,   tiltCur = 0.0f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectralSaturator)
    };
}
