#pragma once
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>
#include <algorithm>

namespace wv
{

// Circular delay buffer. Call read() before push(); delay must be >= 1 sample.
struct DelayBuffer
{
    std::vector<float> data;
    int mask = 0, w = 0;

    void prepare (int maxDelay)
    {
        int s = 1;
        while (s < maxDelay + 4) s <<= 1;
        data.assign ((size_t) s, 0.0f);
        mask = s - 1;
        w = 0;
    }

    float read (float delay) const
    {
        float rp = (float) w - delay;
        while (rp < 0.0f) rp += (float) (mask + 1);
        const int i0 = (int) rp;
        const float fr = rp - (float) i0;
        const float a = data[(size_t) (i0 & mask)];
        const float b = data[(size_t) ((i0 + 1) & mask)];
        return a + fr * (b - a);
    }

    void push (float x) { data[(size_t) w] = x; w = (w + 1) & mask; }
};

// YIN pitch detector running on a decimated mono signal.
class PitchDetector
{
public:
    void prepare (double sampleRate)
    {
        decim  = std::max (1, (int) std::round (sampleRate / 22050.0));
        asr    = sampleRate / decim;
        tauMin = std::max (2, (int) (asr / 1200.0));
        tauMax = (int) (asr / 70.0);
        bufSize = 1;
        while (bufSize < N + tauMax + 2) bufSize <<= 1;
        buf.assign ((size_t) bufSize, 0.0f);
        frame.assign ((size_t) (N + tauMax + 1), 0.0f);
        d.assign ((size_t) (tauMax + 2), 1.0f);
        writePos = decimCount = hopCount = 0;
        acc = 0.0f;
        freq = 0.0f;
    }

    // Returns true when a fresh estimate is available.
    bool push (float x)
    {
        acc += x;
        if (++decimCount < decim) return false;
        buf[(size_t) writePos] = acc / (float) decim;
        acc = 0.0f;
        decimCount = 0;
        writePos = (writePos + 1) & (bufSize - 1);
        if (++hopCount < hop) return false;
        hopCount = 0;
        analyse();
        return true;
    }

    float getFrequency() const { return freq; } // 0 = unvoiced / silence

private:
    void analyse()
    {
        const int len = N + tauMax;
        const int start = (writePos - len + bufSize * 4) & (bufSize - 1);
        float energy = 0.0f;
        for (int i = 0; i < len; ++i)
            frame[(size_t) i] = buf[(size_t) ((start + i) & (bufSize - 1))];
        for (int i = 0; i < N; ++i)
            energy += frame[(size_t) i] * frame[(size_t) i];

        if (energy / (float) N < 1.0e-5f) { freq = 0.0f; return; } // below ~ -50 dB

        d[0] = 1.0f;
        float running = 0.0f;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            float sum = 0.0f;
            for (int j = 0; j < N; ++j)
            {
                const float diff = frame[(size_t) j] - frame[(size_t) (j + tau)];
                sum += diff * diff;
            }
            running += sum;
            d[(size_t) tau] = running > 0.0f ? sum * (float) tau / running : 1.0f;
        }

        int best = -1;
        for (int tau = tauMin; tau <= tauMax; ++tau)
        {
            if (d[(size_t) tau] < threshold)
            {
                while (tau + 1 <= tauMax && d[(size_t) (tau + 1)] < d[(size_t) tau]) ++tau;
                best = tau;
                break;
            }
        }
        if (best < 0) { freq = 0.0f; return; }

        float b = (float) best;
        if (best > tauMin && best < tauMax)
        {
            const float a = d[(size_t) (best - 1)], c = d[(size_t) best], e = d[(size_t) (best + 1)];
            const float den = a - 2.0f * c + e;
            if (std::abs (den) > 1.0e-9f) b += 0.5f * (a - e) / den;
        }
        freq = (float) (asr / b);
        if (freq < 70.0f || freq > 1200.0f) freq = 0.0f;
    }

    int N = 512, hop = 128;
    float threshold = 0.15f;
    int decim = 2, tauMin = 20, tauMax = 300, bufSize = 1024;
    double asr = 22050.0;
    std::vector<float> buf, frame, d;
    int writePos = 0, decimCount = 0, hopCount = 0;
    float acc = 0.0f, freq = 0.0f;
};

// Two-tap crossfading delay-line pitch shifter (constant latency).
class PitchShifter
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        W = (int) (sampleRate * 0.03);
        lines.resize ((size_t) numChannels);
        for (auto& l : lines) l.prepare (W + minDelay + 8);
        p = 0.0f;
        ratio = 1.0f;
    }

    int getLatency() const { return W / 2 + minDelay; }
    void setRatio (float r) { ratio = r; }

    void process (float* frame, int numChannels)
    {
        const float d1 = p * (float) W + (float) minDelay;
        float p2 = p + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;
        const float d2 = p2 * (float) W + (float) minDelay;
        const float s = std::sin (juce::MathConstants<float>::pi * p);
        const float g1 = s * s, g2 = 1.0f - g1;

        for (int c = 0; c < numChannels && c < (int) lines.size(); ++c)
        {
            auto& l = lines[(size_t) c];
            const float out = l.read (d1) * g1 + l.read (d2) * g2;
            l.push (frame[c]);
            frame[c] = out;
        }

        if (std::abs (ratio - 1.0f) < 1.0e-4f)
        {
            // No shift needed: glide back to the single-tap resting position.
            const float step = 0.003f / (float) W;
            if (p > 0.5f) { p += step; if (p >= 1.0f) p = 0.0f; }
            else          { p -= step; if (p < 0.0f)  p = 0.0f; }
        }
        else
        {
            p += (1.0f - ratio) / (float) W;
            p -= std::floor (p);
        }
    }

private:
    std::vector<DelayBuffer> lines;
    int W = 1440, minDelay = 32;
    float p = 0.0f, ratio = 1.0f;
};

// Detect -> snap to key/scale -> smooth -> shift.
class PitchCorrector
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        sr = sampleRate;
        detector.prepare (sampleRate);
        shifter.prepare (sampleRate, numChannels);
        smoothed = target = 0.0f;
        current = -1;
    }

    int getLatency() const { return shifter.getLatency(); }

    void setParams (float amountIn, float retuneMs, int keyIn, int scaleIn)
    {
        amount = amountIn;
        key = keyIn;
        scale = scaleIn;
        coef = retuneMs <= 0.0f ? 0.0f : (float) std::exp (-1.0 / (retuneMs * 0.001 * sr));
    }

    void processFrame (float* frame, int numChannels)
    {
        float mono = 0.0f;
        for (int c = 0; c < numChannels; ++c) mono += frame[c];
        mono /= (float) numChannels;

        if (detector.push (mono)) updateTarget();

        smoothed = target + (smoothed - target) * coef;
        shifter.setRatio (std::exp2 (smoothed / 12.0f));
        shifter.process (frame, numChannels);
    }

private:
    bool allowed (int n) const
    {
        static const bool masks[3][12] = {
            { 1,1,1,1,1,1,1,1,1,1,1,1 },   // chromatic
            { 1,0,1,0,1,1,0,1,0,1,0,1 },   // major
            { 1,0,1,1,0,1,0,1,1,0,1,0 } }; // minor
        const int pc = ((n - key) % 12 + 12) % 12;
        return masks[juce::jlimit (0, 2, scale)][pc];
    }

    int snap (float midi) const
    {
        if (current >= 0 && allowed (current) && std::abs (midi - (float) current) < 0.65f)
            return current;
        const int r = (int) std::lround (midi);
        int best = r;
        float bestD = 1.0e9f;
        for (int n = r - 3; n <= r + 3; ++n)
            if (allowed (n))
            {
                const float dist = std::abs (midi - (float) n);
                if (dist < bestD) { bestD = dist; best = n; }
            }
        return best;
    }

    void updateTarget()
    {
        const float f = detector.getFrequency();
        if (f <= 0.0f || amount <= 0.0f) { target = 0.0f; current = -1; return; }
        const float midi = 69.0f + 12.0f * std::log2 (f / 440.0f);
        current = snap (midi);
        target = juce::jlimit (-3.0f, 3.0f, ((float) current - midi) * amount);
    }

    PitchDetector detector;
    PitchShifter shifter;
    double sr = 44100.0;
    float amount = 0.5f, coef = 0.0f, smoothed = 0.0f, target = 0.0f;
    int key = 0, scale = 0, current = -1;
};

// RBJ-cookbook biquad, up to 2 channels, allocation-free coefficient updates.
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1[2] {}, z2[2] {};

    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0.0f; }

    float process (float x, int ch)
    {
        const float y = b0 * x + z1[ch];
        z1[ch] = b1 * x - a1 * y + z2[ch];
        z2[ch] = b2 * x - a2 * y;
        return y;
    }

    void set (double nb0, double nb1, double nb2, double na0, double na1, double na2)
    {
        b0 = (float) (nb0 / na0); b1 = (float) (nb1 / na0); b2 = (float) (nb2 / na0);
        a1 = (float) (na1 / na0); a2 = (float) (na2 / na0);
    }

    struct W { double cs, sn; };
    static W w (double sr, double f)
    {
        const double w0 = 2.0 * juce::MathConstants<double>::pi * juce::jlimit (10.0, sr * 0.45, f) / sr;
        return { std::cos (w0), std::sin (w0) };
    }

    void highpass (double sr, double f, double q = 0.707)
    {
        auto [cs, sn] = w (sr, f); const double al = sn / (2 * q);
        set ((1 + cs) / 2, -(1 + cs), (1 + cs) / 2, 1 + al, -2 * cs, 1 - al);
    }
    void lowpass (double sr, double f, double q = 0.707)
    {
        auto [cs, sn] = w (sr, f); const double al = sn / (2 * q);
        set ((1 - cs) / 2, 1 - cs, (1 - cs) / 2, 1 + al, -2 * cs, 1 - al);
    }
    void peak (double sr, double f, double q, double db)
    {
        auto [cs, sn] = w (sr, f); const double al = sn / (2 * q), A = std::pow (10.0, db / 40.0);
        set (1 + al * A, -2 * cs, 1 - al * A, 1 + al / A, -2 * cs, 1 - al / A);
    }
    void lowShelf (double sr, double f, double db)
    {
        auto [cs, sn] = w (sr, f); const double A = std::pow (10.0, db / 40.0), k = 2 * std::sqrt (A) * sn / 2 * std::sqrt (2.0);
        set (A * ((A + 1) - (A - 1) * cs + k), 2 * A * ((A - 1) - (A + 1) * cs), A * ((A + 1) - (A - 1) * cs - k),
             (A + 1) + (A - 1) * cs + k, -2 * ((A - 1) + (A + 1) * cs), (A + 1) + (A - 1) * cs - k);
    }
    void highShelf (double sr, double f, double db)
    {
        auto [cs, sn] = w (sr, f); const double A = std::pow (10.0, db / 40.0), k = 2 * std::sqrt (A) * sn / 2 * std::sqrt (2.0);
        set (A * ((A + 1) + (A - 1) * cs + k), -2 * A * ((A - 1) + (A + 1) * cs), A * ((A + 1) + (A - 1) * cs - k),
             (A + 1) - (A - 1) * cs + k, 2 * ((A - 1) - (A + 1) * cs), (A + 1) - (A - 1) * cs - k);
    }
};

} // namespace wv
