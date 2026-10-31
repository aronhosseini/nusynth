#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace nusynth
{
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::StringArray wavetableNames();     // 56 built-in tables
    int wavetableCategory (int tableIndex);   // 0 Analog 1 Digital 2 Vowel 3 User
    juce::StringArray warpModeNames();
    juce::StringArray filterTypeNames();    // 30 filter types
    juce::StringArray routeNames();
    juce::StringArray noiseTypeNames();
    juce::StringArray modSourceNames();
    juce::StringArray modDestinationNames();
    juce::StringArray fxNames();
}
