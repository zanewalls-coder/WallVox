#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>

// Pitch correction using pitch-synchronous overlap-add (TD-PSOLA).
//  - The voice is cut into single cycles (aligned by correlation) and re-spaced: no comb/chorus smear,
//    formants stay where they are.
//  - Everything always runs through the same engine (unvoiced sounds just pass with ratio 1), so there
//    is never a dry/wet switch that could click.
//  - The target note is chosen from a smoothed pitch with hysteresis so vibrato and scoops don't make it
//    flip between notes, and only the smoothed pitch is corrected (natural cycle-to-cycle variation stays).
namespace tune
{
inline const juce::StringArray scaleNames { "Chromatic", "Major", "Minor", "Major Pentatonic", "Minor Pentatonic", "Harmonic Minor" };

class Tuner
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        maxT = sr / minHz;
        D = (int) std::ceil (2.7 * maxT) + 16;        // look-ahead needed to place grains; reported to the host
        int size = 1;
        while (size < D * 4 + (int) maxT * 8 + 8192) size <<= 1;
        mask = size - 1;
        for (auto& r : ring) r.assign ((size_t) size, 0.0f);
        mono.assign ((size_t) size, 0.0f);
        marks.assign (256, 0.0);
        markHead = 0; markCount = 0;

        decim = std::max (1, (int) std::lround (sr / 22050.0));
        dsr = sr / decim;
        tauMin = std::max (2, (int) (dsr / maxHz));
        tauMax = (int) (dsr / minHz) + 2;
        N = std::max (512, (int) (dsr * 0.034));
        dbuf.assign ((size_t) (N + tauMax + 8) * 2, 0.0f);
        dpos = 0; dacc = 0; dcount = 0; hopCount = 0; lp1 = lp2 = 0;
        diff.assign ((size_t) tauMax + 2, 0.0f);

        t = 0;
        T = Tmark = sr / 150.0;
        unvoicedP = 0.004 * sr;
        lastMark = 0;
        nextSynth = 0;
        voicedDet = false; unvoicedHops = 99; octaveVotes = 0;
        haveTrack = false; fast = slow = mid = 0;
        current = -1; shift = 0; centreShift = centreTarget = flattenShift = flattenTarget = 0; holdSamples = 0;
        heardMidi = 0;
        for (auto& g : grains) g.active = false;
        history[0] = history[1] = history[2] = 0;
        fadeIn = 0;
    }

    int getLatency() const { return D; }

    // retuneMs: 0 = instant (hard tune). amount 0..1. natural 0..1 keeps vibrato and slides.
    void setParams (float retuneMs, float amountIn, int keyIn, int scaleIn, float naturalIn)
    {
        amount = amountIn; key = keyIn; scale = scaleIn; natural = naturalIn;
        speedCoef = (float) std::exp (-1.0 / (std::max (2.5f, retuneMs) * 0.001 * sr));
        // how much of the fast movement (vibrato, wobble) gets flattened: fast speed + low Natural = hard tune
        flattenAmount = (1.0f - natural) * std::exp (-retuneMs / 40.0f);
    }

    int detectedNote() const { return haveTrack ? (int) std::lround (heardMidi) : -1; }
    float detectedCents() const { return haveTrack ? (float) ((heardMidi - std::round (heardMidi)) * 100.0) : 0.0f; }
    int targetNote() const { return haveTrack ? current : -1; }

    void process (float* const* ch, int nch, int n, bool enabled)
    {
        nch = std::min (nch, 2);
        for (int i = 0; i < n; ++i)
        {
            float m = 0;
            for (int c = 0; c < nch; ++c) { ring[(size_t) c][(size_t) (t & mask)] = ch[c][i]; m += ch[c][i]; }
            m /= (float) nch;
            mono[(size_t) (t & mask)] = m;
            pushDetector (m);
            advanceMarks();

            const int64_t tIn = t - D;
            if (! enabled)
            {
                for (int c = 0; c < nch; ++c) ch[c][i] = ring[(size_t) c][(size_t) (tIn & mask)];
                for (auto& g : grains) g.active = false;
                nextSynth = (double) t;
                fadeIn = 0;
                ++t;
                continue;
            }

            // correction, smoothed by the retune speed; held briefly through consonants
            if (! voicedNow() && holdSamples > 0) --holdSamples;
            else if (! voicedNow()) { centreTarget = 0; flattenTarget = 0; }
            centreShift = centreTarget + (centreShift - centreTarget) * speedCoef;     // moves to the note at the retune speed
            flattenShift = flattenTarget + (flattenShift - flattenTarget) * 0.995;    // ~4 ms de-stepping only
            shift = std::clamp (centreShift + flattenShift, -6.0, 6.0);

            if ((double) t >= nextSynth) spawnGrain();

            fadeIn = std::min (1.0f, fadeIn + 1.0f / (float) (0.01 * sr));
            for (int c = 0; c < nch; ++c)
            {
                float sum = 0;
                for (auto& g : grains)
                {
                    if (! g.active) continue;
                    const double j = (double) t - g.start;
                    if (j < 0) continue;
                    const float w = 0.5f - 0.5f * std::cos ((float) (juce::MathConstants<double>::pi * j / g.h));
                    sum += g.gain * w * read (c, g.center + (j - g.h));
                }
                const float dry = ring[(size_t) c][(size_t) (tIn & mask)];
                ch[c][i] = fadeIn < 1.0f ? dry + fadeIn * (sum - dry) : sum;
            }
            for (auto& g : grains) if (g.active && (double) t - g.start >= 2.0 * g.h) g.active = false;
            ++t;
        }
    }

private:
    bool voicedNow() const { return voicedDet; }

    float read (int c, double pos) const
    {
        const double fl = std::floor (pos);
        const int64_t i0 = (int64_t) fl;
        const float fr = (float) (pos - fl);
        const float a = ring[(size_t) c][(size_t) (i0 & mask)], b = ring[(size_t) c][(size_t) ((i0 + 1) & mask)];
        return a + fr * (b - a);
    }

    // ---------------------------------------------------------------- analysis marks (one per cycle)
    double period() const { return voicedDet ? Tmark : unvoicedP; }

    void pushMark (double m)
    {
        marks[markHead] = m;
        markHead = (markHead + 1) % marks.size();
        markCount = std::min (markCount + 1, marks.size());
        lastMark = m;
    }

    void advanceMarks()
    {
        const double P = period();
        const double limit = (double) t - 0.8 * maxT - 4.0;     // refinement needs a little data past each mark
        if (markCount == 0 || lastMark < limit - 16.0 * maxT) { markCount = 0; pushMark (limit); }
        while (lastMark + P <= limit)
        {
            double next = lastMark + P;
            if (voicedDet)
            {
                next = refine (lastMark, next, P);
                const double interval = next - lastMark;
                if (std::abs (interval - T) < 0.2 * T)
                {
                    Tmark += 0.5 * (interval - Tmark);
                    onCycle (69.0 + 12.0 * std::log2 (sr / interval / 440.0), interval);
                }
                else Tmark = T;
            }
            pushMark (std::max (lastMark + P * 0.5, next));
        }
    }

    double refine (double prev, double pred, double P)
    {
        const int L = std::max (32, (int) P);
        const int search = std::max (2, (int) (P / 5));
        const int64_t p0 = (int64_t) std::lround (prev), c0 = (int64_t) std::lround (pred);
        auto score = [&] (int64_t cand)
        {
            double acc = 0, e = 1.0e-9;
            for (int k = -L / 2; k < L / 2; ++k)
            {
                const float a = mono[(size_t) ((p0 + k) & mask)], b = mono[(size_t) ((cand + k) & mask)];
                acc += a * b; e += b * b;
            }
            return acc / std::sqrt (e);
        };
        double best = -1.0e30;
        int bestK = 0;
        for (int k = -search; k <= search; ++k)
        {
            const double v = score (c0 + k);
            if (v > best) { best = v; bestK = k; }
        }
        const double s0 = score (c0 + bestK - 1), s2 = score (c0 + bestK + 1);
        const double den = s0 - 2 * best + s2;
        const double frac = std::abs (den) > 1e-12 ? std::clamp (0.5 * (s0 - s2) / den, -0.5, 0.5) : 0.0;
        return (double) (c0 + bestK) + frac + (prev - (double) p0);
    }

    // ---------------------------------------------------------------- synthesis
    void spawnGrain()
    {
        const bool v = voicedDet;
        const double P = period();
        const double ratio = v ? std::pow (2.0, shift / 12.0) : 1.0;
        const double S = P / ratio;
        const double h = std::max (P, S);
        // read the cycle whose position lines up with this grain's centre (keeps output aligned with input)
        const double want = nextSynth + h - (double) D;
        double best = lastMark, bestD = 1.0e30;
        for (size_t k = 0; k < markCount; ++k)
        {
            const double m = marks[(markHead + marks.size() - 1 - k) % marks.size()];
            const double d = std::abs (m - want);
            if (d < bestD) { bestD = d; best = m; }
            if (m < want - 4 * maxT) break;
        }
        for (auto& g : grains)
            if (! g.active)
            {
                g = { true, best, nextSynth, h, (float) (S / h) };
                break;
            }
        nextSynth = (nextSynth < (double) t - S) ? (double) t + S : nextSynth + S;
    }

    // ---------------------------------------------------------------- pitch tracking & correction
    void onCycle (double midiCycle, double interval)
    {
        // smoothing in time constants of seconds, applied per cycle
        auto alpha = [&] (double tau) { return 1.0 - std::exp (-interval / (tau * sr)); };
        if (! haveTrack) { fast = slow = mid = midiCycle; haveTrack = true; }
        fast += alpha (0.008) * (midiCycle - fast);   // removes cycle-to-cycle jitter only
        mid  += alpha (0.035) * (midiCycle - mid);    // note decisions (vibrato mostly averaged out)
        slow += alpha (0.09) * (midiCycle - slow);    // centre of the note (vibrato averaged out)
        heardMidi = fast;

        // target note with hysteresis
        const int before = current;
        if (current < 0 || ! allowed (current) || std::abs (mid - current) > 0.62)
        {
            const int r = (int) std::lround (mid);
            int bestN = r;
            double bestD = 1.0e9;
            for (int nn = r - 3; nn <= r + 3; ++nn)
                if (allowed (nn) && std::abs (mid - nn) < bestD) { bestD = std::abs (mid - nn); bestN = nn; }
            current = bestN;
        }
        if (current != before) slow = fast;           // new note: start averaging afresh (no overshoot)
        centreTarget = (current - slow) * amount;
        flattenTarget = (slow - fast) * amount * flattenAmount;
        holdSamples = (int) (0.06 * sr);
    }

    bool allowed (int nn) const
    {
        static const bool masks[6][12] = {
            { 1,1,1,1,1,1,1,1,1,1,1,1 },
            { 1,0,1,0,1,1,0,1,0,1,0,1 },
            { 1,0,1,1,0,1,0,1,1,0,1,0 },
            { 1,0,1,0,1,0,0,1,0,1,0,0 },
            { 1,0,0,1,0,1,0,1,0,0,1,0 },
            { 1,0,1,1,0,1,0,1,1,0,0,1 } };
        const int pc = ((nn - key) % 12 + 12) % 12;
        return masks[std::clamp (scale, 0, 5)][pc];
    }

    // ---------------------------------------------------------------- detector (YIN, decimated)
    void pushDetector (float x)
    {
        lp1 += 0.45f * (x - lp1);
        lp2 += 0.45f * (lp1 - lp2);
        dacc += lp2;
        if (++dcount < decim) return;
        const float v = dacc / (float) decim;
        dacc = 0; dcount = 0;
        const size_t half = dbuf.size() / 2;
        dbuf[dpos] = v; dbuf[dpos + half] = v;
        dpos = (dpos + 1) % half;
        if (++hopCount < hop) return;
        hopCount = 0;
        analyse();
    }

    void analyse()
    {
        const size_t half = dbuf.size() / 2;
        const int len = N + tauMax;
        const float* f = &dbuf[(dpos + half - (size_t) len) % half];
        float energy = 0;
        for (int i = 0; i < N; ++i) energy += f[i] * f[i];
        float freq = 0;
        if (energy / (float) N > 1.0e-6f)
        {
            diff[0] = 1;
            double running = 0;
            for (int tau = 1; tau <= tauMax; ++tau)
            {
                double s = 0;
                for (int j = 0; j < N; ++j) { const float d = f[j] - f[j + tau]; s += d * d; }
                running += s;
                diff[(size_t) tau] = running > 0 ? (float) (s * tau / running) : 1.0f;
            }
            int best = -1;
            for (int tau = tauMin; tau < tauMax; ++tau)
                if (diff[(size_t) tau] < 0.2f)
                {
                    while (tau + 1 < tauMax && diff[(size_t) (tau + 1)] < diff[(size_t) tau]) ++tau;
                    best = tau;
                    break;
                }
            if (best > 0)
            {
                double b = best;
                if (best > 1 && best < tauMax)
                {
                    const float a = diff[(size_t) best - 1], c = diff[(size_t) best], e = diff[(size_t) best + 1];
                    const float den = a - 2 * c + e;
                    if (std::abs (den) > 1e-9f) b += 0.5 * (a - e) / den;
                }
                freq = (float) (dsr / b);
            }
        }
        history[0] = history[1]; history[1] = history[2]; history[2] = freq;
        float sorted[3] = { history[0], history[1], history[2] };
        std::sort (sorted, sorted + 3);
        const float est = (history[2] > 0 && sorted[1] > 0) ? sorted[1] : history[2];

        if (est > 0)
        {
            double newT = sr / est;
            // octave-jump guard: while locked, ignore a sudden 2x / 0.5x period unless it persists
            if (voicedDet)
            {
                const double r = newT / T;
                if ((std::abs (r - 2.0) < 0.15 || std::abs (r - 0.5) < 0.06) && ++octaveVotes < 6) newT = T;
                else if (! (std::abs (r - 2.0) < 0.15 || std::abs (r - 0.5) < 0.06)) octaveVotes = 0;
            }
            if (! voicedDet) Tmark = newT;
            T = newT;
            unvoicedHops = 0;
            voicedDet = true;
        }
        else if (++unvoicedHops > 3)
        {
            if (voicedDet) { voicedDet = false; haveTrack = false; current = -1; }
        }
    }

    struct Grain { bool active = false; double center = 0, start = 0, h = 1; float gain = 1; };

    double sr = 44100, dsr = 22050, T = 300, Tmark = 300, maxT = 700, unvoicedP = 176, nextSynth = 0, lastMark = 0;
    int D = 2000, decim = 2, tauMin = 20, tauMax = 340, N = 700, hop = 64;
    int64_t mask = 0, t = 0;
    std::vector<float> ring[2], mono, dbuf, diff;
    std::vector<double> marks;
    size_t markHead = 0, markCount = 0, dpos = 0;
    float dacc = 0, lp1 = 0, lp2 = 0, fadeIn = 0;
    int dcount = 0, hopCount = 0, unvoicedHops = 99, octaveVotes = 0, holdSamples = 0;
    float history[3] {};
    Grain grains[16];

    bool voicedDet = false, haveTrack = false;
    int current = -1, key = 0, scale = 0;
    double fast = 0, mid = 0, slow = 0, heardMidi = 0, shift = 0;
    double centreShift = 0, centreTarget = 0, flattenShift = 0, flattenTarget = 0;
    float amount = 1, natural = 0, speedCoef = 0, flattenAmount = 1;
    static constexpr double minHz = 70.0, maxHz = 1100.0;
};
} // namespace tune
