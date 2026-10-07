#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <atomic>
#include <memory>
#include <vector>

// The building blocks of the WallVox / Wall Chop chain.
namespace mods
{
constexpr int maxParams = 8;

struct ParamSpec
{
    const char* name;
    float min, max, def, centre;     // centre > 0 = skewed knob
    const char* unit;                // "dB", "ms", "Hz", "st", "cents", "s", "%", ":1", "x" (plain), ""
    juce::StringArray choices;       // non-empty = drop-down

    bool isChoice() const { return ! choices.isEmpty(); }
    float toValue (float norm) const;
    float toNorm (float value) const;
    juce::String text (float value) const;
};

enum Meter { MeterNone, MeterGR, MeterEQ, MeterPitch };

struct ModuleInfo
{
    const char* id;
    const char* name;
    const char* group;
    const char* description;
    juce::uint32 colour;
    Meter meter;
    std::vector<ParamSpec> params;
};

// Index 0 is "Empty". Wall Chop builds include the chop modules at the end.
const std::vector<ModuleInfo>& catalogue();
int indexOf (const juce::String& id);
juce::StringArray typeNames();

struct Context
{
    double sampleRate = 44100, bpm = 120, ppq = 0, beatsPerSample = 0;
    bool playing = false;
    int key = 0;
};

class Module
{
public:
    virtual ~Module() = default;
    virtual void prepare (double sampleRate, int maxBlock) = 0;
    // values = real (de-normalised) parameter values
    virtual void process (juce::AudioBuffer<float>&, int numCh, const Context&, const float* values, bool enabled) = 0;
    virtual int latency() const { return 0; }
    virtual bool handlesBypass() const { return false; }   // true: gets called even when switched off
    int type = 0;
    std::atomic<float> meter { 0.0f };       // gain reduction dB (positive) for dynamics
    std::atomic<float> meter2 { 0.0f };      // extra readout (tuner: target note, cents)
    std::atomic<int> note { -1 };
};

std::unique_ptr<Module> create (int type);

// EQ curve for the UI (dB at a frequency) computed from the same filter designs the audio uses
float eqResponseDb (int type, const float* values, float freq);

} // namespace mods
