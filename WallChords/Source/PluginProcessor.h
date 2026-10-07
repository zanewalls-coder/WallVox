#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Learn.h"
#include "Sampler.h"

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

    // ---- editor API (message thread)
    juce::AudioProcessorValueTreeState apvts;
    lib::Library library;
    std::shared_ptr<const gen::Loop> getLoop() const;
    void newIdea();
    void stepHistory (int delta);
    juce::String historyLabel() const;
    void regenerate();
    void addToLibrary (const juce::StringArray& paths);
    juce::File writeMidiFile (int partMask);
    void startPreview() { previewRestart = true; preview = true; }
    void stopPreview() { preview = false; }
    bool isPreviewing() const { return preview.load(); }
    double getPlayhead() const { return playhead.load(); }
    juce::String getLiveChordName() const;
    int instrumentParamIndex() const;          // instrument for the current mode
    juce::String instrumentParamId() const;
    void refreshInstruments();
    void downloadSoundPack (std::function<void (juce::String)> done);
    std::shared_ptr<std::atomic<float>> downloadProgress = std::make_shared<std::atomic<float>> (-1.0f);

private:
    void handleAsyncUpdate() override { regenerate(); }
    void parameterChanged (const juce::String&, float) override;
    float p (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    gen::Settings currentSettings() const;
    void loadInstrumentFor (smp::Player& player, int index);

    void schedule (const std::vector<eng::Ev>& evs, double loopLen, double pos, double len, int n, juce::MidiBuffer& out);
    void allNotesOff (juce::MidiBuffer& out);
    void rebuildLive (double bpm);

    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
    std::shared_ptr<const gen::Loop> loop, audioLoop;
    std::vector<std::shared_ptr<const gen::Loop>> retired;
    juce::SpinLock loopLock;

    std::vector<int> history;
    int historyPos = 0;
    std::atomic<int> seed { 1 };
    std::atomic<double> lastBpm { 120.0 }, playhead { -1.0 };
    std::atomic<bool> preview { false }, previewRestart { false };

    double sr = 44100.0, previewBeat = 0.0, freeBeat = 0.0, expectedNext = -1.0;
    bool wasRunning = false;
    bool active[4][128] {};
    smp::Player mainPlayer, backingPlayer;
    juce::MidiBuffer partMidi[4];
    juce::AudioBuffer<float> backBuf;
    juce::AudioFormatManager formats;
    std::map<int, std::shared_ptr<const smp::Instrument>> loadedInstruments;
    int mainInstrument = -1, backingInstrument = -1;

    // live chord mode
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
