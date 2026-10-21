cmake_minimum_required(VERSION 3.22)

# must be set before project() or Xcode uses the SDK version as the minimum
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(Liquidation VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(LIQ_BUILD_TESTS "Build DSP unit tests" ON)
option(LIQ_BUILD_PLUGIN "Build the plugin" ON)
option(LIQ_BUILD_SNAPSHOT "Build the UI snapshot tool (dev only)" OFF)

# ---------------------------------------------------------------------------
# JUCE: use a local checkout if JUCE_DIR is given, otherwise fetch it.
# ---------------------------------------------------------------------------
set(JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout (optional)")

if(JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
    add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.10
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------------------
# Plugin
# ---------------------------------------------------------------------------
juce_add_binary_data(LiquidationFonts SOURCES
    Resources/Fonts/BarlowSemiCondensed-Regular.ttf
    Resources/Fonts/BarlowSemiCondensed-Medium.ttf
    Resources/Fonts/BarlowSemiCondensed-SemiBold.ttf)
set_target_properties(LiquidationFonts PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(LIQ_DSP_SOURCES
    Source/dsp/Scale.cpp
    Source/dsp/PitchDetector.cpp
    Source/dsp/GrainFilter.cpp
    Source/dsp/GrainEngine.cpp
    Source/dsp/Imager.cpp
    Source/dsp/GrainDelay.cpp
    Source/dsp/EffectsRack.cpp
    Source/dsp/Modulation.cpp
    Source/dsp/Animator.cpp
    Source/dsp/SpectralMorph.cpp
    Source/dsp/Modeler.cpp
    Source/dsp/Dynamics.cpp
    Source/Params.cpp
    Source/Presets.cpp)

if(LIQ_BUILD_PLUGIN)
set(LIQ_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND LIQ_FORMATS AU)
endif()

juce_add_plugin(Liquidation
    COMPANY_NAME                "JustCreateit"
    COMPANY_WEBSITE             "https://justcreateit.ch"
    BUNDLE_ID                   "ch.justcreateit.liquidation"
    PLUGIN_MANUFACTURER_CODE    Jcit
    PLUGIN_CODE                 Lqdt
    PRODUCT_NAME                "Liquidation"
    DESCRIPTION                 "Real-time scale-locked granular effect"
    FORMATS                     ${LIQ_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Fx Delay Pitch Spatial
    AU_MAIN_TYPE                kAudioUnitType_MusicEffect
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT  "Liquidation processes the audio input in real time."
    COPY_PLUGIN_AFTER_BUILD     FALSE)


target_sources(Liquidation PRIVATE
    ${LIQ_DSP_SOURCES}
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ui/LiqLookAndFeel.cpp
    Source/ui/Widgets.cpp
    Source/ui/WaveformDisplay.cpp
    Source/ui/Blocks.cpp
    Source/ui/Panels.cpp)

target_include_directories(Liquidation PRIVATE Source)

target_compile_definitions(Liquidation PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Liquidation
    PRIVATE
        LiquidationFonts
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

endif()

# ---------------------------------------------------------------------------
# Tests (headless DSP checks)
# ---------------------------------------------------------------------------
if(LIQ_BUILD_TESTS)
    juce_add_console_app(LiquidationTests PRODUCT_NAME "LiquidationTests")
    target_sources(LiquidationTests PRIVATE
        Tests/TestMain.cpp
        ${LIQ_DSP_SOURCES}
        Source/PluginProcessor.cpp)
    target_include_directories(LiquidationTests PRIVATE Source)
    target_compile_definitions(LiquidationTests PRIVATE
        LIQ_HEADLESS=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JucePlugin_Name="Liquidation"
        JucePlugin_WantsMidiInput=1
        JucePlugin_ProducesMidiOutput=0
        JucePlugin_IsMidiEffect=0
        JucePlugin_IsSynth=0)
    target_link_libraries(LiquidationTests PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags)
    enable_testing()
    add_test(NAME dsp COMMAND LiquidationTests)
endif()

# ---------------------------------------------------------------------------
# UI snapshot tool (renders the editor to PNG files, dev only)
# ---------------------------------------------------------------------------
if(LIQ_BUILD_SNAPSHOT)
    juce_add_console_app(LiquidationSnapshot PRODUCT_NAME "LiquidationSnapshot")
    target_sources(LiquidationSnapshot PRIVATE
        Tests/Snapshot.cpp
        ${LIQ_DSP_SOURCES}
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/ui/LiqLookAndFeel.cpp
        Source/ui/Widgets.cpp
        Source/ui/WaveformDisplay.cpp
        Source/ui/Blocks.cpp
        Source/ui/Panels.cpp)
    target_include_directories(LiquidationSnapshot PRIVATE Source)
    target_compile_definitions(LiquidationSnapshot PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1
        JucePlugin_Name="Liquidation" JucePlugin_WantsMidiInput=1 JucePlugin_ProducesMidiOutput=0
        JucePlugin_IsMidiEffect=0 JucePlugin_IsSynth=0)
    target_link_libraries(LiquidationSnapshot PRIVATE LiquidationFonts juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()

