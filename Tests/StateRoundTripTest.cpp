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
    auto& apvts = proc.getAPVTS();

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

    if (failures == 0)
        check (true, "all " + juce::String (proc.getParameters().size())
                     + " parameters round-trip through save/restore");

    std::cout << (failures == 0 ? "\nSTATE ROUND-TRIP OK\n" : "\nSTATE ROUND-TRIP FAILED\n");
    return failures == 0 ? 0 : 1;
}
