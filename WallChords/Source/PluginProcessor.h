#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine.h"
#include "Synth.h"

class WallChordsProcessor : public juce::AudioProcessor,
                            private juce::AsyncUpdater,
                            private juce::AudioProcessorValueTreeState::Listener
{
public:
    WallChordsProcessor();
    ~WallChordsProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Wall Chords"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    double getTailLengthSeconds() const override { return 2.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- used by the editor (message thread)
    juce::AudioProcessorValueTreeState apvts;
    std::shared_ptr<const eng::Song> getSong() const;
    void newIdea();
    int getSeed() const { return seed.load(); }
    juce::File writeMidiFile (int partMask);
    void startPreview (double fromBeat);
    void stopPreview();
    bool isPreviewing() const { return preview.load(); }
    double getPlayhead() const { return playhead.load(); }
    juce::String getLiveChordName() const;

private:
    void handleAsyncUpdate() override { regenerate(); }
    void parameterChanged (const juce::String&, float) override { triggerAsyncUpdate(); }
    void regenerate();
    eng::Settings currentSettings() const;
    float p (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    int timbreFor (int part) const;

    void schedule (const std::vector<eng::Ev>& evs, double loopLen, double pos, double len, int n, juce::MidiBuffer& out);
    void allNotesOff (juce::MidiBuffer& out, int sampleOffset);
    void rebuildLive (double bpm);

    std::shared_ptr<const eng::Song> song, audioSong;
    std::vector<std::shared_ptr<const eng::Song>> retired;
    juce::SpinLock songLock;

    std::atomic<int> seed { 1 };
    std::atomic<double> lastBpm { 120.0 }, playhead { -1.0 }, previewFrom { 0.0 };
    std::atomic<bool> preview { false }, previewRestart { false };

    double sr = 44100.0, previewBeat = 0.0, freeBeat = 0.0, expectedNext = -1.0;
    bool wasRunning = false;
    bool active[4][128] {};
    ps::Synth synths[4];
    juce::MidiBuffer partMidi[4];

    // live chord mode (audio thread)
    bool held[128] {};
    bool heldChanged = false;
    std::vector<eng::NoteEv> liveNotes;
    std::vector<eng::CCEv> liveCCs;
    std::vector<eng::Ev> liveEvs;
    std::vector<int> liveVoicing;
    juce::Random liveRng;
    juce::String liveName;
    juce::SpinLock liveNameLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WallChordsProcessor)
};
