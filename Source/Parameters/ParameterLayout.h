#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace nusynth
{
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::StringArray wavetableNames();     // 65 built-in + 8 user + 144 Serum-style tables
    int wavetableCategory (int tableIndex);   // 0 Analog 1 Digital 2 Vowel 3 User 4-7 Analog/Digital/Spectral/Vowel (Serum-style extras)
    juce::StringArray warpModeNames();
    juce::StringArray filterTypeNames();    // 30 filter types
    juce::StringArray routeNames();
    juce::StringArray noiseTypeNames();
    juce::StringArray modSourceNames();
    juce::StringArray modDestinationNames();
    juce::StringArray fxNames();
}
