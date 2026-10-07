#pragma once
#include "DSP.h"
#include <cstdint>

namespace wc
{
// Power-of-two multichannel ring buffer addressed by absolute sample index.
struct Ring
{
    std::vector<float> data[2];
    int64_t mask = 0;

    void prepare (int minSize)
    {
        int64_t s = 1;
        while (s < minSize) s <<= 1;
        for (auto& d : data) d.assign ((size_t) s, 0.0f);
        mask = s - 1;
    }
    void write (int ch, int64_t idx, float x) { data[ch][(size_t) (idx & mask)] = x; }
    float at (int ch, int64_t idx) const { return data[ch][(size_t) (idx & mask)]; }
    float read (int ch, double pos) const
    {
        const double fl = std::floor (pos);
        const int64_t i = (int64_t) fl;
        const float fr = (float) (pos - fl);
        const float a = at (ch, i), b = at (ch, i + 1);
        return a + fr * (b - a);
    }
};

// ---------------------------------------------------------------------------
// Vocal bender: pitch-synchronous overlap-add (PSOLA). Grain spacing sets the
// pitch, grain playback speed sets the formants, so the two move independently.
class Bender
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        D = (int) (0.04 * sr);
        ring.prepare (D * 4 + (int) (sr / 70.0) * 8 + 64);
        detector.prepare (sr);
        t = 0;
        T = sr / 150.0;
        aMark = 0.0;
        nextMark = 0.0;
        for (auto& g : grains) g.active = false;
    }

    int getLatency() const { return D; }

    void setParams (float pitchSt, float formantSt, float mixIn, bool robotIn, int rootKey)
    {
        pitch = pitchSt;
        ratio = std::exp2 (pitchSt / 12.0);
        F = std::exp2 (formantSt / 12.0);
        mix = mixIn;
        robot = robotIn;
        key = rootKey;
    }

    void processFrame (float* fr, int nc, bool enabled)
    {
        float mono = 0.0f;
        for (int c = 0; c < nc; ++c) { ring.write (c, t, fr[c]); mono += fr[c]; }

        if (detector.push (mono / (float) nc))
        {
            const float f = detector.getFrequency();
            voicedFreq = f;
            if (f > 0.0f) T += (sr / f - T) * 0.5;
        }

        if (! enabled)
        {
            for (int c = 0; c < nc; ++c) fr[c] = ring.at (c, t - D);
            for (auto& g : grains) g.active = false;
            nextMark = (double) t;
            aMark = (double) (t - D);
            ++t;
            return;
        }

        if ((double) t >= nextMark) spawnGrain();

        for (int c = 0; c < nc; ++c)
        {
            float wet = 0.0f;
            for (auto& g : grains)
            {
                if (! g.active) continue;
                const double j = (double) (t - g.start);
                const float w = 0.5f - 0.5f * std::cos ((float) (juce::MathConstants<double>::pi * j / g.h));
                wet += g.scale * w * ring.read (c, g.center + (j - g.h) * F);
            }
            const float dry = ring.at (c, t - D);
            fr[c] = dry + mix * (wet - dry);
        }

        for (auto& g : grains)
            if (g.active && (double) (t - g.start) >= 2.0 * g.h - 1.0) g.active = false;
        ++t;
    }

private:
    void spawnGrain()
    {
        const double tIn = (double) (t - D);
        if (aMark > tIn || aMark < tIn - 4.0 * T) aMark = tIn;
        while (aMark + T <= tIn) aMark += T;

        double tOut = T / ratio;
        if (robot)
        {
            const float f = voicedFreq > 0.0f ? voicedFreq : 150.0f;
            const double midi = 69.0 + 12.0 * std::log2 (f / 440.0);
            const double n = key + 12.0 * std::round ((midi - key) / 12.0) + pitch;
            tOut = sr / (440.0 * std::exp2 ((n - 69.0) / 12.0));
        }
        // Each grain spans two input periods (one each side), resampled by F for the formant shift
        double h = juce::jlimit (16.0, sr / 40.0, T / F);
        if (h * F > D - 2) h = (D - 2) / F;
        const float scale = (float) (juce::jmin (2.0, tOut / h) * juce::jlimit (0.5, 2.0, T / tOut));   // keeps loudness steady

        for (auto& g : grains)
            if (! g.active)
            {
                g = { true, aMark, t, h, scale };
                break;
            }
        nextMark = (double) t + tOut;
    }

    struct Grain { bool active = false; double center = 0; int64_t start = 0; double h = 1; float scale = 1; };

    wv::PitchDetector detector;
    Ring ring;
    Grain grains[10];
    double sr = 44100, T = 300, aMark = 0, nextMark = 0, ratio = 1, F = 1;
    float pitch = 0, mix = 1, voicedFreq = 0;
    bool robot = false;
    int key = 0, D = 1764;
    int64_t t = 0;
};

// ---------------------------------------------------------------------------
// Tempo-synced stutter / beat repeat.
class Stutter
{
public:
    void prepare (double sampleRate) { sr = sampleRate; ring.prepare ((int) (sr * 10)); w = 0; lastWin = -1.0e9; n = 0; }

    // intervalBeats 0 = always repeating
    void process (float* fr, int nc, double ppq, double bpm, bool enabled,
                  double intervalBeats, double lengthBeats, double sliceBeats,
                  float dropSt, float decay, float mix)
    {
        for (int c = 0; c < nc; ++c) ring.write (c, w, fr[c]);

        bool active = false;
        double win = 0.0;
        if (enabled)
        {
            const double L = intervalBeats > 0.0 ? juce::jmin (lengthBeats, intervalBeats) : lengthBeats;
            if (intervalBeats <= 0.0) { active = true; win = std::floor (ppq / L); }
            else
            {
                const double ph = ppq - intervalBeats * std::floor (ppq / intervalBeats);
                active = ph >= intervalBeats - L;
                win = std::floor (ppq / intervalBeats);
            }
        }

        if (active)
        {
            if (win != lastWin) { lastWin = win; capStart = w; n = 0; }
            const double sl = juce::jmax (64.0, sliceBeats * 60.0 / bpm * sr);
            const double k = std::floor ((double) n / sl);
            if (k >= 1.0)
            {
                const double o = (double) n - k * sl;
                const double rate = std::exp2 (-dropSt * k / 12.0);
                const float g = std::pow (1.0f - decay * 0.6f, (float) k);
                const double fadeN = juce::jmin (sr * 0.003, sl * 0.25);
                const float env = (float) juce::jmin (1.0, o / fadeN, (sl - o) / fadeN);
                for (int c = 0; c < nc; ++c)
                {
                    const float wet = ring.read (c, (double) capStart + o * rate) * g * env;
                    fr[c] += mix * (wet - fr[c]);
                }
            }
            ++n;
        }
        else lastWin = -1.0e9;
        ++w;
    }

private:
    Ring ring;
    double sr = 44100, lastWin = -1.0e9;
    int64_t w = 0, capStart = 0, n = 0;
};

// ---------------------------------------------------------------------------
// Tape stop: while engaged, playback slows to a halt.
class TapeStop
{
public:
    void prepare (double sampleRate) { sr = sampleRate; ring.prepare ((int) (sr * 12)); w = 0; engaged = false; fadeIn = 1.0f; }

    void process (float* fr, int nc, bool engage, float timeMs)
    {
        for (int c = 0; c < nc; ++c) ring.write (c, w, fr[c]);
        if (engage)
        {
            if (! engaged) { engaged = true; readPos = (double) w; speed = 1.0; }
            speed = juce::jmax (0.0, speed - 1.0 / (timeMs * 0.001 * sr));
            readPos += speed * speed;
            const float amp = (float) juce::jmin (1.0, speed * 6.0);
            for (int c = 0; c < nc; ++c) fr[c] = ring.read (c, readPos - 1.0) * amp;
        }
        else
        {
            if (engaged) { engaged = false; fadeIn = 0.0f; }
            if (fadeIn < 1.0f)
            {
                fadeIn = juce::jmin (1.0f, fadeIn + 1.0f / (float) (0.008 * sr));
                for (int c = 0; c < nc; ++c) fr[c] *= fadeIn;
            }
        }
        ++w;
    }

private:
    Ring ring;
    double sr = 44100, readPos = 0, speed = 1;
    int64_t w = 0;
    bool engaged = false;
    float fadeIn = 1.0f;
};

// ---------------------------------------------------------------------------
// 16-step trance gate.
class Gate
{
public:
    static juce::StringArray patternNames() { return { "1/16 Chop", "Offbeat", "Trance 1", "Trance 2", "Build Up", "Half Time" }; }

    void prepare (double sampleRate) { coef = std::exp (-1.0f / (0.002f * (float) sampleRate)); env = 1.0f; }

    float gain (double ppq, int pattern, double stepBeats, float duty, float depth)
    {
        static const char* pats[] = { "xxxxxxxxxxxxxxxx", "..x...x...x...x.", "x.xx.xx.x.xx.xx.",
                                      "x..x..x.x..x..x.", "x...x...x.x.xxxx", "x.......x......." };
        const double sp = ppq / stepBeats;
        const double fl = std::floor (sp);
        const int step = (int) (((int64_t) fl % 16 + 16) % 16);
        const bool open = pats[juce::jlimit (0, 5, pattern)][step] == 'x' && (sp - fl) < duty;
        env = (open ? 1.0f : 0.0f) + coef * (env - (open ? 1.0f : 0.0f));
        return 1.0f - depth * (1.0f - env);
    }

private:
    float coef = 0.99f, env = 1.0f;
};

// ---------------------------------------------------------------------------
// Resonant state-variable filter with a tempo-synced sweep.
class SweepFilter
{
public:
    void prepare (double sampleRate) { sr = sampleRate; for (auto& s : ic) s[0] = s[1] = 0.0f; }

    void process (float* fr, int nc, double ppq, int type, float cutoff, float reso,
                  double lfoBeats, float lfoDepth)
    {
        const double ph = ppq / lfoBeats;
        const float lfo = std::sin ((float) (juce::MathConstants<double>::twoPi * (ph - std::floor (ph))));
        const float fc = juce::jlimit (20.0f, (float) (sr * 0.45), cutoff * std::exp2 (lfoDepth * 3.0f * lfo));
        const float g = std::tan (juce::MathConstants<float>::pi * fc / (float) sr);
        const float k = 1.0f / (0.5f + reso * 9.5f);
        const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;

        for (int c = 0; c < nc; ++c)
        {
            const float x = fr[c];
            const float v3 = x - ic[c][1];
            const float v1 = a1 * ic[c][0] + a2 * v3;
            const float v2 = ic[c][1] + a2 * ic[c][0] + a3 * v3;
            ic[c][0] = 2.0f * v1 - ic[c][0];
            ic[c][1] = 2.0f * v2 - ic[c][1];
            fr[c] = type == 0 ? v2 : type == 1 ? x - k * v1 - v2 : v1 * k * 2.0f;
        }
    }

private:
    double sr = 44100;
    float ic[2][2] {};
};

} // namespace wc
