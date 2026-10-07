#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <vector>
#include <functional>

// The user's sample library: dropped MIDI / audio files, analysed into notes and chords.
namespace lib
{
enum Kind { KChords = 0, KMelody = 1, KBass = 2 };
inline const char* kindName (int k) { return k == KChords ? "Chords" : k == KMelody ? "Melody" : "Bass"; }

// Chord qualities understood by the analyser / generator
enum Quality { QMaj, QMin, QDim, QSus2, QSus4, QDom7, QMaj7, QMin7, QNum };
inline const std::vector<int>& qualityIntervals (int q)
{
    static const std::vector<int> t[QNum] = { { 0, 4, 7 }, { 0, 3, 7 }, { 0, 3, 6 }, { 0, 2, 7 }, { 0, 5, 7 },
                                              { 0, 4, 7, 10 }, { 0, 4, 7, 11 }, { 0, 3, 7, 10 } };
    return t[juce::jlimit (0, (int) QNum - 1, q)];
}
inline const char* qualitySuffix (int q)
{
    static const char* s[QNum] = { "", "m", "dim", "sus2", "sus4", "7", "maj7", "m7" };
    return s[juce::jlimit (0, (int) QNum - 1, q)];
}

struct Note    { double beat, len; int pitch, vel; };
struct ChordEv { double beat, len; int root, bass, quality; };

struct Entry
{
    juce::String id, name, file, source;   // source: "midi" or "audio"
    int kind = KChords, key = 0, bars = 4;
    bool minor = false, enabled = true;
    double bpm = 120.0;
    std::vector<Note> notes;               // melody / bass lines, or chord voicings from MIDI
    std::vector<ChordEv> chords;           // chord entries
    std::vector<int> rhythm;               // one 16-step onset mask per bar

    juce::var toVar() const;
    static Entry fromVar (const juce::var&);
    juce::String describe() const;         // "C minor  -  92 BPM  -  4 bars"
};

// ---- analysis (pure functions, safe on a background thread)
std::vector<Entry> analyseMidi (const juce::File&);
std::vector<Entry> analyseAudio (const juce::File&, juce::AudioFormatManager&);
void detectKey (const float pcWeights[12], int& key, bool& minor);
std::vector<ChordEv> chordsFromNotes (const std::vector<Note>&, int bars);
void bestChord (const float w[12], int bassPc, int& root, int& quality, float& score);
juce::String chordName (int root, int quality, int bass, int key, bool minor);

// ---- persistent library
class Library
{
public:
    Library();
    ~Library();

    static juce::File folder();
    void load();
    void addFiles (const juce::StringArray& paths, std::function<void()> onChanged);
    void remove (const juce::String& id);
    void setEnabled (const juce::String& id, bool on);
    std::vector<Entry> snapshot() const;
    int pendingJobs() const { return pending.load(); }
    int changeCount() const { return changes.load(); }

    static bool isSupported (const juce::String& path);

private:
    void save();
    mutable juce::CriticalSection lock;
    std::vector<Entry> entries;
    std::atomic<int> pending { 0 }, changes { 0 };
    juce::ThreadPool pool { 1 };
    juce::AudioFormatManager formats;
};

} // namespace lib
