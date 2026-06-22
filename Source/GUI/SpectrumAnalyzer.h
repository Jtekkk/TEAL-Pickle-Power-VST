/*
    ==============================================================================

    Pickle Power 🥒⚡  —  visual feedback
    SpectrumAnalyzer.h

    Real-time output scope shared across the control pages:
      • log-frequency magnitude spectrum (filled curve, peak-decay smoothed)
      • translucent per-band level fills split at the multiband crossovers
        (acts as the multiband meters)
      • dashed crossover markers when Multiband is engaged
      • the live Dynamic-EQ response curve overlaid when Dynamic EQ is engaged

    Audio is tapped lock-free from the processor's analyzer FIFO.

    ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "../PluginProcessor.h"
#include "../Utils/Theme.h"
#include "../Utils/Constants.h"

namespace pp
{
    class SpectrumAnalyzer : public juce::Component,
                             private juce::Timer
    {
    public:
        explicit SpectrumAnalyzer (PicklePowerProcessor& p)
            : processor (p), fft (fftOrder),
              window ((size_t) fftSize, juce::dsp::WindowingFunction<float>::hann)
        {
            setInterceptsMouseClicks (false, false);
            demoMode = juce::SystemStats::getEnvironmentVariable ("PP_PICKLE_DEMO", "0") != "0";
            startTimerHz (30);
        }

        ~SpectrumAnalyzer() override { stopTimer(); }

        void paint (juce::Graphics& g) override
        {
            auto area = getLocalBounds().toFloat();
            g.setColour (theme::background.withAlpha (0.65f));
            g.fillRoundedRectangle (area, 6.0f);

            auto plot = area.reduced (4.0f);

            // Frequency grid.
            g.setColour (theme::textDim.withAlpha (0.18f));
            for (float f : { 100.0f, 1000.0f, 10000.0f })
            {
                const float x = xFromFreq (f, plot);
                g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            }

            auto* apvts = &processor.getAPVTS();
            const float fLow  = apvts->getRawParameterValue (id::mbFreqLow)->load();
            const float fHigh = apvts->getRawParameterValue (id::mbFreqHigh)->load();
            const bool  mbOn  = apvts->getRawParameterValue (id::multiband)->load() > 0.5f;
            const bool  deqOn = apvts->getRawParameterValue (id::dynEqOn)->load() > 0.5f;

            // Per-band level fills (multiband metering).
            drawBandFill (g, plot, plot.getX(),               xFromFreq (fLow, plot),  bandLevel[0]);
            drawBandFill (g, plot, xFromFreq (fLow, plot),    xFromFreq (fHigh, plot), bandLevel[1]);
            drawBandFill (g, plot, xFromFreq (fHigh, plot),   plot.getRight(),         bandLevel[2]);

            // Spectrum curve.
            juce::Path spec;
            spec.startNewSubPath (plot.getX(), plot.getBottom());
            for (int p = 0; p < numPoints; ++p)
            {
                const float x = plot.getX() + plot.getWidth() * (float) p / (float) (numPoints - 1);
                const float y = plot.getBottom() - plot.getHeight() * bars[(size_t) p];
                spec.lineTo (x, y);
            }
            spec.lineTo (plot.getRight(), plot.getBottom());
            spec.closeSubPath();

            g.setGradientFill (juce::ColourGradient (theme::neonGreen.withAlpha (0.45f), plot.getBottomLeft(),
                                                     theme::neonGreen.withAlpha (0.05f), plot.getTopLeft(), false));
            g.fillPath (spec);
            g.setColour (theme::neonLime.withAlpha (0.9f));
            g.strokePath (spec, juce::PathStrokeType (1.4f));

            // Crossover markers.
            if (mbOn)
            {
                g.setColour (theme::neonCyan.withAlpha (0.6f));
                for (float f : { fLow, fHigh })
                {
                    const float x = xFromFreq (f, plot);
                    for (float y = plot.getY(); y < plot.getBottom(); y += 6.0f)
                        g.drawLine (x, y, x, juce::jmin (y + 3.0f, plot.getBottom()), 1.0f);
                }
            }

            // Live Dynamic-EQ curve.
            if (deqOn)
            {
                const float f1 = apvts->getRawParameterValue (id::deqFreq1)->load();
                const float f2 = apvts->getRawParameterValue (id::deqFreq2)->load();
                const float g1 = demoMode ? apvts->getRawParameterValue (id::deqRange1)->load()
                                          : processor.getDeqGainDb (0);
                const float g2 = demoMode ? apvts->getRawParameterValue (id::deqRange2)->load()
                                          : processor.getDeqGainDb (1);

                juce::Path curve;
                for (int px = 0; px <= (int) plot.getWidth(); ++px)
                {
                    const float x = plot.getX() + (float) px;
                    const float f = freqFromX (x, plot);
                    const float db = bell (f, f1, g1) + bell (f, f2, g2);
                    const float y = plot.getCentreY() - (db / 24.0f) * (plot.getHeight() * 0.45f);
                    if (px == 0) curve.startNewSubPath (x, y);
                    else         curve.lineTo (x, juce::jlimit (plot.getY(), plot.getBottom(), y));
                }
                g.setColour (theme::neonYellow.withAlpha (0.9f));
                g.strokePath (curve, juce::PathStrokeType (1.8f));
            }

            g.setColour (theme::neonGreen.withAlpha (0.35f));
            g.drawRoundedRectangle (area, 6.0f, 1.0f);
        }

    private:
        void timerCallback() override
        {
            const float sr = (float) juce::jmax (8000.0, processor.getSampleRate());

            if (demoMode)
            {
                // Self-generated test tone routed through the real FFT path so the
                // scope is alive without an audio device (demos / screenshots).
                for (int i = 0; i < fftSize; ++i)
                {
                    const float t = demoPhase + (float) i / sr;
                    ring[(size_t) i] = 0.35f * std::sin (juce::MathConstants<float>::twoPi * 220.0f  * t)
                                     + 0.22f * std::sin (juce::MathConstants<float>::twoPi * 1320.0f * t)
                                     + 0.12f * std::sin (juce::MathConstants<float>::twoPi * 6000.0f * t)
                                     + 0.02f * (rng.nextFloat() - 0.5f);
                }
                demoPhase += (float) fftSize / sr;
                writePos = 0;
                filled = fftSize;
            }
            else
            {
                float tmp[fftSize];
                int got;
                bool any = false;
                while ((got = processor.readAnalyzer (tmp, fftSize)) > 0)
                {
                    for (int i = 0; i < got; ++i)
                    {
                        ring[(size_t) writePos] = tmp[i];
                        writePos = (writePos + 1) & (fftSize - 1);
                    }
                    filled = juce::jmin (fftSize, filled + got);
                    any = true;
                }

                if (! any || filled < fftSize)
                {
                    for (auto& b : bars) b *= 0.82f;          // decay when idle
                    repaint();
                    return;
                }
            }

            for (int i = 0; i < fftSize; ++i)
                fftData[(size_t) i] = ring[(size_t) ((writePos + i) & (fftSize - 1))];

            window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
            fft.performFrequencyOnlyForwardTransform (fftData.data());

            for (int p = 0; p < numPoints; ++p)
            {
                const float frac = (float) p / (float) (numPoints - 1);
                const float freq = 20.0f * std::pow (1000.0f, frac);
                const int   bin  = juce::jlimit (1, fftSize / 2, (int) std::round (freq / sr * fftSize));
                const float db   = juce::Decibels::gainToDecibels (fftData[(size_t) bin] / (fftSize * 0.25f), -120.0f);
                const float norm = juce::jlimit (0.0f, 1.0f, juce::jmap (db, -90.0f, 0.0f, 0.0f, 1.0f));
                bars[(size_t) p] = juce::jmax (norm, bars[(size_t) p] * 0.82f);
            }

            // Band levels for the multiband meters.
            const float fLow  = processor.getAPVTS().getRawParameterValue (id::mbFreqLow)->load();
            const float fHigh = processor.getAPVTS().getRawParameterValue (id::mbFreqHigh)->load();
            updateBand (0, 20.0f, fLow,  sr);
            updateBand (1, fLow,  fHigh, sr);
            updateBand (2, fHigh, 20000.0f, sr);

            repaint();
        }

        void updateBand (int b, float fa, float fb, float sr)
        {
            const int binA = juce::jlimit (1, fftSize / 2, (int) (fa / sr * fftSize));
            const int binB = juce::jlimit (binA + 1, fftSize / 2, (int) (fb / sr * fftSize));
            double sum = 0.0;
            for (int i = binA; i < binB; ++i)
                sum += (double) fftData[(size_t) i] * fftData[(size_t) i];
            const float rms = (float) std::sqrt (sum / juce::jmax (1, binB - binA)) / (fftSize * 0.25f);
            const float n   = juce::jlimit (0.0f, 1.0f, juce::jmap (juce::Decibels::gainToDecibels (rms, -120.0f),
                                                                    -90.0f, 0.0f, 0.0f, 1.0f));
            bandLevel[(size_t) b] = juce::jmax (n, bandLevel[(size_t) b] * 0.85f);
        }

        void drawBandFill (juce::Graphics& g, juce::Rectangle<float> plot, float x0, float x1, float level) const
        {
            if (x1 <= x0 || level < 0.01f) return;
            auto r = juce::Rectangle<float> (x0, plot.getBottom() - plot.getHeight() * level,
                                             x1 - x0, plot.getHeight() * level);
            g.setColour (theme::neonGreen.withAlpha (0.07f));
            g.fillRect (r);
        }

        float xFromFreq (float f, juce::Rectangle<float> plot) const
        {
            f = juce::jlimit (20.0f, 20000.0f, f);
            return plot.getX() + plot.getWidth() * (std::log (f / 20.0f) / std::log (1000.0f));
        }

        float freqFromX (float x, juce::Rectangle<float> plot) const
        {
            const float frac = juce::jlimit (0.0f, 1.0f, (x - plot.getX()) / plot.getWidth());
            return 20.0f * std::pow (1000.0f, frac);
        }

        static float bell (float f, float f0, float gainDb)
        {
            const float d = (std::log (f) - std::log (f0)) / 0.5f;   // ~Q 1.4 in log units
            return gainDb * std::exp (-0.5f * d * d);
        }

        static constexpr int fftOrder  = 11;
        static constexpr int fftSize   = 1 << fftOrder;   // 2048
        static constexpr int numPoints = 160;

        PicklePowerProcessor& processor;
        juce::dsp::FFT fft;
        juce::dsp::WindowingFunction<float> window;

        std::array<float, fftSize>     ring {};
        std::array<float, 2 * fftSize> fftData {};
        std::array<float, numPoints>   bars {};
        std::array<float, 3>           bandLevel {};
        int writePos = 0, filled = 0;

        bool  demoMode = false;
        float demoPhase = 0.0f;
        juce::Random rng;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumAnalyzer)
    };
}
