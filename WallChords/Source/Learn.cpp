#include "Learn.h"
#include <map>
#include <algorithm>
#include <cmath>

namespace gen
{
using namespace lib;

static int wrap12 (int x) { return ((x % 12) + 12) % 12; }
static int frameOf (int key, bool minor) { return minor ? (key + 3) % 12 : key; }   // relative-major tonic
static int shiftBetween (int fromFrame, int toFrame) { int d = wrap12 (toFrame - fromFrame); return d > 5 ? d - 12 : d; }

static th::Chord toChord (int root, int bass, int quality)
{
    th::Chord c;
    c.root = wrap12 (root);
    c.bass = wrap12 (bass);
    c.intervals = qualityIntervals (quality);
    c.name = chordName (c.root, quality, c.bass, 0, false);
    return c;
}

static bool inScale (int pitch, int key, bool minor)
{
    const int* sc = minor ? th::minorScale : th::majorScale;
    const int rel = wrap12 (pitch - key);
    for (int i = 0; i < 7; ++i) if (sc[i] == rel) return true;
    return false;
}

static int snapToScale (int pitch, int key, bool minor)
{
    if (inScale (pitch, key, minor)) return pitch;
    return inScale (pitch - 1, key, minor) ? pitch - 1 : pitch + 1;
}

static bool isChordTone (const th::Chord& c, int pitch)
{
    for (int iv : c.intervals) if (wrap12 (c.root + iv) == wrap12 (pitch)) return true;
    return false;
}

static const eng::Slot& slotAt (const std::vector<eng::Slot>& prog, double beat)
{
    for (auto& s : prog) if (beat >= s.start - 1.0e-6 && beat < s.start + s.len) return s;
    return prog.back();
}

template <typename T>
static const T* pickWeighted (juce::Random& rng, const std::vector<const T*>& items, std::function<double (const T&)> weight)
{
    double total = 0;
    for (auto* i : items) total += weight (*i);
    double r = rng.nextDouble() * total;
    for (auto* i : items) { r -= weight (*i); if (r <= 0) return i; }
    return items.empty() ? nullptr : items.back();
}

static int sampleMap (juce::Random& rng, const std::map<int, double>& m, int exclude = -1)
{
    double total = 0;
    for (auto& [k, w] : m) if (k != exclude) total += w;
    if (total <= 0) return exclude;
    double r = rng.nextDouble() * total;
    for (auto& [k, w] : m) { if (k == exclude) continue; r -= w; if (r <= 0) return k; }
    return m.rbegin()->first;
}

// ================================================================ progressions
struct Tok { int r, q, b; int code() const { return (r * QNum + q) * 12 + b; } };
static Tok decode (int c) { return { c / 12 / QNum, (c / 12) % QNum, c % 12 }; }

static std::vector<eng::Slot> progressionFromLibrary (const Settings& st, const std::vector<const Entry*>& items,
                                                      juce::Random& rng, const Entry*& quoted)
{
    const int tf = frameOf (st.key, st.minor);
    auto weight = [&] (const Entry& e) { return (e.minor == st.minor ? 3.0 : 1.0) * (e.source == "midi" ? 1.2 : 1.0); };
    quoted = pickWeighted<Entry> (rng, items, weight);

    // learn chord-to-chord movement from every chord sample (in a key-independent form)
    std::map<int, std::map<int, double>> t1;
    std::map<long long, std::map<int, double>> t2;
    auto tokOf = [] (const Entry& e, const ChordEv& c)
    { const int f = frameOf (e.key, e.minor); return Tok { wrap12 (c.root - f), c.quality, wrap12 (c.bass - c.root) }; };
    for (auto* e : items)
    {
        const double w = weight (*e);
        const size_t n = e->chords.size();
        for (size_t i = 0; i < n; ++i)
        {
            const int cur = tokOf (*e, e->chords[i]).code();
            const int p1 = tokOf (*e, e->chords[(i + n - 1) % n]).code();
            const int p2 = tokOf (*e, e->chords[(i + n - 2) % n]).code();
            if (n > 1) t1[p1][cur] += w;
            if (n > 2) t2[(long long) p2 * 100000 + p1][cur] += w;
        }
    }

    // start from the chosen sample's progression, stretched or looped to the requested length
    struct S { double start, len; Tok t; };
    std::vector<S> seq;
    const double total = st.bars * 4.0, entryLen = quoted->bars * 4.0;
    for (int rep = 0; rep < 64 && (seq.empty() || seq.back().start + seq.back().len < total - 1.0e-6); ++rep)
        for (auto& c : quoted->chords)
        {
            const double s = rep * entryLen + c.beat;
            if (s >= total - 1.0e-6) break;
            seq.push_back ({ s, juce::jmin (c.len, total - s), tokOf (*quoted, c) });
        }
    if (seq.empty()) return {};
    seq.front().len += seq.front().start;
    seq.front().start = 0;
    for (size_t i = 0; i + 1 < seq.size(); ++i) seq[i].len = seq[i + 1].start - seq[i].start;
    seq.back().len = total - seq.back().start;

    // mutate: lower similarity = more chords re-chosen from what the whole library tends to do
    const float p = (1.0f - st.similarity) * 0.9f;
    for (size_t i = 0; i < seq.size(); ++i)
    {
        if (i > 0 && seq[i].len >= 4.0 && std::fmod (seq[i].len, 2.0) == 0 && rng.nextFloat() < p * 0.35f)
        {
            const double half = seq[i].len * 0.5;
            S second { seq[i].start + half, half, seq[i].t };
            seq[i].len = half;
            seq.insert (seq.begin() + (long) i + 1, second);
        }
        if (rng.nextFloat() >= p) continue;
        const int prev1 = i > 0 ? seq[i - 1].t.code() : seq.back().t.code();
        const int prev2 = i > 1 ? seq[i - 2].t.code() : prev1;
        const auto it2 = t2.find ((long long) prev2 * 100000 + prev1);
        int next = -1;
        if (it2 != t2.end() && rng.nextFloat() < 0.7f) next = sampleMap (rng, it2->second, seq[i].t.code());
        if (next < 0) { auto it1 = t1.find (prev1); if (it1 != t1.end()) next = sampleMap (rng, it1->second, seq[i].t.code()); }
        if (next >= 0) seq[i].t = decode (next);
    }

    std::vector<eng::Slot> out;
    for (auto& s : seq)
    {
        const int root = wrap12 (s.t.r + tf);
        out.push_back ({ s.start, s.len, toChord (root, root + s.t.b, s.t.q), 0 });
    }
    return out;
}

static std::vector<eng::Slot> progressionBuiltIn (const Settings& st, int genre, juce::Random& rng)
{
    const auto& list = th::progressions (genre, st.minor, th::RoleMain);
    const auto tokens = juce::StringArray::fromTokens (list[rng.nextInt (list.size())], " ", "");
    std::vector<eng::Slot> out;
    for (int bar = 0; bar < st.bars; ++bar)
        out.push_back ({ bar * 4.0, 4.0, th::makeChord (tokens[bar % tokens.size()], st.key, st.minor, genre, 0), 0 });
    return out;
}

// ================================================================ chord performance
int engineStyleFor (int r)
{
    switch (r)
    {
        case RStrum: return th::CStrum;       case RFunk: return th::CFunk;         case RFingerpick: return th::CFingerpick;
        case RAmbient: return th::CAmbient;   case RPianoFlow: return th::CPianoFlow; case RBounce: return th::CPopBounce;
        case RRolled: return th::CRolled;     case RStabs: return th::CStabs;       case RPluckArp: return th::CPluckArp;
        case RSupersaw: return th::CSupersaw; default: return th::CBlock;
    }
}

static void performChords (eng::Performer& P, const std::vector<eng::Slot>& prog, const Settings& st, int genre,
                           const std::vector<int>& masks, int lo, int hi)
{
    std::vector<int> prev;
    const float e = st.complexity;
    const bool useMasks = st.rhythm == RLibrary && std::any_of (masks.begin(), masks.end(), [] (int m) { return m != 0; });
    int rhythm = st.rhythm;
    if (rhythm == RLibrary && ! useMasks)
        rhythm = genre == th::Worship ? RPianoFlow : genre == th::EDM ? RSupersaw : RBounce;

    for (auto& s : prog)
    {
        const int style = engineStyleFor (rhythm);
        const bool guitar = ! useMasks && (style == th::CStrum || style == th::CFunk || style == th::CFingerpick);
        auto v = guitar ? eng::guitarVoicing (s.chord) : eng::voiceChord (s.chord, prev, lo, hi);
        if (! guitar) prev = v;

        if (useMasks)
        {
            std::vector<double> hits;
            for (double bar = std::floor (s.start / 4.0) * 4.0; bar < s.start + s.len; bar += 4.0)
            {
                const int m = masks[(size_t) ((int) (bar / 4.0)) % masks.size()];
                for (int step = 0; step < 16; ++step)
                    if ((m >> step) & 1)
                    {
                        const double t = bar + step * 0.25;
                        if (t >= s.start - 1.0e-6 && t < s.start + s.len - 1.0e-6) hits.push_back (t);
                    }
            }
            if (hits.empty() || hits.front() > s.start + 0.01) hits.insert (hits.begin(), s.start);
            for (size_t h = 0; h < hits.size(); ++h)
            {
                const double next = h + 1 < hits.size() ? hits[h + 1] : s.start + s.len;
                const bool down = std::fmod (hits[h], 1.0) < 0.01;
                for (int n : v) P.note (eng::PChords, hits[h], (next - hits[h]) * 0.95, n, (58.0f + e * 45.0f) * (down ? 1.0f : 0.84f));
            }
        }
        else if (rhythm == RSustain)
        {
            for (int n : v) P.note (eng::PChords, s.start, s.len * 0.98, n, 60.0f + e * 35.0f);
        }
        else P.chords (v, style, s.start, s.len, e, e);
    }
}

// ================================================================ melody & bass from the library
struct BarNote { double beat, len; int pitch, vel; };

static std::vector<std::vector<BarNote>> barsOf (const Entry& e, int shift)
{
    std::vector<std::vector<BarNote>> bars ((size_t) juce::jmax (1, e.bars));
    for (auto& n : e.notes)
    {
        const int b = (int) std::floor (n.beat / 4.0);
        if (b >= 0 && b < e.bars) bars[(size_t) b].push_back ({ n.beat - b * 4.0, n.len, n.pitch + shift, n.vel });
    }
    for (auto& b : bars) std::sort (b.begin(), b.end(), [] (const BarNote& a, const BarNote& c) { return a.beat < c.beat; });
    return bars;
}

static void lineFromLibrary (eng::Performer& P, const std::vector<eng::Slot>& prog, const Settings& st,
                             const std::vector<const Entry*>& items, juce::Random& rng, const Entry*& quoted, bool bass)
{
    const int tf = frameOf (st.key, st.minor);
    auto weight = [&] (const Entry& e) { return (e.minor == st.minor ? 3.0 : 1.0) * (e.notes.size() >= 4 ? 1.0 : 0.2); };
    quoted = pickWeighted<Entry> (rng, items, weight);
    const auto srcBars = barsOf (*quoted, shiftBetween (frameOf (quoted->key, quoted->minor), tf));
    const float p = (1.0f - st.similarity) * 0.9f;

    // what intervals / bass offsets does the library like?
    std::map<int, double> intervals, offsets;
    int lo = 127, hi = 0;
    for (auto* e : items)
    {
        for (size_t i = 1; i < e->notes.size(); ++i)
            intervals[juce::jlimit (-12, 12, e->notes[i].pitch - e->notes[i - 1].pitch)] += 1.0;
        for (auto& bar : barsOf (*e, 0))
            for (auto& n : bar) if (! bar.empty()) offsets[juce::jlimit (-12, 19, n.pitch - bar.front().pitch)] += 1.0;
    }
    for (auto& b : srcBars) for (auto& n : b) { lo = std::min (lo, n.pitch); hi = std::max (hi, n.pitch); }
    if (lo > hi) { lo = bass ? 36 : 60; hi = bass ? 48 : 76; }
    lo -= 2; hi += 2;

    int prevSrc = -1, prevOut = -1;
    for (int bar = 0; bar < st.bars; ++bar)
    {
        // usually follow the chosen sample's bar; sometimes borrow a bar from another sample
        const std::vector<BarNote>* notes = &srcBars[(size_t) bar % srcBars.size()];
        std::vector<std::vector<BarNote>> borrowed;
        if (rng.nextFloat() < p * 0.5f && items.size() > 1)
        {
            auto* other = items[(size_t) rng.nextInt ((int) items.size())];
            borrowed = barsOf (*other, shiftBetween (frameOf (other->key, other->minor), tf));
            notes = &borrowed[(size_t) rng.nextInt ((int) borrowed.size())];
            prevSrc = -1;
        }
        const int anchorSrc = notes->empty() ? 0 : notes->front().pitch;

        for (size_t i = 0; i < notes->size(); ++i)
        {
            const auto& n = (*notes)[i];
            const double beat = bar * 4.0 + n.beat;
            if (beat >= st.bars * 4.0) break;
            if (rng.nextFloat() < p * 0.25f) continue;   // occasionally leave a note out
            const auto& slot = slotAt (prog, beat);
            const bool strong = std::fmod (n.beat, 1.0) < 0.01;
            int pitch;

            if (bass)
            {
                // keep the sample's movement relative to the bar's first note, but follow the new chords
                int offset = n.pitch - anchorSrc;
                if (rng.nextFloat() < p) offset = sampleMap (rng, offsets);
                const int centre = (lo + hi) / 2;
                int root = slot.chord.bass;
                int base = centre - 6;
                while (wrap12 (base) != root) ++base;
                const bool chordStart = std::abs (beat - slot.start) < 0.01;
                pitch = chordStart ? base : base + offset;
                if (! inScale (pitch, st.key, st.minor) && rng.nextFloat() < 0.8f) pitch = snapToScale (pitch, st.key, st.minor);
            }
            else
            {
                // keep the sample's contour; lower similarity re-rolls intervals from the library's habits
                int interval = prevSrc < 0 ? 0 : n.pitch - prevSrc;
                if (prevOut >= 0 && rng.nextFloat() < p) interval = sampleMap (rng, intervals);
                pitch = prevOut < 0 ? n.pitch : prevOut + interval;
                if (! inScale (pitch, st.key, st.minor) && ! (st.similarity > 0.85f && ! inScale (n.pitch, st.key, st.minor)))
                    pitch = snapToScale (pitch, st.key, st.minor);
                if (strong && ! isChordTone (slot.chord, pitch) && rng.nextFloat() < 0.75f)
                    for (int d : { 1, -1, 2, -2, 3, -3 })
                        if (isChordTone (slot.chord, pitch + d)) { pitch += d; break; }
            }
            while (pitch < lo) pitch += 12;
            while (pitch > hi) pitch -= 12;

            const double nextBeat = i + 1 < notes->size() ? bar * 4.0 + (*notes)[i + 1].beat : beat + n.len;
            P.note (eng::PChords, beat, juce::jmin (n.len, nextBeat - beat) * 0.97, pitch, (float) n.vel);
            prevSrc = n.pitch;
            prevOut = pitch;
        }
    }
}

// ================================================================ main entry
std::shared_ptr<Loop> generate (const Settings& st, const std::vector<Entry>& library)
{
    auto loop = std::make_shared<Loop>();
    auto& song = loop->song;
    song.bpm = st.bpm; song.key = st.key; song.minor = st.minor;
    song.length = st.bars * 4.0;
    song.notes.reserve (4096);
    song.ccs.reserve (1024);

    juce::Random rng (st.seed * 7919 + 13);
    eng::Performer P (song.notes, song.ccs, rng, st.bpm, st.humanize, 0.6f, st.complexity, st.strumMs);

    std::vector<const Entry*> chordItems, melodyItems, bassItems;
    for (auto& e : library)
    {
        if (! e.enabled) continue;
        if (e.kind == KChords && ! e.chords.empty()) chordItems.push_back (&e);
        if (e.kind == KMelody && e.notes.size() >= 3) melodyItems.push_back (&e);
        if (e.kind == KBass && e.notes.size() >= 2) bassItems.push_back (&e);
    }
    const bool useLib = st.source == SLibrary;
    const int genre = st.source == SWorship ? th::Worship : st.source == SEDM ? th::EDM : th::Pop;
    juce::StringArray inspired;

    // 1) the progression
    const Entry* chordQuote = nullptr;
    if (useLib && ! chordItems.empty()) loop->chords = progressionFromLibrary (st, chordItems, rng, chordQuote);
    if (loop->chords.empty()) loop->chords = progressionBuiltIn (st, genre, rng);
    if (chordQuote != nullptr) inspired.add (chordQuote->name);

    // 2) the main part
    if (st.mode == MChords)
    {
        std::vector<int> masks;
        if (chordQuote != nullptr) masks = chordQuote->rhythm;
        if (masks.empty() && useLib && ! chordItems.empty()) masks = chordItems.front()->rhythm;
        int lo = 52, hi = 76;
        if (chordQuote != nullptr && chordQuote->source == "midi" && ! chordQuote->notes.empty())
        {
            double mean = 0;
            for (auto& n : chordQuote->notes) mean += n.pitch;
            mean /= (double) chordQuote->notes.size();
            lo = juce::jlimit (40, 64, (int) mean - 10);
            hi = lo + 22;
        }
        performChords (P, loop->chords, st, genre, masks, lo, hi);
    }
    else
    {
        const auto& items = st.mode == MMelody ? melodyItems : bassItems;
        const Entry* q = nullptr;
        if (useLib && ! items.empty())
        {
            lineFromLibrary (P, loop->chords, st, items, rng, q, st.mode == MBass);
            if (q != nullptr) inspired.addIfNotAlreadyThere (q->name);
        }
        else if (st.mode == MMelody)
        {
            eng::generateLead (P, loop->chords, 0, loop->chords.size(), th::LAuto, genre, 0.0, song.length,
                               st.complexity, st.complexity, rng, st.key, st.minor);
        }
        else
        {
            int prevBass = 40;
            for (size_t i = 0; i < loop->chords.size(); ++i)
            {
                const auto& s = loop->chords[i];
                int pitch = 33;
                while (wrap12 (pitch) != s.chord.bass) ++pitch;
                if (std::abs (pitch + 12 - prevBass) < std::abs (pitch - prevBass) && pitch + 12 <= 47) pitch += 12;
                const auto& nx = loop->chords[(i + 1) % loop->chords.size()];
                int np = 33;
                while (wrap12 (np) != nx.chord.bass) ++np;
                P.bass (pitch, np, eng::autoBassStyle (genre, "Chorus", st.complexity), s.start, s.len, st.complexity);
                prevBass = pitch;
            }
        }
        for (auto& n : song.notes) n.part = eng::PChords;   // everything generated so far is the main part
        song.ccs.clear();

        // 3) quiet backing chords so you can hear the melody / bass in context
        if (st.backing)
        {
            std::vector<int> prev;
            for (auto& s : loop->chords)
            {
                auto v = eng::voiceChord (s.chord, prev, 52, 72);
                prev = v;
                for (int n : v) song.notes.push_back ({ s.start, s.len * 0.98, n, 48, eng::PPad });
            }
        }
    }

    for (auto& n : song.notes) if (n.part == eng::PChords) n.pitch = juce::jlimit (0, 127, n.pitch + 12 * st.octave);
    song.notes.erase (std::remove_if (song.notes.begin(), song.notes.end(),
                                      [&] (const eng::NoteEv& n) { return n.beat >= song.length; }), song.notes.end());
    for (auto& s : loop->chords) song.slots.push_back (s);
    eng::finalise (song);

    if (useLib && inspired.isEmpty())
        loop->info = "Add samples to your library to learn from them. Using the built-in Pop style for now.";
    else if (inspired.isEmpty())
        loop->info = "Built-in " + sourceNames[st.source] + " style";
    else
        loop->info = "Inspired by: " + inspired.joinIntoString (", ");
    return loop;
}

} // namespace gen
