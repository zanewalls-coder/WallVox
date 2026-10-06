#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "ChopDSP.h"
#include "PresetManager.h"
#include "WallLook.h"

class WallChopProcessor : public juce::AudioProcessor
{
public:
    WallChopProcessor();
    ~WallChopProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Wall Chop"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets;
    wl::ScopeData scope;

private:
    float p (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    void updateFilters();

    double sr = 44100.0;
    wv::PitchCorrector tuner;
    wc::Bender bender;
    wc::Stutter stutter;
    wc::TapeStop tapeStop;
    wc::Gate gate;
    wc::SweepFilter sweep;
    double freePpq = 0.0;
    wv::Biquad hpf, mud, deessSplit, deessSide, body, presence, air, dlyHp, dlyLp;
    float deessEnv = 0.0f;

    juce::dsp::Compressor<float> comp;
    juce::dsp::Limiter<float> limiter;

    wv::DelayBuffer dblLine[2], dlyLine[2];
    float dblPhase[2] {}, dlySamples = 0.0f;

    juce::Reverb reverb;
    juce::AudioBuffer<float> revBuf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WallChopProcessor)
};
