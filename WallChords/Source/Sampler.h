#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include "Synth.h"

// Multisampled instruments (from the downloadable sound pack) with synth fallbacks.
namespace smp
{
struct InstDef { const char* name; const char* folder; int fallback; int octave; };

// folder = name of the sampled instrument in the sound pack ("" = synthesised)
inline const std::vector<InstDef>& instruments()
{
    static const std::vector<InstDef> list {
        { "Grand Piano",     "grand_piano",    ps::Keys,     0 },
        { "Soft Piano",      "soft_piano",     ps::Keys,     0 },
        { "Electric Piano",  "electric_piano", ps::Keys,     0 },
        { "Acoustic Guitar", "",               ps::AcGuitar, 0 },
        { "Electric Guitar", "",               ps::ElGuitar, 0 },
        { "Strings",         "strings",        ps::Pad,      0 },
        { "Pizzicato",       "pizzicato",      ps::Pluck,    0 },
        { "Harp",            "harp",           ps::Pluck,    0 },
        { "Vibraphone",      "vibraphone",     ps::Keys,     0 },
        { "Organ",           "organ",          ps::Pad,      0 },
        { "Upright Bass",    "upright_bass",   ps::Bass,     0 },
        { "Synth Bass",      "",               ps::Bass,     0 },
        { "Sub Bass",        "",               ps::Sub,      0 },
        { "Warm Pad",        "",               ps::Pad,      0 },
        { "Supersaw",        "",               ps::Saw,      0 },
        { "Synth Lead",      "",               ps::Lead,     0 },
        { "Pluck",           "",               ps::Pluck,    0 } };
    return list;
}
inline juce::StringArray instrumentNames()
{
    juce::StringArray s;
    for (auto& d : instruments()) s.add (d.name);
    return s;
}

struct Zone
{
    int root = 60, lo = 0, hi = 127, vlo = 0, vhi = 127;
    juce::AudioBuffer<float> data;
    double rate = 44100.0;
    int loopStart = -1, loopEnd = -1;
};

struct Instrument
{
    juce::String name;
    std::vector<Zone> zones;
    float attack = 0.002f, release = 0.35f, gain = 1.0f;
    bool loop = false;
};

std::shared_ptr<const Instrument> load (const juce::File& dir, juce::AudioFormatManager&);
juce::File soundsFolder();
bool soundPackInstalled();

class Voice : public juce::SynthesiserVoice
{
public:
    explicit Voice (std::shared_ptr<const Instrument>& current, juce::SpinLock& l) : inst (current), lock (l) {}
    bool canPlaySound (juce::SynthesiserSound*) override { return true; }
    void startNote (int note, float vel, juce::SynthesiserSound*, int) override;
    void stopNote (float, bool allowTailOff) override;
    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}
    void renderNextBlock (juce::AudioBuffer<float>&, int start, int num) override;
private:
    std::shared_ptr<const Instrument>& inst;
    juce::SpinLock& lock;
    std::shared_ptr<const Instrument> playing;
    const Zone* zone = nullptr;
    double pos = 0, step = 1;
    float gain = 1;
    juce::ADSR env;
};

// Plays one part: a sampled instrument when available, otherwise the built-in synth.
class Player
{
public:
    Player();
    void prepare (double sr);
    void setInstrument (int index, std::shared_ptr<const Instrument> sampled);   // message thread
    void render (juce::AudioBuffer<float>&, const juce::MidiBuffer&, int n);
    void allOff();
    ps::Synth synthFallback;
private:
    juce::Synthesiser sampler;
    std::shared_ptr<const Instrument> current;
    juce::SpinLock lock;
    std::atomic<bool> useSampler { false };
};

} // namespace smp
