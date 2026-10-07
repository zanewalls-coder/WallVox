#include "Library.h"
#include <juce_dsp/juce_dsp.h>
#include <juce_events/juce_events.h>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <cstdio>

namespace lib
{
static const char* pcNames[12] { "C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B" };

// ============================================================ serialisation
juce::var Entry::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("id", id); o->setProperty ("name", name); o->setProperty ("file", file); o->setProperty ("source", source);
    o->setProperty ("kind", kind); o->setProperty ("key", key); o->setProperty ("minor", minor); o->setProperty ("bars", bars);
    o->setProperty ("enabled", enabled); o->setProperty ("bpm", bpm);
    juce::Array<juce::var> n, c, r;
    for (auto& x : notes)  n.add (juce::Array<juce::var> { x.beat, x.len, x.pitch, x.vel });
    for (auto& x : chords) c.add (juce::Array<juce::var> { x.beat, x.len, x.root, x.bass, x.quality });
    for (int m : rhythm) r.add (m);
    o->setProperty ("notes", n); o->setProperty ("chords", c); o->setProperty ("rhythm", r);
    return juce::var (o);
}

Entry Entry::fromVar (const juce::var& v)
{
    Entry e;
    e.id = v["id"].toString(); e.name = v["name"].toString(); e.file = v["file"].toString(); e.source = v["source"].toString();
    e.kind = (int) v["kind"]; e.key = (int) v["key"]; e.minor = (bool) v["minor"]; e.bars = juce::jmax (1, (int) v["bars"]);
    e.enabled = v.hasProperty ("enabled") ? (bool) v["enabled"] : true;
    e.bpm = (double) v["bpm"];
    if (auto* n = v["notes"].getArray())
        for (auto& x : *n) if (x.size() >= 4) e.notes.push_back ({ (double) x[0], (double) x[1], (int) x[2], (int) x[3] });
    if (auto* c = v["chords"].getArray())
        for (auto& x : *c) if (x.size() >= 5) e.chords.push_back ({ (double) x[0], (double) x[1], (int) x[2], (int) x[3], (int) x[4] });
    if (auto* r = v["rhythm"].getArray()) for (auto& x : *r) e.rhythm.push_back ((int) x);
    return e;
}

juce::String Entry::describe() const
{
    return juce::String (pcNames[key]) + (minor ? " minor" : " major") + "  -  " + juce::String (juce::roundToInt (bpm))
           + " BPM  -  " + juce::String (bars) + (bars == 1 ? " bar" : " bars");
}

juce::String chordName (int root, int quality, int bass, int key, bool minor)
{
    juce::ignoreUnused (key, minor);
    juce::String s = juce::String (pcNames[(root % 12 + 12) % 12]) + qualitySuffix (quality);
    if (bass >= 0 && bass % 12 != root % 12) s << "/" << pcNames[(bass % 12 + 12) % 12];
    return s;
}

// ============================================================ music helpers
void detectKey (const float w[12], int& key, bool& minor)
{
    static const float maj[12] { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
    static const float mnr[12] { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };
    auto corr = [&] (const float* prof, int k)
    {
        float mx = 0, my = 0;
        for (int i = 0; i < 12; ++i) { mx += w[(i + k) % 12]; my += prof[i]; }
        mx /= 12; my /= 12;
        float sxy = 0, sxx = 0, syy = 0;
        for (int i = 0; i < 12; ++i)
        {
            const float dx = w[(i + k) % 12] - mx, dy = prof[i] - my;
            sxy += dx * dy; sxx += dx * dx; syy += dy * dy;
        }
        return sxx > 0 && syy > 0 ? sxy / std::sqrt (sxx * syy) : 0.0f;
    };
    float best = -2; key = 0; minor = false;
    for (int k = 0; k < 12; ++k)
    {
        const float a = corr (maj, k), b = corr (mnr, k);
        if (a > best) { best = a; key = k; minor = false; }
        if (b > best) { best = b; key = k; minor = true; }
    }
}

void bestChord (const float w[12], int bassPc, int& root, int& quality, float& score)
{
    float total = 0;
    for (int i = 0; i < 12; ++i) total += w[i];
    score = -1.0e9f; root = 0; quality = QMaj;
    if (total <= 0) return;
    for (int r = 0; r < 12; ++r)
        for (int q = 0; q < QNum; ++q)
        {
            const auto& iv = qualityIntervals (q);
            bool in[12] {};
            float s = 0;
            for (int x : iv) { in[(r + x) % 12] = true; }
            float minTone = 1.0e9f;
            for (int x : iv) { s += w[(r + x) % 12]; minTone = std::min (minTone, w[(r + x) % 12]); }
            for (int i = 0; i < 12; ++i) if (! in[i]) s -= 0.6f * w[i];
            s += 0.4f * minTone;                                   // every chord tone should be present
            if (bassPc == r) s += 0.15f * total;
            if (iv.size() > 3) s -= 0.06f * total;                 // prefer triads unless the 7th is real
            if (q == QSus2 || q == QSus4 || q == QDim) s -= 0.04f * total;
            if (s > score) { score = s; root = r; quality = q; }
        }
    score /= total;
}

static int classify (const std::vector<Note>& notes)
{
    if (notes.empty()) return KMelody;
    std::vector<Note> s = notes;
    std::sort (s.begin(), s.end(), [] (const Note& a, const Note& b) { return a.beat < b.beat; });
    int groups = 0;
    double gStart = -100;
    for (auto& n : s) if (n.beat - gStart > 0.08) { ++groups; gStart = n.beat; }
    const double poly = (double) s.size() / juce::jmax (1, groups);
    std::vector<int> p;
    for (auto& n : s) p.push_back (n.pitch);
    std::nth_element (p.begin(), p.begin() + (long) p.size() / 2, p.end());
    const int median = p[p.size() / 2];
    if (poly >= 2.0) return KChords;
    return median < 50 ? KBass : KMelody;
}

static std::vector<int> rhythmFromOnsets (std::vector<double> onsets, int bars)
{
    std::vector<int> masks ((size_t) bars, 0);
    for (double b : onsets)
    {
        const int step = (int) std::lround (b * 4.0);
        const int bar = step / 16;
        if (bar >= 0 && bar < bars) masks[(size_t) bar] |= 1 << (step % 16);
    }
    return masks;
}

std::vector<ChordEv> chordsFromNotes (const std::vector<Note>& notes, int bars)
{
    std::vector<ChordEv> out;
    const int beats = bars * 4;
    int prevRoot = -1, prevQ = 0, prevBass = -1;
    for (int b = 0; b < beats; ++b)
    {
        float w[12] {};
        int lowest = 200;
        for (auto& n : notes)
        {
            const double ov = std::min (n.beat + n.len, (double) b + 1) - std::max (n.beat, (double) b);
            if (ov <= 0) continue;
            w[n.pitch % 12] += (float) ov * (0.5f + n.vel / 254.0f);
            lowest = std::min (lowest, n.pitch);
        }
        int root, q; float sc;
        bestChord (w, lowest < 200 ? lowest % 12 : -1, root, q, sc);
        int bass = lowest < 200 ? lowest % 12 : root;
        if (lowest >= 200) { if (prevRoot < 0) continue; root = prevRoot; q = prevQ; bass = prevBass; }
        if (! out.empty() && out.back().root == root && out.back().quality == q && out.back().bass == bass
            && std::abs (out.back().beat + out.back().len - b) < 0.01)
            out.back().len += 1.0;
        else out.push_back ({ (double) b, 1.0, root, bass, q });
        prevRoot = root; prevQ = q; prevBass = bass;
    }
    // remove one-beat passing chords sitting between two identical chords
    for (size_t i = 1; i + 1 < out.size();)
    {
        if (out[i].len <= 1.0 && out[i - 1].root == out[i + 1].root && out[i - 1].quality == out[i + 1].quality)
        {
            out[i - 1].len += out[i].len + out[i + 1].len;
            out.erase (out.begin() + (long) i, out.begin() + (long) i + 2);
        }
        else ++i;
    }
    return out;
}

static void finishEntry (Entry& e)
{
    float w[12] {};
    for (auto& n : e.notes) w[n.pitch % 12] += (float) n.len;
    for (auto& c : e.chords)
        for (int iv : qualityIntervals (c.quality)) w[(c.root + iv) % 12] += (float) c.len * (iv == 0 ? 1.5f : 1.0f);
    detectKey (w, e.key, e.minor);
}

// ============================================================ MIDI
std::vector<Entry> analyseMidi (const juce::File& f)
{
    std::vector<Entry> out;
    juce::FileInputStream in (f);
    juce::MidiFile mf;
    if (! in.openedOk() || ! mf.readFrom (in)) return out;

    double tpq = mf.getTimeFormat();
    double bpm = 120.0;
    if (tpq <= 0) { mf.convertTimestampTicksToSeconds(); tpq = 0; }
    for (int t = 0; t < mf.getNumTracks(); ++t)
        for (auto* ev : *mf.getTrack (t))
            if (ev->message.isTempoMetaEvent()) { bpm = 60.0 / ev->message.getTempoSecondsPerQuarterNote(); t = mf.getNumTracks(); break; }
    auto toBeat = [&] (double ts) { return tpq > 0 ? ts / tpq : ts * bpm / 60.0; };

    // collect each track, then merge tracks that play the same role
    std::vector<std::vector<Note>> byKind (3);
    double first = 1.0e9;
    for (int t = 0; t < mf.getNumTracks(); ++t)
    {
        juce::MidiMessageSequence seq (*mf.getTrack (t));
        seq.updateMatchedPairs();
        std::vector<Note> notes;
        for (auto* ev : seq)
        {
            const auto& m = ev->message;
            if (! m.isNoteOn() || m.getChannel() == 10) continue;
            const double b = toBeat (m.getTimeStamp());
            const double e = ev->noteOffObject != nullptr ? toBeat (ev->noteOffObject->message.getTimeStamp()) : b + 0.5;
            notes.push_back ({ b, juce::jmax (0.05, e - b), m.getNoteNumber(), m.getVelocity() });
        }
        if (notes.empty()) continue;
        for (auto& n : notes) first = std::min (first, n.beat);
        auto& dst = byKind[(size_t) classify (notes)];
        dst.insert (dst.end(), notes.begin(), notes.end());
    }

    const double offset = std::floor (first / 4.0) * 4.0;
    for (int k = 0; k < 3; ++k)
    {
        auto& notes = byKind[(size_t) k];
        if (notes.empty()) continue;
        Entry e;
        e.source = "midi"; e.kind = k; e.bpm = bpm;
        double last = 0;
        for (auto& n : notes) { n.beat -= offset; last = std::max (last, n.beat + n.len); }
        std::sort (notes.begin(), notes.end(), [] (const Note& a, const Note& b) { return a.beat < b.beat; });
        e.bars = juce::jlimit (1, 32, (int) std::ceil (last / 4.0 - 0.05));
        notes.erase (std::remove_if (notes.begin(), notes.end(), [&] (const Note& n) { return n.beat >= e.bars * 4.0; }), notes.end());
        e.notes = notes;
        std::vector<double> onsets;
        double g = -100;
        for (auto& n : notes) if (n.beat - g > 0.08) { onsets.push_back (n.beat); g = n.beat; }
        e.rhythm = rhythmFromOnsets (onsets, e.bars);
        if (k == KChords) e.chords = chordsFromNotes (notes, e.bars);
        finishEntry (e);
        out.push_back (std::move (e));
    }
    return out;
}

// ============================================================ audio
static double bpmFromName (const juce::String& name)
{
    const auto s = name.toLowerCase();
    for (int pos = s.indexOf ("bpm"); pos >= 0; pos = s.indexOf (pos + 1, "bpm"))
    {
        int i = pos - 1;
        while (i >= 0 && (s[i] == ' ' || s[i] == '_' || s[i] == '-')) --i;
        int j = i;
        while (j >= 0 && juce::CharacterFunctions::isDigit (s[j])) --j;
        if (i > j) { const double v = s.substring (j + 1, i + 1).getDoubleValue(); if (v >= 50 && v <= 220) return v; }
        int k = pos + 3;
        while (k < s.length() && (s[k] == ' ' || s[k] == '_' || s[k] == '-')) ++k;
        int m = k;
        while (m < s.length() && juce::CharacterFunctions::isDigit (s[m])) ++m;
        if (m > k) { const double v = s.substring (k, m).getDoubleValue(); if (v >= 50 && v <= 220) return v; }
    }
    return 0;
}

static bool keyFromName (const juce::String& name, int& key, bool& minor)
{
    juce::StringArray toks;
    toks.addTokens (name.replaceCharacters ("-_.()[]", "       "), " ", "");
    toks.removeEmptyStrings();
    for (int i = 0; i < toks.size(); ++i)
    {
        const auto t = toks[i];
        if (t.isEmpty() || ! juce::String ("ABCDEFG").containsChar (t[0])) continue;
        int len = 1, acc = 0;
        if (t.length() > 1 && (t[1] == '#' || t[1] == 'b')) { acc = t[1] == '#' ? 1 : -1; len = 2; }
        const auto rest = t.substring (len).toLowerCase();
        const auto next = toks[i + 1].toLowerCase();
        const auto prev = toks[i - 1].toLowerCase();
        bool isMin = false, ok = false;
        if (rest == "m" || rest == "min" || rest == "minor") { isMin = true; ok = true; }
        else if (rest == "maj" || rest == "major") ok = true;
        else if (rest.isEmpty() && (next == "min" || next == "minor" || next == "m")) { isMin = true; ok = true; }
        else if (rest.isEmpty() && (next == "maj" || next == "major" || prev == "key")) ok = true;
        if (! ok) continue;
        static const int base[7] { 9, 11, 0, 2, 4, 5, 7 };   // A..G
        key = (base[t[0] - 'A'] + acc + 12) % 12;
        minor = isMin;
        return true;
    }
    return false;
}

std::vector<Entry> analyseAudio (const juce::File& f, juce::AudioFormatManager& fm)
{
    std::vector<Entry> out;
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (f));
    if (reader == nullptr) return out;

    const double srcRate = reader->sampleRate;
    const int srcLen = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (srcRate * 90.0));
    if (srcLen < srcRate * 0.5) return out;
    juce::AudioBuffer<float> buf ((int) juce::jmin (2u, reader->numChannels), srcLen);
    reader->read (&buf, 0, srcLen, 0, true, buf.getNumChannels() > 1);

    // mono, low-passed and resampled to 22.05 kHz
    const double fs = 22050.0;
    std::vector<float> mono ((size_t) srcLen);
    for (int i = 0; i < srcLen; ++i)
    {
        float s = 0;
        for (int c = 0; c < buf.getNumChannels(); ++c) s += buf.getSample (c, i);
        mono[(size_t) i] = s / (float) buf.getNumChannels();
    }
    if (srcRate > fs * 1.01)
    {
        juce::IIRFilter lp1, lp2;
        lp1.setCoefficients (juce::IIRCoefficients::makeLowPass (srcRate, 9000.0));
        lp2.setCoefficients (juce::IIRCoefficients::makeLowPass (srcRate, 9000.0));
        lp1.processSamples (mono.data(), srcLen);
        lp2.processSamples (mono.data(), srcLen);
    }
    const double ratio = srcRate / fs;
    const int n = (int) (srcLen / ratio) - 4;
    std::vector<float> x ((size_t) juce::jmax (0, n));
    {
        juce::LagrangeInterpolator interp;
        interp.process (ratio, mono.data(), x.data(), n);
    }
    const double duration = n / fs;
    float peak = 0;
    for (float v : x) peak = std::max (peak, std::abs (v));
    if (peak < 1.0e-4f) return out;
    for (auto& v : x) v /= peak;

    // ---- spectral frames
    const int fftOrder = 13, N = 1 << fftOrder, hop = 1024;   // 8192-point for chroma (good bass resolution)
    juce::dsp::FFT fft (fftOrder);
    std::vector<float> win ((size_t) N);
    for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * i / (N - 1));
    const int frames = juce::jmax (1, (n - N) / hop + 1);
    std::vector<std::array<float, 12>> chroma ((size_t) frames), bassChroma ((size_t) frames);
    std::vector<float> flux ((size_t) frames, 0.0f), rms ((size_t) frames, 0.0f), lowE ((size_t) frames, 0.0f), highE ((size_t) frames, 0.0f);
    std::vector<float> prevMag ((size_t) N / 2, 0.0f), work ((size_t) N * 2);
    std::vector<int> binPc ((size_t) N / 2, -1);
    std::vector<float> binW ((size_t) N / 2, 0.0f);
    for (int k = 1; k < N / 2; ++k)
    {
        const double fr = k * fs / N;
        if (fr < 30 || fr > 4200) continue;
        const double midi = 69.0 + 12.0 * std::log2 (fr / 440.0);
        const double near = std::round (midi);
        binPc[(size_t) k] = ((int) near % 12 + 12) % 12;
        binW[(size_t) k] = (float) juce::jmax (0.0, 1.0 - 2.0 * std::abs (midi - near));
    }
    for (int fi = 0; fi < frames; ++fi)
    {
        std::fill (work.begin(), work.end(), 0.0f);
        float e = 0;
        for (int i = 0; i < N; ++i) { const float s = x[(size_t) (fi * hop + i)]; work[(size_t) i] = s * win[(size_t) i]; e += s * s; }
        rms[(size_t) fi] = std::sqrt (e / N);
        fft.performFrequencyOnlyForwardTransform (work.data());
        auto& ch = chroma[(size_t) fi];
        auto& bc = bassChroma[(size_t) fi];
        ch.fill (0); bc.fill (0);
        float fl = 0;
        for (int k = 1; k < N / 2; ++k)
        {
            const float mag = work[(size_t) k];
            const float lm = std::log1p (mag), lp = std::log1p (prevMag[(size_t) k]);
            if (lm > lp) fl += lm - lp;
            prevMag[(size_t) k] = mag;
            const int pc = binPc[(size_t) k];
            if (pc < 0) continue;
            const double fr = k * fs / N;
            (fr < 250 ? lowE : highE)[(size_t) fi] += mag;
            if (fr <= 220) bc[(size_t) pc] += mag * binW[(size_t) k];
            if (fr >= 80) ch[(size_t) pc] += mag * binW[(size_t) k];
        }
        flux[(size_t) fi] = fl;
    }
    const double frameDt = hop / fs;
    float maxRms = 0;
    for (float r : rms) maxRms = std::max (maxRms, r);

    // ---- tempo
    double bpm = bpmFromName (f.getFileNameWithoutExtension());
    if (bpm <= 0)
    {
        auto acScore = [&] (double cand)
        {
            const double lag = 60.0 / cand / frameDt;
            double s = 0;
            for (int m = 1; m <= 4; ++m)
            {
                const int L = (int) std::lround (lag * m);
                for (int i = 0; i + L < frames; ++i) s += flux[(size_t) i] * flux[(size_t) (i + L)];
            }
            return s;
        };
        double best = 0, bestScore = -1;
        for (int bars : { 1, 2, 4, 8, 16 })
            for (double mult : { 0.5, 1.0, 2.0 })
            {
                const double cand = bars * 4 * 60.0 / duration * mult;
                if (cand < 70 || cand > 180) continue;
                const double sc = acScore (cand) * (mult == 1.0 ? 1.15 : 1.0);
                if (sc > bestScore) { bestScore = sc; best = cand; }
            }
        if (best <= 0)
            for (double cand = 70; cand <= 180; cand += 0.5)
            {
                const double sc = acScore (cand);
                if (sc > bestScore) { bestScore = sc; best = cand; }
            }
        bpm = best > 0 ? best : 120.0;
    }
    const double beatsPerSec = bpm / 60.0;
    const int bars = juce::jlimit (1, 32, (int) std::lround (duration * beatsPerSec / 4.0));

    // ---- onsets (peak picking on flux)
    std::vector<double> onsets;
    {
        double mean = 0;
        for (float v : flux) mean += v;
        mean /= frames;
        double var = 0;
        for (float v : flux) var += (v - mean) * (v - mean);
        const double thr = mean + 0.8 * std::sqrt (var / frames);
        double lastT = -1;
        for (int i = 1; i + 1 < frames; ++i)
            if (flux[(size_t) i] > thr && flux[(size_t) i] >= flux[(size_t) (i - 1)] && flux[(size_t) i] >= flux[(size_t) (i + 1)])
            {
                const double t = (i * hop + N * 0.25) / fs;   // flux peak sits a little before the window centre
                if (t - lastT > 0.07) { onsets.push_back (t * beatsPerSec); lastT = t; }
            }
    }

    // ---- monophonic or chords?  (YIN on short frames)
    struct PF { double t; float midi; float level; };
    std::vector<PF> pf;
    int energetic = 0, voiced = 0;
    {
        const int W = 2048, H = 256, tauMax = (int) (fs / 40.0), tauMin = (int) (fs / 1100.0);
        std::vector<float> d ((size_t) tauMax + 2);
        for (int s = 0; s + W + tauMax < n; s += H)
        {
            float e = 0;
            for (int i = 0; i < W; ++i) e += x[(size_t) (s + i)] * x[(size_t) (s + i)];
            const float level = std::sqrt (e / W);
            if (level < 0.02f) { pf.push_back ({ s / fs, -1.0f, level }); continue; }
            ++energetic;
            const int Wn = 1024;
            d[0] = 1;
            float running = 0;
            for (int tau = 1; tau <= tauMax; ++tau)
            {
                float sum = 0;
                for (int j = 0; j < Wn; ++j) { const float df = x[(size_t) (s + j)] - x[(size_t) (s + j + tau)]; sum += df * df; }
                running += sum;
                d[(size_t) tau] = running > 0 ? sum * tau / running : 1;
            }
            int best = -1;
            for (int tau = tauMin; tau <= tauMax; ++tau)
                if (d[(size_t) tau] < 0.2f)
                {
                    while (tau + 1 <= tauMax && d[(size_t) (tau + 1)] < d[(size_t) tau]) ++tau;
                    best = tau; break;
                }
            float midi = -1;
            if (best > 0)
            {
                double b = best;
                if (best > 1 && best < tauMax)
                {
                    const float a = d[(size_t) (best - 1)], c = d[(size_t) best], e2 = d[(size_t) (best + 1)];
                    const float den = a - 2 * c + e2;
                    if (std::abs (den) > 1e-9f) b += 0.5 * (a - e2) / den;
                }
                midi = (float) (69.0 + 12.0 * std::log2 (fs / b / 440.0));
                ++voiced;
            }
            pf.push_back ({ s / fs, midi, level });
        }
    }
    float avgPeaks = 0;
    int counted = 0;
    for (int fi = 0; fi < frames; ++fi)
    {
        if (rms[(size_t) fi] < 0.15f * maxRms) continue;
        const auto& ch = chroma[(size_t) fi];
        const float mx = *std::max_element (ch.begin(), ch.end());
        if (mx <= 0) continue;
        int c = 0;
        for (float v : ch) if (v > 0.45f * mx) ++c;
        avgPeaks += (float) c; ++counted;
    }
    avgPeaks = counted > 0 ? avgPeaks / counted : 6.0f;
    const float voicedRatio = energetic > 0 ? (float) voiced / (float) energetic : 0.0f;

    // Chords have several strong pitch classes; a single note has one (plus weaker harmonics).
    double r3Sum = 0;
    int r3N = 0;
    for (int fi = 0; fi < frames; ++fi)
    {
        if (rms[(size_t) fi] < 0.15f * maxRms) continue;
        std::array<float, 12> full;
        for (int k = 0; k < 12; ++k) full[(size_t) k] = chroma[(size_t) fi][(size_t) k] + bassChroma[(size_t) fi][(size_t) k];
        std::sort (full.begin(), full.end(), std::greater<float>());
        if (full[0] <= 0) continue;
        r3Sum += full[2] / full[0];
        ++r3N;
    }
    const float r3 = r3N > 0 ? (float) (r3Sum / r3N) : 1.0f;
    double lo = 0, hi = 0;
    for (int fi = 0; fi < frames; ++fi) { lo += lowE[(size_t) fi]; hi += highE[(size_t) fi]; }
    const float trebleRatio = (float) (hi / juce::jmax (1.0e-9, lo + hi));
    std::vector<float> voicedPitches;
    for (auto& p : pf) if (p.midi > 0) voicedPitches.push_back (p.midi);
    std::sort (voicedPitches.begin(), voicedPitches.end());
    const float medianPitch = voicedPitches.empty() ? 0.0f : voicedPitches[voicedPitches.size() / 2];
    const bool bassLine = voicedRatio > 0.6f && medianPitch > 0 && medianPitch < 52 && trebleRatio < 0.3f;
    const bool isMono = bassLine || (voicedRatio > 0.6f && r3 < 0.4f);
    if (juce::SystemStats::getEnvironmentVariable ("WALL_DEBUG", {}).isNotEmpty())
        std::fprintf (stderr, "voiced %.2f r3 %.2f treble %.2f median %.1f\n", voicedRatio, r3, trebleRatio, medianPitch);

    Entry e;
    e.source = "audio"; e.bpm = bpm; e.bars = bars;

    if (isMono)
    {
        // median-smooth the pitch track and cut it into notes
        std::vector<float> sm (pf.size(), -1.0f);
        for (size_t i = 0; i < pf.size(); ++i)
        {
            std::vector<float> win5;
            for (int k = -2; k <= 2; ++k)
            {
                const long j = (long) i + k;
                if (j >= 0 && j < (long) pf.size() && pf[(size_t) j].midi > 0) win5.push_back (pf[(size_t) j].midi);
            }
            if (pf[i].midi > 0 && win5.size() >= 3) { std::sort (win5.begin(), win5.end()); sm[i] = win5[win5.size() / 2]; }
        }
        struct Cur { bool on = false; size_t start = 0; int pitch = 0; float peak = 0; };
        Cur cur;
        int miss = 0, change = 0, changePitch = -1;
        auto close = [&] (size_t endIdx)
        {
            if (! cur.on) return;
            const double t0 = pf[cur.start].t, t1 = pf[endIdx].t;
            if (t1 - t0 >= 0.06)
            {
                const double b0 = std::round (t0 * beatsPerSec * 4.0) / 4.0;
                const double len = juce::jmax (0.25, std::round ((t1 - t0) * beatsPerSec * 4.0) / 4.0);
                const int vel = juce::jlimit (30, 127, (int) (60 + 60 * std::log10 (1 + 9 * cur.peak)));
                e.notes.push_back ({ b0, len, cur.pitch, vel });
            }
            cur.on = false;
        };
        for (size_t i = 0; i < pf.size(); ++i)
        {
            const int p = sm[i] > 0 ? (int) std::lround (sm[i]) : -1;
            const bool attack = i >= 3 && pf[i].level > 1.8f * pf[i - 3].level && pf[i].level > 0.05f;
            if (p < 0)
            {
                if (cur.on && ++miss >= 3) close (i - 2);
                continue;
            }
            miss = 0;
            if (! cur.on) { cur = { true, i, p, pf[i].level }; change = 0; continue; }
            if (attack && i - cur.start > 4) { close (i); cur = { true, i, p, pf[i].level }; change = 0; continue; }
            if (p != cur.pitch)
            {
                change = (p == changePitch) ? change + 1 : 1;
                changePitch = p;
                if (change >= 3) { close (i - 2); cur = { true, i - 2, p, pf[i].level }; change = 0; }
            }
            else change = 0;
            cur.peak = std::max (cur.peak, pf[i].level);
        }
        if (cur.on) close (pf.size() - 1);
        e.notes.erase (std::remove_if (e.notes.begin(), e.notes.end(), [&] (const Note& nn) { return nn.beat >= bars * 4.0; }), e.notes.end());
        std::vector<int> ps;
        for (auto& nn : e.notes) ps.push_back (nn.pitch);
        if (ps.empty()) return out;
        std::sort (ps.begin(), ps.end());
        e.kind = ps[ps.size() / 2] < 50 ? KBass : KMelody;
        std::vector<double> on;
        for (auto& nn : e.notes) on.push_back (nn.beat);
        e.rhythm = rhythmFromOnsets (on, bars);
    }
    else
    {
        // chord per beat from chroma, bass note from the low band
        e.kind = KChords;
        const int beats = bars * 4;
        int prevRoot = -1, prevQ = 0, prevBass = 0;
        for (int b = 0; b < beats; ++b)
        {
            const double t0 = b / beatsPerSec, t1 = (b + 1) / beatsPerSec;
            float w[12] {}, bw[12] {};
            int cnt = 0;
            for (int fi = 0; fi < frames; ++fi)
            {
                const double tc = (fi * hop + N * 0.5) / fs;
                if (tc < t0 || tc >= t1) continue;
                for (int k = 0; k < 12; ++k) { w[k] += chroma[(size_t) fi][(size_t) k]; bw[k] += bassChroma[(size_t) fi][(size_t) k]; }
                ++cnt;
            }
            float tot = 0, mx = 0;
            for (float v : w) { tot += v; mx = std::max (mx, v); }
            int root, q, bass;
            if (cnt == 0 || tot <= 1.0e-6f)
            {
                if (prevRoot < 0) continue;
                root = prevRoot; q = prevQ; bass = prevBass;
            }
            else
            {
                for (auto& v : w) v = std::max (0.0f, v / mx - 0.15f);   // drop the noise floor
                const int bpc = (int) (std::max_element (bw, bw + 12) - bw);
                float sc;
                bestChord (w, bpc, root, q, sc);
                bass = bpc;
            }
            if (! e.chords.empty() && e.chords.back().root == root && e.chords.back().quality == q && e.chords.back().bass == bass)
                e.chords.back().len += 1.0;
            else e.chords.push_back ({ (double) b, 1.0, root, bass, q });
            prevRoot = root; prevQ = q; prevBass = bass;
        }
        for (size_t i = 1; i < e.chords.size();)
        {
            auto& c = e.chords;
            const bool sameFamily = c[i].root == c[i - 1].root && c[i].bass == c[i - 1].bass
                && ((c[i].quality == QMin7 && c[i - 1].quality == QMin) || (c[i].quality == QMin && c[i - 1].quality == QMin7)
                 || (c[i].quality == QMaj7 && c[i - 1].quality == QMaj) || (c[i].quality == QMaj && c[i - 1].quality == QMaj7));
            if (sameFamily)
            {
                if (c[i].len > c[i - 1].len) c[i - 1].quality = c[i].quality;
                c[i - 1].len += c[i].len;
                c.erase (c.begin() + (long) i);
            }
            else ++i;
        }
        for (size_t i = 1; i + 1 < e.chords.size();)
        {
            auto& c = e.chords;
            if (c[i].len <= 1.0 && c[i - 1].root == c[i + 1].root && c[i - 1].quality == c[i + 1].quality)
            {
                c[i - 1].len += c[i].len + c[i + 1].len;
                c.erase (c.begin() + (long) i, c.begin() + (long) i + 2);
            }
            else ++i;
        }
        if (e.chords.empty()) return out;
        e.rhythm = rhythmFromOnsets (onsets, bars);
    }

    finishEntry (e);
    int k; bool mnr;
    if (keyFromName (f.getFileNameWithoutExtension(), k, mnr)) { e.key = k; e.minor = mnr; }
    out.push_back (std::move (e));
    return out;
}

// ============================================================ library storage
Library::Library() { formats.registerBasicFormats(); load(); }
Library::~Library() { pool.removeAllJobs (true, 10000); }

juce::File Library::folder()
{
    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    auto d = base.getChildFile ("Application Support/WallChords/Library");
   #else
    auto d = base.getChildFile ("WallChords/Library");
   #endif
    d.getChildFile ("files").createDirectory();
    return d;
}

bool Library::isSupported (const juce::String& path)
{
    return juce::File (path).hasFileExtension ("mid;midi;wav;aif;aiff;mp3;m4a;flac;ogg");
}

void Library::load()
{
    const juce::ScopedLock sl (lock);
    entries.clear();
    const auto v = juce::JSON::parse (folder().getChildFile ("index.json").loadFileAsString());
    if (auto* a = v.getArray())
        for (auto& x : *a)
        {
            auto e = Entry::fromVar (x);
            if (e.id.isNotEmpty()) entries.push_back (std::move (e));
        }
    ++changes;
}

void Library::save()
{
    juce::Array<juce::var> a;
    {
        const juce::ScopedLock sl (lock);
        for (auto& e : entries) a.add (e.toVar());
    }
    folder().getChildFile ("index.json").replaceWithText (juce::JSON::toString (juce::var (a), true));
}

std::vector<Entry> Library::snapshot() const
{
    const juce::ScopedLock sl (lock);
    return entries;
}

void Library::addFiles (const juce::StringArray& paths, std::function<void()> onChanged)
{
    for (auto& p : paths)
    {
        if (! isSupported (p)) continue;
        ++pending;
        pool.addJob ([this, p, onChanged]
        {
            const juce::File src (p);
            auto dst = folder().getChildFile ("files").getNonexistentChildFile (src.getFileNameWithoutExtension(), src.getFileExtension(), false);
            src.copyFileTo (dst);
            auto found = dst.hasFileExtension ("mid;midi") ? analyseMidi (dst) : analyseAudio (dst, formats);
            if (found.empty()) dst.deleteFile();
            {
                const juce::ScopedLock sl (lock);
                for (auto& e : found)
                {
                    e.id = juce::Uuid().toString();
                    e.file = dst.getFileName();
                    e.name = src.getFileNameWithoutExtension() + (found.size() > 1 ? juce::String (" (") + kindName (e.kind) + ")" : juce::String());
                    entries.push_back (e);
                }
            }
            save();
            --pending;
            ++changes;
            if (onChanged) juce::MessageManager::callAsync (onChanged);
        });
    }
}

void Library::remove (const juce::String& id)
{
    juce::String file;
    {
        const juce::ScopedLock sl (lock);
        for (auto it = entries.begin(); it != entries.end(); ++it)
            if (it->id == id) { file = it->file; entries.erase (it); break; }
        for (auto& e : entries) if (e.file == file) file = {};
    }
    if (file.isNotEmpty()) folder().getChildFile ("files").getChildFile (file).deleteFile();
    save();
    ++changes;
}

void Library::setEnabled (const juce::String& id, bool on)
{
    {
        const juce::ScopedLock sl (lock);
        for (auto& e : entries) if (e.id == id) e.enabled = on;
    }
    save();
    ++changes;
}

} // namespace lib
