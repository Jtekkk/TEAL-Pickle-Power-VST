/*
    ==============================================================================

    Pickle Power 🥒⚡
    PluginProcessor.h

    Audio processor: hosts the APVTS, owns the DSP chain and exposes a handful of
    metering atomics for the editor.

    Signal flow:
        Input -> [Oversample up -> Brine -> Crunch -> Snap -> Fermentation ->
                  Pickle Juice -> Oversample down] -> Width -> Mix -> Output ->
                  True-Peak Limiter -> Output

    ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Utils/Constants.h"
#include "Utils/PresetManager.h"
#include "DSP/Oversampler.h"
#include "DSP/BrineSaturator.h"
#include "DSP/CrunchDesigner.h"
#include "DSP/SnapExciter.h"
#include "DSP/FermentationEngine.h"
#include "DSP/PickleJuice.h"
#include "DSP/MultibandSaturator.h"
#include "DSP/DynamicEQ.h"
#include "DSP/SpectralSaturator.h"
#include "DSP/TruePeakLimiter.h"

namespace pp
{
    class PicklePowerProcessor : public juce::AudioProcessor,
                                 private juce::AsyncUpdater
    {
    public:
        PicklePowerProcessor();
        ~PicklePowerProcessor() override = default;

        //==========================================================================
        void prepareToPlay (double sampleRate, int samplesPerBlock) override;
        void releaseResources() override {}
        bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
        using juce::AudioProcessor::processBlock;   // keep the double-precision overload visible
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

        //==========================================================================
        juce::AudioProcessorEditor* createEditor() override;
        bool hasEditor() const override { return true; }
        juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter (id::bypass); }

        const juce::String getName() const override { return meta::pluginName; }
        bool acceptsMidi() const override  { return false; }
        bool producesMidi() const override { return false; }
        bool isMidiEffect() const override { return false; }
        double getTailLengthSeconds() const override { return 0.0; }

        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}

        void getStateInformation (juce::MemoryBlock&) override;
        void setStateInformation (const void*, int sizeInBytes) override;

        //==========================================================================
        juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }
        PresetManager& getPresetManager() noexcept { return presetManager; }

        // Metering accessors for the editor (real-time safe, lock-free).
        float getMeterRms()        const noexcept { return meterRms.load(); }
        float getMeterPeak()       const noexcept { return meterPeak.load(); }
        float getGainReductionDb() const noexcept { return meterGR.load(); }
        bool  isNuclear()          const noexcept { return nuclearMode.load(); }
        float getDeqGainDb (int band) const noexcept { return deqGainDb[band].load(); }

        /** Drains up to @p maxSamples of recent output (mono) for the UI analyzer. */
        int readAnalyzer (float* dest, int maxSamples);

        /** Easter egg: trigger the Pickle Power jingle (mixed into the output). */
        void triggerJingle() noexcept { jingleTrigger.store (true); }

        static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    private:
        //==========================================================================
        void prepareInternal (double sampleRate, int samplesPerBlock);
        void handleAsyncUpdate() override;   // rebuilds oversampling off the audio thread

        void applyWidth      (juce::AudioBuffer<float>&);
        void applyMixAndGain (juce::AudioBuffer<float>&, bool autoGainOn);
        void updateMeters    (const juce::AudioBuffer<float>&);
        void updateNuclearState();
        void pushAnalyzer    (const juce::AudioBuffer<float>&);
        void loadJingle();
        void prepareJingle   (double hostRate);
        void mixJingle       (juce::AudioBuffer<float>&);

        static void encodeMidSide (juce::AudioBuffer<float>&);
        static void decodeMidSide (juce::AudioBuffer<float>&);

        //==========================================================================
        juce::AudioProcessorValueTreeState apvts;
        PresetManager presetManager { apvts };

        Oversampler        oversampler;
        BrineSaturator     brineSat;
        CrunchDesigner     crunchDesigner;
        SnapExciter        snapExciter;
        FermentationEngine fermentation;
        PickleJuice        pickleJuice;
        MultibandSaturator multiband;
        DynamicEQ          dynamicEq;
        SpectralSaturator  spectral;
        TruePeakLimiter    limiter;

        juce::AudioBuffer<float> dryBuffer;
        juce::SmoothedValue<float> widthSmoothed, mixSmoothed, outputSmoothed;

        double hostSampleRate = 0.0;
        int    hostBlock      = 0;
        int    numChannels    = 2;
        OversampleChoice currentOversampleChoice = OversampleChoice::Off;
        bool   currentSpectralOn = false;   // re-prepare PDC when this toggles

        // Latency-compensated bypass: delays the dry signal by the reported latency
        // so a bypassed signal stays time-aligned with the processed one.
        int reportedLatency = 0;
        std::vector<std::vector<float>> bypassRing;
        int bypassWrite = 0;

        float  autoGainGain = 1.0f;   // smoothed auto-gain compensation

        // Dev/demo aid (PP_PICKLE_DEMO env): inject a test tone so the analyzer /
        // meters / pickle show real activity without an audio device. No effect
        // unless the env var is set.
        bool   demoInject = false;
        double demoInjectPhase = 0.0;

        std::atomic<float> meterRms  { 0.0f };
        std::atomic<float> meterPeak { 0.0f };
        std::atomic<float> meterGR   { 0.0f };
        std::atomic<bool>  nuclearMode { false };
        std::atomic<float> deqGainDb[2] { { 0.0f }, { 0.0f } };

        // Lock-free output tap for the UI spectrum analyzer.
        juce::AbstractFifo analyzerFifo { 1 << 14 };
        std::vector<float> analyzerBuffer;

        // Embedded jingle (decoded), played on the pickle-button easter egg.
        juce::AudioBuffer<float> jingleSource;   // decoded at its native rate
        double jingleSourceRate = 0.0;
        juce::AudioBuffer<float> jingleBuffer;   // resampled to host rate
        std::atomic<bool> jingleTrigger { false };
        int jinglePos = -1;

        // Cached raw parameter pointers.
        std::atomic<float>* pBypass        = nullptr;
        std::atomic<float>* pBrine         = nullptr;
        std::atomic<float>* pBrineType     = nullptr;
        std::atomic<float>* pCrunchAttack  = nullptr;
        std::atomic<float>* pCrunchSustain = nullptr;
        std::atomic<float>* pSnapLow       = nullptr;
        std::atomic<float>* pSnapMid       = nullptr;
        std::atomic<float>* pSnapHigh      = nullptr;
        std::atomic<float>* pFermentation  = nullptr;
        std::atomic<float>* pAge          = nullptr;
        std::atomic<float>* pPickleJuice  = nullptr;
        std::atomic<float>* pWidth        = nullptr;
        std::atomic<float>* pMix          = nullptr;
        std::atomic<float>* pOutput       = nullptr;
        std::atomic<float>* pOversampling = nullptr;

        // PRO
        std::atomic<float>* pStereoMode   = nullptr;
        std::atomic<float>* pAutoGain     = nullptr;
        std::atomic<float>* pMultiband    = nullptr;
        std::atomic<float>* pMbLow        = nullptr;
        std::atomic<float>* pMbMid        = nullptr;
        std::atomic<float>* pMbHigh       = nullptr;
        std::atomic<float>* pMbFreqLow    = nullptr;
        std::atomic<float>* pMbFreqHigh   = nullptr;

        std::atomic<float>* pDynEqOn      = nullptr;
        std::atomic<float>* pDeqFreq1     = nullptr;
        std::atomic<float>* pDeqThresh1   = nullptr;
        std::atomic<float>* pDeqRange1    = nullptr;
        std::atomic<float>* pDeqFreq2     = nullptr;
        std::atomic<float>* pDeqThresh2   = nullptr;
        std::atomic<float>* pDeqRange2    = nullptr;

        std::atomic<float>* pSpectralOn     = nullptr;
        std::atomic<float>* pSpectralAmount = nullptr;
        std::atomic<float>* pSpectralTilt   = nullptr;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PicklePowerProcessor)
    };
}
