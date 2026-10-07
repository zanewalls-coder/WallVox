#pragma once
#include "Engine.h"
#include "Library.h"

// Generates 4/6/8 bar loops that resemble the user's library (or a built-in style).
namespace gen
{
enum Mode { MChords = 0, MMelody = 1, MBass = 2 };
enum Source { SLibrary = 0, SWorship = 1, SPop = 2, SEDM = 3 };

inline const juce::StringArray modeNames   { "Chords", "Melody", "Bass" };
inline const juce::StringArray sourceNames { "My Library", "Worship", "Pop", "EDM" };
inline const juce::StringArray barChoices  { "4 Bars", "6 Bars", "8 Bars" };
inline int barsFor (int choice) { return choice == 1 ? 6 : choice == 2 ? 8 : 4; }

// Chord rhythm / articulation choices
inline const juce::StringArray rhythmNames { "From Library", "Block", "Strum", "Funk Strum", "Fingerpick", "Ambient Arp",
                                             "Piano Flow", "Pop Bounce", "Rolled", "Stabs", "Pluck Arp", "Supersaw Hits", "Sustain" };
enum Rhythm { RLibrary, RBlock, RStrum, RFunk, RFingerpick, RAmbient, RPianoFlow, RBounce, RRolled, RStabs, RPluckArp, RSupersaw, RSustain };

struct Settings
{
    int mode = MChords, source = SLibrary, bars = 4, key = 0, rhythm = RLibrary, octave = 0, seed = 1;
    bool minor = false, backing = true;
    float similarity = 0.7f, complexity = 0.5f, humanize = 0.3f, strumMs = 18.0f;
    double bpm = 120.0;
};

struct Loop
{
    std::vector<eng::Slot> chords;       // the progression (shown in the display)
    eng::Song song;                      // notes: part 0 = main, part 3 = backing chords
    juce::String info;                   // e.g. "Inspired by: Dreamy Keys (+3 more)"
};

std::shared_ptr<Loop> generate (const Settings&, const std::vector<lib::Entry>& library);
int engineStyleFor (int rhythm);   // maps a Rhythm choice to an eng chord style

} // namespace gen
