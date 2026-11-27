#pragma once
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>
#include "DSP/UserContent.h"

namespace nusynth::ui
{
/** Wavetable import options shown when an audio file is dropped on an oscillator (same choices as Serum's import menu). */
inline void showWavetableImportMenu (juce::Component* target, std::function<void (UserContent::ImportMode)> chosen)
{
    using M = UserContent::ImportMode;
    juce::PopupMenu m;
    m.addSectionHeader ("WAVETABLE IMPORT");
    m.addSectionHeader ("Normal");
    m.addItem (1, "DYNAMIC PITCH - ZERO SNAP");
    m.addItem (2, "DYNAMIC PITCH - FOLLOW");
    m.addItem (3, "FREQUENCY ESTIMATION");
    m.addSectionHeader ("Constant frame size");
    m.addItem (4, "PITCH AVERAGE");
    m.addSectionHeader ("FFT");
    m.addItem (5, "256");  m.addItem (6, "512");  m.addItem (7, "1024");  m.addItem (8, "2048");
    m.addSeparator();
    m.addItem (9, "Plain 2048-sample slices");
    juce::Component::SafePointer<juce::Component> safe (target);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target), [safe, chosen] (int r)
    {
        static const M modes[9] = { M::DynamicZeroSnap, M::DynamicFollow, M::FrequencyEstimation, M::PitchAverage,
                                    M::Fft256, M::Fft512, M::Fft1024, M::Fft2048, M::Default };
        if (r >= 1 && r <= 9 && chosen) chosen (modes[r - 1]);
        juce::ignoreUnused (safe);
    });
}

}
