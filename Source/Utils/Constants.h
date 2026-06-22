/*
    ==============================================================================

    Pickle Power 🥒⚡
    Constants.h

    Central definitions for parameter IDs, ranges, defaults and a few global
    plugin constants. Everything that touches the APVTS references the IDs and
    ranges declared here so the processor, editor and DSP stay in sync.

    ==============================================================================
*/

#pragma once

// Only juce_core is needed here (StringArray / String / maths). Keeping the DSP
// layer free of GUI/processor headers lets it be unit-tested on its own.
#include <juce_core/juce_core.h>

namespace pp
{
    //==============================================================================
    // Plugin meta
    namespace meta
    {
        inline constexpr const char* pluginName   = "Pickle Power";
        inline constexpr const char* version      = "0.1.0";
        inline constexpr int         editorWidth  = 760;
        inline constexpr int         editorHeight = 600;
    }

    //==============================================================================
    // Parameter identifiers. Keep these stable across versions — hosts persist
    // automation against them.
    namespace id
    {
        inline constexpr const char* bypass        = "bypass";
        inline constexpr const char* brine         = "brine";
        inline constexpr const char* brineType     = "brineType";
        inline constexpr const char* crunchAttack  = "crunchAttack";
        inline constexpr const char* crunchSustain = "crunchSustain";
        inline constexpr const char* snapLow       = "snapLow";
        inline constexpr const char* snapMid       = "snapMid";
        inline constexpr const char* snapHigh      = "snapHigh";
        inline constexpr const char* fermentation  = "fermentation";
        inline constexpr const char* age           = "age";
        inline constexpr const char* pickleJuice   = "pickleJuice";
        inline constexpr const char* width         = "width";
        inline constexpr const char* mix           = "mix";
        inline constexpr const char* output        = "output";
        inline constexpr const char* oversampling  = "oversampling";

        // PRO
        inline constexpr const char* stereoMode    = "stereoMode";
        inline constexpr const char* autoGain      = "autoGain";
        inline constexpr const char* multiband     = "multiband";
        inline constexpr const char* mbLow         = "mbLow";
        inline constexpr const char* mbMid         = "mbMid";
        inline constexpr const char* mbHigh        = "mbHigh";
        inline constexpr const char* mbFreqLow     = "mbFreqLow";
        inline constexpr const char* mbFreqHigh    = "mbFreqHigh";
    }

    namespace name
    {
        inline constexpr const char* bypass        = "Bypass";
        inline constexpr const char* brine         = "Brine";
        inline constexpr const char* brineType     = "Brine Type";
        inline constexpr const char* crunchAttack  = "Attack";
        inline constexpr const char* crunchSustain = "Sustain";
        inline constexpr const char* snapLow       = "Low Air";
        inline constexpr const char* snapMid       = "Mid Air";
        inline constexpr const char* snapHigh      = "High Air";
        inline constexpr const char* fermentation  = "Fermentation";
        inline constexpr const char* age           = "Age";
        inline constexpr const char* pickleJuice   = "Pickle Juice";
        inline constexpr const char* width         = "Width";
        inline constexpr const char* mix           = "Mix";
        inline constexpr const char* output        = "Output";
        inline constexpr const char* oversampling  = "Oversampling";

        // PRO
        inline constexpr const char* stereoMode    = "Stereo Mode";
        inline constexpr const char* autoGain      = "Auto Gain";
        inline constexpr const char* multiband     = "Multiband";
        inline constexpr const char* mbLow         = "MB Low";
        inline constexpr const char* mbMid         = "MB Mid";
        inline constexpr const char* mbHigh        = "MB High";
        inline constexpr const char* mbFreqLow     = "MB Freq Low";
        inline constexpr const char* mbFreqHigh    = "MB Freq High";
    }

    //==============================================================================
    // Brine flavour selection. Each flavour reshapes the saturation curve.
    enum class BrineType
    {
        Dill = 0,   // gentle, soft, mostly odd harmonics
        Kosher,     // classic balanced drive
        Garlic,     // rich, asymmetric, even-harmonic warmth
        Spicy,      // aggressive, hard, biting

        numTypes
    };

    inline juce::StringArray brineTypeChoices()
    {
        return { "Dill", "Kosher", "Garlic", "Spicy" };
    }

    //==============================================================================
    // Oversampling factor choices (index maps to 2^index oversampling).
    enum class OversampleChoice
    {
        Off = 0,    // 1x
        x2,         // 2x
        x4,         // 4x
        x8,         // 8x

        numChoices
    };

    inline juce::StringArray oversamplingChoices()
    {
        return { "Off", "2x", "4x", "8x" };
    }

    //==============================================================================
    // PRO: how the nonlinear core treats stereo.
    enum class StereoMode
    {
        Stereo = 0,   // process L / R
        MidSide,      // process Mid / Side

        numModes
    };

    inline juce::StringArray stereoModeChoices()
    {
        return { "Stereo", "Mid / Side" };
    }

    //==============================================================================
    // The "Age" parameter is presented to the user as a duration from one day to
    // ten years. Internally we work with a normalised 0..1 maturity value, but we
    // display a friendly string. These helpers convert between the two.
    namespace age
    {
        inline constexpr float minDays = 1.0f;       // 1 day
        inline constexpr float maxDays = 3650.0f;    // ~10 years

        // Convert a normalised 0..1 value to a number of days (logarithmic feel).
        inline float normalisedToDays (float norm) noexcept
        {
            norm = juce::jlimit (0.0f, 1.0f, norm);
            return minDays * std::pow (maxDays / minDays, norm);
        }

        inline juce::String daysToText (float days)
        {
            if (days < 30.0f)
                return juce::String (juce::roundToInt (days)) + (days < 1.5f ? " day" : " days");

            if (days < 365.0f)
            {
                auto months = days / 30.4375f;
                return juce::String (months, 1) + (months < 1.05f ? " month" : " months");
            }

            auto years = days / 365.0f;
            return juce::String (years, 1) + (years < 1.05f ? " year" : " years");
        }
    }
}
