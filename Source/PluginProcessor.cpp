/*
    ==============================================================================

    Pickle Power 🥒⚡
    PluginProcessor.cpp

    ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace pp
{
    //==============================================================================
    PicklePowerProcessor::PicklePowerProcessor()
        : AudioProcessor (BusesProperties()
                              .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                              .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
          apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
    {
        pBypass        = apvts.getRawParameterValue (id::bypass);
        pBrine         = apvts.getRawParameterValue (id::brine);
        pBrineType     = apvts.getRawParameterValue (id::brineType);
        pCrunchAttack  = apvts.getRawParameterValue (id::crunchAttack);
        pCrunchSustain = apvts.getRawParameterValue (id::crunchSustain);
        pSnapLow       = apvts.getRawParameterValue (id::snapLow);
        pSnapMid       = apvts.getRawParameterValue (id::snapMid);
        pSnapHigh      = apvts.getRawParameterValue (id::snapHigh);
        pFermentation  = apvts.getRawParameterValue (id::fermentation);
        pAge          = apvts.getRawParameterValue (id::age);
        pPickleJuice  = apvts.getRawParameterValue (id::pickleJuice);
        pWidth        = apvts.getRawParameterValue (id::width);
        pMix          = apvts.getRawParameterValue (id::mix);
        pOutput       = apvts.getRawParameterValue (id::output);
        pOversampling = apvts.getRawParameterValue (id::oversampling);

        pStereoMode   = apvts.getRawParameterValue (id::stereoMode);
        pAutoGain     = apvts.getRawParameterValue (id::autoGain);
        pMultiband    = apvts.getRawParameterValue (id::multiband);
        pMbLow        = apvts.getRawParameterValue (id::mbLow);
        pMbMid        = apvts.getRawParameterValue (id::mbMid);
        pMbHigh       = apvts.getRawParameterValue (id::mbHigh);
        pMbFreqLow    = apvts.getRawParameterValue (id::mbFreqLow);
        pMbFreqHigh   = apvts.getRawParameterValue (id::mbFreqHigh);
    }

    //==============================================================================
    juce::AudioProcessorValueTreeState::ParameterLayout PicklePowerProcessor::createParameterLayout()
    {
        using namespace juce;
        AudioProcessorValueTreeState::ParameterLayout layout;

        auto pct = [] (const char* min = "%") { return AudioParameterFloatAttributes().withLabel (min); };

        layout.add (std::make_unique<AudioParameterBool> (
            ParameterID { id::bypass, 1 }, name::bypass, false));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id::brine, 1 }, name::brine,
            NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, pct()));

        layout.add (std::make_unique<AudioParameterChoice> (
            ParameterID { id::brineType, 1 }, name::brineType, brineTypeChoices(), 1));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id::crunchAttack, 1 }, name::crunchAttack,
            NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f, pct()));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id::crunchSustain, 1 }, name::crunchSustain,
            NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f, pct()));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id::snapLow, 1 }, name::snapLow,
            NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, pct()));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id::snapMid, 1 }, name::snapMid,
            NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, pct()));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id::snapHigh, 1 }, name::snapHigh,
            NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, pct()));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id::fermentation, 1 }, name::fermentation,
            NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, pct()));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id::age, 1 }, name::age,
            NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.3f,
            AudioParameterFloatAttributes().withStringFromValueFunction (
                [] (float v, int) { return age::daysToText (age::normalisedToDays (v)); })));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id::pickleJuice, 1 }, name::pickleJuice,
            NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, pct()));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id::width, 1 }, name::width,
            NormalisableRange<float> (0.0f, 200.0f, 0.1f), 100.0f, pct()));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id::mix, 1 }, name::mix,
            NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, pct()));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id::output, 1 }, name::output,
            NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, pct(" dB")));

        layout.add (std::make_unique<AudioParameterChoice> (
            ParameterID { id::oversampling, 1 }, name::oversampling, oversamplingChoices(), 1));

        //---- PRO -------------------------------------------------------------
        layout.add (std::make_unique<AudioParameterChoice> (
            ParameterID { id::stereoMode, 1 }, name::stereoMode, stereoModeChoices(), 0));

        layout.add (std::make_unique<AudioParameterBool> (
            ParameterID { id::autoGain, 1 }, name::autoGain, false));

        layout.add (std::make_unique<AudioParameterBool> (
            ParameterID { id::multiband, 1 }, name::multiband, false));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id::mbLow, 1 }, name::mbLow,
            NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, pct()));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id::mbMid, 1 }, name::mbMid,
            NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, pct()));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id::mbHigh, 1 }, name::mbHigh,
            NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, pct()));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id::mbFreqLow, 1 }, name::mbFreqLow,
            NormalisableRange<float> (40.0f, 1000.0f, 1.0f, 0.3f), 200.0f,
            AudioParameterFloatAttributes().withLabel (" Hz")));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id::mbFreqHigh, 1 }, name::mbFreqHigh,
            NormalisableRange<float> (1000.0f, 12000.0f, 1.0f, 0.4f), 2500.0f,
            AudioParameterFloatAttributes().withLabel (" Hz")));

        return layout;
    }

    //==============================================================================
    void PicklePowerProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
    {
        prepareInternal (sampleRate, samplesPerBlock);
    }

    void PicklePowerProcessor::prepareInternal (double sampleRate, int samplesPerBlock)
    {
        hostSampleRate = sampleRate;
        hostBlock      = samplesPerBlock;
        numChannels    = juce::jmax (getTotalNumInputChannels(), getTotalNumOutputChannels(), 1);

        currentOversampleChoice = (OversampleChoice) (int) std::round (pOversampling->load());
        oversampler.prepare (samplesPerBlock, numChannels, currentOversampleChoice);

        const int   factor = oversampler.getFactor();
        const double osRate = sampleRate * factor;

        const int osBlock = samplesPerBlock * factor;

        brineSat     .prepare (osRate, numChannels);
        crunchDesigner.prepare (osRate, numChannels);
        snapExciter  .prepare (osRate, numChannels);
        fermentation .prepare (osRate, numChannels);
        pickleJuice  .prepare (osRate, numChannels);
        multiband    .prepare (osRate, numChannels, osBlock);

        limiter.prepare (sampleRate, numChannels);
        autoGainGain = 1.0f;

        widthSmoothed .reset (sampleRate, 0.02);
        mixSmoothed   .reset (sampleRate, 0.02);
        outputSmoothed.reset (sampleRate, 0.05);

        dryBuffer.setSize (numChannels, samplesPerBlock, false, false, true);

        setLatencySamples ((int) std::round (oversampler.getLatencySamples()
                                             + limiter.getLatencySamples()));

        meterRms = meterPeak = meterGR = 0.0f;
    }

    void PicklePowerProcessor::handleAsyncUpdate()
    {
        if (hostSampleRate <= 0.0)
            return;

        suspendProcessing (true);
        prepareInternal (hostSampleRate, hostBlock);
        suspendProcessing (false);
    }

    bool PicklePowerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
    {
        const auto out = layouts.getMainOutputChannelSet();

        if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
            return false;

        return out == layouts.getMainInputChannelSet();
    }

    //==============================================================================
    void PicklePowerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
    {
        juce::ScopedNoDenormals noDenormals;

        const auto totalIn  = getTotalNumInputChannels();
        const auto totalOut = getTotalNumOutputChannels();
        const auto numSamples = buffer.getNumSamples();

        for (int ch = totalIn; ch < totalOut; ++ch)
            buffer.clear (ch, 0, numSamples);

        updateNuclearState();

        // Reconfigure oversampling off-thread if the user changed it.
        const auto desiredOs = (OversampleChoice) (int) std::round (pOversampling->load());
        if (desiredOs != currentOversampleChoice)
            triggerAsyncUpdate();

        if (pBypass->load() > 0.5f)
        {
            updateMeters (buffer);
            return;
        }

        const auto numCh = buffer.getNumChannels();

        // Keep a dry copy for the Mix control.
        for (int ch = 0; ch < numCh; ++ch)
            dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

        // ---- push parameters into the DSP ------------------------------------
        brineSat.setParameters (pBrine->load() * 0.01f,
                                (BrineType) (int) std::round (pBrineType->load()));
        crunchDesigner.setParameters (pCrunchAttack->load() * 0.01f,
                                      pCrunchSustain->load() * 0.01f);
        snapExciter.setParameters (pSnapLow->load()  * 0.01f,
                                   pSnapMid->load()  * 0.01f,
                                   pSnapHigh->load() * 0.01f);
        fermentation.setParameters (pFermentation->load() * 0.01f, pAge->load());
        pickleJuice.setParameters (pPickleJuice->load() * 0.01f);
        multiband.setParameters (pMbLow->load()  * 0.01f, pMbMid->load() * 0.01f,
                                 pMbHigh->load() * 0.01f, pMbFreqLow->load(), pMbFreqHigh->load());

        widthSmoothed .setTargetValue (pWidth->load()  * 0.01f);
        mixSmoothed   .setTargetValue (pMix->load()    * 0.01f);
        outputSmoothed.setTargetValue (pOutput->load());

        // PRO options.
        const bool msMode = numCh >= 2
                         && (StereoMode) (int) std::round (pStereoMode->load()) == StereoMode::MidSide;
        const bool mbOn   = pMultiband->load() > 0.5f;
        const bool agOn   = pAutoGain->load()  > 0.5f;

        // ---- nonlinear core (oversampled, optionally Mid/Side) ----------------
        if (msMode)
            encodeMidSide (buffer);

        juce::dsp::AudioBlock<float> block (buffer);
        auto osBlock = oversampler.processUp (block);

        brineSat      .process (osBlock);
        crunchDesigner.process (osBlock);
        snapExciter   .process (osBlock);
        fermentation  .process (osBlock);
        pickleJuice   .process (osBlock);
        if (mbOn)
            multiband .process (osBlock);

        oversampler.processDown (block);

        if (msMode)
            decodeMidSide (buffer);

        // ---- host-rate stages ------------------------------------------------
        applyWidth (buffer);
        applyMixAndGain (buffer, agOn);

        limiter.process (block);

        updateMeters (buffer);
    }

    //==============================================================================
    void PicklePowerProcessor::applyWidth (juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumChannels() < 2)
        {
            widthSmoothed.skip (buffer.getNumSamples());
            return;
        }

        auto* L = buffer.getWritePointer (0);
        auto* R = buffer.getWritePointer (1);

        for (int s = 0; s < buffer.getNumSamples(); ++s)
        {
            const float w = widthSmoothed.getNextValue();   // 0..2
            const float mid  = (L[s] + R[s]) * 0.5f;
            const float side = (L[s] - R[s]) * 0.5f * w;
            L[s] = mid + side;
            R[s] = mid - side;
        }
    }

    void PicklePowerProcessor::applyMixAndGain (juce::AudioBuffer<float>& buffer, bool autoGainOn)
    {
        const auto numCh = buffer.getNumChannels();
        const auto numSamples = buffer.getNumSamples();

        // Pass 1: dry/wet mix, measuring input vs processed loudness for auto-gain.
        double drySumSq = 0.0, wetSumSq = 0.0;

        for (int s = 0; s < numSamples; ++s)
        {
            const float mix = mixSmoothed.getNextValue();

            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* d = buffer.getWritePointer (ch);
                const float dry = dryBuffer.getSample (ch, s);
                const float wet = dry + mix * (d[s] - dry);
                d[s] = wet;
                drySumSq += (double) dry * dry;
                wetSumSq += (double) wet * wet;
            }
        }

        // Auto-gain: drift the compensation toward matching input loudness (±18 dB).
        float target = 1.0f;
        if (autoGainOn && wetSumSq > 1.0e-9)
            target = juce::jlimit (0.125f, 8.0f,
                                   (float) std::sqrt (drySumSq / juce::jmax (1.0e-12, wetSumSq)));
        autoGainGain += (target - autoGainGain) * 0.08f;

        // Pass 2: output trim (+ auto-gain).
        for (int s = 0; s < numSamples; ++s)
        {
            const float gain = juce::Decibels::decibelsToGain (outputSmoothed.getNextValue()) * autoGainGain;
            for (int ch = 0; ch < numCh; ++ch)
                buffer.getWritePointer (ch)[s] *= gain;
        }
    }

    void PicklePowerProcessor::encodeMidSide (juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumChannels() < 2) return;
        auto* L = buffer.getWritePointer (0);
        auto* R = buffer.getWritePointer (1);
        for (int s = 0; s < buffer.getNumSamples(); ++s)
        {
            const float m = (L[s] + R[s]) * 0.5f;
            const float side = (L[s] - R[s]) * 0.5f;
            L[s] = m;       // channel 0 = Mid
            R[s] = side;    // channel 1 = Side
        }
    }

    void PicklePowerProcessor::decodeMidSide (juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumChannels() < 2) return;
        auto* M = buffer.getWritePointer (0);
        auto* S = buffer.getWritePointer (1);
        for (int s = 0; s < buffer.getNumSamples(); ++s)
        {
            const float l = M[s] + S[s];
            const float r = M[s] - S[s];
            M[s] = l;
            S[s] = r;
        }
    }

    //==============================================================================
    void PicklePowerProcessor::updateMeters (const juce::AudioBuffer<float>& buffer)
    {
        const auto numCh = buffer.getNumChannels();
        const auto numSamples = buffer.getNumSamples();

        if (numCh == 0 || numSamples == 0)
            return;

        float sumSq = 0.0f, peak = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
        {
            const auto* d = buffer.getReadPointer (ch);
            for (int s = 0; s < numSamples; ++s)
            {
                const float a = std::abs (d[s]);
                peak = juce::jmax (peak, a);
                sumSq += d[s] * d[s];
            }
        }

        meterRms.store (std::sqrt (sumSq / (float) (numCh * numSamples)));
        meterPeak.store (peak);
        meterGR.store (limiter.getGainReductionDb());
    }

    void PicklePowerProcessor::updateNuclearState()
    {
        const bool n = pBrine->load()         > 95.0f
                    && pSnapHigh->load()      > 95.0f
                    && pFermentation->load()  > 95.0f
                    && pPickleJuice->load()   > 95.0f
                    && std::abs (pCrunchAttack->load()) > 95.0f;

        nuclearMode.store (n);
    }

    //==============================================================================
    juce::AudioProcessorEditor* PicklePowerProcessor::createEditor()
    {
        return new PicklePowerEditor (*this);
    }

    void PicklePowerProcessor::getStateInformation (juce::MemoryBlock& destData)
    {
        if (auto xml = apvts.copyState().createXml())
            copyXmlToBinary (*xml, destData);
    }

    void PicklePowerProcessor::setStateInformation (const void* data, int sizeInBytes)
    {
        if (auto xml = getXmlFromBinary (data, sizeInBytes))
            if (xml->hasTagName (apvts.state.getType()))
                apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }
}

//==============================================================================
// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new pp::PicklePowerProcessor();
}
