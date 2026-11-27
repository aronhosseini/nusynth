#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <initializer_list>
#include <utility>
#include <vector>
#include <functional>
#include <algorithm>
#include <PresetData.h>
#include "Presets/Migrate.h"
#include "Parameters/ParameterIDs.h"
#include "Parameters/FxSpecs.h"
#include "DSP/SharedData.h"
#include "Presets/Patch.h"

namespace nusynth
{
/** Main presets (embedded .nsp files) and user presets saved as XML files in Documents/Aron H/NuSynth/Presets. */
class PresetManager
{
public:
    struct Item
    {
        juce::String category, name;
        int resource = -1;
        bool isInit = false;
        juce::File file;
        juce::String folder;
    };

    explicit PresetManager (juce::AudioProcessorValueTreeState& a) : apvts (a) { refresh(); }

    std::function<void()> beforeSave, afterLoad;
    std::function<void (const juce::String&)> afterSave;

    static juce::File rootFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
            .getChildFile ("Aron H").getChildFile ("NuSynth");
    }
    static juce::File userFolder() { return rootFolder().getChildFile ("Presets"); }
    static juce::File userWavetableFolder() { return rootFolder().getChildFile ("User Wavetable"); }
    static juce::File userNoiseFolder() { return rootFolder().getChildFile ("User Noise"); }

    static juce::StringArray userSubFolders()
    {
        juce::StringArray out;
        const auto root = userFolder();
        for (auto& d : root.findChildFiles (juce::File::findDirectories, true))
            out.add (d.getRelativePathFrom (root).replaceCharacter ('\\\\', '/'));
        out.sortNatural();
        return out;
    }

    /** Folder of the preset menu for a file prefix: ARP = ARP, BA = BASS, FX = FX, LD = LEAD, PD = PAD, PL = PLUCKED, SY = SYNTH.
        (case-insensitive; the long words work too) */
    static juce::String categoryFromPrefix (const juce::String& prefixIn)
    {
        const auto prefix = prefixIn.trim().toUpperCase();
        if (prefix == "ARP")                                  return "ARP";
        if (prefix == "BA" || prefix == "BASS")               return "BASS";
        if (prefix == "FX")                                   return "FX";
        if (prefix == "LD" || prefix == "LEAD")               return "LEAD";
        if (prefix == "PD" || prefix == "PAD")                return "PAD";
        if (prefix == "PL" || prefix == "PLUCK" || prefix == "PLUCKED") return "PLUCKED";
        if (prefix == "SY" || prefix == "SYNTH")              return "SYNTH";
        return "MAIN";
    }

    void refresh()
    {
        list.clear();
        Item init;
        init.category = "INIT";
        init.name = "Init";
        init.isInit = true;
        list.push_back (init);

        std::vector<Item> main;
        for (int i = 0; i < PresetData::namedResourceListSize; ++i)
        {
            const juce::String file = juce::File::createFileWithoutCheckingPath (PresetData::originalFilenames[i]).getFileName();      // (never a path)
            const juce::String name = file.upToLastOccurrenceOf (".", false, false)
                .replaceCharacter ('_', ' ').trim();
            Item it;
            it.category = categoryFromPrefix (name.upToFirstOccurrenceOf (" ", false, false));
            it.name = name;
            it.resource = i;
            main.push_back (it);
        }

        const juce::StringArray order { "ARP", "BASS", "LEAD", "PAD", "PLUCKED", "SYNTH", "FX", "MAIN" };
        std::stable_sort (main.begin(), main.end(), [&order] (const Item& a, const Item& b)
        {
            const int ca = order.indexOf (a.category), cb = order.indexOf (b.category);
            return ca != cb ? ca < cb : a.name.compareIgnoreCase (b.name) < 0;
        });
        for (auto& it : main) list.push_back (it);

        userFolder().createDirectory();
        userWavetableFolder().createDirectory();
        userNoiseFolder().createDirectory();

        const auto root = userFolder();
        auto files = root.findChildFiles (juce::File::findFiles, true, "*.nsp");
        std::sort (files.begin(), files.end(), [&root] (const juce::File& a, const juce::File& b)
        {
            return a.getRelativePathFrom (root).compareNatural (b.getRelativePathFrom (root)) < 0;
        });

        for (auto& f : files)
        {
            Item it;
            it.category = "USER";
            it.name = f.getFileNameWithoutExtension();
            it.file = f;
            const auto rel = f.getParentDirectory().getRelativePathFrom (root).replaceCharacter ('\\\\', '/');
            it.folder = f.getParentDirectory() == root ? juce::String() : rel;
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
        {
            applyPatchVar (apvts, Patch().toVar());
        }
        else if (it.resource >= 0)
        {
            int size = 0;
            const char* data = PresetData::getNamedResource (PresetData::namedResourceList[it.resource], size);
            if (data != nullptr)
                if (auto xml = juce::XmlDocument::parse (juce::String::fromUTF8 (data, size)))
                    applyStateXml (*xml);
        }
        else if (it.file.existsAsFile())
        {
            if (auto xml = juce::XmlDocument::parse (it.file))
                applyStateXml (*xml);
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

    static bool createUserSubFolder (const juce::String& rel)
    {
        juce::File f = userFolder();
        for (auto part : juce::StringArray::fromTokens (rel, "/\\\\", ""))
        {
            part = juce::File::createLegalFileName (part.trim());
            if (part.isNotEmpty()) f = f.getChildFile (part);
        }
        return f.createDirectory();
    }

    bool saveUser (const juce::String& rawName, const juce::String& relFolder = {})
    {
        auto name = juce::File::createLegalFileName (rawName.trim());
        if (name.isEmpty()) return false;

        auto folder = userFolder();
        for (auto part : juce::StringArray::fromTokens (relFolder, "/\\\\", ""))
            if (part.isNotEmpty()) folder = folder.getChildFile (juce::File::createLegalFileName (part));
        if (! folder.createDirectory()) return false;

        if (beforeSave) beforeSave();
        auto state = apvts.copyState();
        state.setProperty ("presetName", name, nullptr);

        if (auto xml = state.createXml())
        {
            auto file = folder.getChildFile (name + ".nsp");
            if (! xml->writeTo (file)) return false;
            apvts.state.setProperty ("presetName", name, nullptr);
            if (afterSave) afterSave (name);
            refresh();
            for (int i = 0; i < (int) list.size(); ++i)
                if (list[(size_t) i].file == file) { current = i; break; }
            return true;
        }
        return false;
    }

private:
    void applyStateXml (const juce::XmlElement& xml)
    {
        if (! xml.hasTagName (apvts.state.getType())) return;
        for (auto* p : apvts.processor.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                rp->setValueNotifyingHost (rp->getDefaultValue());
        auto tree = juce::ValueTree::fromXml (xml);
        migrateLegacyState (tree);
        apvts.replaceState (tree);
    }

    juce::AudioProcessorValueTreeState& apvts;
    std::vector<Item> list;
    int current = 0;
};
}
