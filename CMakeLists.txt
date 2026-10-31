cmake_minimum_required(VERSION 3.22)

# Must be set before project() so macOS builds are universal (Apple Silicon + Intel)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "" FORCE)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(NUSYNTH_VERSION "0.1.0" CACHE STRING "Plugin version (X.Y.Z)")

# Sanitize: an empty/odd -DNUSYNTH_VERSION (e.g. from CI) must never leave the project without a version.
string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+" NUSYNTH_VERSION_CLEAN "${NUSYNTH_VERSION}")
if(NOT NUSYNTH_VERSION_CLEAN)
    message(WARNING "NUSYNTH_VERSION='${NUSYNTH_VERSION}' is not X.Y.Z - using 0.1.0")
    set(NUSYNTH_VERSION_CLEAN "0.1.0")
endif()

project(NuSynth VERSION ${NUSYNTH_VERSION_CLEAN} LANGUAGES C CXX)
message(STATUS "NuSynth CMakeLists rev 8 - configuring version ${PROJECT_VERSION}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.6
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(NuSynth
    VERSION                     "${NUSYNTH_VERSION_CLEAN}"
    COMPANY_NAME                "Aron H"
    COMPANY_WEBSITE             "https://www.instagram.com/aronh.official/"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    PLUGIN_MANUFACTURER_CODE    ArnH
    PLUGIN_CODE                 NuSy
    FORMATS                     VST3 Standalone
    VST3_CATEGORIES             Instrument Synth
    PRODUCT_NAME                "NuSynth"
    COPY_PLUGIN_AFTER_BUILD     FALSE)

target_sources(NuSynth PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Parameters/ParameterLayout.cpp
    Source/DSP/Wavetables.cpp
    Source/DSP/Engine.cpp
    Source/DSP/Fx.cpp
    Source/AI/PromptBridge.cpp)

target_include_directories(NuSynth PRIVATE Source)

target_compile_definitions(NuSynth PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(NuSynth
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
