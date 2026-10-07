#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>

// Pitch correction using pitch-synchronous overlap-add (TD-PSOLA).
// Each output grain is one cycle of the real voice, so there is no comb/chorus smear,
// formants stay put, and when no correction is needed the dry (delayed) signal passes untouched.
namespace tune
{
inline const juce::StringArray scaleNames { "Chromatic", "Major", "Minor", "Major Pentatonic", "Minor Pentatonic", "Harmonic Minor" };

class Tuner
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        D = (int) std::lround (0.025 * sr);                 // fixed latency, reported to the host
        maxT = (int) std::ceil (sr / minHz);
        int size = 1;
        while (size < D * 4 + maxT * 8 + 4096) size <<= 1;
        mask = size - 1;
        for (auto& r : ring) r.assign ((size_t) size, 0.0f);
        mono.assign ((size_t) size, 0.0f);

        decim = std::max (1, (int) std::lround (sr / 22050.0));
        dsr = sr / decim;
        tauMin = std::max (2, (int) (dsr / maxHz));
        tauMax = (int) (dsr / minHz) + 2;
        N = std::max (512, (int) (dsr * 0.032));
        dbuf.assign ((size_t) (N + tauMax + 8) * 2, 0.0f);
        dpos = 0; dacc = 0; dcount = 0; hopCount = 0;
        diff.assign ((size_t) tauMax + 2, 0.0f);
        lp1 = lp2 = 0;

        t = 0;
        T = sr / 150.0;
        lastMark = 0; Tmark = T; midiAcc = 0;
        nextSynth = 0;
        voiced = false; unvoicedHops = 99;
        midiNow = midiSlow = 0; haveSlow = false;
        current = -1; shift = 0; shiftTarget = 0; wet = 0;
        for (auto& g : grains) g.active = false;
        history[0] = history[1] = history[2] = 0;
    }

    int getLatency() const { return D; }

    // retuneMs: 0 = instant (hard tune). amount 0..1. natural 0..1 keeps vibrato/slides.
    void setParams (float retuneMs, float amountIn, int keyIn, int scaleIn, float naturalIn)
    {
        amount = amountIn; key = keyIn; scale = scaleIn; natural = naturalIn;
        speedCoef = retuneMs <= 0.5f ? 0.0f : (float) std::exp (-1.0 / (retuneMs * 0.001 * sr));
    }

    // readouts for the UI
    int detectedNote() const { return voiced ? (int) std::lround (midiNow) : -1; }
    float detectedCents() const { return voiced ? (float) ((midiNow - std::round (midiNow)) * 100.0) : 0.0f; }
    int targetNote() const { return voiced ? current : -1; }

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

            const int64_t tIn = t - D;
            if (! enabled)
            {
                for (int c = 0; c < nch; ++c) ch[c][i] = ring[(size_t) c][(size_t) (tIn & mask)];
                ++t;
                continue;
            }

            // correction amount, smoothed by the retune speed
            shift = shiftTarget + (shift - shiftTarget) * speedCoef;
            const double ratio = std::pow (2.0, shift / 12.0);
            const float wantWet = (voiced && amount > 0.0f && std::abs (shift) > 0.01) ? 1.0f : 0.0f;
            wet += (wantWet - wet) * wetCoef;

            advanceMarks (tIn);
            if (wet > 0.0005f || wantWet > 0)
            {
                if ((double) t >= nextSynth) spawnGrain (tIn, ratio);
            }
            else
            {
                for (auto& g : grains) g.active = false;
                nextSynth = (double) t;
            }

            for (int c = 0; c < nch; ++c)
            {
                const float dry = ring[(size_t) c][(size_t) (tIn & mask)];
                float out = dry;
                if (wet > 0.0005f)
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
                    out = dry + wet * (sum - dry);
                }
                ch[c][i] = out;
            }
            for (auto& g : grains) if (g.active && (double) t - g.start >= 2.0 * g.h) g.active = false;
            ++t;
        }
    }

private:
    float read (int c, double pos) const
    {
        const double fl = std::floor (pos);
        const int64_t i0 = (int64_t) fl;
        const float fr = (float) (pos - fl);
        const float a = ring[(size_t) c][(size_t) (i0 & mask)], b = ring[(size_t) c][(size_t) ((i0 + 1) & mask)];
        return a + fr * (b - a);
    }

    // ---- analysis marks: one per pitch period, aligned by waveform correlation so grains stay in phase
    void advanceMarks (int64_t tIn)
    {
        if (lastMark > (double) tIn || lastMark < (double) (tIn - 8 * maxT)) { lastMark = (double) tIn; Tmark = T; }
        const double period = voiced && std::abs (Tmark - T) < 0.15 * T ? Tmark : T;
        while (lastMark + period <= (double) tIn)
        {
            double next = lastMark + period;
            if (voiced)
            {
                next = refine (lastMark, next, period);
                const double interval = next - lastMark;
                if (std::abs (interval - T) < 0.15 * T) Tmark += 0.35 * (interval - Tmark);
                else Tmark = T;
                midiAcc = 69.0 + 12.0 * std::log2 (sr / Tmark / 440.0);
                updateShift();
            }
            lastMark = std::max (lastMark + period * 0.5, next);
        }
    }

    // correlation alignment against the previous mark; returns a sub-sample position
    double refine (double prev, double pred, double period)
    {
        const int L = std::max (32, (int) period);
        const int search = std::max (2, (int) (period / 5));
        const int64_t p0 = (int64_t) std::lround (prev), c0 = (int64_t) std::lround (pred);
        double sc[3] = { -1e30, -1e30, -1e30 }, best = -1.0e30;
        int bestK = 0;
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
        for (int k = -search; k <= search; ++k)
        {
            const double v = score (c0 + k);
            if (v > best) { best = v; bestK = k; }
        }
        sc[0] = score (c0 + bestK - 1); sc[1] = best; sc[2] = score (c0 + bestK + 1);
        double frac = 0;
        const double den = sc[0] - 2 * sc[1] + sc[2];
        if (std::abs (den) > 1e-12) frac = std::clamp (0.5 * (sc[0] - sc[2]) / den, -0.5, 0.5);
        return (double) (c0 + bestK) + frac + (prev - (double) p0);
    }

    void spawnGrain (int64_t tIn, double ratio)
    {
        juce::ignoreUnused (tIn);
        const double P = std::abs (Tmark - T) < 0.15 * T ? Tmark : T;
        const double S = P / ratio;                   // output spacing
        const double h = std::max (P, S);            // half-length: windows always overlap enough
        for (auto& g : grains)
            if (! g.active)
            {
                g = { true, lastMark, nextSynth, h, (float) (S / h) };
                break;
            }
        nextSynth = (nextSynth < (double) t - S) ? (double) t + S : nextSynth + S;   // keep the fraction
    }

    // ---- pitch detection (YIN on a decimated signal)
    void pushDetector (float x)
    {
        // gentle 2-pole low-pass before decimation
        lp1 += 0.45f * (x - lp1);
        lp2 += 0.45f * (lp1 - lp2);
        dacc += lp2;
        if (++dcount < decim) return;
        const float v = dacc / (float) decim;
        dacc = 0; dcount = 0;
        const size_t half = dbuf.size() / 2;
        dbuf[dpos] = v; dbuf[dpos + half] = v;    // mirrored buffer: contiguous reads
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
        if (energy / (float) N > 2.0e-6f)
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
                if (diff[(size_t) tau] < 0.15f)
                {
                    while (tau + 1 < tauMax && diff[(size_t) (tau + 1)] < diff[(size_t) tau]) ++tau;
                    best = tau;
                    break;
                }
            if (best < 0)
            {
                // no clear dip: accept the global minimum only if it's reasonably periodic
                float mn = 1;
                for (int tau = tauMin; tau < tauMax; ++tau) if (diff[(size_t) tau] < mn) { mn = diff[(size_t) tau]; best = tau; }
                if (mn > 0.3f) best = -1;
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

        // median of the last three estimates removes single-frame octave slips
        history[0] = history[1]; history[1] = history[2]; history[2] = freq;
        float sorted[3] = { history[0], history[1], history[2] };
        std::sort (sorted, sorted + 3);
        const float est = (history[2] > 0 && sorted[1] > 0) ? sorted[1] : history[2];

        if (est > 0)
        {
            unvoicedHops = 0;
            voiced = true;
            const double newT = sr / est;
            T = (std::abs (newT - T) / T > 0.12) ? newT : T + 0.6 * (newT - T);
            midiNow = 69.0 + 12.0 * std::log2 (est / 440.0);
            if (! haveSlow) { midiSlow = midiNow; haveSlow = true; }
            midiSlow += (midiNow - midiSlow) * slowCoef;
            updateTarget();
        }
        else if (++unvoicedHops > 3)
        {
            voiced = false;
            haveSlow = false;
            shiftTarget = 0;
            current = -1;
        }
    }

    bool allowed (int n) const
    {
        static const bool masks[6][12] = {
            { 1,1,1,1,1,1,1,1,1,1,1,1 },
            { 1,0,1,0,1,1,0,1,0,1,0,1 },
            { 1,0,1,1,0,1,0,1,1,0,1,0 },
            { 1,0,1,0,1,0,0,1,0,1,0,0 },
            { 1,0,0,1,0,1,0,1,0,0,1,0 },
            { 1,0,1,1,0,1,0,1,1,0,0,1 } };
        const int pc = ((n - key) % 12 + 12) % 12;
        return masks[std::clamp (scale, 0, 5)][pc];
    }

    void updateShift()
    {
        if (current < 0) return;
        const double basis = midiAcc + natural * (midiSlow - midiNow);
        shiftTarget = std::clamp ((current - basis) * amount, -6.0, 6.0);
    }

    void updateTarget()
    {
        // what pitch are we correcting? natural mode follows the slow average so vibrato survives
        const double basis = midiNow + natural * (midiSlow - midiNow);
        if (current >= 0 && allowed (current) && std::abs (basis - current) < 0.62) {}
        else
        {
            const int r = (int) std::lround (basis);
            int best = r;
            double bestD = 1.0e9;
            for (int n = r - 3; n <= r + 3; ++n)
                if (allowed (n) && std::abs (basis - n) < bestD) { bestD = std::abs (basis - n); best = n; }
            current = best;
        }
        if (std::abs (midiAcc - midiNow) > 0.5) midiAcc = midiNow;   // precise pitch not locked yet
        updateShift();
    }

    struct Grain { bool active = false; double center = 0, start = 0, h = 1; float gain = 1; };

    double sr = 44100, dsr = 22050, T = 300, nextSynth = 0;
    int D = 1100, maxT = 700, decim = 2, tauMin = 20, tauMax = 340, N = 700, hop = 64;
    int64_t mask = 0, t = 0;
    double lastMark = 0, Tmark = 300, midiAcc = 0;
    std::vector<float> ring[2], mono, dbuf, diff;
    size_t dpos = 0;
    float dacc = 0, lp1 = 0, lp2 = 0;
    int dcount = 0, hopCount = 0;
    float history[3] {};
    Grain grains[12];

    bool voiced = false, haveSlow = false;
    int unvoicedHops = 99, current = -1, key = 0, scale = 0;
    double midiNow = 0, midiSlow = 0, shift = 0, shiftTarget = 0;
    float amount = 1, natural = 0, speedCoef = 0, wet = 0;
    const float wetCoef = 0.004f;           // ~6 ms dry/wet fade
    const double slowCoef = 0.04;           // per 2.9 ms hop, ~2 Hz
    static constexpr double minHz = 65.0, maxHz = 1100.0;
};
} // namespace tune
