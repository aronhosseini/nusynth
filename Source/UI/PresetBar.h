#pragma once
#include <map>
#include <memory>
#include <vector>
#include "PluginProcessor.h"
#include "Widgets.h"

namespace nusynth::ui
{
/** < [ preset name v ] >  [SAVE]   - main presets (embedded) and user presets */
struct PresetBar final : juce::Component, private juce::Timer
{
    explicit PresetBar (NuSynthAudioProcessor& p) : proc (p)
    {
        prev.setButtonText ("<"); next.setButtonText (">"); save.setButtonText ("SAVE");
        nameButton.setColour (juce::TextButton::buttonColourId, colours::panelDark);
        nameButton.setColour (juce::TextButton::textColourOffId, colours::text);
        save.setColour (juce::TextButton::buttonColourId, colours::panelDark);
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

    /** Folder tree of the user presets (folders can be nested: Bass/Wobble ...). */
    struct UserNode
    {
        std::map<juce::String, std::shared_ptr<UserNode>> kids;
        std::vector<std::pair<int, juce::String>> presets;       // (menu id, name)

        UserNode& child (const juce::String& name)
        {
            auto& k = kids[name];
            if (k == nullptr) k = std::make_shared<UserNode>();
            return *k;
        }
        juce::PopupMenu toMenu (int currentIdx) const
        {
            juce::PopupMenu m;
            for (const auto& k : kids) m.addSubMenu (k.first, k.second->toMenu (currentIdx));
            for (const auto& p : presets) m.addItem (p.first, p.second, true, p.first - 1 == currentIdx);
            if (kids.empty() && presets.empty()) m.addItem (-1, "(empty)", false);
            return m;
        }
    };

    static constexpr int kIdNewFolder = 900001, kIdOpenFolder = 900002;

    void showMenu()
    {
        auto& pm = proc.getPresets();
        pm.refresh();
        const auto& items = pm.items();
        juce::PopupMenu root;
        std::vector<std::pair<juce::String, juce::PopupMenu>> subs;
        UserNode userRoot;
        for (const auto& f : PresetManager::userSubFolders())            // empty folders show up too
        {
            UserNode* n = &userRoot;
            for (const auto& part : juce::StringArray::fromTokens (f, "/", "")) n = &n->child (part);
        }
        for (int i = 0; i < (int) items.size(); ++i)
        {
            const auto& it = items[(size_t) i];
            if (it.isInit) { root.addItem (i + 1, "INIT"); root.addSeparator(); continue; }
            if (it.category == "USER")
            {
                UserNode* n = &userRoot;
                for (const auto& part : juce::StringArray::fromTokens (it.folder, "/", "")) n = &n->child (part);
                n->presets.push_back ({ i + 1, it.name });
                continue;
            }
            auto found = std::find_if (subs.begin(), subs.end(), [&it] (const auto& s) { return s.first == it.category; });
            if (found == subs.end()) { subs.push_back ({ it.category, juce::PopupMenu() }); found = subs.end() - 1; }
            found->second.addItem (i + 1, it.name, true, i == pm.currentIndex());
        }
        for (auto& s : subs) root.addSubMenu (s.first, s.second);
        root.addSeparator();

        auto userMenu = userRoot.toMenu (pm.currentIndex());
        if (userRoot.kids.empty() && userRoot.presets.empty()) { userMenu.clear(); userMenu.addItem (-1, "(no user presets yet - press SAVE)", false); }
        userMenu.addSeparator();
        userMenu.addItem (kIdNewFolder, "New folder...");
        userMenu.addItem (kIdOpenFolder, "Show user presets folder");
        root.addSubMenu ("USER", userMenu);

        juce::Component::SafePointer<PresetBar> safe (this);
        root.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&nameButton), [safe] (int result)
        {
            if (safe == nullptr || result <= 0) return;
            if (result == kIdNewFolder)  { safe->promptNewFolder(); return; }
            if (result == kIdOpenFolder) { PresetManager::userFolder().createDirectory(); PresetManager::userFolder().revealToUser(); return; }
            safe->proc.getPresets().load (result - 1);
            safe->updateName();
        });
    }

    void promptNewFolder()
    {
        auto* w = new juce::AlertWindow ("New folder", "Folder name inside your user presets (use / for a sub-folder, e.g. Bass/Wobble):", juce::MessageBoxIconType::NoIcon);
        w->addTextEditor ("name", "");
        w->addButton ("Create", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        w->enterModalState (true, new SaveCallback ([w] (int result)
        {
            if (result == 1) PresetManager::createUserSubFolder (w->getTextEditorContents ("name"));
        }), true);
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
        auto folders = PresetManager::userSubFolders();
        juce::StringArray choices;
        choices.add ("(top level)");
        choices.addArray (folders);
        w->addComboBox ("folder", choices, "Folder");
        w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        juce::Component::SafePointer<PresetBar> safe (this);
        w->enterModalState (true, new SaveCallback ([safe, w, folders] (int result)
        {
            if (result != 1 || safe == nullptr) return;
            auto* box = w->getComboBoxComponent ("folder");
            const int fi = box != nullptr ? box->getSelectedItemIndex() : 0;
            safe->proc.getPresets().saveUser (w->getTextEditorContents ("name"), fi > 0 ? folders[fi - 1] : juce::String());
            safe->updateName();
        }), true);
    }

    NuSynthAudioProcessor& proc;
    juce::TextButton prev, nameButton, next, save;
};
}
