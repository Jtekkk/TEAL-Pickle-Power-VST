/*
    ==============================================================================

    Pickle Power 🥒⚡
    StateRoundTripTest.cpp

    Regression test for parameter state save/restore, exercising the *real*
    processor. This guards the bug pluginval's "Plugin state restoration" test
    found: stepped/bool/choice parameters could keep a stale value after restore
    because APVTS only pushes a tree value to its parameter when the snapped tree
    value changes. Two normalised values that snap to the same step (e.g. 0.94 and
    0.55 on a bool — both -> true) would therefore not round-trip.

    For every parameter we set value A, save, set a *different* value B that snaps
    to the same legal step as A would be most adversarial, restore, and require
    getValue() to come back within tolerance of A.

    ==============================================================================
*/

#include <juce_audio_processors/juce_audio_processors.h>

#include "../Source/PluginProcessor.h"

#include <iostream>

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;   // message manager for AsyncUpdater etc.

    pp::PicklePowerProcessor proc;
    proc.prepareToPlay (48000.0, 512);

    int failures = 0;
    auto check = [&] (bool ok, const juce::String& msg)
    {
        std::cout << (ok ? "  [PASS] " : "  [FAIL] ") << msg << std::endl;
        if (! ok) ++failures;
    };

    std::cout << "Parameter state save/restore round-trip:" << std::endl;

    // Adversarial value pairs: each is set, saved, then the *other* is set before
    // restore. For a bool both 0.94 and 0.55 map to 'true'; for a choice both map
    // to the top index — the exact case where a change-detected push is skipped.
    const std::pair<float, float> pairs[] = { { 0.94f, 0.55f }, { 0.10f, 0.40f }, { 1.0f, 0.0f } };

    for (auto* base : proc.getParameters())
    {
        auto* p = dynamic_cast<juce::RangedAudioParameter*> (base);
        if (p == nullptr)
            continue;

        const auto name = p->getName (64);

        for (auto [a, b] : pairs)
        {
            p->setValueNotifyingHost (a);
            const float expected = p->getValue();

            juce::MemoryBlock state;
            proc.getStateInformation (state);

            p->setValueNotifyingHost (b);
            proc.setStateInformation (state.getData(), (int) state.getSize());

            const float actual = p->getValue();

            // Tolerance: half a step for discrete params (so a snapped restore that
            // lands on the legal step nearest 'expected' counts as correct), else a
            // small epsilon for continuous params.
            const int steps = p->getNumSteps();
            const float tol = (steps > 1 && steps < 10000)
                                ? (0.5f / (float) (steps - 1) + 1.0e-4f)
                                : 1.0e-3f;

            if (std::abs (actual - expected) > tol)
                check (false, name + " did not restore (set " + juce::String (a, 3)
                              + " -> got " + juce::String (actual, 4)
                              + ", expected ~" + juce::String (expected, 4) + ")");
        }
    }

    check (failures == 0, "all " + juce::String (proc.getParameters().size())
                          + " parameters round-trip through save/restore");

    //==========================================================================
    std::cout << "\nFactory presets:" << std::endl;
    {
        auto& pm = proc.getPresetManager();
        auto& a  = proc.getAPVTS();
        auto raw = [&] (const char* pid) { return a.getRawParameterValue (pid)->load(); };

        const int n = pm.getNumFactory();
        check (n >= 10, "factory bank is well stocked (" + juce::String (n) + " presets)");

        // Apply every preset: must not crash and must leave all params finite/in-range.
        bool allFinite = true;
        for (int i = 0; i < n; ++i)
        {
            pm.applyFactory (i);
            for (auto* base : proc.getParameters())
                if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (base))
                {
                    const float v = p->getValue();
                    if (! std::isfinite (v) || v < -0.001f || v > 1.001f) allFinite = false;
                }
        }
        check (allFinite, "every factory preset leaves all params finite and in [0,1]");

        // Stale-state guard: engage Multiband, then load a preset that doesn't use it
        // — applyFactory must reset it back to default (off).
        if (auto* mbp = a.getParameter (pp::id::multiband))
            mbp->setValueNotifyingHost (1.0f);
        const int vocalGlue = pm.getFactoryNames().indexOf ("Vocal Glue");
        if (vocalGlue >= 0) pm.applyFactory (vocalGlue);
        check (raw (pp::id::multiband) < 0.5f, "preset apply clears stale Multiband state");

        // A Pro preset must actually engage its Pro features.
        const int master = pm.getFactoryNames().indexOf ("Master Polish");
        if (master >= 0) pm.applyFactory (master);
        check (master >= 0 && raw (pp::id::multiband)  > 0.5f
                           && raw (pp::id::dynEqOn)    > 0.5f
                           && raw (pp::id::spectralOn) > 0.5f,
               "'Master Polish' engages Multiband + Dyn EQ + Spectral");

        // Bypass is transport, not sound: a preset change must leave it untouched.
        if (auto* bp = a.getParameter (pp::id::bypass)) bp->setValueNotifyingHost (1.0f);
        pm.applyFactory (0);
        check (raw (pp::id::bypass) > 0.5f, "preset apply preserves Bypass state");
    }

    std::cout << (failures == 0 ? "\nSTATE + PRESETS OK\n" : "\nSTATE + PRESETS FAILED\n");
    return failures == 0 ? 0 : 1;
}
