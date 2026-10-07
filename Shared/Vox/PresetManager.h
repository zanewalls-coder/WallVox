#pragma once
#include "PluginProcessor.h"
#include <functional>

// Presets are small JSON files describing a chain:
// {"name": "...", "category": "...", "in": 0, "out": 0,
//  "chain": [{"type": "fet", "on": true, "params": {"Input": 12, "Ratio": "4:1"}}, ...]}
// Older (v1) presets with a flat "params" object are converted automatically. Presets never contain code.
class PresetManager
{
public:
    struct Preset
    {
        juce::String name, category;
        juce::var data;
        bool factory = false;
    };

    explicit PresetManager (VoxProcessor&);

    void refresh();
    const juce::Array<Preset>& getAll() const { return list; }
    void apply (int index);
    void applyDefault();
    bool saveCurrent (const juce::String& name);
    juce::String importFile (const juce::File&);   // error message, or empty on success

    static juce::File getUserDir();
    static bool isValid (const juce::var&);
    static chain::Chain toChain (const juce::var& preset, float& inDb, float& outDb);
    static void downloadOnline (std::function<void (juce::String)> onDone);

    juce::String currentName;

private:
    VoxProcessor& proc;
    juce::Array<Preset> list;
};
