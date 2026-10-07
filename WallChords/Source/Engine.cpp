#include "Engine.h"
#include <algorithm>
#include <cmath>

namespace eng
{
using namespace th;

// ---------------------------------------------------------------- voicing helpers
static std::vector<int> pitchClasses (const Chord& c)
{
    std::vector<int> pcs;
    for (int iv : c.intervals)
    {
        const int pc = (c.root + iv) % 12;
        if (std::find (pcs.begin(), pcs.end(), pc) == pcs.end()) pcs.push_back (pc);
    }
    return pcs;
}

static int nearestWithPc (int pc, int target, int lo, int hi)
{
    int best = -1, bestD = 1000;
    for (int p = lo; p <= hi; ++p)
        if (((p % 12) + 12) % 12 == pc && std::abs (p - target) < bestD) { best = p; bestD = std::abs (p - target); }
    return best < 0 ? lo + ((pc - lo % 12) + 12) % 12 : best;
}

std::vector<int> voiceChord (const Chord& c, const std::vector<int>& prev, int lo, int hi)
{
    const auto pcs = pitchClasses (c);
    const int n = (int) pcs.size();
    std::vector<int> best;
    double bestScore = 1.0e9;
    const double centre = (lo + hi) * 0.5 - 4.0;

    for (int r = 0; r < n; ++r)
        for (int base = lo; base < lo + 12; ++base)
        {
            if (base % 12 != pcs[(size_t) r]) continue;
            std::vector<int> cand { base };
            bool ok = true;
            for (int k = 1; k < n && ok; ++k)
            {
                const int pc = pcs[(size_t) ((r + k) % n)];
                int p = cand.back() + 1;
                while (p % 12 != pc) ++p;
                if (p > hi) ok = false; else cand.push_back (p);
            }
            if (! ok) continue;
            double score = 0.0;
            if (prev.empty())
            {
                double mean = 0; for (int p : cand) mean += p;
                score = std::abs (mean / n - centre);
            }
            else
                for (int p : cand)
                {
                    int d = 1000;
                    for (int q : prev) d = std::min (d, std::abs (p - q));
                    score += d;
                }
            if (score < bestScore) { bestScore = score; best = cand; }
        }

    if (best.empty())
        for (int pc : pcs) best.push_back (nearestWithPc (pc, lo + 6, lo, hi + 12));
    std::sort (best.begin(), best.end());
    return best;
}

std::vector<int> guitarVoicing (const Chord& c)
{
    // Open-position style: low bass, then 5th, root, 3rd, colour, 5th stacked upward
    std::vector<int> v { nearestWithPc (c.bass, 45, 40, 51) };
    std::vector<int> order;
    const int iv1 = c.intervals.size() > 1 ? c.intervals[1] : 4;
    const int iv2 = c.intervals.size() > 2 ? c.intervals[2] : 7;
    order = { (c.root + iv2) % 12, c.root, (c.root + iv1) % 12 };
    if (c.intervals.size() > 3) order.push_back ((c.root + c.intervals[3]) % 12);
    order.push_back ((c.root + iv2) % 12);
    order.push_back (c.root);
    for (int pc : order)
    {
        if (v.size() >= 6) break;
        int p = v.back() + 2;
        while (p % 12 != pc) ++p;
        if (p > 74) break;
        v.push_back (p);
    }
    return v;
}

std::vector<int> scalePitches (int key, bool minor, int lo, int hi)
{
    const int* sc = minor ? minorScale : majorScale;
    std::vector<int> out;
    for (int p = lo; p <= hi; ++p)
    {
        const int rel = ((p - key) % 12 + 12) % 12;
        for (int i = 0; i < 7; ++i) if (sc[i] == rel) { out.push_back (p); break; }
    }
    return out;
}

// ---------------------------------------------------------------- genre defaults
int autoChordStyle (int genre, const juce::String& s, float e)
{
    if (genre == Worship) return e < 0.45f ? CAmbient : e < 0.7f ? CPianoFlow : CStrum;
    if (genre == Pop)
    {
        if (s.startsWith ("Pre")) return CBlock;
        if (s.startsWith ("Bridge")) return CRolled;
        if (s.startsWith ("Verse")) return CFunk;
        return CPopBounce;
    }
    if (s.startsWith ("Drop")) return CSupersaw;
    if (s.startsWith ("Breakdown")) return CRolled;
    if (s.startsWith ("Build")) return CStabs;
    return CPluckArp;
}

int autoBassStyle (int genre, const juce::String& s, float e)
{
    if (genre == Worship) return e < 0.5f ? BSub : e < 0.75f ? BWalk : BRoot8;
    if (genre == Pop)
    {
        if (s.startsWith ("Verse")) return BSync;
        if (s.startsWith ("Chorus") || s.startsWith ("Post")) return BOctave;
        if (s.startsWith ("Pre")) return BRoot8;
        return BWhole;
    }
    return s.startsWith ("Drop") ? BOffbeat : BSub;
}

int autoLeadStyle (int genre) { return genre == Worship ? LAmbient : genre == Pop ? LHook : LAnthem; }

int autoPadStyle (int genre, const juce::String& s, float e, bool build)
{
    if (build) return PSectionSwell;
    if (genre == Worship) return e < 0.5f ? PChordSwell : PSectionSwell;
    if (genre == Pop) return s.startsWith ("Bridge") ? PChordSwell : PSustain;
    if (s.startsWith ("Drop")) return PPulse;
    return s.startsWith ("Breakdown") ? PChordSwell : PSectionSwell;
}

// ---------------------------------------------------------------- performer
Performer::Performer (std::vector<NoteEv>& n, std::vector<CCEv>& c, juce::Random& r, double bpm,
                      float hum, float dyn, float mov, float strumMs)
    : notes (n), ccs (c), rng (r), beatsPerSec (bpm / 60.0), humanize (hum), dynamics (dyn), movement (mov),
      strum (strumMs * 0.001 * bpm / 60.0) {}

int Performer::density (float e, float mov)
{
    const float d = e * 0.75f + mov * 0.5f;
    return d < 0.45f ? 0 : d < 0.68f ? 1 : d < 0.92f ? 2 : 3;
}

float Performer::velocity (float raw)
{
    const float v = 64.0f + (raw - 64.0f) * (0.35f + dynamics * 0.9f)
                    + (rng.nextFloat() * 2.0f - 1.0f) * humanize * 9.0f;
    return juce::jlimit (1.0f, 127.0f, v);
}

void Performer::note (int part, double beat, double len, int pitch, float vel)
{
    if (pitch < 0 || pitch > 127) return;
    if (beat > 0.01) beat += (rng.nextDouble() * 2.0 - 1.0) * humanize * 0.012 * beatsPerSec;
    notes.push_back ({ juce::jmax (0.0, beat), juce::jmax (0.05, len), pitch, (int) velocity (vel), part });
}

void Performer::cc (int part, double beat, int num, int val)
{
    ccs.push_back ({ beat, num, juce::jlimit (0, 127, val), part });
}

static bool maskHit (const char* m, int i) { return m[i] == 'x'; }

void Performer::chords (const std::vector<int>& v, int style, double start, double len, float e0, float e1)
{
    if (v.empty()) return;
    const int n = (int) v.size();
    struct Hit { double t; int dir; };   // dir 1 = down strum, -1 = up strum

    for (double b = start; b < start + len - 1.0e-6; b += 4.0)
    {
        const double barLen = juce::jmin (4.0, start + len - b);
        const float e = e0 + (e1 - e0) * (float) ((b - start) / len);
        const int lv = density (e, movement);
        const float base = 52.0f + e * 58.0f;

        auto strumHit = [&] (double t, double l, int dir, float vel, double spread)
        {
            if (t >= barLen) return;
            if (dir >= 0)
                for (int i = 0; i < n; ++i) note (PChords, b + t + i * spread, l - i * spread, v[(size_t) i], vel * (0.9f + 0.1f * i / n));
            else
                for (int i = n - 1, k = 0; i >= juce::jmax (0, n - 4); --i, ++k) note (PChords, b + t + k * spread, l - k * spread, v[(size_t) i], vel * 0.72f);
        };
        auto pedal = [&] { cc (PChords, b + 0.02, 64, 127); cc (PChords, b + barLen - 0.06, 64, 0); };

        switch (style)
        {
            case CStrum:
            {
                static const std::vector<Hit> L[4] = {
                    { { 0, 1 } },
                    { { 0, 1 }, { 2, 1 }, { 2.5, -1 } },
                    { { 0, 1 }, { 1, 1 }, { 1.5, -1 }, { 2.5, -1 }, { 3, 1 }, { 3.5, -1 } },
                    { { 0, 1 }, { .5, -1 }, { 1, 1 }, { 1.5, -1 }, { 2, 1 }, { 2.5, -1 }, { 3, 1 }, { 3.5, -1 } } };
                const auto& hits = L[lv];
                for (size_t h = 0; h < hits.size(); ++h)
                {
                    const double next = h + 1 < hits.size() ? hits[h + 1].t : barLen;
                    strumHit (hits[h].t, (next - hits[h].t) * 0.98, hits[h].dir, base * (hits[h].t == 0 ? 1.08f : 0.95f), strum);
                }
                break;
            }
            case CFunk:
            {
                static const char* M[4] = { "x.......x.......", "x..x..x.x.......", "x..x..x...x.x...", "x.xx.x.xx.x..x.x" };
                for (int i = 0; i < 16; ++i)
                    if (maskHit (M[lv], i)) strumHit (i * 0.25, 0.2, i % 2 == 0 ? 1 : -1, base * (i % 4 == 0 ? 1.0f : 0.82f), strum * 0.5);
                break;
            }
            case CFingerpick:
            {
                const int thumbs[2] = { 0, juce::jmin (2, n - 1) };
                const int fingers[4] = { n - 1, n - 2, n - 1, juce::jmax (0, n - 3) };
                for (int k = 0; k < 8; ++k)
                {
                    const double t = k * 0.5;
                    if (t >= barLen) break;
                    if (k % 2 == 0) note (PChords, b + t, 1.6, v[(size_t) thumbs[(k / 2) % 2]], base * 0.95f);
                    else if (lv >= 1 || k == 1) note (PChords, b + t, 1.2, v[(size_t) fingers[(k / 2) % 4]], base * 0.78f);
                    if (k == 0 && lv >= 2) note (PChords, b, 1.4, v[(size_t) (n - 1)], base * 0.85f);
                }
                break;
            }
            case CAmbient:
            case CPluckArp:
            {
                std::vector<int> tones (v.begin(), v.end());
                for (int p : v) if (p + 12 <= 96) tones.push_back (p + 12);
                std::sort (tones.begin(), tones.end());
                tones.erase (std::unique (tones.begin(), tones.end()), tones.end());
                const int m = (int) tones.size();
                const bool ambient = style == CAmbient;
                const double step = ambient ? (lv == 0 ? 1.0 : lv == 1 ? 0.5 : 0.75) : (lv == 0 ? 0.5 : 0.25);
                int idx = (int) ((b - start) / 4.0) % 2, dirn = 1;
                for (double t = 0; t < barLen - 1.0e-6; t += step)
                {
                    const float swellIn = ambient ? 0.8f + 0.2f * std::sin ((float) (t / barLen) * 3.14159f) : (std::fmod (t, 1.0) == 0 ? 1.0f : 0.8f);
                    note (PChords, b + t, ambient ? step * 2.5 : step * 0.9, tones[(size_t) idx], base * (ambient ? 0.75f : 0.85f) * swellIn);
                    idx += dirn;
                    if (idx >= m - 1 || idx <= 0) dirn = -dirn;
                    idx = juce::jlimit (0, m - 1, idx);
                }
                break;
            }
            case CPianoFlow:
            {
                int lh = v[0];
                while (lh > 55) lh -= 12;
                note (PChords, b, juce::jmin (2.0, barLen), lh, base * 0.85f);
                if (lv >= 1 && barLen > 2.0) note (PChords, b + 2, 2.0, lh + 7, base * 0.75f);
                static const int pat[8] = { 0, 1, 2, 3, 2, 1, 2, 3 };
                const double step = lv == 0 ? 1.0 : 0.5;
                int k = 0;
                for (double t = 0; t < barLen - 1.0e-6; t += step, ++k)
                    note (PChords, b + t, step * 1.8, v[(size_t) juce::jmin (n - 1, pat[k % 8] + (n > 3 ? 0 : 0))], base * (k % 2 == 0 ? 0.82f : 0.72f));
                if (lv >= 2)
                    for (double t : { 0.0, 2.0 })
                        if (t < barLen) for (int p : v) note (PChords, b + t, 1.0, p, base * 0.7f);
                pedal();
                break;
            }
            case CPopBounce:
            {
                static const char* M[4] = { "x.....x.x.......", "x..x..x.x..x..x.", "x..x..x.x..x.xx.", "x.xx..x.x.xx..x." };
                for (int i = 0; i < 16; ++i)
                    if (maskHit (M[lv], i) && i * 0.25 < barLen)
                        for (int p : v) note (PChords, b + i * 0.25, i == 0 ? 0.5 : 0.3, p, base * (i % 4 == 0 ? 1.0f : 0.82f));
                break;
            }
            case CRolled:
            {
                std::vector<double> hits { 0.0 };
                if (lv >= 2) hits.push_back (2.0);
                if (lv >= 3) hits.push_back (3.5);
                for (size_t h = 0; h < hits.size(); ++h)
                {
                    if (hits[h] >= barLen) break;
                    const double next = h + 1 < hits.size() ? hits[h + 1] : barLen;
                    for (int i = 0; i < n; ++i)
                        note (PChords, b + hits[h] + i * strum * 1.6, (next - hits[h]) * 0.98 - i * strum * 1.6, v[(size_t) i], base * (h == 0 ? 0.9f : 0.7f));
                }
                pedal();
                break;
            }
            case CStabs:
            {
                std::vector<double> hits = lv == 0 ? std::vector<double> { 0.5, 2.5 } : std::vector<double> { 0.5, 1.5, 2.5, 3.5 };
                if (lv >= 3) hits.push_back (3.75);
                for (double t : hits) if (t < barLen) for (int p : v) note (PChords, b + t, 0.22, p, base * 1.0f);
                break;
            }
            case CSupersaw:
            {
                static const char* M[4] = { "x...............", "x.......x.x.....", "x..x..x...x.x...", "x..x..x...x.x..." };
                std::vector<int> steps;
                for (int i = 0; i < 16; ++i) if (maskHit (M[lv], i)) steps.push_back (i);
                for (size_t h = 0; h < steps.size(); ++h)
                {
                    const double t = steps[h] * 0.25;
                    if (t >= barLen) break;
                    const double next = h + 1 < steps.size() ? steps[h + 1] * 0.25 : barLen;
                    for (int p : v) note (PChords, b + t, (next - t) * 0.9, p, base * 1.05f);
                }
                break;
            }
            default: // CBlock
            {
                const double step = lv == 0 ? 4.0 : lv == 1 ? 2.0 : lv == 2 ? 1.0 : 0.5;
                for (double t = 0; t < barLen - 1.0e-6; t += step)
                    for (int p : v) note (PChords, b + t, juce::jmin (step, barLen - t) * 0.92, p, base * (t == 0 ? 1.0f : 0.85f));
                break;
            }
        }
    }
}

void Performer::buildRoll (const std::vector<int>& v, double start, double len, double secStart, double secLen)
{
    const double gap = secStart + secLen - 1.0;
    for (double t = start; t < start + len - 1.0e-6;)
    {
        const double p = (t - secStart) / secLen;
        const double step = p < 0.25 ? 1.0 : p < 0.5 ? 0.5 : 0.25;
        if (t < gap)
            for (int n : v) note (PChords, t, step * 0.6, n, 55.0f + 65.0f * (float) p);
        t += step;
    }
}

void Performer::bass (int pitch, int nextPitch, int style, double start, double len, float e)
{
    const float base = 70.0f + e * 40.0f;
    const int lv = density (e, movement);
    for (double b = start; b < start + len - 1.0e-6; b += 4.0)
    {
        const double barLen = juce::jmin (4.0, start + len - b);
        const bool lastBar = b + 4.0 >= start + len - 1.0e-6;
        switch (style)
        {
            case BWhole:
                if (b == start) note (PBass, b, len * 0.97, pitch, base);
                break;
            case BSub:
                note (PBass, b, barLen * 0.97, pitch, base);
                break;
            case BRoot8:
                for (double t = 0; t < barLen - 1.0e-6; t += 0.5)
                    note (PBass, b + t, 0.42, pitch, base * (std::fmod (t, 1.0) == 0 ? 1.0f : 0.8f));
                break;
            case BOctave:
                for (double t = 0; t < barLen - 1.0e-6; t += (lv == 0 ? 1.0 : 0.5))
                    note (PBass, b + t, 0.38, std::fmod (t, 1.0) == 0 ? pitch : pitch + 12, base * (std::fmod (t, 1.0) == 0 ? 1.0f : 0.85f));
                break;
            case BSync:
            {
                static const char* m = "x..x..x...x.x...";
                for (int i = 0; i < 16; ++i)
                    if (m[i] == 'x' && i * 0.25 < barLen) note (PBass, b + i * 0.25, 0.28, i == 10 ? pitch + 7 : pitch, base * (i == 0 ? 1.0f : 0.85f));
                break;
            }
            case BWalk:
            {
                note (PBass, b, juce::jmin (barLen, lv >= 1 ? 1.9 : 2.9), pitch, base);
                if (lv >= 1 && barLen > 2.0) note (PBass, b + 2, 0.9, pitch + 7 > 50 ? pitch - 5 : pitch + 7, base * 0.85f);
                if (barLen > 3.0)
                {
                    const int approach = lastBar && std::abs (nextPitch - pitch) > 2 ? (nextPitch > pitch ? nextPitch - 1 : nextPitch + 1) : pitch;
                    note (PBass, b + 3, 0.9, approach, base * 0.8f);
                }
                break;
            }
            case BOffbeat:
                for (double t = 0.5; t < barLen; t += 1.0) note (PBass, b + t, 0.35, pitch, base);
                break;
            default:
                note (PBass, b, barLen * 0.97, pitch, base);
        }
    }
}

void Performer::walkUp (int fromPitch, int toPitch, const int* sc, int key, double start)
{
    juce::ignoreUnused (fromPitch);
    std::vector<int> below;
    for (int p = toPitch - 1; p > toPitch - 13 && below.size() < 4; --p)
    {
        const int rel = ((p - key) % 12 + 12) % 12;
        for (int i = 0; i < 7; ++i) if (sc[i] == rel) { below.push_back (p); break; }
    }
    for (size_t i = 0; i < below.size(); ++i)
        note (PBass, start + (double) i, 0.9, below[below.size() - 1 - i], 72.0f + 8.0f * (float) i);
}

void Performer::pad (const std::vector<int>& v, int style, double start, double len, float e,
                     double secStart, double secEnd, bool build, float swell)
{
    for (int p : v) note (PPad, start, len * 0.995, p, 50.0f + e * 30.0f);

    for (double t = start; t < start + len - 1.0e-6; t += 0.25)
    {
        const double frac = (t - start) / len;
        switch (style)
        {
            case PSustain:
                if (t == start) { cc (PPad, t, 1, 95); cc (PPad, t, 11, 100); }
                return;
            case PChordSwell:
            {
                const double s = frac < 0.6 ? frac / 0.6 : 1.0 - (frac - 0.6) / 0.4 * 0.3;
                cc (PPad, t, 1, (int) (25 + s * 102 * swell));
                if (t == start) cc (PPad, t, 11, 100);
                break;
            }
            case PSectionSwell:
            {
                const double rs = build ? secStart : juce::jmax (secStart, secEnd - 8.0);
                const double ramp = t >= rs ? (t - rs) / (secEnd - rs) : 0.0;
                cc (PPad, t, 1, (int) (40 + ramp * 87 * swell));
                if (t == start) cc (PPad, t, 11, 100);
                break;
            }
            case PPulse:
            {
                const double ph = std::fmod (t - secStart, 1.0);
                cc (PPad, t, 11, (int) (127 - (1.0 - juce::jmin (1.0, ph / 0.6)) * 100 * swell));
                if (t == start) cc (PPad, t, 1, 90);
                break;
            }
            default: return;
        }
    }
}

// ---------------------------------------------------------------- lead melody
void generateLead (Performer& P, const std::vector<Slot>& slots, size_t s0, size_t s1, int style,
                          int genre, double secStart, double secEnd, float e0, float e1,
                          juce::Random& rng, int key, bool minor)
{
    if (style == LAuto) style = autoLeadStyle (genre);
    const int lo = style == LHook ? 62 : style == LAmbient ? 71 : style == LAnthem ? 67 : 64;
    const int hi = style == LHook ? 79 : style == LAmbient ? 86 : style == LAnthem ? 86 : 88;
    const auto scale = scalePitches (key, minor, lo, hi);
    if (scale.empty()) return;

    static const char* hook[] = { "x.x.x..x..x.x...", "..x.x.x.x..x....", "x..x..x.x.x.x...", "x.x...x.x.x....." };
    static const char* amb[]  = { "x.......x.......", "x...............", "x.....x.x......." };
    static const char* anth[] = { "x..x..x.x..x.x..", "x.x..x..x..x..x.", "x..x..x...x.x.x." };
    const char* const* set = style == LHook ? hook : style == LAmbient ? amb : anth;
    const int nSet = style == LHook ? 4 : 3;
    const char* endMask = style == LHook ? "x...x.......x..." : style == LAmbient ? "x..............." : "x..x..x.x.......";
    const char* arpMask = "xxxxxxxxxxxxxxxx";

    const int A = rng.nextInt (nSet), B = (A + 1 + rng.nextInt (nSet - 1)) % nSet;
    int contour[2][16];
    for (auto& c : contour)
        for (int i = 0; i < 16; ++i) c[i] = rng.nextInt (3) - (i < 8 ? 0 : 1);   // rise early, fall late

    auto chordAt = [&] (double beat) -> const Chord&
    {
        for (size_t i = s0; i < s1; ++i)
            if (beat >= slots[i].start - 1.0e-6 && beat < slots[i].start + slots[i].len) return slots[i].chord;
        return slots[s1 - 1].chord;
    };
    auto isChordTone = [] (const Chord& c, int p)
    {
        for (int iv : c.intervals) if ((c.root + iv) % 12 == p % 12) return true;
        return false;
    };

    int curIdx = (int) scale.size() / 2, arpIdx = 0, arpDir = 1;
    for (double bar = secStart; bar < secEnd - 1.0e-6; bar += 4.0)
    {
        const int pos = (int) ((bar - secStart) / 4.0) % 4;
        const char* mask = style == LArp ? arpMask : pos == 3 ? endMask : (pos == 1 ? set[B] : set[A]);
        const int* cont = contour[pos == 1 ? 1 : 0];
        const float e = e0 + (e1 - e0) * (float) ((bar - secStart) / (secEnd - secStart));

        std::vector<int> steps;
        for (int i = 0; i < 16; ++i) if (mask[i] == 'x') steps.push_back (i);

        for (size_t h = 0; h < steps.size(); ++h)
        {
            const int s = steps[h];
            const double beat = bar + s * 0.25;
            if (beat >= secEnd) break;
            const double next = h + 1 < steps.size() ? bar + steps[h + 1] * 0.25 : bar + 4.0;
            const Chord& c = chordAt (beat);

            if (style == LArp)
            {
                std::vector<int> tones;
                for (int p : scale) if (isChordTone (c, p)) tones.push_back (p);
                if (tones.empty()) continue;
                arpIdx = juce::jlimit (0, (int) tones.size() - 1, arpIdx);
                P.note (PLead, beat, 0.2, tones[(size_t) arpIdx], 70.0f + e * 35.0f * (s % 4 == 0 ? 1.0f : 0.8f));
                arpIdx += arpDir;
                if (arpIdx >= (int) tones.size() - 1 || arpIdx <= 0) arpDir = -arpDir;
                continue;
            }

            const bool strong = s % 4 == 0;
            const bool phraseEnd = pos == 3 && h + 1 == steps.size();
            if (strong || phraseEnd)
            {
                const int target = juce::jlimit (0, (int) scale.size() - 1, curIdx + cont[h % 16] * 2);
                int best = target, bestD = 1000;
                for (int i = 0; i < (int) scale.size(); ++i)
                {
                    const int p = scale[(size_t) i];
                    const bool ok = phraseEnd ? (p % 12 == c.root || p % 12 == (c.root + c.intervals[1]) % 12) : isChordTone (c, p);
                    if (ok && std::abs (i - target) < bestD) { best = i; bestD = std::abs (i - target); }
                }
                curIdx = best;
            }
            else curIdx = juce::jlimit (0, (int) scale.size() - 1, curIdx + cont[h % 16]);

            const int pitch = scale[(size_t) curIdx];
            const double gapLen = next - beat;
            const double len = style == LAmbient ? gapLen * 0.98 : style == LAnthem ? juce::jmin (gapLen * 0.85, 0.75) : gapLen * 0.9;
            const float vel = (66.0f + e * 40.0f) * (strong ? 1.0f : 0.85f);
            P.note (PLead, beat, len, pitch, vel);
            if (style == LAmbient && pitch + 12 <= 98) P.note (PLead, beat, len, pitch + 12, vel * 0.7f);
            if (style == LAnthem && e >= 0.95f) P.note (PLead, beat, len, pitch - 12, vel * 0.75f);
        }
    }
}

// ---------------------------------------------------------------- song generation
std::shared_ptr<Song> generate (const Settings& st)
{
    auto song = std::make_shared<Song>();
    song->genre = st.genre; song->key = st.key; song->minor = st.minor; song->bpm = st.bpm;
    song->notes.reserve (8192);
    song->ccs.reserve (8192);

    juce::Random rng (st.seed * 7919 + 17);
    Performer P (song->notes, song->ccs, rng, st.bpm, st.humanize, st.dynamics, st.movement, st.strumMs);
    const int* sc = st.minor ? minorScale : majorScale;

    const auto& structureDefs = structure (st.genre);
    const bool single = st.sectionChoice > 0;
    std::vector<int> order;
    if (single) order.push_back (findSection (st.genre, st.sectionChoice));
    else for (int i = 0; i < (int) structureDefs.size(); ++i) order.push_back (i);

    const int colorEff = st.color == 0 ? (st.genre == Pop ? -1 : 2) : st.color + 0;   // UI 2 == add9/sus
    std::vector<int> prevChord, prevPad;
    int prevBass = 40;
    double cursor = 0.0;

    auto pickTokens = [&] (const SectionDef& def)
    {
        const auto& list = progressions (st.genre, st.minor, def.role);
        const juce::String name (def.name);
        int pIdx = (st.seed * 7 + (int) def.role * 13) % list.size();
        const bool chorusLike = name.startsWith ("Chorus") || name.startsWith ("Post");
        if (def.role == RoleMain && st.prog > 0 && st.prog <= list.size()) pIdx = st.prog - 1;
        else if (def.role == RoleMain && chorusLike && st.genre == Pop) pIdx = (pIdx + 1) % list.size();
        return juce::StringArray::fromTokens (list[pIdx], " ", "");
    };

    for (size_t oi = 0; oi < order.size(); ++oi)
    {
        const int k = order[oi];
        const auto& def = structureDefs[(size_t) k];
        static const int barsOpt[] = { 4, 8, 16 };
        const int bars = single && st.barsChoice > 0 ? barsOpt[st.barsChoice - 1] : def.bars;
        const double secStart = cursor, secEnd = cursor + bars * 4.0;
        const juce::String name (def.name);
        const juce::String flags (def.parts);

        // ---- progression for this section
        const auto tokens = pickTokens (def);

        const double chordLen = (st.genre == Worship && def.e0 < 0.35f ? 2 : 1) * 4.0;
        const size_t s0 = song->slots.size();
        int ti = 0;
        for (double t = secStart; t < secEnd - 1.0e-6; ti++)
        {
            const auto& tok = tokens[ti % tokens.size()];
            const double clen = juce::jmin (chordLen, secEnd - t);
            const int sectionIdx = (int) song->sections.size();
            const Chord normal = makeChord (tok, st.key, st.minor, st.genre, st.color == 0 ? 0 : st.color, false);
            const bool majorChord = normal.intervals.size() > 1 && normal.intervals[1] == 4;
            if (colorEff == 2 && tok.startsWith ("5") && ! tok.contains ("/") && majorChord && clen >= 4.0)
            {
                song->slots.push_back ({ t, clen * 0.5, makeChord (tok, st.key, st.minor, st.genre, 2, true), sectionIdx });
                song->slots.push_back ({ t + clen * 0.5, clen * 0.5, normal, sectionIdx });
            }
            else song->slots.push_back ({ t, clen, normal, sectionIdx });
            t += clen;
        }
        const size_t s1 = song->slots.size();

        const bool useBass = st.partOn[PBass] && (single || flags.containsChar ('B'));
        const bool usePad  = st.partOn[PPad]  && (single || flags.containsChar ('P'));
        const bool useLead = st.partOn[PLead] && (single || flags.containsChar ('L'));
        const bool hasNext = ! single && k + 1 < (int) structureDefs.size();
        const bool walk = st.genre != EDM && hasNext && structureDefs[(size_t) k + 1].e0 > def.e1 + 0.08f;

        for (size_t si = s0; si < s1; ++si)
        {
            const auto& slot = song->slots[si];
            const float eA = def.e0 + (def.e1 - def.e0) * (float) ((slot.start - secStart) / (secEnd - secStart));
            const float eB = def.e0 + (def.e1 - def.e0) * (float) ((slot.start + slot.len - secStart) / (secEnd - secStart));

            if (st.partOn[PChords])
            {
                const int style = st.chordStyle != CAuto ? st.chordStyle : autoChordStyle (st.genre, name, eA);
                const bool guitar = style == CStrum || style == CFunk || style == CFingerpick;
                int lo = 52, hi = 76;
                if (style == CAmbient) { lo = 59; hi = 83; }
                if (style == CStabs || style == CSupersaw) { lo = 55; hi = 79; }
                if (style == CPluckArp) { lo = 60; hi = 84; }
                std::vector<int> v = guitar ? guitarVoicing (slot.chord) : voiceChord (slot.chord, prevChord, lo, hi);
                if (! guitar) prevChord = v;

                if (def.build && st.genre == EDM && st.chordStyle == CAuto)
                    P.buildRoll (v, slot.start, slot.len, secStart, secEnd - secStart);
                else
                    P.chords (v, style, slot.start, slot.len, eA, eB);
            }

            if (useBass)
            {
                const int pitch = nearestWithPc (slot.chord.bass, prevBass, 33, 47);
                const auto& nextChord = si + 1 < s1 ? song->slots[si + 1].chord : slot.chord;
                const int nextPitch = nearestWithPc (nextChord.bass, pitch, 33, 47);
                const int style = st.bassStyle != BAuto ? st.bassStyle : autoBassStyle (st.genre, name, eA);
                if (walk && si + 1 == s1 && slot.len >= 4.0)
                {
                    const auto upcoming = pickTokens (structureDefs[(size_t) k + 1]);
                    const Chord target = makeChord (upcoming[0], st.key, st.minor, st.genre, 1);
                    if (slot.len > 4.0) P.bass (pitch, pitch, style, slot.start, slot.len - 4.0, eA);
                    P.walkUp (pitch, nearestWithPc (target.bass, pitch + 2, 36, 50), sc, st.key, slot.start + slot.len - 4.0);
                }
                else P.bass (pitch, nextPitch, style, slot.start, slot.len, eA);
                prevBass = pitch;
            }

            if (usePad)
            {
                auto v = voiceChord (slot.chord, prevPad, 55, 79);
                prevPad = v;
                if (v.size() >= 3 && v[1] + 12 <= 86) { v[1] += 12; std::sort (v.begin(), v.end()); }
                const int style = st.padStyle != PAuto ? st.padStyle : autoPadStyle (st.genre, name, eA, def.build);
                P.pad (v, style, slot.start, slot.len, eA, secStart, secEnd, def.build, st.swell);
            }
        }

        if (useLead && s1 > s0)
            generateLead (P, song->slots, s0, s1, st.leadStyle, st.genre, secStart, secEnd, def.e0, def.e1, rng, st.key, st.minor);

        // ---- drop-outs before big moments: EDM build gap (everything), pop pre-chorus stop (chords + bass)
        double gapStart = -1.0;
        int gapMask = 0;
        if (def.build && st.genre == EDM) { gapStart = secEnd - 1.0; gapMask = 0xF; }
        if (st.genre == Pop && name.startsWith ("Pre") && hasNext) { gapStart = secEnd - 2.0; gapMask = (1 << PChords) | (1 << PBass); }
        if (gapStart > 0.0)
        {
            auto& nv = song->notes;
            nv.erase (std::remove_if (nv.begin(), nv.end(), [&] (const NoteEv& n)
                      { return ((gapMask >> n.part) & 1) && n.beat >= gapStart && n.beat < secEnd; }), nv.end());
            for (auto& n : nv)
                if (((gapMask >> n.part) & 1) && n.beat >= secStart && n.beat < gapStart && n.beat + n.len > gapStart)
                    n.len = gapStart - n.beat;
        }

        song->sections.push_back ({ name, secStart, secEnd - secStart });
        cursor = secEnd;
    }

    song->length = juce::jmax (4.0, cursor);
    finalise (*song);
    return song;
}

void finalise (Song& s)
{
    s.events.clear();
    s.events.reserve (s.notes.size() * 2 + s.ccs.size());
    const double maxBeat = s.length - 1.0e-4;
    for (auto& n : s.notes)
    {
        if (n.beat >= maxBeat) continue;
        s.events.push_back ({ n.beat, 1, (uint8_t) n.part, (uint8_t) n.pitch, (uint8_t) n.vel });
        s.events.push_back ({ juce::jmin (n.beat + n.len, maxBeat), 0, (uint8_t) n.part, (uint8_t) n.pitch, 0 });
    }
    for (auto& c : s.ccs)
        if (c.beat < maxBeat) s.events.push_back ({ c.beat, 2, (uint8_t) c.part, (uint8_t) c.cc, (uint8_t) c.val });
    auto rank = [] (uint8_t t) { return t == 0 ? 0 : t == 2 ? 1 : 2; };
    std::stable_sort (s.events.begin(), s.events.end(), [&] (const Ev& a, const Ev& b)
                      { return a.beat != b.beat ? a.beat < b.beat : rank (a.type) < rank (b.type); });
}

} // namespace eng
