#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "WallLook.h"

using namespace th;

static const char* soundPackUrl = "https://github.com/zanewalls-coder/WallVox/releases/download/sounds/WallChords-Sounds.zip";

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
                                  juce::AudioParameterFloatAttributes()
                                      .withStringFromValueFunction (wl::valueToText (unit, lo, hi))
                                      .withValueFromStringFunction (wl::textToValue (unit, lo, hi)))); };

    choice ("mode", "Mode", gen::modeNames, 0);
    choice ("source", "Learn From", gen::sourceNames, 0);
    choice ("bars", "Length", gen::barChoices, 0);
    choice ("key", "Key", { "C", "C#/Db", "D", "D#/Eb", "E", "F", "F#/Gb", "G", "G#/Ab", "A", "A#/Bb", "B" }, 0);
    choice ("scale", "Scale", { "Major", "Minor" }, 0);
    choice ("rhythm", "Rhythm", gen::rhythmNames, 0);
    const auto inst = smp::instrumentNames();
    choice ("instChords", "Chord Sound", inst, 0);
    choice ("instMelody", "Melody Sound", inst, 15);
    choice ("instBass", "Bass Sound", inst, 11);
    choice ("octave", "Octave", { "-2", "-1", "0", "+1", "+2" }, 2);
    flt ("similarity", "Similarity", 0, 1, 0.7f);
    flt ("complexity", "Complexity", 0, 1, 0.5f);
    flt ("humanize", "Humanize", 0, 1, 0.35f);
    flt ("strum", "Strum Speed", 4, 60, 18, "ms");
    flt ("volume", "Volume", -30, 6, -6, "dB");
    l.add (std::make_unique<B> (juce::ParameterID { "backing", 1 }, "Backing Chords", true));
    l.add (std::make_unique<B> (juce::ParameterID { "followDaw", 1 }, "Play With DAW", true));
    l.add (std::make_unique<B> (juce::ParameterID { "sound", 1 }, "Built-in Sound", true));
    l.add (std::make_unique<B> (juce::ParameterID { "liveMode", 1 }, "Live Chords", false));
    return l;
}

static const char* watchedParams[] = { "mode", "source", "bars", "key", "scale", "rhythm", "instChords", "instMelody", "instBass",
                                       "octave", "similarity", "complexity", "humanize", "strum", "backing" };

WallChordsProcessor::WallChordsProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "WallChords", createLayout())
{
    formats.registerBasicFormats();
    for (auto* id : watchedParams) apvts.addParameterListener (id, this);
    liveNotes.reserve (4096); liveCCs.reserve (1024); liveEvs.reserve (8192); liveVoicing.reserve (16);
    for (auto& m : partMidi) m.ensureSize (4096);
    seed = juce::Random::getSystemRandom().nextInt ({ 1, 1000000 });
    history.push_back (seed.load());
    regenerate();
}

WallChordsProcessor::~WallChordsProcessor()
{
    alive->store (false);
    for (auto* id : watchedParams) apvts.removeParameterListener (id, this);
    cancelPendingUpdate();
}

bool WallChordsProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void WallChordsProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    backBuf.setSize (2, juce::jmax (512, samplesPerBlock));
    mainPlayer.prepare (sr);
    backingPlayer.prepare (sr);
}

void WallChordsProcessor::parameterChanged (const juce::String&, float) { triggerAsyncUpdate(); }

gen::Settings WallChordsProcessor::currentSettings() const
{
    gen::Settings s;
    s.mode = (int) p ("mode"); s.source = (int) p ("source"); s.bars = gen::barsFor ((int) p ("bars"));
    s.key = (int) p ("key"); s.minor = (int) p ("scale") == 1; s.rhythm = (int) p ("rhythm");
    s.octave = (int) p ("octave") - 2;
    s.similarity = p ("similarity"); s.complexity = p ("complexity"); s.humanize = p ("humanize"); s.strumMs = p ("strum");
    s.backing = p ("backing") > 0.5f;
    s.bpm = lastBpm.load();
    s.seed = seed.load();
    return s;
}

juce::String WallChordsProcessor::instrumentParamId() const
{
    const int mode = (int) p ("mode");
    return mode == gen::MMelody ? "instMelody" : mode == gen::MBass ? "instBass" : "instChords";
}

int WallChordsProcessor::instrumentParamIndex() const
{
    return (int) apvts.getRawParameterValue (instrumentParamId())->load();
}

void WallChordsProcessor::regenerate()
{
    std::shared_ptr<const gen::Loop> fresh = gen::generate (currentSettings(), library.snapshot());
    {
        const juce::SpinLock::ScopedLockType sl (loopLock);
        if (loop != nullptr) retired.push_back (loop);
        loop = fresh;
    }
    retired.erase (std::remove_if (retired.begin(), retired.end(), [] (auto& l) { return l.use_count() == 1; }), retired.end());
    refreshInstruments();
}

std::shared_ptr<const gen::Loop> WallChordsProcessor::getLoop() const
{
    const juce::SpinLock::ScopedLockType sl (loopLock);
    return loop;
}

void WallChordsProcessor::newIdea()
{
    seed = juce::Random::getSystemRandom().nextInt ({ 1, 1000000 });
    history.resize ((size_t) historyPos + 1);
    history.push_back (seed.load());
    historyPos = (int) history.size() - 1;
    regenerate();
}

void WallChordsProcessor::stepHistory (int delta)
{
    const int next = juce::jlimit (0, (int) history.size() - 1, historyPos + delta);
    if (next == historyPos) return;
    historyPos = next;
    seed = history[(size_t) historyPos];
    regenerate();
}

juce::String WallChordsProcessor::historyLabel() const
{
    return "Idea " + juce::String (historyPos + 1) + " of " + juce::String ((int) history.size());
}

void WallChordsProcessor::addToLibrary (const juce::StringArray& paths)
{
    auto token = alive;
    library.addFiles (paths, [this, token] { if (token->load()) triggerAsyncUpdate(); });
}

juce::String WallChordsProcessor::getLiveChordName() const
{
    const juce::SpinLock::ScopedLockType sl (liveNameLock);
    return liveName;
}

// ------------------------------------------------------------------------- instruments
void WallChordsProcessor::loadInstrumentFor (smp::Player& player, int index)
{
    const auto& defs = smp::instruments();
    index = juce::jlimit (0, (int) defs.size() - 1, index);
    const juce::String folder (defs[(size_t) index].folder);
    if (folder.isEmpty() || ! smp::soundsFolder().getChildFile (folder).isDirectory()) { player.setInstrument (index, nullptr); return; }

    auto it = loadedInstruments.find (index);
    if (it != loadedInstruments.end()) { player.setInstrument (index, it->second); return; }

    player.setInstrument (index, nullptr);   // synth stand-in while the samples load
    const auto dir = smp::soundsFolder().getChildFile (folder);
    auto token = alive;
    juce::Thread::launch ([this, token, dir, index]
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        auto inst = smp::load (dir, fm);
        juce::MessageManager::callAsync ([this, token, inst, index]
        {
            if (! token->load() || inst == nullptr) return;
            loadedInstruments[index] = inst;
            mainInstrument = backingInstrument = -1;   // re-apply
            refreshInstruments();
        });
    });
}

void WallChordsProcessor::refreshInstruments()
{
    const int wantMain = instrumentParamIndex();
    const int wantBacking = smp::soundPackInstalled() ? 1 : 0;   // soft piano behind melodies and bass
    if (wantMain != mainInstrument) { mainInstrument = wantMain; loadInstrumentFor (mainPlayer, wantMain); }
    if (wantBacking != backingInstrument) { backingInstrument = wantBacking; loadInstrumentFor (backingPlayer, wantBacking); }
}

void WallChordsProcessor::downloadSoundPack (std::function<void (juce::String)> done)
{
    if (downloadProgress->load() >= 0.0f) return;
    downloadProgress->store (0.0f);
    auto token = alive;
    auto progress = downloadProgress;
    juce::Thread::launch ([this, token, done, progress]
    {
        juce::String msg;
        auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("WallChordsSounds.zip");
        const auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                              .withConnectionTimeoutMs (15000).withNumRedirectsToFollow (6);
        if (auto in = juce::URL (soundPackUrl).createInputStream (opts))
        {
            tmp.deleteFile();
            juce::FileOutputStream out (tmp);
            const auto total = in->getTotalLength();
            juce::HeapBlock<char> buf (1 << 16);
            juce::int64 got = 0;
            for (int n; (n = in->read (buf, 1 << 16)) > 0;)
            {
                out.write (buf, (size_t) n);
                got += n;
                if (total > 0) progress->store (0.9f * (float) got / (float) total);
            }
            out.flush();
            juce::ZipFile zip (tmp);
            auto dest = smp::soundsFolder();
            dest.createDirectory();
            msg = zip.getNumEntries() > 0 && zip.uncompressTo (dest).wasOk() ? "Real instruments installed!" : "The sound pack download was damaged. Try again.";
            tmp.deleteFile();
        }
        else msg = "Couldn't download the sound pack. Check your internet.";

        juce::MessageManager::callAsync ([this, token, done, msg, progress]
        {
            progress->store (-1.0f);
            if (! token->load()) return;
            loadedInstruments.clear();
            mainInstrument = backingInstrument = -1;
            refreshInstruments();
            if (done) done (msg);
        });
    });
}

// ------------------------------------------------------------------------- playback
void WallChordsProcessor::allNotesOff (juce::MidiBuffer& out)
{
    for (int part = 0; part < 4; ++part)
    {
        for (int n = 0; n < 128; ++n)
            if (active[part][n])
            {
                const auto m = juce::MidiMessage::noteOff (part + 1, n);
                out.addEvent (m, 0);
                partMidi[part].addEvent (m, 0);
                active[part][n] = false;
            }
        partMidi[part].addEvent (juce::MidiMessage::controllerEvent (part + 1, 64, 0), 0);
    }
}

void WallChordsProcessor::schedule (const std::vector<eng::Ev>& evs, double L, double pos, double len, int n, juce::MidiBuffer& out)
{
    auto run = [&] (double a, double b, double offsetBeats)
    {
        auto it = std::lower_bound (evs.begin(), evs.end(), a, [] (const eng::Ev& e, double v) { return e.beat < v; });
        for (; it != evs.end() && it->beat < b; ++it)
        {
            const int s = juce::jlimit (0, n - 1, (int) ((it->beat - a + offsetBeats) / len * n));
            const int part = it->part, ch = part + 1;
            juce::MidiMessage m;
            if (it->type == 1)
            {
                if (active[part][it->d1]) { const auto off = juce::MidiMessage::noteOff (ch, it->d1); out.addEvent (off, s); partMidi[part].addEvent (off, s); }
                m = juce::MidiMessage::noteOn (ch, it->d1, (juce::uint8) juce::jmax (1, (int) it->d2));
                active[part][it->d1] = true;
            }
            else if (it->type == 0)
            {
                if (! active[part][it->d1]) continue;
                m = juce::MidiMessage::noteOff (ch, it->d1);
                active[part][it->d1] = false;
            }
            else m = juce::MidiMessage::controllerEvent (ch, it->d1, it->d2);
            out.addEvent (m, s);
            partMidi[part].addEvent (m, s);
        }
    };
    if (pos + len <= L) run (pos, pos + len, 0.0);
    else { run (pos, L, 0.0); run (0.0, pos + len - L, L - pos); }
}

void WallChordsProcessor::rebuildLive (double bpm)
{
    liveNotes.clear(); liveCCs.clear(); liveEvs.clear(); liveVoicing.clear();
    for (int i = 0; i < 128; ++i) if (held[i]) liveVoicing.push_back (i);
    if (liveVoicing.empty()) return;

    const int key = (int) p ("key");
    const bool minor = (int) p ("scale") == 1;
    Chord chord;
    if (liveVoicing.size() == 1)
    {
        const int* sc = minor ? minorScale : majorScale;
        const int rel = ((liveVoicing[0] - key) % 12 + 12) % 12;
        int deg = -1;
        for (int i = 0; i < 7; ++i) if (sc[i] == rel) deg = i + 1;
        chord = deg > 0 ? makeChord (juce::String (deg), key, minor, Pop, 0) : makeChord ("1", rel, false, Pop, 0);
    }
    else
    {
        chord.root = chord.bass = liveVoicing[0] % 12;
        for (int n : liveVoicing) chord.intervals.push_back (n - liveVoicing[0]);
        chord.name = noteName (chord.root, key, minor) + " (your voicing)";
    }

    int rhythm = (int) p ("rhythm");
    if (rhythm == gen::RLibrary || rhythm == gen::RSustain) rhythm = gen::RBounce;
    const int style = gen::engineStyleFor (rhythm);
    const bool guitar = style == CStrum || style == CFunk || style == CFingerpick;
    std::vector<int> v = liveVoicing.size() == 1 ? (guitar ? eng::guitarVoicing (chord) : eng::voiceChord (chord, {}, 52, 76)) : liveVoicing;
    eng::Performer P (liveNotes, liveCCs, liveRng, bpm, p ("humanize"), 0.6f, p ("complexity"), p ("strum"));
    P.chords (v, style, 0.0, 4.0, p ("complexity"), p ("complexity"));

    for (auto& nv : liveNotes)
    {
        const double b = std::fmod (nv.beat, 4.0);
        liveEvs.push_back ({ b, 1, 0, (uint8_t) nv.pitch, (uint8_t) nv.vel });
        liveEvs.push_back ({ juce::jmin (b + nv.len, 3.999), 0, 0, (uint8_t) nv.pitch, 0 });
    }
    for (auto& c : liveCCs) liveEvs.push_back ({ std::fmod (c.beat, 4.0), 2, 0, (uint8_t) c.cc, (uint8_t) c.val });
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

    const bool liveMode = p ("liveMode") > 0.5f;
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (liveMode && (m.isNoteOn() || m.isNoteOff())) { held[m.getNoteNumber()] = m.isNoteOn(); heldChanged = true; }
        else if (! liveMode) out.addEvent (m, meta.samplePosition);
    }
    bool anyHeld = false;
    for (bool h : held) anyHeld = anyHeld || h;
    if (! liveMode && anyHeld) { std::fill (std::begin (held), std::end (held), false); heldChanged = true; anyHeld = false; }

    {
        const juce::SpinLock::ScopedTryLockType tl (loopLock);
        if (tl.isLocked()) audioLoop = loop;
    }

    if (previewRestart.exchange (false)) { previewBeat = 0.0; expectedNext = -1.0; }
    double b0 = 0.0;
    bool run = true;
    if (p ("followDaw") > 0.5f && hostPlaying && hostPpq >= 0.0) b0 = hostPpq;
    else if (preview.load()) { b0 = previewBeat; previewBeat += len; }
    else if (liveMode && (anyHeld || heldChanged)) b0 = freeBeat;
    else run = false;
    freeBeat += len;

    if (! run)
    {
        if (wasRunning) allNotesOff (out);
        wasRunning = false;
        playhead = -1.0;
    }
    else
    {
        if (! wasRunning || std::abs (b0 - expectedNext) > 0.05) allNotesOff (out);
        expectedNext = b0 + len;
        wasRunning = true;
        if (liveMode)
        {
            if (heldChanged) { allNotesOff (out); rebuildLive (bpm); heldChanged = false; }
            if (! liveEvs.empty()) schedule (liveEvs, 4.0, std::fmod (b0, 4.0), len, n, out);
            playhead = -1.0;
        }
        else if (audioLoop != nullptr && audioLoop->song.length > 0.0)
        {
            const double pos = std::fmod (b0, audioLoop->song.length);
            schedule (audioLoop->song.events, audioLoop->song.length, pos, len, n, out);
            playhead = pos;
        }
    }

    if (p ("sound") > 0.5f)
    {
        mainPlayer.render (buffer, partMidi[0], n);
        backBuf.setSize (2, n, false, false, true);
        backBuf.clear();
        backingPlayer.render (backBuf, partMidi[3], n);
        for (int c = 0; c < buffer.getNumChannels(); ++c) buffer.addFrom (c, 0, backBuf, juce::jmin (c, 1), 0, n, 0.55f);
        buffer.applyGain (juce::Decibels::decibelsToGain (p ("volume")));
    }
    midi.swapWith (out);
}

// ------------------------------------------------------------------------- MIDI export
juce::File WallChordsProcessor::writeMidiFile (int partMask)
{
    auto l = getLoop();
    if (l == nullptr) return {};
    const auto& s = l->song;
    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (960);
    juce::MidiMessageSequence tempo;
    tempo.addEvent (juce::MidiMessage::tempoMetaEvent ((int) (60000000.0 / s.bpm)));
    tempo.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4));
    mf.addTrack (tempo);

    const juce::String mainName = gen::modeNames[(int) p ("mode")];
    juce::StringArray used;
    for (int part : { 0, 3 })
    {
        if (((partMask >> part) & 1) == 0) continue;
        juce::MidiMessageSequence seq;
        const juce::String name = part == 0 ? mainName : juce::String ("Chords");
        seq.addEvent (juce::MidiMessage::textMetaEvent (3, name));
        bool any = false;
        for (auto& nv : s.notes)
            if (nv.part == part)
            {
                seq.addEvent (juce::MidiMessage::noteOn (1, nv.pitch, (juce::uint8) juce::jlimit (1, 127, part == 3 ? 80 : nv.vel)), nv.beat * 960.0);
                seq.addEvent (juce::MidiMessage::noteOff (1, nv.pitch), juce::jmin (nv.beat + nv.len, s.length) * 960.0);
                any = true;
            }
        for (auto& c : s.ccs)
            if (c.part == part) seq.addEvent (juce::MidiMessage::controllerEvent (1, c.cc, c.val), c.beat * 960.0);
        if (! any) continue;
        seq.sort();
        seq.updateMatchedPairs();
        mf.addTrack (seq);
        used.add (name);
    }

    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("WallChords");
    dir.createDirectory();
    const juce::String label = used.joinIntoString (" + ") + " " + noteName (s.key, s.key, s.minor) + (s.minor ? "m " : " ")
                               + juce::String (juce::roundToInt (s.length / 4)) + " bars";
    auto file = dir.getChildFile (juce::File::createLegalFileName ("Wall Chords " + label + ".mid"));
    file.deleteFile();
    juce::FileOutputStream os (file);
    if (! os.openedOk() || ! mf.writeTo (os)) return {};
    return file;
}

// ------------------------------------------------------------------------- state
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
            if (st.hasProperty ("seed")) { seed = (int) st.getProperty ("seed"); history = { seed.load() }; historyPos = 0; }
            apvts.replaceState (st);
            triggerAsyncUpdate();
        }
}

juce::AudioProcessorEditor* WallChordsProcessor::createEditor() { return new WallChordsEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WallChordsProcessor(); }
