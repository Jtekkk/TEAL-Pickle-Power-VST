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
        static Preset make (juce::String name, float brine, int brineType,
                            float attack, float sustain,
                            float snapLow, float snapMid, float snapHigh,
                            float ferment, float age, float juice,
                            float width, float mix, float output, int oversampling)
        {
            return { std::move (name), {
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
        }

        void buildFactory()
        {
            //                  name             brine type  atk    sus   low   mid  high  ferm  age   juice  wid   mix   out  OS
            factory.push_back (make ("Init",          0,  1,   0,    0,    0,    0,   0,    0,   0.30f,  0,  100,  100,   0,  1));
            factory.push_back (make ("Fat Drums",    35,  1,  55,  -10,   20,   25,  35,   15,  0.30f, 45,  110,  100,   0,  2));
            factory.push_back (make ("Punchy Snare", 30,  3,  80,  -35,    0,   30,  55,   10,  0.30f, 30,  100,  100,   0,  2));
            factory.push_back (make ("Bass Brine",   55,  2,  10,   30,   55,   10,   0,   35,  0.50f, 40,   80,  100,   0,  1));
            factory.push_back (make ("Vocal Glue",   18,  0,   0,   10,    0,   20,  35,   25,  0.40f, 70,  100,  100,   0,  1));
            factory.push_back (make ("Tape Warmth",  50,  2, -10,   15,   30,   15,  10,   55,  0.70f, 30,  105,  100,   0,  1));
            factory.push_back (make ("Air Sheen",    12,  0,  15,    0,    0,   40,  70,   10,  0.30f, 20,  120,  100,   0,  2));
            factory.push_back (make ("Lo-Fi Crush",  70,  3,  30,  -20,   20,   20,  10,   60,  0.80f, 55,   90,   85,   0,  0));
            factory.push_back (make ("Nuclear Pickle",100, 3, 100,  100,  100,  100, 100,  100,  1.00f,100,  150,  100,  -3,  3));
        }

        juce::AudioProcessorValueTreeState& apvts;
        std::vector<Preset> factory;
        inline static const juce::String extension { "pppreset" };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
    };
}
