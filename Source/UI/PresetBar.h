#pragma once
#include <map>
#include "PluginProcessor.h"
#include "Widgets.h"

namespace nusynth::ui
{
/** < [ preset name v ] >  [SAVE]   - factory (INIT + 100 sounds) and user presets */
struct PresetBar final : juce::Component, private juce::Timer
{
    explicit PresetBar (NuSynthAudioProcessor& p) : proc (p)
    {
        prev.setButtonText ("<"); next.setButtonText (">"); save.setButtonText ("SAVE");
        nameButton.setColour (juce::TextButton::buttonColourId, colours::panel);
        nameButton.setColour (juce::TextButton::textColourOffId, colours::text);
        save.setColour (juce::TextButton::buttonColourId, colours::accentDim);
        save.setColour (juce::TextButton::textColourOffId, colours::text);

        prev.onClick = [this] { proc.getPresets().step (-1); updateName(); };
        next.onClick = [this] { proc.getPresets().step (+1); updateName(); };
        nameButton.onClick = [this] { showMenu(); };
        save.onClick = [this] { promptSave(); };

        for (juce::Component* c : std::initializer_list<juce::Component*> { &prev, &nameButton, &next, &save })
            addAndMakeVisible (c);
        updateName();
        startTimerHz (4);
    }
    ~PresetBar() override { stopTimer(); }

    void timerCallback() override { updateName(); }
    void updateName() { nameButton.setButtonText (proc.getPresets().currentName() + "  v"); }

    void resized() override
    {
        auto r = getLocalBounds();
        prev.setBounds (r.removeFromLeft (28));
        save.setBounds (r.removeFromRight (56).reduced (2, 0));
        next.setBounds (r.removeFromRight (28));
        nameButton.setBounds (r.reduced (3, 0));
    }

    void showMenu()
    {
        auto& pm = proc.getPresets();
        pm.refresh();
        const auto& items = pm.items();
        juce::PopupMenu root;
        std::vector<std::pair<juce::String, juce::PopupMenu>> subs;
        for (int i = 0; i < (int) items.size(); ++i)
        {
            const auto& it = items[(size_t) i];
            if (it.isInit) { root.addItem (i + 1, "INIT"); root.addSeparator(); continue; }
            auto found = std::find_if (subs.begin(), subs.end(), [&it] (const auto& s) { return s.first == it.category; });
            if (found == subs.end()) { subs.push_back ({ it.category, juce::PopupMenu() }); found = subs.end() - 1; }
            found->second.addItem (i + 1, it.name, true, i == pm.currentIndex());
        }
        bool haveUser = false;
        for (auto& s : subs) if (s.first == "USER") haveUser = true;
        for (auto& s : subs) if (s.first != "USER") root.addSubMenu (s.first, s.second);
        root.addSeparator();
        if (haveUser) { for (auto& s : subs) if (s.first == "USER") root.addSubMenu ("USER", s.second); }
        else { juce::PopupMenu empty; empty.addItem (-1, "(no user presets yet - press SAVE)", false); root.addSubMenu ("USER", empty); }

        juce::Component::SafePointer<PresetBar> safe (this);
        root.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&nameButton), [safe] (int result)
        {
            if (safe == nullptr || result <= 0) return;
            safe->proc.getPresets().load (result - 1);
            safe->updateName();
        });
    }

    struct SaveCallback final : juce::ModalComponentManager::Callback
    {
        std::function<void (int)> fn;
        explicit SaveCallback (std::function<void (int)> f) : fn (std::move (f)) {}
        void modalStateFinished (int r) override { if (fn) fn (r); }
    };

    void promptSave()
    {
        auto* w = new juce::AlertWindow ("Save preset", "Name your sound (saved in your user presets):", juce::MessageBoxIconType::NoIcon);
        w->addTextEditor ("name", proc.getPresets().currentName());
        w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        juce::Component::SafePointer<PresetBar> safe (this);
        w->enterModalState (true, new SaveCallback ([safe, w] (int result)
        {
            if (result != 1 || safe == nullptr) return;
            safe->proc.getPresets().saveUser (w->getTextEditorContents ("name"));
            safe->updateName();
        }), true);
    }

    NuSynthAudioProcessor& proc;
    juce::TextButton prev, nameButton, next, save;
};
}
