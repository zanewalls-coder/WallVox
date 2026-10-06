#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>

// Presets are small JSON files: {"name": "...", "category": "...", "params": {"air": 4.0, ...}}
// They only ever contain parameter values, never code.
class PresetManager
{
public:
    struct Preset
    {
        juce::String name, category;
        juce::var params;
        bool factory = false;
    };

    explicit PresetManager (juce::AudioProcessorValueTreeState&);

    void refresh();
    const juce::Array<Preset>& getAll() const { return list; }
    void apply (int index);
    bool saveCurrent (const juce::String& name);
    juce::String importFile (const juce::File&);   // returns an error message, or empty on success

    static juce::File getUserDir();
    static bool isValid (const juce::var&);

    // Fetches the online preset list on a background thread and saves new presets.
    // onDone is called on the message thread with a status message.
    static void downloadOnline (std::function<void (juce::String)> onDone);

    juce::String currentName;

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::Array<Preset> list;
};
