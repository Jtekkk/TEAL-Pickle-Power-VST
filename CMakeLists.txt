cmake_minimum_required(VERSION 3.22)

project(PicklePower VERSION 1.0.0 LANGUAGES C CXX)

#==============================================================================
# Pickle Power 🥒⚡  —  JUCE 8 audio plugin (VST3 / AU / Standalone)
#==============================================================================

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

#------------------------------------------------------------------------------
# JUCE: use a local copy if JUCE_DIR is provided, otherwise fetch a pinned tag.
#------------------------------------------------------------------------------
set(PP_JUCE_TAG "8.0.13" CACHE STRING "JUCE git tag to fetch when JUCE_DIR is not set")

if(DEFINED JUCE_DIR)
    message(STATUS "Pickle Power: using local JUCE at ${JUCE_DIR}")
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    message(STATUS "Pickle Power: fetching JUCE ${PP_JUCE_TAG} via FetchContent")
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        ${PP_JUCE_TAG}
        GIT_SHALLOW    ON
    )
    FetchContent_MakeAvailable(JUCE)
endif()

#------------------------------------------------------------------------------
# The plugin target
#------------------------------------------------------------------------------
juce_add_plugin(PicklePower
    COMPANY_NAME                "Tekk Engineering Audio Labs"
    BUNDLE_ID                   "com.tekkengineeringaudiolabs.tealpicklepowervst"
    PLUGIN_MANUFACTURER_CODE    Teal
    PLUGIN_CODE                 Pkpw
    FORMATS                     VST3 AU Standalone
    PRODUCT_NAME                "TEAL-Pickle-Power-VST"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Distortion
    AU_MAIN_TYPE                kAudioUnitType_Effect)

target_sources(PicklePower PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

# Header-only DSP / GUI / Utils so IDEs list them in the project tree.
target_sources(PicklePower PRIVATE
    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/DSP/DSPHelpers.h
    Source/DSP/Oversampler.h
    Source/DSP/BrineSaturator.h
    Source/DSP/CrunchDesigner.h
    Source/DSP/SnapExciter.h
    Source/DSP/FermentationEngine.h
    Source/DSP/PickleJuice.h
    Source/DSP/MultibandSaturator.h
    Source/DSP/DynamicEQ.h
    Source/DSP/SpectralSaturator.h
    Source/DSP/TruePeakLimiter.h
    Source/GUI/NeonLookAndFeel.h
    Source/GUI/DancingPickleComponent.h
    Source/GUI/PickleJarComponent.h
    Source/GUI/BubbleSystem.h
    Source/GUI/PickleMeter.h
    Source/Utils/Constants.h
    Source/Utils/Theme.h
    Source/Utils/PresetManager.h)

# Embed the Pickle Power jingle so the editor's pickle button can play it.
juce_add_binary_data(PicklePowerData SOURCES assets/audio/TEAL_pickle_power.mp3)

target_compile_definitions(PicklePower PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_USE_MP3AUDIOFORMAT=1
    # Splash screen left on: disabling it requires a JUCE licence for closed-source
    # commercial releases. Set to 0 once you hold the appropriate licence.
    JUCE_DISPLAY_SPLASH_SCREEN=1)

target_link_libraries(PicklePower
    PRIVATE
        PicklePowerData
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

#------------------------------------------------------------------------------
# Optional offline DSP smoke test (console app):  -DPP_BUILD_TESTS=ON
#------------------------------------------------------------------------------
option(PP_BUILD_TESTS "Build the offline DSP smoke test console app" OFF)

if(PP_BUILD_TESTS)
    juce_add_console_app(PicklePowerTest PRODUCT_NAME "PicklePowerTest")

    target_sources(PicklePowerTest PRIVATE Tests/DspSmokeTest.cpp)

    target_compile_definitions(PicklePowerTest PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_STANDALONE_APPLICATION=1)

    target_link_libraries(PicklePowerTest
        PRIVATE
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)

    enable_testing()
    add_test(NAME DspSmokeTest COMMAND PicklePowerTest)

    #--------------------------------------------------------------------------
    # State round-trip diagnostic (links the real processor): -DPP_BUILD_TESTS=ON
    #--------------------------------------------------------------------------
    juce_add_console_app(PicklePowerStateTest PRODUCT_NAME "PicklePowerStateTest")

    target_sources(PicklePowerStateTest PRIVATE
        Tests/StateRoundTripTest.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

    target_compile_definitions(PicklePowerStateTest PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_USE_MP3AUDIOFORMAT=1
        JUCE_STANDALONE_APPLICATION=1)

    target_link_libraries(PicklePowerStateTest
        PRIVATE
            PicklePowerData
            juce::juce_audio_utils
            juce::juce_audio_formats
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)

    add_test(NAME StateRoundTripTest COMMAND PicklePowerStateTest)
endif()
