#pragma once
#include "Theory.h"
#include <memory>

namespace eng
{
enum Part { PChords = 0, PBass = 1, PLead = 2, PPad = 3 };

struct NoteEv { double beat, len; int pitch, vel, part; };
struct CCEv   { double beat; int cc, val, part; };
struct Ev     { double beat; uint8_t type, part, d1, d2; };   // type 0 = off, 1 = on, 2 = cc
struct Slot   { double start, len; th::Chord chord; int section; };
struct SectionInfo { juce::String name; double start, len; };

struct Song
{
    std::vector<NoteEv> notes;
    std::vector<CCEv> ccs;
    std::vector<Ev> events;       // sorted, for playback
    std::vector<Slot> slots;
    std::vector<SectionInfo> sections;
    double length = 16.0, bpm = 120.0;
    int genre = 0, key = 0;
    bool minor = false;
};

struct Settings
{
    int genre = 0, key = 0, sectionChoice = 0, barsChoice = 0, prog = 0, color = 0;
    bool minor = false;
    int chordStyle = 0, bassStyle = 0, leadStyle = 0, padStyle = 0;
    bool partOn[4] { true, true, true, true };
    float movement = 0.5f, humanize = 0.3f, strumMs = 18.0f, swell = 0.7f, dynamics = 0.6f;
    double bpm = 120.0;
    int seed = 1;
};

// Turns chords into performed MIDI (strums, picks, swells...). Also used live on the audio thread,
// so it only appends to vectors the caller has reserved.
class Performer
{
public:
    Performer (std::vector<NoteEv>& n, std::vector<CCEv>& c, juce::Random& r, double bpm,
               float humanize, float dynamics, float movement, float strumMs);

    void note (int part, double beat, double len, int pitch, float vel);
    void cc (int part, double beat, int num, int val);

    void chords (const std::vector<int>& voicing, int style, double start, double len, float e0, float e1);
    void buildRoll (const std::vector<int>& voicing, double start, double len, double secStart, double secLen);
    void bass (int pitch, int nextPitch, int style, double start, double len, float e);
    void walkUp (int fromPitch, int toPitch, const int* scale, int key, double start);
    void pad (const std::vector<int>& voicing, int style, double start, double len, float e,
              double secStart, double secEnd, bool build, float swell);

    static int density (float e, float movement);

private:
    float velocity (float raw);
    std::vector<NoteEv>& notes;
    std::vector<CCEv>& ccs;
    juce::Random& rng;
    double beatsPerSec;
    float humanize, dynamics, movement;
    double strum;
};

std::vector<int> voiceChord (const th::Chord&, const std::vector<int>& prev, int lo, int hi);
std::vector<int> guitarVoicing (const th::Chord&);
std::vector<int> scalePitches (int key, bool minor, int lo, int hi);

int autoChordStyle (int genre, const juce::String& section, float e);
int autoBassStyle (int genre, const juce::String& section, float e);
int autoLeadStyle (int genre);
int autoPadStyle (int genre, const juce::String& section, float e, bool build);

std::shared_ptr<Song> generate (const Settings&);
void finalise (Song&);   // builds the sorted playback list

} // namespace eng
