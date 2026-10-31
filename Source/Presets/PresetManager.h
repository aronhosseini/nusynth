#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <functional>
#include "PresetTable.h"

namespace nusynth
{
/** Factory presets (INIT + 100 sounds in 7 categories) and user presets saved as XML files. */
class PresetManager
{
public:
    struct Item
    {
        juce::String category, name;
        int factoryRow = -1;      // index into presetRows(), -1 = init / user
        bool isInit = false;
        juce::File file;          // user presets
    };

    explicit PresetManager (juce::AudioProcessorValueTreeState& a) : apvts (a) { refresh(); }

    std::function<void()> beforeSave, afterLoad;   // set by the processor (user waves / noise sample live outside the APVTS)

    static juce::File userFolder()
    {
        auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
       #if JUCE_MAC
        base = base.getChildFile ("Application Support");
       #endif
        return base.getChildFile ("Aron H").getChildFile ("NuSynth").getChildFile ("Presets");
    }

    void refresh()
    {
        list.clear();
        Item init; init.category = "INIT"; init.name = "Init"; init.isInit = true;
        list.push_back (init);

        int n = 0;
        const PresetRow* rows = presetRows (n);
        for (const char* cat : { "ARP", "BASS", "KEYS", "PLUCK", "PAD", "LEAD", "FX" })
            for (int i = 0; i < n; ++i)
                if (juce::String (rows[i].cat) == cat)
                {
                    Item it; it.category = cat; it.name = rows[i].name; it.factoryRow = i;
                    list.push_back (it);
                }

        auto files = userFolder().findChildFiles (juce::File::findFiles, false, "*.nsp");
        std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b) { return a.getFileName() < b.getFileName(); });
        for (auto& f : files)
        {
            Item it; it.category = "USER"; it.name = f.getFileNameWithoutExtension(); it.file = f;
            list.push_back (it);
        }
    }

    const std::vector<Item>& items() const { return list; }
    int currentIndex() const { return current; }

    juce::String currentName() const
    {
        const auto n = apvts.state.getProperty ("presetName", "Init").toString();
        return n.isEmpty() ? juce::String ("Init") : n;
    }

    void load (int index)
    {
        if (index < 0 || index >= (int) list.size()) return;
        const auto& it = list[(size_t) index];

        if (it.isInit)
            applyPatchVar (apvts, Patch().toVar());
        else if (it.factoryRow >= 0)
        {
            int n = 0;
            const PresetRow* rows = presetRows (n);
            applyPatchVar (apvts, buildRow (rows[it.factoryRow]).toVar());
        }
        else if (it.file.existsAsFile())
        {
            if (auto xml = juce::XmlDocument::parse (it.file))
                if (xml->hasTagName (apvts.state.getType()))
                    apvts.replaceState (juce::ValueTree::fromXml (*xml));
        }
        if (afterLoad) afterLoad();
        apvts.state.setProperty ("presetName", it.name, nullptr);
        current = index;
    }

    void step (int dir)
    {
        if (list.empty()) return;
        const int n = (int) list.size();
        load ((current + dir + n) % n);
    }

    bool saveUser (const juce::String& rawName)
    {
        auto name = juce::File::createLegalFileName (rawName.trim());
        if (name.isEmpty()) return false;
        auto folder = userFolder();
        if (! folder.createDirectory()) return false;

        if (beforeSave) beforeSave();
        auto state = apvts.copyState();
        state.setProperty ("presetName", name, nullptr);
        if (auto xml = state.createXml())
        {
            auto file = folder.getChildFile (name + ".nsp");
            if (! xml->writeTo (file)) return false;
            apvts.state.setProperty ("presetName", name, nullptr);
            refresh();
            for (int i = 0; i < (int) list.size(); ++i)
                if (list[(size_t) i].file == file) { current = i; break; }
            return true;
        }
        return false;
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
    std::vector<Item> list;
    int current = 0;
};
}
