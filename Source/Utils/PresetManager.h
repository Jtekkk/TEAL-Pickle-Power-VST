/*
    ==============================================================================

    Pickle Power 🥒⚡
    PresetManager.h

    Factory presets (in-memory) plus user presets saved to / loaded from disk as
    APVTS-state XML in the user application-data folder. Applying a factory preset
    or loading a user preset both push through the APVTS so host + UI stay in sync.

    ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Constants.h"

namespace pp
{
    class PresetManager
    {
    public:
        struct Preset
        {
            juce::String name;
            std::vector<std::pair<juce::String, float>> values;
        };

        explicit PresetManager (juce::AudioProcessorValueTreeState& s) : apvts (s)
        {
            buildFactory();
        }

        //==========================================================================
        // Factory
        juce::StringArray getFactoryNames() const
        {
            juce::StringArray names;
            for (auto& p : factory)
                names.add (p.name);
            return names;
        }

        int getNumFactory() const noexcept { return (int) factory.size(); }

        void applyFactory (int index)
        {
            if (! juce::isPositiveAndBelow (index, (int) factory.size()))
                return;

            // Reset every parameter to its default first so a preset is deterministic
            // — values it doesn't mention (e.g. the Pro section) return to default
            // instead of inheriting whatever the previous preset/user left behind.
            // Bypass is left alone: it's a transport control, not part of the sound.
            resetToDefault();

            for (auto& [paramID, value] : factory[(size_t) index].values)
                if (auto* p = apvts.getParameter (paramID))
                    p->setValueNotifyingHost (p->convertTo0to1 (value));
        }

        //==========================================================================
        // User presets (disk)
        juce::File getUserDir() const
        {
            auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                           .getChildFile ("Pickle Audio")
                           .getChildFile ("Pickle Power")
                           .getChildFile ("Presets");
            dir.createDirectory();
            return dir;
        }

        juce::Array<juce::File> getUserPresets() const
        {
            auto files = getUserDir().findChildFiles (juce::File::findFiles, false, "*." + extension);
            files.sort();
            return files;
        }

        /** Saves the current state under @p name. Returns the written file. */
        juce::File saveUserPreset (const juce::String& name)
        {
            auto file = getUserDir().getChildFile (juce::File::createLegalFileName (name) + "." + extension);
            if (auto xml = apvts.copyState().createXml())
                xml->writeTo (file);
            return file;
        }

        bool loadUserPreset (const juce::File& file)
        {
            if (auto xml = juce::XmlDocument::parse (file))
            {
                if (xml->hasTagName (apvts.state.getType()))
                {
                    apvts.replaceState (juce::ValueTree::fromXml (*xml));
                    return true;
                }
            }
            return false;
        }

    private:
        using Pair  = std::pair<juce::String, float>;
        using Extra = std::vector<Pair>;

        // Reset every parameter (except bypass) to its registered default.
        void resetToDefault()
        {
            for (auto child : apvts.state)
            {
                const auto pid = child.getProperty ("id").toString();
                if (pid.isEmpty() || pid == id::bypass)
                    continue;
                if (auto* p = apvts.getParameter (pid))
                    p->setValueNotifyingHost (p->getDefaultValue());
            }
        }

        static Preset make (juce::String name, float brine, int brineType,
                            float attack, float sustain,
                            float snapLow, float snapMid, float snapHigh,
                            float ferment, float age, float juice,
                            float width, float mix, float output, int oversampling,
                            Extra pro = {})
        {
            Preset p { std::move (name), {
                { id::brine,         brine },
                { id::brineType,     (float) brineType },
                { id::crunchAttack,  attack },
                { id::crunchSustain, sustain },
                { id::snapLow,       snapLow },
                { id::snapMid,       snapMid },
                { id::snapHigh,      snapHigh },
                { id::fermentation,  ferment },
                { id::age,           age },
                { id::pickleJuice,   juice },
                { id::width,         width },
                { id::mix,           mix },
                { id::output,        output },
                { id::oversampling,  (float) oversampling } } };

            for (auto& e : pro)             // append any Pro-section overrides
                p.values.push_back (e);
            return p;
        }

        // brineType: 0 Dill · 1 Kosher · 2 Garlic · 3 Spicy
        // stereoMode: 0 Stereo · 1 Mid/Side    oversampling: 0 Off · 1 2x · 2 4x · 3 8x
        void buildFactory()
        {
            using namespace pp;
            //                  name             brine type  atk    sus   low   mid  high  ferm  age   juice  wid   mix   out  OS
            factory.push_back (make ("Init",          0,  1,   0,    0,    0,    0,   0,    0,   0.30f,  0,  100,  100,   0,  1));
            factory.push_back (make ("Fat Drums",    35,  1,  55,  -10,   20,   25,  35,   15,  0.30f, 45,  110,  100,   0,  2));
            factory.push_back (make ("Punchy Snare", 30,  3,  80,  -35,    0,   30,  55,   10,  0.30f, 30,  100,  100,   0,  2));

            factory.push_back (make ("Drum Bus Glue",25,  1,  30,   10,   10,   15,  20,   20,  0.35f, 25,  105,  100,   0,  1,
                { { id::multiband, 1 }, { id::mbLow, 20 }, { id::mbMid, 30 }, { id::mbHigh, 35 },
                  { id::mbFreqLow, 150 }, { id::mbFreqHigh, 3000 }, { id::autoGain, 1 } }));

            factory.push_back (make ("Bass Brine",   55,  2,  10,   30,   55,   10,   0,   35,  0.50f, 40,   80,  100,   0,  1));

            factory.push_back (make ("Sub Tight",    30,  2,   0,   15,    0,    0,   0,   25,  0.40f, 30,   70,  100,   0,  1,
                { { id::stereoMode, 1 }, { id::multiband, 1 }, { id::mbLow, 35 }, { id::mbMid, 10 },
                  { id::mbFreqLow, 120 }, { id::dynEqOn, 1 }, { id::deqFreq1, 80 },
                  { id::deqThresh1, -30 }, { id::deqRange1, -6 }, { id::autoGain, 1 } }));

            factory.push_back (make ("Vocal Glue",   18,  0,   0,   10,    0,   20,  35,   25,  0.40f, 70,  100,  100,   0,  1));

            factory.push_back (make ("De-Ess & Tame",12,  0,   0,    0,    0,    0,  15,   10,  0.30f, 30,  100,  100,   0,  2,
                { { id::dynEqOn, 1 }, { id::deqFreq2, 7000 }, { id::deqThresh2, -34 },
                  { id::deqRange2, -10 }, { id::autoGain, 1 } }));

            factory.push_back (make ("Vocal Air",    14,  0,   0,    0,    0,   20,  45,    8,  0.30f, 40,  100,  100,   0,  2,
                { { id::spectralOn, 1 }, { id::spectralAmount, 35 }, { id::spectralTilt, 40 },
                  { id::autoGain, 1 } }));

            factory.push_back (make ("Tape Warmth",  50,  2, -10,   15,   30,   15,  10,   55,  0.70f, 30,  105,  100,   0,  1));
            factory.push_back (make ("Air Sheen",    12,  0,  15,    0,    0,   40,  70,   10,  0.30f, 20,  120,  100,   0,  2));

            factory.push_back (make ("Wide & Warm",  35,  2,  -5,   12,   20,   15,  15,   40,  0.60f, 35,  140,  100,   0,  1,
                { { id::stereoMode, 1 }, { id::multiband, 1 }, { id::mbLow, 15 }, { id::mbMid, 20 },
                  { id::mbHigh, 30 } }));

            factory.push_back (make ("Master Polish",12,  1,   8,    5,    5,    8,  12,   12,  0.35f, 15,  105,  100,   0,  2,
                { { id::multiband, 1 }, { id::mbLow, 12 }, { id::mbMid, 10 }, { id::mbHigh, 18 },
                  { id::mbFreqLow, 180 }, { id::mbFreqHigh, 4000 },
                  { id::dynEqOn, 1 }, { id::deqFreq1, 200 }, { id::deqThresh1, -28 }, { id::deqRange1, -3 },
                  { id::deqFreq2, 8000 }, { id::deqThresh2, -30 }, { id::deqRange2, 4 },
                  { id::spectralOn, 1 }, { id::spectralAmount, 18 }, { id::spectralTilt, 20 },
                  { id::autoGain, 1 } }));

            factory.push_back (make ("Spectral Shimmer",20,0,  0,    0,    0,   25,  50,   15,  0.30f, 40,  115,  100,   0,  2,
                { { id::spectralOn, 1 }, { id::spectralAmount, 55 }, { id::spectralTilt, 55 },
                  { id::autoGain, 1 } }));

            factory.push_back (make ("Lo-Fi Crush",  70,  3,  30,  -20,   20,   20,  10,   60,  0.80f, 55,   90,   85,   0,  0));

            factory.push_back (make ("Nuclear Pickle",100, 3, 100,  100,  100,  100, 100,  100,  1.00f,100,  150,  100,  -6,  3,
                { { id::multiband, 1 }, { id::mbLow, 100 }, { id::mbMid, 100 }, { id::mbHigh, 100 },
                  { id::spectralOn, 1 }, { id::spectralAmount, 100 } }));
        }

        juce::AudioProcessorValueTreeState& apvts;
        std::vector<Preset> factory;
        inline static const juce::String extension { "pppreset" };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
    };
}
