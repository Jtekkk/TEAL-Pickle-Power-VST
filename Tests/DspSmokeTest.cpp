/*
    ==============================================================================

    Pickle Power 🥒⚡  —  offline DSP smoke test

    A tiny console harness that runs the DSP building blocks over representative
    signals (sine, impulse, very loud, silence) and asserts the output stays
    finite, bounded and — for the limiter — within its ceiling. Build with
    -DPP_BUILD_TESTS=ON and run the resulting console app; it returns non-zero on
    any failure.

    ==============================================================================
*/

#include <juce_dsp/juce_dsp.h>

#include "../Source/DSP/Oversampler.h"
#include "../Source/DSP/BrineSaturator.h"
#include "../Source/DSP/CrunchDesigner.h"
#include "../Source/DSP/SnapExciter.h"
#include "../Source/DSP/FermentationEngine.h"
#include "../Source/DSP/PickleJuice.h"
#include "../Source/DSP/TruePeakLimiter.h"

namespace
{
    int failures = 0;

    void check (bool condition, const juce::String& message)
    {
        if (condition)
        {
            std::cout << "  [PASS] " << message << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] " << message << std::endl;
            ++failures;
        }
    }

    bool allFinite (const juce::AudioBuffer<float>& b)
    {
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int s = 0; s < b.getNumSamples(); ++s)
                if (! std::isfinite (b.getSample (ch, s)))
                    return false;
        return true;
    }

    void fillSine (juce::AudioBuffer<float>& b, double sr, float freq, float amp)
    {
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int s = 0; s < b.getNumSamples(); ++s)
                b.setSample (ch, s, amp * std::sin (juce::MathConstants<float>::twoPi
                                                    * freq * (float) s / (float) sr));
    }
}

int main()
{
    constexpr double sr = 48000.0;
    constexpr int    block = 512;
    constexpr int    channels = 2;

    std::cout << "Pickle Power DSP smoke test @ " << sr << " Hz\n" << std::endl;

    //==========================================================================
    std::cout << "Nonlinear core (heavy settings, sustained drive):" << std::endl;
    {
        pp::BrineSaturator     brine;
        pp::CrunchDesigner     crunch;
        pp::SnapExciter        snap;
        pp::FermentationEngine ferment;
        pp::PickleJuice        juice;

        brine.prepare (sr, channels);
        crunch.prepare (sr, channels);
        snap.prepare (sr, channels);
        ferment.prepare (sr, channels);
        juice.prepare (sr, channels);

        brine.setParameters (0.9f, pp::BrineType::Spicy);
        crunch.setParameters (0.8f, -0.3f);
        snap.setParameters (0.5f, 0.5f, 0.7f);
        ferment.setParameters (0.95f, 0.85f);
        juice.setParameters (0.8f);

        juce::AudioBuffer<float> buffer (channels, block);
        bool finite = true;
        float maxPeak = 0.0f;

        // Run a couple of seconds so the Fermentation engine accumulates "age".
        for (int n = 0; n < 200; ++n)
        {
            fillSine (buffer, sr, 220.0f, 0.7f);
            juce::dsp::AudioBlock<float> blk (buffer);

            brine.process (blk);
            crunch.process (blk);
            snap.process (blk);
            ferment.process (blk);
            juice.process (blk);

            finite = finite && allFinite (buffer);
            maxPeak = juce::jmax (maxPeak, buffer.getMagnitude (0, block));
        }

        check (finite, "core output is finite over 200 blocks");
        check (maxPeak < 50.0f, "core output stays bounded (peak " + juce::String (maxPeak, 2) + ")");
    }

    //==========================================================================
    std::cout << "\nFermentation aging (output should grow while driven):" << std::endl;
    {
        pp::FermentationEngine ferment;
        ferment.prepare (sr, channels);
        ferment.setParameters (1.0f, 0.2f);   // fast ramp so it matures quickly

        juce::AudioBuffer<float> buffer (channels, block);
        float earlyMag = 0.0f, lateMag = 0.0f;

        for (int n = 0; n < 100; ++n)
        {
            fillSine (buffer, sr, 110.0f, 0.5f);
            juce::dsp::AudioBlock<float> blk (buffer);
            ferment.process (blk);

            if (n == 1)  earlyMag = buffer.getMagnitude (0, block);
            if (n == 99) lateMag  = buffer.getMagnitude (0, block);
        }

        check (lateMag >= earlyMag * 0.99f,
               "fermented signal does not collapse (early " + juce::String (earlyMag, 3)
                   + " -> late " + juce::String (lateMag, 3) + ")");
        check (ferment.getMaturityReadout() > 0.0f, "maturity readout advanced");
    }

    //==========================================================================
    std::cout << "\nTrue-peak limiter (ceiling -1 dB, very loud input):" << std::endl;
    {
        pp::TruePeakLimiter limiter;
        limiter.prepare (sr, channels);
        limiter.setCeiling (-1.0f);
        const float ceiling = juce::Decibels::decibelsToGain (-1.0f);

        juce::AudioBuffer<float> buffer (channels, block);
        float maxPeak = 0.0f;
        bool finite = true;

        for (int n = 0; n < 40; ++n)
        {
            fillSine (buffer, sr, 1000.0f, 4.0f);   // way over 0 dBFS
            juce::dsp::AudioBlock<float> blk (buffer);
            limiter.process (blk);

            finite = finite && allFinite (buffer);
            if (n > 4)   // skip look-ahead warm-up
                maxPeak = juce::jmax (maxPeak, buffer.getMagnitude (0, block));
        }

        check (finite, "limiter output is finite");
        check (maxPeak <= ceiling + 1.0e-4f,
               "limiter holds ceiling (peak " + juce::String (maxPeak, 4)
                   + " <= " + juce::String (ceiling, 4) + ")");
        check (limiter.getLatencySamples() > 0.0f, "limiter reports look-ahead latency");
    }

    //==========================================================================
    std::cout << "\nImpulse + silence (stability / denormals):" << std::endl;
    {
        pp::BrineSaturator brine;
        brine.prepare (sr, channels);
        brine.setParameters (0.8f, pp::BrineType::Garlic);

        juce::AudioBuffer<float> buffer (channels, block);
        buffer.clear();
        buffer.setSample (0, 0, 1.0f);
        buffer.setSample (1, 0, -1.0f);

        juce::dsp::AudioBlock<float> blk (buffer);
        brine.process (blk);
        bool finite = allFinite (buffer);

        for (int n = 0; n < 20; ++n)   // silence afterwards
        {
            buffer.clear();
            juce::dsp::AudioBlock<float> s (buffer);
            brine.process (s);
            finite = finite && allFinite (buffer);
        }
        check (finite, "brine handles impulse + silence without NaN/Inf");
    }

    //==========================================================================
    std::cout << "\nOversampler (4x round-trip):" << std::endl;
    {
        pp::Oversampler os;
        os.prepare (block, channels, pp::OversampleChoice::x4);

        juce::AudioBuffer<float> buffer (channels, block);
        fillSine (buffer, sr, 2000.0f, 0.5f);

        juce::dsp::AudioBlock<float> blk (buffer);
        auto up = os.processUp (blk);
        os.processDown (blk);

        check (os.getFactor() == 4, "oversampling factor is 4x");
        check ((int) up.getNumSamples() == block * 4, "upsampled block is 4x longer");
        check (allFinite (buffer), "oversampled round-trip is finite");
        check (os.getLatencySamples() > 0.0f, "oversampler reports latency");
    }

    //==========================================================================
    std::cout << "\n" << (failures == 0 ? "ALL TESTS PASSED 🥒" : juce::String (failures) + " TEST(S) FAILED")
              << std::endl;
    return failures == 0 ? 0 : 1;
}
