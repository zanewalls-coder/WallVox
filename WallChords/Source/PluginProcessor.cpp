#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace th;

static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using C = juce::AudioParameterChoice;
    using F = juce::AudioParameterFloat;
    using B = juce::AudioParameterBool;
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    auto choice = [&] (const char* id, const char* name, const juce::StringArray& items, int def)
    { l.add (std::make_unique<C> (juce::ParameterID { id, 1 }, name, items, def)); };
    auto flt = [&] (const char* id, const char* name, float lo, float hi, float def, const char* unit = "")
    { l.add (std::make_unique<F> (juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (lo, hi), def,
                                  juce::AudioParameterFloatAttributes().withLabel (unit))); };

    choice ("genre", "Genre", genreNames, 0);
    choice ("key", "Key", { "C", "C#/Db", "D", "D#/Eb", "E", "F", "F#/Gb", "G", "G#/Ab", "A", "A#/Bb", "B" }, 0);
    choice ("mode", "Mode", { "Major", "Minor" }, 0);
    choice ("section", "Song Part", sectionChoices, 0);
    choice ("bars", "Bars", { "Auto", "4", "8", "16" }, 0);
    l.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "prog", 1 }, "Progression", 0, 8, 0));
    choice ("color", "Chord Color", colorNames, 0);
    choice ("chordStyle", "Chord Style", chordStyles, 0);
    choice ("bassStyle", "Bass Style", bassStyles, 0);
    choice ("leadStyle", "Lead Style", leadStyles, 0);
    choice ("padStyle", "Pad Style", padStyles, 0);
    for (int i = 0; i < 4; ++i)
        l.add (std::make_unique<B> (juce::ParameterID { "on" + juce::String (i), 1 }, partNames[i] + " On", true));
    flt ("movement", "Movement", 0, 1, 0.5f);
    flt ("humanize", "Humanize", 0, 1, 0.35f);
    flt ("strum", "Strum Speed", 4, 60, 18, "ms");
    flt ("swell", "Swell", 0, 1, 0.75f);
    flt ("dynamics", "Dynamics", 0, 1, 0.6f);
    l.add (std::make_unique<B> (juce::ParameterID { "liveMode", 1 }, "Live Chords", false));
    flt ("liveIntensity", "Live Intensity", 0, 1, 0.65f);
    l.add (std::make_unique<B> (juce::ParameterID { "followDaw", 1 }, "Play With DAW", true));
    l.add (std::make_unique<B> (juce::ParameterID { "sound", 1 }, "Built-in Sound", true));
    flt ("volume", "Volume", -30, 6, -6, "dB");
    return l;
}

static const char* regenParams[] = { "genre", "key", "mode", "section", "bars", "prog", "color", "chordStyle", "bassStyle",
                                     "leadStyle", "padStyle", "on0", "on1", "on2", "on3", "movement", "humanize",
                                     "strum", "swell", "dynamics" };

WallChordsProcessor::WallChordsProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "WallChords", createLayout())
{
    for (auto* id : regenParams) apvts.addParameterListener (id, this);
    liveNotes.reserve (4096); liveCCs.reserve (1024); liveEvs.reserve (8192); liveVoicing.reserve (16);
    for (auto& m : partMidi) m.ensureSize (4096);
    seed = juce::Random::getSystemRandom().nextInt ({ 1, 100000 });
    regenerate();
}

WallChordsProcessor::~WallChordsProcessor()
{
    for (auto* id : regenParams) apvts.removeParameterListener (id, this);
    cancelPendingUpdate();
}

bool WallChordsProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void WallChordsProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    for (auto& s : synths) s.prepare (sr);
}

eng::Settings WallChordsProcessor::currentSettings() const
{
    eng::Settings s;
    s.genre = (int) p ("genre"); s.key = (int) p ("key"); s.minor = (int) p ("mode") == 1;
    s.sectionChoice = (int) p ("section"); s.barsChoice = (int) p ("bars"); s.prog = (int) p ("prog");
    s.color = (int) p ("color");
    s.chordStyle = (int) p ("chordStyle"); s.bassStyle = (int) p ("bassStyle");
    s.leadStyle = (int) p ("leadStyle"); s.padStyle = (int) p ("padStyle");
    for (int i = 0; i < 4; ++i) s.partOn[i] = p (("on" + juce::String (i)).toRawUTF8()) > 0.5f;
    s.movement = p ("movement"); s.humanize = p ("humanize"); s.strumMs = p ("strum");
    s.swell = p ("swell"); s.dynamics = p ("dynamics");
    s.bpm = lastBpm.load();
    s.seed = seed.load();
    return s;
}

void WallChordsProcessor::regenerate()
{
    std::shared_ptr<const eng::Song> fresh = eng::generate (currentSettings());
    {
        const juce::SpinLock::ScopedLockType sl (songLock);
        if (song != nullptr) retired.push_back (song);
        song = fresh;
    }
    // free old songs only once the audio thread has let go of them
    retired.erase (std::remove_if (retired.begin(), retired.end(), [] (auto& s) { return s.use_count() == 1; }), retired.end());
}

std::shared_ptr<const eng::Song> WallChordsProcessor::getSong() const
{
    const juce::SpinLock::ScopedLockType sl (songLock);
    return song;
}

void WallChordsProcessor::newIdea()
{
    seed = juce::Random::getSystemRandom().nextInt ({ 1, 100000 });
    regenerate();
}

void WallChordsProcessor::startPreview (double fromBeat) { previewFrom = fromBeat; previewRestart = true; preview = true; }
void WallChordsProcessor::stopPreview() { preview = false; }

juce::String WallChordsProcessor::getLiveChordName() const
{
    const juce::SpinLock::ScopedLockType sl (liveNameLock);
    return liveName;
}

int WallChordsProcessor::timbreFor (int part) const
{
    const int genre = (int) p ("genre");
    if (part == eng::PBass) return ps::Bass;
    if (part == eng::PPad) return ps::Pad;
    if (part == eng::PLead)
    {
        const int ls = (int) p ("leadStyle");
        return (ls == LAmbient || (ls == LAuto && genre == Worship)) ? ps::Pluck : ps::Lead;
    }
    switch ((int) p ("chordStyle"))
    {
        case CStrum: case CFunk: case CFingerpick: case CAmbient: case CPluckArp: return ps::Pluck;
        case CStabs: case CSupersaw: return ps::Saw;
        case CAuto: return genre == EDM ? ps::Saw : ps::Keys;
        default: return ps::Keys;
    }
}

// ------------------------------------------------------------------------------- playback
void WallChordsProcessor::allNotesOff (juce::MidiBuffer& out, int off)
{
    for (int part = 0; part < 4; ++part)
    {
        for (int n = 0; n < 128; ++n)
            if (active[part][n])
            {
                const auto m = juce::MidiMessage::noteOff (part + 1, n);
                out.addEvent (m, off);
                partMidi[part].addEvent (m, off);
                active[part][n] = false;
            }
        partMidi[part].addEvent (juce::MidiMessage::controllerEvent (part + 1, 64, 0), off);
    }
}

void WallChordsProcessor::schedule (const std::vector<eng::Ev>& evs, double L, double pos, double len, int n,
                                    juce::MidiBuffer& out)
{
    auto run = [&] (double a, double b, double offsetBeats)
    {
        auto it = std::lower_bound (evs.begin(), evs.end(), a, [] (const eng::Ev& e, double v) { return e.beat < v; });
        for (; it != evs.end() && it->beat < b; ++it)
        {
            const int s = juce::jlimit (0, n - 1, (int) ((it->beat - a + offsetBeats) / len * n));
            const int ch = it->part + 1;
            juce::MidiMessage m;
            if (it->type == 1)
            {
                if (active[it->part][it->d1]) { const auto off = juce::MidiMessage::noteOff (ch, it->d1); out.addEvent (off, s); partMidi[it->part].addEvent (off, s); }
                m = juce::MidiMessage::noteOn (ch, it->d1, (juce::uint8) juce::jmax (1, (int) it->d2));
                active[it->part][it->d1] = true;
            }
            else if (it->type == 0)
            {
                if (! active[it->part][it->d1]) continue;
                m = juce::MidiMessage::noteOff (ch, it->d1);
                active[it->part][it->d1] = false;
            }
            else
            {
                m = juce::MidiMessage::controllerEvent (ch, it->d1, it->d2);
                auto& sh = synths[it->part].shared;
                if (it->d1 == 1) sh.mod = it->d2 / 127.0f;
                if (it->d1 == 11) sh.expr = it->d2 / 127.0f;
            }
            out.addEvent (m, s);
            partMidi[it->part].addEvent (m, s);
        }
    };

    if (pos + len <= L) run (pos, pos + len, 0.0);
    else
    {
        run (pos, L, 0.0);
        run (0.0, pos + len - L, L - pos);
    }
}

void WallChordsProcessor::rebuildLive (double bpm)
{
    liveNotes.clear(); liveCCs.clear(); liveEvs.clear(); liveVoicing.clear();
    for (int i = 0; i < 128; ++i) if (held[i]) liveVoicing.push_back (i);
    if (liveVoicing.empty()) return;

    const int genre = (int) p ("genre"), key = (int) p ("key");
    const bool minor = (int) p ("mode") == 1;
    const float e = p ("liveIntensity");
    Chord chord;

    if (liveVoicing.size() == 1)
    {
        // one finger: build the chord from the key
        const int* sc = minor ? minorScale : majorScale;
        const int rel = ((liveVoicing[0] - key) % 12 + 12) % 12;
        int deg = -1;
        for (int i = 0; i < 7; ++i) if (sc[i] == rel) deg = i + 1;
        chord = deg > 0 ? makeChord (juce::String (deg), key, minor, genre, (int) p ("color"))
                        : makeChord ("1", rel, false, genre, (int) p ("color"));
    }
    else
    {
        chord.root = chord.bass = liveVoicing[0] % 12;
        for (int n : liveVoicing) chord.intervals.push_back (n - liveVoicing[0]);
        chord.name = noteName (chord.root, key, minor) + " (your voicing)";
    }

    int style = (int) p ("chordStyle");
    if (style == CAuto) style = eng::autoChordStyle (genre, "Chorus", e);
    const bool guitar = style == CStrum || style == CFunk || style == CFingerpick;
    std::vector<int> v;
    if (liveVoicing.size() == 1) v = guitar ? eng::guitarVoicing (chord) : eng::voiceChord (chord, {}, 52, 76);
    else
    {
        v = liveVoicing;
        if (guitar && v[0] - 12 >= 40) v.insert (v.begin(), v[0] - 12);
    }

    eng::Performer P (liveNotes, liveCCs, liveRng, bpm, p ("humanize"), p ("dynamics"), p ("movement"), p ("strum"));
    if (p ("on0") > 0.5f) P.chords (v, style, 0.0, 4.0, e, e);
    if (p ("on1") > 0.5f)
    {
        int bassPitch = 33 + ((chord.bass - 33) % 12 + 12) % 12;
        int bStyle = (int) p ("bassStyle");
        if (bStyle == BAuto) bStyle = eng::autoBassStyle (genre, "Chorus", e);
        P.bass (bassPitch, bassPitch, bStyle, 0.0, 4.0, e);
    }

    for (auto& nv : liveNotes)
    {
        const double b = std::fmod (nv.beat, 4.0);
        liveEvs.push_back ({ b, 1, (uint8_t) nv.part, (uint8_t) nv.pitch, (uint8_t) nv.vel });
        liveEvs.push_back ({ juce::jmin (b + nv.len, 3.999), 0, (uint8_t) nv.part, (uint8_t) nv.pitch, 0 });
    }
    for (auto& c : liveCCs)
        liveEvs.push_back ({ std::fmod (c.beat, 4.0), 2, (uint8_t) c.part, (uint8_t) c.cc, (uint8_t) c.val });
    auto rank = [] (uint8_t t) { return t == 0 ? 0 : t == 2 ? 1 : 2; };
    std::sort (liveEvs.begin(), liveEvs.end(), [&] (const eng::Ev& a, const eng::Ev& b)
               { return a.beat != b.beat ? a.beat < b.beat : rank (a.type) < rank (b.type); });

    const juce::SpinLock::ScopedTryLockType tl (liveNameLock);
    if (tl.isLocked()) liveName = chord.name;
}

void WallChordsProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    const int n = buffer.getNumSamples();
    if (n == 0) return;
    for (auto& m : partMidi) m.clear();
    juce::MidiBuffer out;

    // ---- host transport
    double bpm = 120.0, hostPpq = 0.0;
    bool hostPlaying = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = juce::jlimit (40.0, 300.0, *b);
            hostPlaying = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition()) hostPpq = *q;
        }
    if (std::abs (bpm - lastBpm.load()) > 0.5) { lastBpm = bpm; triggerAsyncUpdate(); }
    const double len = n * bpm / 60.0 / sr;

    // ---- live chord input
    const bool liveMode = p ("liveMode") > 0.5f;
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (liveMode && (m.isNoteOn() || m.isNoteOff()))
        {
            held[m.getNoteNumber()] = m.isNoteOn();
            heldChanged = true;
        }
        else if (! liveMode) out.addEvent (m, meta.samplePosition);   // pass through
    }
    bool anyHeld = false;
    for (bool h : held) anyHeld = anyHeld || h;
    if (! liveMode && anyHeld) { std::fill (std::begin (held), std::end (held), false); heldChanged = true; anyHeld = false; }

    {
        const juce::SpinLock::ScopedTryLockType tl (songLock);
        if (tl.isLocked()) audioSong = song;
    }

    // ---- clock
    if (previewRestart.exchange (false)) { previewBeat = previewFrom.load(); expectedNext = -1.0; }
    double b0 = 0.0;
    bool run = true;
    if (p ("followDaw") > 0.5f && hostPlaying && hostPpq >= 0.0) b0 = hostPpq;
    else if (preview.load()) { b0 = previewBeat; previewBeat += len; }
    else if (liveMode && (anyHeld || heldChanged)) b0 = freeBeat;
    else run = false;
    freeBeat += len;

    if (! run)
    {
        if (wasRunning) allNotesOff (out, 0);
        wasRunning = false;
        playhead = -1.0;
    }
    else
    {
        if (! wasRunning || std::abs (b0 - expectedNext) > 0.05) allNotesOff (out, 0);
        expectedNext = b0 + len;
        wasRunning = true;

        if (liveMode)
        {
            if (heldChanged)
            {
                allNotesOff (out, 0);
                rebuildLive (bpm);
                heldChanged = false;
            }
            if (! liveEvs.empty()) schedule (liveEvs, 4.0, std::fmod (b0, 4.0), len, n, out);
            playhead = -1.0;
        }
        else if (audioSong != nullptr && audioSong->length > 0.0)
        {
            const double pos = std::fmod (b0, audioSong->length);
            schedule (audioSong->events, audioSong->length, pos, len, n, out);
            playhead = pos;
        }
    }

    // ---- built-in sound
    if (p ("sound") > 0.5f)
    {
        for (int part = 0; part < 4; ++part)
        {
            synths[part].shared.timbre = timbreFor (part);
            synths[part].render (buffer, partMidi[part], n);
        }
        buffer.applyGain (juce::Decibels::decibelsToGain (p ("volume")));
    }
    midi.swapWith (out);
}

// ------------------------------------------------------------------------------- MIDI export
juce::File WallChordsProcessor::writeMidiFile (int partMask)
{
    auto s = getSong();
    if (s == nullptr) return {};
    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (960);
    juce::MidiMessageSequence tempo;
    tempo.addEvent (juce::MidiMessage::tempoMetaEvent ((int) (60000000.0 / s->bpm)));
    tempo.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4));
    mf.addTrack (tempo);

    juce::StringArray used;
    for (int part = 0; part < 4; ++part)
    {
        if (((partMask >> part) & 1) == 0) continue;
        juce::MidiMessageSequence seq;
        seq.addEvent (juce::MidiMessage::textMetaEvent (3, partNames[part]));
        bool any = false;
        for (auto& nv : s->notes)
            if (nv.part == part)
            {
                seq.addEvent (juce::MidiMessage::noteOn (1, nv.pitch, (juce::uint8) nv.vel), nv.beat * 960.0);
                seq.addEvent (juce::MidiMessage::noteOff (1, nv.pitch), juce::jmin (nv.beat + nv.len, s->length) * 960.0);
                any = true;
            }
        for (auto& c : s->ccs)
            if (c.part == part) seq.addEvent (juce::MidiMessage::controllerEvent (1, c.cc, c.val), c.beat * 960.0);
        if (! any) continue;
        seq.sort();
        seq.updateMatchedPairs();
        mf.addTrack (seq);
        used.add (partNames[part]);
    }

    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("WallChords");
    dir.createDirectory();
    const juce::String label = genreNames[s->genre] + " " + noteName (s->key, s->key, s->minor) + (s->minor ? "m " : " ")
                               + (used.size() == 1 ? used[0] : juce::String ("All Parts"));
    auto file = dir.getChildFile (juce::File::createLegalFileName ("Wall Chords " + label + ".mid"));
    file.deleteFile();
    juce::FileOutputStream os (file);
    if (! os.openedOk() || ! mf.writeTo (os)) return {};
    return file;
}

// ------------------------------------------------------------------------------- state
void WallChordsProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto st = apvts.copyState();
    st.setProperty ("seed", seed.load(), nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, dest);
}

void WallChordsProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto st = juce::ValueTree::fromXml (*xml);
            if (st.hasProperty ("seed")) seed = (int) st.getProperty ("seed");
            apvts.replaceState (st);
            triggerAsyncUpdate();
        }
}

juce::AudioProcessorEditor* WallChordsProcessor::createEditor() { return new WallChordsEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WallChordsProcessor(); }
