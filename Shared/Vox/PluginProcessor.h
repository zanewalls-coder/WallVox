#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Modules.h"
#include "WallLook.h"

namespace chain
{
constexpr int numSlots = 10;
struct SlotData
{
    int type = 0;
    bool on = true;
    float norm[mods::maxParams] {};
};
using Chain = std::vector<SlotData>;   // modules in order (empty slots removed)

inline juce::String typeId (int s)          { return "s" + juce::String (s) + "type"; }
inline juce::String onId (int s)            { return "s" + juce::String (s) + "on"; }
inline juce::String paramId (int s, int k)  { return "s" + juce::String (s) + "p" + juce::String (k); }
SlotData defaultsFor (int type);
}

class PresetManager;

class VoxProcessor : public juce::AudioProcessor,
                     private juce::AsyncUpdater,
                     private juce::AudioProcessorValueTreeState::Listener
{
public:
    VoxProcessor();
    ~VoxProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- chain editing (message thread)
    chain::Chain getChain() const;
    void setChain (const chain::Chain&);
    int addModule (int type);               // returns its position
    void removeModule (int pos);
    void moveModule (int from, int to);
    void duplicateModule (int pos);
    void replaceModule (int pos, int type);
    int slotType (int s) const;
    float realValue (int s, int k) const;
    float getMeter (int s) const { return meters[s].load(); }
    float getMeter2 (int s) const { return meters2[s].load(); }
    int getNote (int s) const { return notes[s].load(); }

    juce::AudioProcessorValueTreeState apvts;
    std::unique_ptr<PresetManager> presets;
    wl::ScopeData scope;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void handleAsyncUpdate() override { syncModules(); }
    void parameterChanged (const juce::String&, float) override { triggerAsyncUpdate(); }
    void syncModules();
    void updateLatency();
    float p (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }
    void setParam (const juce::String& id, float norm);

    std::unique_ptr<mods::Module> owned[chain::numSlots];
    juce::SpinLock chainLock;
    bool lastOn[chain::numSlots] {};
    std::atomic<float> meters[chain::numSlots], meters2[chain::numSlots];
    std::atomic<int> notes[chain::numSlots];
    std::atomic<float>* typeParams[chain::numSlots] {};
    std::atomic<float>* onParams[chain::numSlots] {};
    std::atomic<float>* valueParams[chain::numSlots][mods::maxParams] {};

    juce::AudioBuffer<float> dryBuf;
    double sr = 44100.0, freePpq = 0.0;
    int maxBlock = 512;
    float limGain = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxProcessor)
};
