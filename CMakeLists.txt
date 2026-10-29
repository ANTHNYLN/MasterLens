cmake_minimum_required(VERSION 3.22)

project(MasterLens VERSION 1.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

if (NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
endif()

# JUCE is vendored in .tools/JUCE (see README for the exact tag)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/.tools/JUCE ${CMAKE_BINARY_DIR}/JUCE EXCLUDE_FROM_ALL)

juce_add_plugin(MasterLens
    VERSION 1.2.0
    COMPANY_NAME "OrchAudio"
    BUNDLE_ID "com.orchaudio.masterlens"
    PLUGIN_MANUFACTURER_CODE OrcA
    PLUGIN_CODE Maln
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "MasterLens"
    DESCRIPTION "Mastering loudness and mix quality inspector"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    MICROPHONE_PERMISSION_ENABLED FALSE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Analyzer
)

target_sources(MasterLens PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/dsp/AnalysisEngine.cpp
    Source/dsp/SpectrumAnalyzer.cpp
    Source/dsp/ReferenceSpectrum.cpp
    Source/ui/Theme.cpp
    Source/ui/Panels.cpp
    Source/ui/Displays.cpp
    Source/ui/Advisor.cpp
    Source/ui/Modules.cpp
)

target_compile_definitions(MasterLens PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_MODAL_LOOPS_PERMITTED=0
)

target_link_libraries(MasterLens
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
)

# Keep the build output tidy: build/MasterLens_artefacts/<CONFIG>/VST3/MasterLens.vst3 etc.
set_target_properties(MasterLens PROPERTIES XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "-")

# ---------------------------------------------------------------------------
# Offline DSP self-test (not part of the shipped bundles)
# ---------------------------------------------------------------------------
juce_add_console_app(MasterLensDspTest PRODUCT_NAME "MasterLensDspTest")

target_sources(MasterLensDspTest PRIVATE
    Tests/DspTest.cpp
    Source/dsp/AnalysisEngine.cpp
    Source/dsp/SpectrumAnalyzer.cpp
)

target_include_directories(MasterLensDspTest PRIVATE Source)

target_compile_definitions(MasterLensDspTest PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=1
)

target_link_libraries(MasterLensDspTest
    PRIVATE
        juce::juce_audio_basics
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
)

# ---------------------------------------------------------------------------
# Offscreen editor renderer (layout verification helper)
# ---------------------------------------------------------------------------
juce_add_gui_app(MasterLensUiShot PRODUCT_NAME "MasterLensUiShot")

target_sources(MasterLensUiShot PRIVATE
    Tools/UiShot.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/dsp/AnalysisEngine.cpp
    Source/dsp/SpectrumAnalyzer.cpp
    Source/dsp/ReferenceSpectrum.cpp
    Source/ui/Theme.cpp
    Source/ui/Panels.cpp
    Source/ui/Displays.cpp
    Source/ui/Advisor.cpp
    Source/ui/Modules.cpp
)

target_include_directories(MasterLensUiShot PRIVATE Source)

target_compile_definitions(MasterLensUiShot PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(MasterLensUiShot
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
)


