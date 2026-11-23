cmake_minimum_required(VERSION 3.22)

# macOS: run on macOS 10.13 and newer (set before project()). The universal build (Apple Silicon + Intel)
# is chosen on the command line: -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"  (see build_mac.sh)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

project(RSquelch VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows: link the Visual C++ runtime statically, so R-Squelch runs on any PC
# (no "VCRUNTIME140.dll is missing" on computers without the Visual C++ Redistributable).
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# ---------------------------------------------------------------------------
# JUCE 8 — downloaded automatically on first configure (no Git needed).
# To use a local copy instead:  cmake -DFETCHCONTENT_SOURCE_DIR_JUCE=C:/path/to/JUCE ...
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(JUCE
    URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.15.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(JUCE)

set(RSQ_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND RSQ_FORMATS AU)
endif()

juce_add_plugin(RSquelch
    PRODUCT_NAME "R-Squelch"
    COMPANY_NAME "RBell"
    BUNDLE_ID "com.rbell.rsquelch"
    PLUGIN_MANUFACTURER_CODE Rbel
    PLUGIN_CODE RSqu
    ICON_BIG assets/icon.png
    FORMATS ${RSQ_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(RSquelchAssets SOURCES assets/rbell_logo.png assets/moon.png assets/brushed_black.jpg assets/stone.jpg)

target_sources(RSquelch PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(RSquelch PRIVATE Source)

target_compile_definitions(RSquelch PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(RSquelch
    PRIVATE
        RSquelchAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# DSP unit tests (plain C++, no JUCE)
# ---------------------------------------------------------------------------
enable_testing()
add_executable(RSquelchDspTests tests/DspTests.cpp)
add_test(NAME DspTests COMMAND RSquelchDspTests)
