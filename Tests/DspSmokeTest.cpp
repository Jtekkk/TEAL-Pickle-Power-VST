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
#include "../Source/DSP/MultibandSaturator.h"
#include "../Source/DSP/DynamicEQ.h"
#include "../Source/DSP/SpectralSaturator.h"
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
    std::cout << "\nFermentation aging (loudness-stable; maturity grows):" << std::endl;
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

        check (lateMag > earlyMag * 0.5f && lateMag < earlyMag * 1.6f,
               "fermented signal stays loudness-stable while aging (early " + juce::String (earlyMag, 3)
                   + " -> late " + juce::String (lateMag, 3) + ")");
        check (ferment.getMaturityReadout() > 0.0f, "maturity readout advanced (aging)");
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
    std::cout << "\nMultiband saturator (Pro):" << std::endl;
    {
        pp::MultibandSaturator mb;
        mb.prepare (sr, channels, block);
        juce::AudioBuffer<float> buffer (channels, block);

        // Undriven: crossovers should reconstruct with flat magnitude (RMS preserved).
        mb.setParameters (0.0f, 0.0f, 0.0f, 200.0f, 2500.0f);
        float inRms = 0.0f, outRms = 0.0f;
        for (int n = 0; n < 12; ++n)
        {
            fillSine (buffer, sr, 600.0f, 0.5f);
            inRms = buffer.getRMSLevel (0, 0, block);
            juce::dsp::AudioBlock<float> blk (buffer);
            mb.process (blk);
            outRms = buffer.getRMSLevel (0, 0, block);
        }
        check (std::abs (outRms - inRms) < inRms * 0.1f,
               "undriven bands reconstruct (RMS " + juce::String (inRms, 3)
                   + " -> " + juce::String (outRms, 3) + ")");

        // Driven hard on all bands.
        mb.setParameters (0.9f, 0.9f, 0.9f, 150.0f, 3000.0f);
        bool finite = true; float maxPeak = 0.0f;
        for (int n = 0; n < 40; ++n)
        {
            fillSine (buffer, sr, 600.0f, 0.7f);
            juce::dsp::AudioBlock<float> blk (buffer);
            mb.process (blk);
            finite = finite && allFinite (buffer);
            maxPeak = juce::jmax (maxPeak, buffer.getMagnitude (0, block));
        }
        check (finite, "driven multiband output is finite");
        check (maxPeak < 10.0f, "driven multiband stays bounded (peak " + juce::String (maxPeak, 2) + ")");
    }

    //==========================================================================
    std::cout << "\nDynamic EQ (Pro):" << std::endl;
    {
        pp::DynamicEQ deq;
        deq.prepare (sr, channels);
        juce::AudioBuffer<float> buffer (channels, block);

        // Range 0 -> exactly transparent.
        deq.setBand (0, 180.0f, -24.0f, 0.0f);
        deq.setBand (1, 6000.0f, -24.0f, 0.0f);
        float inRms = 0.0f, outRms = 0.0f;
        for (int n = 0; n < 8; ++n)
        {
            fillSine (buffer, sr, 800.0f, 0.5f);
            inRms = buffer.getRMSLevel (0, 0, block);
            juce::dsp::AudioBlock<float> blk (buffer);
            deq.process (blk);
            outRms = buffer.getRMSLevel (0, 0, block);
        }
        check (std::abs (outRms - inRms) < inRms * 0.02f, "range 0 is transparent");

        // Active bands: finite + bounded.
        deq.setBand (0, 180.0f, -30.0f, -18.0f);
        deq.setBand (1, 6000.0f, -30.0f, 12.0f);
        bool finite = true; float maxPeak = 0.0f;
        for (int n = 0; n < 40; ++n)
        {
            fillSine (buffer, sr, 180.0f, 0.7f);
            juce::dsp::AudioBlock<float> blk (buffer);
            deq.process (blk);
            finite = finite && allFinite (buffer);
            maxPeak = juce::jmax (maxPeak, buffer.getMagnitude (0, block));
        }
        check (finite, "active dynamic EQ is finite");
        check (maxPeak < 8.0f, "active dynamic EQ stays bounded (peak " + juce::String (maxPeak, 2) + ")");

        // Boost vs cut on a tone at the band centre (SVF bell models both correctly).
        auto eqRms = [&] (float rangeDb)
        {
            pp::DynamicEQ d;
            d.prepare (sr, channels);
            d.setBand (0, 1000.0f, -60.0f, rangeDb);   // low threshold -> always engaged
            d.setBand (1, 12000.0f, 0.0f, 0.0f);       // inactive
            juce::AudioBuffer<float> buf (channels, block);
            float r = 0.0f;
            for (int n = 0; n < 80; ++n)
            {
                fillSine (buf, sr, 1000.0f, 0.3f);
                juce::dsp::AudioBlock<float> blk (buf);
                d.process (blk);
                r = buf.getRMSLevel (0, 0, block);
            }
            return r;
        };
        const float flat = eqRms (0.0f), boost = eqRms (18.0f), cut = eqRms (-18.0f);
        check (boost > flat * 1.1f, "dyn EQ boost raises band energy ("  + juce::String (boost, 3) + ")");
        check (cut  < flat * 0.9f, "dyn EQ cut lowers band energy ("    + juce::String (cut, 3) + ")");
    }

    //==========================================================================
    std::cout << "\nSpectral saturator (Pro):" << std::endl;
    {
        pp::SpectralSaturator ss;
        ss.prepare (sr, channels);
        juce::AudioBuffer<float> buffer (channels, block);

        // Amount 0 -> STFT reconstructs the input (RMS preserved once warmed up).
        ss.setParameters (0.0f, 0.0f);
        float inRms = 0.0f, outRms = 0.0f;
        for (int n = 0; n < 24; ++n)
        {
            fillSine (buffer, sr, 1000.0f, 0.5f);
            inRms = buffer.getRMSLevel (0, 0, block);
            juce::dsp::AudioBlock<float> blk (buffer);
            ss.process (blk);
            outRms = buffer.getRMSLevel (0, 0, block);
        }
        check (std::abs (outRms - inRms) < inRms * 0.05f,
               "amount 0 reconstructs (RMS " + juce::String (inRms, 3)
                   + " -> " + juce::String (outRms, 3) + ")");
        check (ss.getLatencySamples() > 0.0f, "spectral reports latency");

        // Driven: finite + bounded.
        ss.setParameters (0.9f, 0.5f);
        bool finite = true; float maxPeak = 0.0f;
        for (int n = 0; n < 40; ++n)
        {
            fillSine (buffer, sr, 1000.0f, 0.6f);
            juce::dsp::AudioBlock<float> blk (buffer);
            ss.process (blk);
            finite = finite && allFinite (buffer);
            maxPeak = juce::jmax (maxPeak, buffer.getMagnitude (0, block));
        }
        check (finite, "driven spectral output is finite");
        check (maxPeak < 8.0f, "driven spectral stays bounded (peak " + juce::String (maxPeak, 2) + ")");
    }

    //==========================================================================
    std::cout << "\nControl behaviour:" << std::endl;
    {
        const float inRms = 0.5f / std::sqrt (2.0f);   // sine, amp 0.5

        auto brineRms = [&] (float amt)
        {
            pp::BrineSaturator b;
            b.prepare (sr, channels);
            b.setParameters (amt, pp::BrineType::Kosher);
            juce::AudioBuffer<float> buf (channels, block);
            float r = 0.0f;
            for (int n = 0; n < 40; ++n)
            {
                fillSine (buf, sr, 220.0f, 0.5f);
                juce::dsp::AudioBlock<float> blk (buf);
                b.process (blk);
                r = buf.getRMSLevel (0, 0, block);
            }
            return r;
        };
        const float rLow  = brineRms (0.2f);
        const float rHigh = brineRms (0.9f);
        check (rHigh > inRms * 0.5f && rHigh < inRms * 2.0f,
               "Brine stays loudness-matched at 90% (in " + juce::String (inRms, 3)
                   + " -> out " + juce::String (rHigh, 3) + ")");
        check (rLow > inRms * 0.5f && rLow < inRms * 2.0f, "Brine loudness-matched at 20%");

        // Crunch Attack +/- should change transient punch in the right direction.
        auto crunchPeak = [&] (float attack)
        {
            pp::CrunchDesigner c;
            c.prepare (sr, channels);
            c.setParameters (attack, 0.0f);
            juce::AudioBuffer<float> buf (channels, block);
            float peak = 0.0f;
            for (int n = 0; n < 80; ++n)
            {
                for (int s = 0; s < block; ++s)
                {
                    const double t = (double) (n * block + s) / sr;
                    const float env = (float) std::exp (-std::fmod (t, 0.25) * 45.0);
                    const float v = env * std::sin (juce::MathConstants<float>::twoPi * 120.0f * (float) t);
                    for (int ch = 0; ch < channels; ++ch) buf.setSample (ch, s, v);
                }
                juce::dsp::AudioBlock<float> blk (buf);
                c.process (blk);
                if (n > 8) peak = juce::jmax (peak, buf.getMagnitude (0, block));
            }
            return peak;
        };
        const float peakPlus  = crunchPeak (1.0f);
        const float peakMinus = crunchPeak (-1.0f);
        check (peakPlus > peakMinus * 1.05f,
               "Crunch Attack+ is punchier than Attack- (" + juce::String (peakPlus, 3)
                   + " > " + juce::String (peakMinus, 3) + ")");

        // Pickle Juice auto-makeup: driven hard, output stays near input loudness.
        {
            const float jin = 0.4f / std::sqrt (2.0f);
            pp::PickleJuice j;
            j.prepare (sr, channels);
            j.setParameters (0.9f);
            juce::AudioBuffer<float> buf (channels, block);
            float r = 0.0f;
            for (int n = 0; n < 80; ++n)
            {
                fillSine (buf, sr, 220.0f, 0.4f);
                juce::dsp::AudioBlock<float> blk (buf);
                j.process (blk);
                r = buf.getRMSLevel (0, 0, block);
            }
            check (r > jin * 0.5f && r < jin * 2.0f,
                   "Pickle Juice loudness-matched at 90% (in " + juce::String (jin, 3)
                       + " -> out " + juce::String (r, 3) + ")");
        }

        // Multiband: all bands driven, sum stays near input loudness.
        {
            const float min_ = 0.4f / std::sqrt (2.0f);
            pp::MultibandSaturator m;
            m.prepare (sr, channels, block);
            m.setParameters (0.9f, 0.9f, 0.9f, 200.0f, 2500.0f);
            juce::AudioBuffer<float> buf (channels, block);
            float r = 0.0f;
            for (int n = 0; n < 80; ++n)
            {
                fillSine (buf, sr, 500.0f, 0.4f);
                juce::dsp::AudioBlock<float> blk (buf);
                m.process (blk);
                r = buf.getRMSLevel (0, 0, block);
            }
            check (r > min_ * 0.4f && r < min_ * 2.2f,
                   "Multiband loudness-matched when driven (in " + juce::String (min_, 3)
                       + " -> out " + juce::String (r, 3) + ")");
        }
    }

    //==========================================================================
    std::cout << "\n" << (failures == 0 ? "ALL TESTS PASSED 🥒" : juce::String (failures) + " TEST(S) FAILED")
              << std::endl;
    return failures == 0 ? 0 : 1;
}
