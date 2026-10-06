#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include <array>

namespace th
{
enum Genre { Worship = 0, Pop = 1, EDM = 2 };
enum Role { RoleMain, RolePre, RoleBridge };

inline const juce::StringArray genreNames { "Worship", "Pop", "EDM" };
inline const juce::StringArray partNames  { "Chords", "Bass", "Lead", "Pad" };

inline const juce::StringArray chordStyles { "Auto", "Block Chords", "Acoustic Strum", "Funk Pop Strum", "Fingerpick",
                                             "Ambient Arp", "Piano Flow", "Pop Piano Bounce", "Rolled Piano",
                                             "Offbeat Stabs", "Pluck Arp", "Supersaw Hits" };
enum ChordStyle { CAuto, CBlock, CStrum, CFunk, CFingerpick, CAmbient, CPianoFlow, CPopBounce, CRolled, CStabs, CPluckArp, CSupersaw };

inline const juce::StringArray bassStyles { "Auto", "Whole Notes", "Root 8ths", "Octave Bounce", "Syncopated", "Walking", "Offbeat", "Sub Sustain" };
enum BassStyle { BAuto, BWhole, BRoot8, BOctave, BSync, BWalk, BOffbeat, BSub };

inline const juce::StringArray leadStyles { "Auto", "Topline Hook", "Ambient Octaves", "Anthem Lead", "Arp Lead" };
enum LeadStyle { LAuto, LHook, LAmbient, LAnthem, LArp };

inline const juce::StringArray padStyles { "Auto", "Sustained", "Chord Swells", "Section Swells", "Pulse" };
enum PadStyle { PAuto, PSustain, PChordSwell, PSectionSwell, PPulse };

inline const juce::StringArray colorNames { "Auto", "Simple Triads", "Add9 & Sus", "7ths", "Lush 9ths" };
inline const juce::StringArray sectionChoices { "Full Song", "Intro", "Verse", "Pre-Chorus", "Chorus", "Bridge",
                                                "Build", "Drop", "Breakdown", "Outro" };

// ---- Song structures ---------------------------------------------------------
struct SectionDef
{
    const char* name;
    int bars;
    float e0, e1;      // energy at start and end (differs for builds)
    Role role;
    const char* parts; // C = chords, B = bass, L = lead, P = pad
    bool build;
};

inline const std::vector<SectionDef>& structure (int genre)
{
    static const std::vector<SectionDef> worship {
        { "Intro", 8, .30f, .30f, RoleMain, "CPL", false },
        { "Verse 1", 16, .40f, .42f, RoleMain, "CP", false },
        { "Pre-Chorus", 8, .55f, .62f, RolePre, "CPB", false },
        { "Chorus", 16, .80f, .82f, RoleMain, "CPBL", false },
        { "Verse 2", 16, .50f, .52f, RoleMain, "CPB", false },
        { "Pre-Chorus", 8, .60f, .68f, RolePre, "CPB", false },
        { "Chorus", 16, .85f, .87f, RoleMain, "CPBL", false },
        { "Bridge", 16, .40f, 1.0f, RoleBridge, "CPBL", true },
        { "Chorus", 16, 1.0f, 1.0f, RoleMain, "CPBL", false },
        { "Outro", 8, .30f, .25f, RoleMain, "CPL", false } };
    static const std::vector<SectionDef> pop {
        { "Intro", 4, .50f, .50f, RoleMain, "CPL", false },
        { "Verse 1", 8, .50f, .52f, RoleMain, "CB", false },
        { "Pre-Chorus", 4, .58f, .75f, RolePre, "CBP", false },
        { "Chorus", 8, .90f, .90f, RoleMain, "CBP", false },
        { "Post-Chorus", 4, .80f, .80f, RoleMain, "CBL", false },
        { "Verse 2", 8, .55f, .57f, RoleMain, "CBP", false },
        { "Pre-Chorus", 4, .60f, .78f, RolePre, "CBP", false },
        { "Chorus", 8, .95f, .95f, RoleMain, "CBP", false },
        { "Post-Chorus", 4, .85f, .85f, RoleMain, "CBL", false },
        { "Bridge", 8, .45f, .55f, RoleBridge, "CP", false },
        { "Chorus", 8, 1.0f, 1.0f, RoleMain, "CBPL", false },
        { "Outro", 4, .40f, .35f, RoleMain, "CPL", false } };
    static const std::vector<SectionDef> edm {
        { "Intro", 16, .40f, .45f, RoleMain, "CP", false },
        { "Breakdown", 16, .35f, .40f, RoleMain, "CPL", false },
        { "Build", 8, .50f, 1.0f, RoleMain, "CPL", true },
        { "Drop", 16, 1.0f, 1.0f, RoleMain, "CBLP", false },
        { "Breakdown", 16, .40f, .45f, RoleBridge, "CP", false },
        { "Build", 8, .50f, 1.0f, RoleMain, "CPL", true },
        { "Drop", 16, 1.0f, 1.0f, RoleMain, "CBLP", false },
        { "Outro", 8, .40f, .30f, RoleMain, "CP", false } };
    return genre == Worship ? worship : genre == Pop ? pop : edm;
}

// Map a "Song Part" choice to a section index for the genre
inline int findSection (int genre, int choice)
{
    static const char* fallback[3][10] = {
        { "", "Intro", "Verse", "Pre-Chorus", "Chorus", "Bridge", "Bridge", "Chorus", "Bridge", "Outro" },
        { "", "Intro", "Verse", "Pre-Chorus", "Chorus", "Bridge", "Pre-Chorus", "Post-Chorus", "Bridge", "Outro" },
        { "", "Intro", "Breakdown", "Build", "Drop", "Breakdown", "Build", "Drop", "Breakdown", "Outro" } };
    const auto& s = structure (genre);
    const juce::String want (fallback[genre][juce::jlimit (1, 9, choice)]);
    for (size_t i = 0; i < s.size(); ++i)
        if (juce::String (s[i].name).startsWith (want)) return (int) i;
    return 0;
}

// ---- Progressions (scale degrees in the current key) ----------------------------
// Token: [b]degree[m|M][/bassDegree]   e.g. "5/7" = V over the 7th, "4m" = borrowed iv, "5M" = major V in minor
inline const juce::StringArray& progressions (int genre, bool minor, Role role)
{
    static const juce::StringArray wMajMain { "1 5/7 6 4", "4 1 5 6", "1 4 6 5", "6 4 1 5", "1 1/3 4 4", "1 6 4 5" };
    static const juce::StringArray wMajPre  { "4 5 6 5/7", "2 1/3 4 5", "4 5 4 5" };
    static const juce::StringArray wMajBr   { "4 5 6 1/3", "4 1/3 5 6", "6 5 4 1" };
    static const juce::StringArray wMinMain { "1 6 3 7", "6 3 7 1", "1 7 6 7", "1 3 7 6" };
    static const juce::StringArray wMinPre  { "6 7 1 1", "4 6 7 7" };
    static const juce::StringArray wMinBr   { "6 7 1 3", "4 6 7 1" };

    static const juce::StringArray pMajMain { "1 5 6 4", "6 4 1 5", "1 6 4 5", "4 5 3 6", "1 3 4 4m", "2 5 1 6" };
    static const juce::StringArray pMajPre  { "4 5 3 6", "2 3 4 5", "4 4 5 5" };
    static const juce::StringArray pMajBr   { "6 4 1 5", "4 5 6 6", "2 4 1 5" };
    static const juce::StringArray pMinMain { "1 6 3 7", "1 4 6 5M", "6 7 1 1", "1 7 6 5M" };
    static const juce::StringArray pMinPre  { "6 7 4 5M", "4 6 7 7" };
    static const juce::StringArray pMinBr   { "6 3 7 1", "4 1 6 7" };

    static const juce::StringArray eMinMain { "1 6 3 7", "6 7 1 1", "1 7 6 7", "6 4 1 5", "1 6 4 7", "4 6 7 1" };
    static const juce::StringArray eMinBr   { "6 7 1 1", "4 6 7 7" };
    static const juce::StringArray eMajMain { "6 4 1 5", "1 5 6 4", "4 5 6 6", "4 1 5 6" };
    static const juce::StringArray eMajBr   { "4 5 6 6", "6 5 4 4" };

    if (genre == Worship)
        return minor ? (role == RolePre ? wMinPre : role == RoleBridge ? wMinBr : wMinMain)
                     : (role == RolePre ? wMajPre : role == RoleBridge ? wMajBr : wMajMain);
    if (genre == Pop)
        return minor ? (role == RolePre ? pMinPre : role == RoleBridge ? pMinBr : pMinMain)
                     : (role == RolePre ? pMajPre : role == RoleBridge ? pMajBr : pMajMain);
    return minor ? (role == RoleBridge ? eMinBr : eMinMain) : (role == RoleBridge ? eMajBr : eMajMain);
}

// ---- Chords ---------------------------------------------------------------------
inline const int majorScale[7] { 0, 2, 4, 5, 7, 9, 11 };
inline const int minorScale[7] { 0, 2, 3, 5, 7, 8, 10 };

struct Chord
{
    int root = 0, bass = 0;       // pitch classes
    std::vector<int> intervals;   // from root, may exceed 12
    juce::String name;
};

inline juce::String noteName (int pc, int key, bool minor)
{
    static const char* sharps[12] { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
    static const char* flats[12]  { "C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B" };
    const int majKey = minor ? (key + 3) % 12 : key;
    const bool useFlats = majKey == 5 || majKey == 10 || majKey == 3 || majKey == 8 || majKey == 1 || majKey == 6;
    return (useFlats ? flats : sharps)[((pc % 12) + 12) % 12];
}

// color: 0 auto, 1 simple, 2 add9/sus, 3 7ths, 4 lush 9ths. susPart: 0 normal, 1 = sus4 half of a V chord
inline Chord makeChord (const juce::String& token, int key, bool minor, int genre, int color, bool susHalf = false)
{
    const int* sc = minor ? minorScale : majorScale;
    juce::String t = token.trim();
    int flat = 0;
    if (t.startsWithChar ('b')) { flat = -1; t = t.substring (1); }
    const int deg = juce::jlimit (1, 7, t.substring (0, 1).getIntValue());
    juce::String rest = t.substring (1);
    int bassDeg = 0;
    if (rest.contains ("/")) { bassDeg = rest.fromFirstOccurrenceOf ("/", false, false).getIntValue(); rest = rest.upToFirstOccurrenceOf ("/", false, false); }

    const int i = deg - 1;
    const int rootOff = sc[i] + flat;
    auto rel = [&] (int step) { const int k = i + step; return sc[k % 7] + 12 * (k / 7) - sc[i]; };
    int third = rel (2), fifth = rel (4), seventh = rel (6);
    if (rest == "m") { third = 3; fifth = 7; seventh = 10; }
    if (rest == "M") { third = 4; fifth = 7; seventh = 10; }
    if (flat != 0)   { third = 4; fifth = 7; seventh = 10; }  // borrowed bVII etc. are major

    const bool isMin = third == 3 && fifth == 7, isDim = third == 3 && fifth == 6;
    const bool dominant = ! isMin && ! isDim && seventh == 10;
    if (color == 0) color = genre == Pop ? (isMin ? 3 : 1) : 2;

    Chord c;
    c.root = (key + rootOff + 12) % 12;
    juce::String suffix;
    if (isDim) { c.intervals = { 0, 3, 6, 10 }; suffix = "m7b5"; if (color == 1) { c.intervals = { 0, 3, 6 }; suffix = "dim"; } }
    else if (color == 1) { c.intervals = { 0, third, fifth }; suffix = isMin ? "m" : ""; }
    else if (color == 2)
    {
        if (susHalf) { c.intervals = { 0, 5, 7 }; suffix = "sus4"; }
        else if (isMin) { c.intervals = { 0, 3, 7, 10 }; suffix = "m7"; }
        else { c.intervals = { 0, 4, 7, 14 }; suffix = "add9"; }
    }
    else if (color == 3)
    {
        if (isMin) { c.intervals = { 0, 3, 7, 10 }; suffix = "m7"; }
        else if (dominant && deg == 5) { c.intervals = { 0, 4, 7, 10 }; suffix = "7"; }
        else { c.intervals = { 0, 4, 7, 11 }; suffix = "maj7"; }
    }
    else
    {
        if (isMin) { c.intervals = { 0, 3, 7, 10, 14 }; suffix = "m9"; }
        else if (dominant && deg == 5) { c.intervals = { 0, 4, 7, 10, 14 }; suffix = "9"; }
        else { c.intervals = { 0, 4, 7, 11, 14 }; suffix = "maj9"; }
    }

    c.bass = c.root;
    c.name = noteName (c.root, key, minor) + suffix;
    if (bassDeg >= 1 && bassDeg <= 7)
    {
        c.bass = (key + sc[bassDeg - 1]) % 12;
        if (c.bass != c.root) c.name << "/" << noteName (c.bass, key, minor);
    }
    return c;
}

inline juce::String progressionLabel (const juce::String& prog, int key, bool minor, int genre, int color)
{
    juce::StringArray out;
    for (auto& tok : juce::StringArray::fromTokens (prog, " ", ""))
        if (tok.isNotEmpty()) out.add (makeChord (tok, key, minor, genre, color).name);
    return out.joinIntoString ("  -  ");
}

} // namespace th
