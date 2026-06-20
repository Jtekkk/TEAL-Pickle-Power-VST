/*
    ==============================================================================

    Pickle Power 🥒⚡
    Theme.h

    Colour palette and shared visual constants for the neon pickle-jar aesthetic.

    ==============================================================================
*/

#pragma once

#include <juce_graphics/juce_graphics.h>

namespace pp::theme
{
    // Background / surfaces
    inline const juce::Colour background   { 0xff0a120c };  // near-black green
    inline const juce::Colour panel        { 0xff10201a };  // dark jar glass
    inline const juce::Colour panelLight   { 0xff183026 };

    // Neon accents
    inline const juce::Colour neonGreen    { 0xff39ff14 };  // electric pickle green
    inline const juce::Colour neonLime     { 0xff9dff5c };
    inline const juce::Colour neonCyan     { 0xff21f0c4 };
    inline const juce::Colour neonYellow   { 0xfff2ff3a };

    // Pickle body
    inline const juce::Colour pickleBody   { 0xff5a7d2a };
    inline const juce::Colour pickleLight  { 0xff8ab534 };
    inline const juce::Colour pickleDark   { 0xff3c5418 };
    inline const juce::Colour pickleBump   { 0xff6f9434 };

    // Brine / liquid
    inline const juce::Colour brineLiquid  { 0x66c8e85a };
    inline const juce::Colour bubble       { 0x88d8ffa0 };

    // Text
    inline const juce::Colour textBright   { 0xffeaffe0 };
    inline const juce::Colour textDim      { 0xff7d9a76 };

    // Warning / nuclear
    inline const juce::Colour danger       { 0xffff3b30 };
    inline const juce::Colour nuclear      { 0xffffe14d };

    // Sizing
    inline constexpr float cornerRadius = 12.0f;
    inline constexpr float glowRadius   = 18.0f;
}
