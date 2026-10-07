#include "Modules.h"
#include "DSP.h"
#include "Tuner.h"
#if WALL_CHOP
 #include "ChopDSP.h"
#endif
#include <complex>
#include <cmath>

namespace mods
{
// =====================================================================================  params
float ParamSpec::toValue (float norm) const
{
    norm = juce::jlimit (0.0f, 1.0f, norm);
    if (isChoice()) return (float) juce::roundToInt (norm * (float) (choices.size() - 1));
    if (centre > min && centre < max)
    {
        const float skew = std::log (0.5f) / std::log ((centre - min) / (max - min));
        return min + (max - min) * std::pow (norm, 1.0f / skew);
    }
    return min + (max - min) * norm;
}

float ParamSpec::toNorm (float value) const
{
    if (isChoice()) return choices.size() > 1 ? juce::jlimit (0.0f, 1.0f, value / (float) (choices.size() - 1)) : 0.0f;
    const float p = juce::jlimit (0.0f, 1.0f, (value - min) / (max - min));
    if (centre > min && centre < max)
    {
        const float skew = std::log (0.5f) / std::log ((centre - min) / (max - min));
        return std::pow (p, skew);
    }
    return p;
}

juce::String ParamSpec::text (float v) const
{
    if (isChoice()) return choices[juce::jlimit (0, choices.size() - 1, juce::roundToInt (v))];
    const juce::String u (unit);
    if (u == "%")  return juce::String (juce::roundToInt (v * 100.0f)) + "%";
    if (u == "dB") return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB";
    if (u == "st") return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " st";
    if (u == "Hz") return v >= 1000.0f ? juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
    if (u == "ms") return v < 10.0f ? juce::String (v, 1) + " ms" : juce::String (juce::roundToInt (v)) + " ms";
    if (u == "s")  return juce::String (v, 1) + " s";
    if (u == ":1") return juce::String (v, 1) + ":1";
    if (u == "cents") return juce::String (juce::roundToInt (v)) + " cents";
    if (max - min <= 20.0f) return juce::String (v, 1);
    return juce::String (juce::roundToInt (v));
}

// =====================================================================================  catalogue
static juce::StringArray keys() { return { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; }

const std::vector<ModuleInfo>& catalogue()
{
    static const std::vector<ModuleInfo> list = []
    {
        const juce::uint32 cPitch = 0xff9b5cff, cDyn = 0xffff5e9c, cEQ = 0xff4cc9f0, cColor = 0xffffb238, cSpace = 0xff39d98a;
       #if WALL_CHOP
        const juce::uint32 cChop = 0xff39ff14;
       #endif
        std::vector<ModuleInfo> l;
        l.push_back ({ "empty", "Empty", "", "", 0xff444444, MeterNone, {} });
        l.push_back ({ "tune", "Tune", "Pitch",
            "Pitch correction that shifts one voice cycle at a time, so it stays clean and natural. Speed 0 = hard tune. Natural keeps vibrato and slides.",
            cPitch, MeterPitch, {
                { "Speed", 0, 400, 30, 60, "ms", {} }, { "Amount", 0, 1, 1, 0, "%", {} },
                { "Key", 0, 11, 0, 0, "", keys() }, { "Scale", 0, 5, 0, 0, "", tune::scaleNames },
                { "Natural", 0, 1, 0.25f, 0, "%", {} } } });
        l.push_back ({ "deess", "De-Esser", "Dynamics", "Tames harsh S, T and SH sounds. Split only turns down the highs; Wide turns down the whole voice.",
            cDyn, MeterGR, {
                { "Frequency", 3000, 12000, 6500, 6000, "Hz", {} }, { "Threshold", -50, 0, -26, 0, "dB", {} },
                { "Range", 0, 18, 8, 0, "dB", {} }, { "Mode", 0, 1, 0, 0, "", { "Split", "Wide" } } } });
        l.push_back ({ "eq", "Clean EQ", "EQ", "Transparent EQ for clean-up and shaping: low cut, two sweepable mids and smooth shelves.",
            cEQ, MeterEQ, {
                { "Low Cut", 20, 500, 80, 100, "Hz", {} }, { "Low", -12, 12, 0, 0, "dB", {} },
                { "Low Mid Freq", 150, 1500, 350, 450, "Hz", {} }, { "Low Mid", -12, 12, 0, 0, "dB", {} },
                { "High Mid Freq", 1000, 8000, 3000, 2800, "Hz", {} }, { "High Mid", -12, 12, 0, 0, "dB", {} },
                { "High", -12, 12, 0, 0, "dB", {} }, { "Air", -12, 12, 0, 0, "dB", {} } } });
        l.push_back ({ "pultec", "Pult EQ", "EQ", "Passive tube program EQ (Pultec-style). Boost and cut the same low frequency for the famous tight-but-big low end; silky top boost.",
            cEQ, MeterEQ, {
                { "Low Freq", 0, 3, 2, 0, "", { "20 Hz", "30 Hz", "60 Hz", "100 Hz" } }, { "Low Boost", 0, 10, 0, 0, "x", {} },
                { "Low Atten", 0, 10, 0, 0, "x", {} }, { "High Freq", 0, 6, 4, 0, "", { "3 kHz", "4 kHz", "5 kHz", "8 kHz", "10 kHz", "12 kHz", "16 kHz" } },
                { "High Boost", 0, 10, 0, 0, "x", {} }, { "Bandwidth", 0, 10, 5, 0, "x", {} },
                { "Atten Freq", 0, 2, 2, 0, "", { "5 kHz", "10 kHz", "20 kHz" } }, { "High Atten", 0, 10, 0, 0, "x", {} } } });
        l.push_back ({ "tubeeq", "Tube EQ", "EQ", "Musical broad-stroke EQ with a tube stage. Great for warmth, presence and expensive-sounding air.",
            cEQ, MeterEQ, {
                { "Low", -10, 10, 0, 0, "dB", {} }, { "Mid Freq", 200, 5000, 1000, 1000, "Hz", {} },
                { "Mid", -10, 10, 0, 0, "dB", {} }, { "High", -10, 10, 0, 0, "dB", {} },
                { "Air", -10, 10, 0, 0, "dB", {} }, { "Drive", 0, 1, 0.25f, 0, "%", {} },
                { "Output", -12, 12, 0, 0, "dB", {} } } });
        l.push_back ({ "comp", "Compressor", "Dynamics", "Clean, precise compressor for controlling level without colour.",
            cDyn, MeterGR, {
                { "Threshold", -50, 0, -18, 0, "dB", {} }, { "Ratio", 1, 20, 4, 4, ":1", {} },
                { "Attack", 0.1f, 100, 5, 10, "ms", {} }, { "Release", 10, 1000, 120, 150, "ms", {} },
                { "Knee", 0, 12, 6, 0, "dB", {} }, { "Makeup", -6, 24, 0, 0, "dB", {} }, { "Mix", 0, 1, 1, 0, "%", {} } } });
        l.push_back ({ "fet", "FET 76", "Dynamics", "Fast, aggressive FET compressor (1176-style). Turn Input up for more compression; 7 = fastest. 'All' is the famous all-buttons smash.",
            cDyn, MeterGR, {
                { "Input", 0, 48, 12, 0, "dB", {} }, { "Output", -24, 24, 0, 0, "dB", {} },
                { "Attack", 1, 7, 3, 0, "x", {} }, { "Release", 1, 7, 5, 0, "x", {} },
                { "Ratio", 0, 4, 0, 0, "", { "4:1", "8:1", "12:1", "20:1", "All" } }, { "Mix", 0, 1, 1, 0, "%", {} } } });
        l.push_back ({ "opto", "Opto 2A", "Dynamics", "Smooth optical leveling amp (LA-2A-style). Gentle, program-dependent release that makes vocals sit forward.",
            cDyn, MeterGR, {
                { "Peak Reduction", 0, 100, 50, 0, "x", {} }, { "Gain", -12, 30, 4, 0, "dB", {} },
                { "Mode", 0, 1, 0, 0, "", { "Compress", "Limit" } }, { "Emphasis", 0, 1, 0.3f, 0, "%", {} },
                { "Mix", 0, 1, 1, 0, "%", {} } } });
        l.push_back ({ "varimu", "Vari-Mu", "Dynamics", "Tube variable-mu compressor (Fairchild/Manley-style). Ratio rises as you push it; thick, glued and warm.",
            cDyn, MeterGR, {
                { "Input", -12, 24, 6, 0, "dB", {} }, { "Threshold", -40, 0, -18, 0, "dB", {} },
                { "Time", 0, 5, 1, 0, "", { "1 Fast", "2", "3", "4 Slow", "5 Auto", "6 Auto Slow" } },
                { "Output", -24, 24, 0, 0, "dB", {} }, { "Mix", 0, 1, 1, 0, "%", {} } } });
        l.push_back ({ "sat", "Saturation", "Color", "Adds harmonics: Tape is warm and rounded, Tube adds rich even harmonics, Crunch is gritty clipping.",
            cColor, MeterNone, {
                { "Type", 0, 2, 0, 0, "", { "Tape", "Tube", "Crunch" } }, { "Drive", 0, 1, 0.3f, 0, "%", {} },
                { "Tone", -1, 1, 0, 0, "x", {} }, { "Mix", 0, 1, 1, 0, "%", {} }, { "Output", -12, 12, 0, 0, "dB", {} } } });
        l.push_back ({ "double", "Doubler", "Space", "Two slightly detuned, delayed copies spread left and right, like a double-tracked vocal.",
            cSpace, MeterNone, {
                { "Amount", 0, 1, 0.4f, 0, "%", {} }, { "Width", 0, 1, 1, 0, "%", {} },
                { "Detune", 0, 30, 9, 0, "cents", {} }, { "Delay", 5, 40, 16, 0, "ms", {} } } });
        l.push_back ({ "delay", "Delay", "Space", "Tempo-synced vocal delay with filtering, tape wobble and ducking (the echo gets out of the way while you sing).",
            cSpace, MeterNone, {
                { "Time", 0, 6, 1, 0, "", { "1/4", "1/8", "1/8 Dotted", "1/16", "1/4 Dotted", "1/2", "1/8 Triplet" } },
                { "Feedback", 0, 0.95f, 0.3f, 0, "%", {} }, { "Mix", 0, 1, 0.2f, 0, "%", {} },
                { "Mode", 0, 2, 0, 0, "", { "Ping-Pong", "Stereo", "Mono" } },
                { "Low Cut", 20, 1000, 250, 200, "Hz", {} }, { "High Cut", 1000, 20000, 6000, 5000, "Hz", {} },
                { "Ducking", 0, 1, 0.5f, 0, "%", {} }, { "Wow", 0, 1, 0.15f, 0, "%", {} } } });
        l.push_back ({ "reverb", "Reverb", "Space", "Dense, smooth algorithmic reverb. Plate for classic vocals, Hall for big worship/ballad space, Room and Chamber for natural depth.",
            cSpace, MeterNone, {
                { "Type", 0, 3, 0, 0, "", { "Plate", "Hall", "Room", "Chamber" } }, { "Decay", 0.2f, 10, 1.8f, 2, "s", {} },
                { "Pre-Delay", 0, 200, 25, 40, "ms", {} }, { "Size", 0, 1, 0.6f, 0, "%", {} },
                { "High Cut", 1000, 20000, 7000, 6000, "Hz", {} }, { "Low Cut", 20, 1000, 200, 200, "Hz", {} },
                { "Width", 0, 1, 1, 0, "%", {} }, { "Mix", 0, 1, 0.2f, 0, "%", {} } } });
       #if WALL_CHOP
        l.push_back ({ "bender", "Vocal Bender", "Chop", "Pitch and formant shifting (AlterBoy-style). Robot locks to a monotone in your key.",
            cChop, MeterNone, {
                { "Mode", 0, 1, 0, 0, "", { "Shift", "Robot" } }, { "Pitch", -12, 12, 0, 0, "st", {} },
                { "Formant", -12, 12, 0, 0, "st", {} }, { "Mix", 0, 1, 1, 0, "%", {} } } });
        l.push_back ({ "stutter", "Stutter", "Chop", "Tempo-synced beat repeat. Pitch Drop makes each repeat fall; press play in your DAW to hear it in time.",
            cChop, MeterNone, {
                { "Repeat", 0, 3, 2, 0, "", { "Always", "Every 1/2 Bar", "Every Bar", "Every 2 Bars" } },
                { "Length", 0, 4, 1, 0, "", { "1/8", "1/4", "1/2", "1 Bar", "2 Bars" } },
                { "Slice", 0, 5, 2, 0, "", { "1/4", "1/8", "1/16", "1/32", "1/8 T", "1/16 T" } },
                { "Pitch Drop", 0, 12, 0, 0, "st", {} }, { "Decay", 0, 1, 0, 0, "%", {} }, { "Mix", 0, 1, 1, 0, "%", {} } } });
        l.push_back ({ "gate", "Trance Gate", "Chop", "16-step rhythmic volume chopping synced to your song.",
            cChop, MeterNone, {
                { "Pattern", 0, 5, 2, 0, "", wc::Gate::patternNames() }, { "Rate", 0, 2, 1, 0, "", { "1/8", "1/16", "1/32" } },
                { "Length", 0.1f, 1, 0.6f, 0, "%", {} }, { "Depth", 0, 1, 1, 0, "%", {} } } });
        l.push_back ({ "filter", "Filter", "Chop", "Resonant filter with a tempo-synced sweep for builds and wobbles.",
            cChop, MeterNone, {
                { "Type", 0, 2, 0, 0, "", { "Low Pass", "High Pass", "Band Pass" } }, { "Cutoff", 20, 20000, 2000, 1000, "Hz", {} },
                { "Resonance", 0, 1, 0.3f, 0, "%", {} }, { "Sweep Rate", 0, 4, 0, 0, "", { "1 Bar", "1/2", "1/4", "1/8", "1/16" } },
                { "Sweep", 0, 1, 0, 0, "%", {} } } });
        l.push_back ({ "tapestop", "Tape Stop", "Chop", "Switch the module ON and the vocal slows to a stop. Automate its power button right before a drop.",
            cChop, MeterNone, { { "Stop Time", 100, 3000, 900, 800, "ms", {} } } });
       #endif
        return l;
    }();
    return list;
}

int indexOf (const juce::String& id)
{
    const auto& c = catalogue();
    for (int i = 0; i < (int) c.size(); ++i) if (id == c[(size_t) i].id) return i;
    return -1;
}

juce::StringArray typeNames()
{
    juce::StringArray s;
    for (auto& m : catalogue()) s.add (m.name);
    return s;
}

// =====================================================================================  helpers
static inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
static inline float gainToDb (float g) { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }
static inline float coefMs (double ms, double sr) { return ms <= 0.0 ? 0.0f : (float) std::exp (-1.0 / (ms * 0.001 * sr)); }

struct BQ
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1[2] {}, z2[2] {};

    float process (float x, int c)
    {
        const double y = b0 * x + z1[c];
        z1[c] = b1 * x - a1 * y + z2[c];
        z2[c] = b2 * x - a2 * y;
        return (float) y;
    }
    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0; }
    void flat() { b0 = 1; b1 = b2 = a1 = a2 = 0; }
    void set (double B0, double B1, double B2, double A0, double A1, double A2)
    { b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0; }

    static void w (double sr, double f, double& cs, double& sn)
    {
        const double w0 = juce::MathConstants<double>::twoPi * juce::jlimit (5.0, sr * 0.49, f) / sr;
        cs = std::cos (w0); sn = std::sin (w0);
    }
    void highpass (double sr, double f, double q)
    {
        double cs, sn; w (sr, f, cs, sn); const double al = sn / (2 * q);
        set ((1 + cs) / 2, -(1 + cs), (1 + cs) / 2, 1 + al, -2 * cs, 1 - al);
    }
    void lowpass (double sr, double f, double q)
    {
        double cs, sn; w (sr, f, cs, sn); const double al = sn / (2 * q);
        set ((1 - cs) / 2, 1 - cs, (1 - cs) / 2, 1 + al, -2 * cs, 1 - al);
    }
    void bandpass (double sr, double f, double q)
    {
        double cs, sn; w (sr, f, cs, sn); const double al = sn / (2 * q);
        set (al, 0, -al, 1 + al, -2 * cs, 1 - al);
    }
    void peak (double sr, double f, double q, double db)
    {
        if (std::abs (db) < 1e-4) { flat(); return; }
        double cs, sn; w (sr, f, cs, sn); const double al = sn / (2 * q), A = std::pow (10.0, db / 40.0);
        set (1 + al * A, -2 * cs, 1 - al * A, 1 + al / A, -2 * cs, 1 - al / A);
    }
    void lowShelf (double sr, double f, double q, double db)
    {
        if (std::abs (db) < 1e-4) { flat(); return; }
        double cs, sn; w (sr, f, cs, sn); const double A = std::pow (10.0, db / 40.0), k = 2 * std::sqrt (A) * sn / (2 * q);
        set (A * ((A + 1) - (A - 1) * cs + k), 2 * A * ((A - 1) - (A + 1) * cs), A * ((A + 1) - (A - 1) * cs - k),
             (A + 1) + (A - 1) * cs + k, -2 * ((A - 1) + (A + 1) * cs), (A + 1) + (A - 1) * cs - k);
    }
    void highShelf (double sr, double f, double q, double db)
    {
        if (std::abs (db) < 1e-4) { flat(); return; }
        double cs, sn; w (sr, f, cs, sn); const double A = std::pow (10.0, db / 40.0), k = 2 * std::sqrt (A) * sn / (2 * q);
        set (A * ((A + 1) + (A - 1) * cs + k), -2 * A * ((A - 1) + (A + 1) * cs), A * ((A + 1) + (A - 1) * cs - k),
             (A + 1) - (A - 1) * cs + k, 2 * ((A - 1) - (A + 1) * cs), (A + 1) - (A - 1) * cs - k);
    }
    double magDb (double f, double sr) const
    {
        const std::complex<double> z = std::polar (1.0, -juce::MathConstants<double>::twoPi * f / sr);
        const auto num = b0 + b1 * z + b2 * z * z, den = 1.0 + a1 * z + a2 * z * z;
        return 20.0 * std::log10 (std::max (1e-12, std::abs (num / den)));
    }
};

// anti-aliased (first-order ADAA) waveshapers
static inline double logcosh (double x) { const double a = std::abs (x); return a + std::log1p (std::exp (-2 * a)) - 0.6931471805599453; }

struct Shaper
{
    double x1[2] {};
    // kind 0 = tanh, 1 = asymmetric tube (bias b), 2 = hard clip
    float run (float xin, int c, int kind, double b = 0.2)
    {
        const double x = xin, p = x1[c], d = x - p;
        x1[c] = x;
        auto f = [&] (double v)
        {
            if (kind == 0) return std::tanh (v);
            if (kind == 1) return std::tanh (v + b) - std::tanh (b);
            return std::clamp (v, -1.0, 1.0);
        };
        auto F = [&] (double v)
        {
            if (kind == 0) return logcosh (v);
            if (kind == 1) return logcosh (v + b) - v * std::tanh (b);
            return std::abs (v) <= 1.0 ? 0.5 * v * v : std::abs (v) - 0.5;
        };
        if (std::abs (d) < 1.0e-5) return (float) f (0.5 * (x + p));
        return (float) ((F (x) - F (p)) / d);
    }
};

struct DCBlock
{
    float x1[2] {}, y1[2] {};
    float process (float x, int c) { const float y = x - x1[c] + 0.9995f * y1[c]; x1[c] = x; y1[c] = y; return y; }
};

// smoothed gain-reduction follower in the dB domain (attack when reduction increases)
struct GRFollower
{
    float g = 0, att = 0, rel = 0;
    float step (float target) { g = target > g ? att * g + (1 - att) * target : rel * g + (1 - rel) * target; return g; }
};

static float softKneeGR (float levelDb, float thr, float ratio, float knee)
{
    const float over = levelDb - thr;
    if (knee > 0.01f && 2 * std::abs (over) <= knee)
        return (1 - 1 / ratio) * (over + knee / 2) * (over + knee / 2) / (2 * knee);
    return over > 0 ? over * (1 - 1 / ratio) : 0.0f;
}

// =====================================================================================  Tune
class TuneMod : public Module
{
public:
    void prepare (double sr, int) override { tuner.prepare (sr); }
    int latency() const override { return tuner.getLatency(); }
    bool handlesBypass() const override { return true; }
    void process (juce::AudioBuffer<float>& b, int nch, const Context&, const float* v, bool on) override
    {
        tuner.setParams (v[0], v[1], (int) v[2], (int) v[3], v[4]);
        tuner.process (b.getArrayOfWritePointers(), nch, b.getNumSamples(), on);
        note = tuner.targetNote();
        meter = tuner.detectedCents();
        meter2 = (float) tuner.detectedNote();
    }
    tune::Tuner tuner;
};

// =====================================================================================  De-esser
class DeEss : public Module
{
public:
    void prepare (double s, int) override { sr = s; det.reset(); for (auto& h : hp) h.reset(); env = 0; gr = 0; }
    void process (juce::AudioBuffer<float>& b, int nch, const Context&, const float* v, bool) override
    {
        det.bandpass (sr, v[0], 1.3);
        for (auto& h : hp) h.highpass (sr, v[0] * 0.7, 0.707);
        const float thr = v[1], range = v[2];
        const bool wide = v[3] > 0.5f;
        const float a = coefMs (0.3, sr), r = coefMs (50, sr), ga = coefMs (0.5, sr), gr2 = coefMs (60, sr);
        float maxGR = 0;
        auto* L = b.getWritePointer (0);
        auto* R = nch > 1 ? b.getWritePointer (1) : nullptr;
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float m = R ? 0.5f * (L[i] + R[i]) : L[i];
            const float s = std::abs (det.process (m, 0));
            env = s > env ? a * env + (1 - a) * s : r * env + (1 - r) * s;
            const float target = juce::jlimit (0.0f, range, (gainToDb (env) - thr) * 0.75f);
            gr = target > gr ? ga * gr + (1 - ga) * target : gr2 * gr + (1 - gr2) * target;
            maxGR = std::max (maxGR, gr);
            const float g = dbToGain (-gr);
            if (wide) { L[i] *= g; if (R) R[i] *= g; }
            else
            {
                L[i] += hp[0].process (L[i], 0) * (g - 1);
                if (R) R[i] += hp[1].process (R[i], 0) * (g - 1);
            }
        }
        meter = maxGR;
    }
private:
    double sr = 44100;
    BQ det, hp[2];
    float env = 0, gr = 0;
};

// =====================================================================================  EQs
static void designClean (BQ f[7], double sr, const float* v)
{
    f[0].highpass (sr, v[0], 0.5412); f[1].highpass (sr, v[0], 1.3066);
    f[2].lowShelf (sr, 120, 0.7, v[1]);
    f[3].peak (sr, v[2], 1.0, v[3]);
    f[4].peak (sr, v[4], 1.0, v[5]);
    f[5].highShelf (sr, 8000, 0.7, v[6]);
    f[6].highShelf (sr, 16000, 0.55, v[7]);
}

static const float pultLow[4] { 20, 30, 60, 100 };
static const float pultHigh[7] { 3000, 4000, 5000, 8000, 10000, 12000, 16000 };
static const float pultAtt[3] { 5000, 10000, 20000 };

static void designPultec (BQ f[4], double sr, const float* v)
{
    const float lf = pultLow[juce::jlimit (0, 3, (int) v[0])];
    f[0].lowShelf (sr, lf, 0.9, v[1] * 1.35);                    // resonant boost
    f[1].peak (sr, lf * 3.2, 0.45, -v[2] * 0.8);                 // broad cut just above: the 'Pultec trick' dip
    const double q = 2.2 * std::pow (0.35 / 2.2, v[5] / 10.0);   // bandwidth knob: sharp -> broad
    f[2].peak (sr, pultHigh[juce::jlimit (0, 6, (int) v[3])], q, v[4] * 1.6);
    f[3].highShelf (sr, pultAtt[juce::jlimit (0, 2, (int) v[6])], 0.7, -v[7] * 1.6);
}

static void designTube (BQ f[4], double sr, const float* v)
{
    f[0].lowShelf (sr, 100, 0.7, v[0]);
    f[1].peak (sr, v[1], 0.8, v[2]);
    f[2].highShelf (sr, 6000, 0.7, v[3]);
    f[3].highShelf (sr, 15000, 0.5, v[4]);
}

class CleanEQ : public Module
{
public:
    void prepare (double s, int) override { sr = s; for (auto& x : f) x.reset(); }
    void process (juce::AudioBuffer<float>& b, int nch, const Context&, const float* v, bool) override
    {
        designClean (f, sr, v);
        for (int c = 0; c < nch; ++c)
        {
            auto* d = b.getWritePointer (c);
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                float x = d[i];
                for (auto& q : f) x = q.process (x, c);
                d[i] = x;
            }
        }
    }
private:
    double sr = 44100;
    BQ f[7];
};

class PultecEQ : public Module
{
public:
    void prepare (double s, int) override { sr = s; for (auto& x : f) x.reset(); }
    void process (juce::AudioBuffer<float>& b, int nch, const Context&, const float* v, bool) override
    {
        designPultec (f, sr, v);
        for (int c = 0; c < nch; ++c)
        {
            auto* d = b.getWritePointer (c);
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                float x = d[i];
                for (auto& q : f) x = q.process (x, c);
                // tube make-up stage: very gentle even harmonics
                d[i] = dc.process (sh.run (x * 0.6f, c, 1, 0.12) / 0.6f, c);
            }
        }
    }
private:
    double sr = 44100;
    BQ f[4];
    Shaper sh;
    DCBlock dc;
};

class TubeEQ : public Module
{
public:
    void prepare (double s, int) override { sr = s; for (auto& x : f) x.reset(); }
    void process (juce::AudioBuffer<float>& b, int nch, const Context&, const float* v, bool) override
    {
        designTube (f, sr, v);
        const float drive = dbToGain (v[5] * 18.0f), comp = 1.0f / drive * dbToGain (v[6]);
        for (int c = 0; c < nch; ++c)
        {
            auto* d = b.getWritePointer (c);
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                float x = d[i];
                for (auto& q : f) x = q.process (x, c);
                d[i] = dc.process (sh.run (x * drive, c, 1, 0.2), c) * comp;
            }
        }
    }
private:
    double sr = 44100;
    BQ f[4];
    Shaper sh;
    DCBlock dc;
};

float eqResponseDb (int type, const float* v, float freq)
{
    const double sr = 48000;
    const auto& id = juce::String (catalogue()[(size_t) type].id);
    double total = 0;
    if (id == "eq")     { BQ f[7]; designClean (f, sr, v);  for (auto& q : f) total += q.magDb (freq, sr); }
    if (id == "pultec") { BQ f[4]; designPultec (f, sr, v); for (auto& q : f) total += q.magDb (freq, sr); }
    if (id == "tubeeq") { BQ f[4]; designTube (f, sr, v);   for (auto& q : f) total += q.magDb (freq, sr); total += v[6]; }
    return (float) total;
}

// =====================================================================================  Compressors
class Comp : public Module
{
public:
    void prepare (double s, int) override { sr = s; fol.g = 0; }
    void process (juce::AudioBuffer<float>& b, int nch, const Context&, const float* v, bool) override
    {
        const float thr = v[0], ratio = v[1], knee = v[4], makeup = dbToGain (v[5]), mix = v[6];
        fol.att = coefMs (v[2], sr); fol.rel = coefMs (v[3], sr);
        float maxGR = 0;
        auto* L = b.getWritePointer (0);
        auto* R = nch > 1 ? b.getWritePointer (1) : nullptr;
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float peak = std::max (std::abs (L[i]), R ? std::abs (R[i]) : 0.0f);
            const float g = fol.step (softKneeGR (gainToDb (peak), thr, ratio, knee));
            maxGR = std::max (maxGR, g);
            const float gain = dbToGain (-g) * makeup;
            L[i] = L[i] * (1 - mix) + L[i] * gain * mix;
            if (R) R[i] = R[i] * (1 - mix) + R[i] * gain * mix;
        }
        meter = maxGR;
    }
private:
    double sr = 44100;
    GRFollower fol;
};

class FET : public Module
{
public:
    void prepare (double s, int) override { sr = s; fol.g = 0; }
    void process (juce::AudioBuffer<float>& b, int nch, const Context&, const float* v, bool) override
    {
        static const float ratios[5] { 4, 8, 12, 20, 20 }, knees[5] { 8, 5, 3, 2, 6 };
        const int r = juce::jlimit (0, 4, (int) v[4]);
        const bool all = r == 4;
        double attMs = 0.8 * std::pow (0.02 / 0.8, (v[2] - 1) / 6.0);       // 1 = 800 us ... 7 = 20 us
        double relMs = 1100.0 * std::pow (50.0 / 1100.0, (v[3] - 1) / 6.0); // 1 = 1.1 s ... 7 = 50 ms
        if (all) { attMs *= 2.5; relMs *= 0.6; }
        fol.att = coefMs (attMs, sr);
        const float inG = dbToGain (v[0]), outG = dbToGain (v[1] - 0.85f * v[0]), mix = v[5];
        const float thr = -6.0f, colour = all ? 2.0f : 1.0f;
        float maxGR = 0;
        auto* L = b.getWritePointer (0);
        auto* R = nch > 1 ? b.getWritePointer (1) : nullptr;
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float xl = L[i] * inG, xr = R ? R[i] * inG : 0.0f;
            const float peak = std::max (std::abs (xl), std::abs (xr));
            float target = softKneeGR (gainToDb (peak), thr, ratios[r], knees[r]);
            if (all) target *= 1.25f;
            // program-dependent release: deeper reduction recovers more slowly
            fol.rel = coefMs (relMs * (1.0 + 0.6 * std::min (1.0f, fol.g / 12.0f)), sr);
            const float g = fol.step (target);
            maxGR = std::max (maxGR, g);
            const float gain = dbToGain (-g);
            const float drive = 1.0f + 0.025f * g * colour;
            auto col = [&] (float x, int c) { return sh.run (x * gain * drive * 0.8f, c, 1, 0.08) / (0.8f * drive); };
            const float yl = dc.process (col (xl, 0), 0) * outG;
            L[i] = L[i] * (1 - mix) + yl * mix;
            if (R) { const float yr = dc.process (col (xr, 1), 1) * outG; R[i] = R[i] * (1 - mix) + yr * mix; }
        }
        meter = maxGR;
    }
private:
    double sr = 44100;
    GRFollower fol;
    Shaper sh;
    DCBlock dc;
};

// dual time-constant optical-cell model shared by the Opto and the Vari-Mu auto modes
struct OptoCell
{
    float e1 = 0, e2 = 0;
    float step (float target, float a1, float r1, float a2, float r2, float memory)
    {
        e1 = target > e1 ? a1 * e1 + (1 - a1) * target : r1 * e1 + (1 - r1) * target;
        const float t2 = memory * e1;
        e2 = t2 > e2 ? a2 * e2 + (1 - a2) * t2 : r2 * e2 + (1 - r2) * t2;
        return std::max (e1, e2);
    }
};

class Opto : public Module
{
public:
    void prepare (double s, int) override { sr = s; cell = {}; env = 0; for (auto& e : emph) e.reset(); }
    void process (juce::AudioBuffer<float>& b, int nch, const Context&, const float* v, bool) override
    {
        const float pr = -15.0f + 0.4f * v[0];
        const bool limit = v[2] > 0.5f;
        const float ratio = limit ? 12.0f : 3.0f, knee = limit ? 6.0f : 12.0f, thr = -20.0f;
        for (auto& e : emph) e.highShelf (sr, 2500, 0.7, v[3] * 12.0);
        const float rmsC = coefMs (5, sr), a1 = coefMs (10, sr), r1 = coefMs (60, sr), a2 = coefMs (1500, sr), r2 = coefMs (3000, sr);
        const float out = dbToGain (v[1]), mix = v[4];
        float maxGR = 0;
        auto* L = b.getWritePointer (0);
        auto* R = nch > 1 ? b.getWritePointer (1) : nullptr;
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float sl = emph[0].process (L[i], 0), sr2 = R ? emph[1].process (R[i], 0) : 0.0f;
            const float s2 = std::max (sl * sl, sr2 * sr2);
            env = rmsC * env + (1 - rmsC) * s2;
            const float level = 10.0f * std::log10 (std::max (env, 1.0e-12f)) + pr;
            const float g = cell.step (softKneeGR (level, thr, ratio, knee), a1, r1, a2, r2, 0.6f);
            maxGR = std::max (maxGR, g);
            const float gain = dbToGain (-g) * out;
            auto col = [&] (float x, int c) { return dc.process (sh.run (x * 0.7f, c, 1, 0.15) / 0.7f, c); };
            const float yl = col (L[i] * gain, 0);
            L[i] = L[i] * (1 - mix) + yl * mix;
            if (R) { const float yr = col (R[i] * gain, 1); R[i] = R[i] * (1 - mix) + yr * mix; }
        }
        meter = maxGR;
    }
private:
    double sr = 44100;
    OptoCell cell;
    float env = 0;
    BQ emph[2];
    Shaper sh;
    DCBlock dc;
};

class VariMu : public Module
{
public:
    void prepare (double s, int) override { sr = s; cell = {}; fol.g = 0; env = 0; }
    void process (juce::AudioBuffer<float>& b, int nch, const Context&, const float* v, bool) override
    {
        static const float attMs[6] { 0.5f, 0.5f, 1.0f, 1.0f, 0.5f, 1.0f }, relMs[6] { 300, 800, 2000, 5000, 300, 300 };
        const int tc = juce::jlimit (0, 5, (int) v[2]);
        const bool autoRel = tc >= 4;
        fol.att = coefMs (attMs[tc], sr); fol.rel = coefMs (relMs[tc], sr);
        const float a2 = coefMs (tc == 4 ? 2000 : 5000, sr), r2 = coefMs (tc == 4 ? 2000 : 10000, sr);
        const float inG = dbToGain (v[0]), outG = dbToGain (v[3] - 0.8f * v[0]), thr = v[1], mix = v[4];
        const float rmsC = coefMs (10, sr);
        const float drive = 0.5f + 0.03f * std::max (0.0f, v[0]);
        float maxGR = 0;
        auto* L = b.getWritePointer (0);
        auto* R = nch > 1 ? b.getWritePointer (1) : nullptr;
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float xl = L[i] * inG, xr = R ? R[i] * inG : 0.0f;
            const float pk = std::max (std::abs (xl), std::abs (xr));
            env = rmsC * env + (1 - rmsC) * pk * pk;
            const float level = 0.5f * gainToDb (pk) + 0.5f * 10.0f * std::log10 (std::max (env, 1.0e-12f));
            // variable-mu: the more you push, the higher the ratio
            const float over = level - thr;
            const float os = 0.5f * (over + std::sqrt (over * over + 36.0f));
            const float target = os - os / (1.0f + 0.15f * os);
            const float g = autoRel ? cell.step (target, fol.att, fol.rel, a2, r2, 0.5f) : fol.step (target);
            maxGR = std::max (maxGR, g);
            const float gain = dbToGain (-g);
            auto col = [&] (float x, int c) { return dc.process (sh.run (x * gain * drive, c, 1, 0.05) / drive, c) * outG; };
            const float yl = col (xl, 0);
            L[i] = L[i] * (1 - mix) + yl * mix;
            if (R) { const float yr = col (xr, 1); R[i] = R[i] * (1 - mix) + yr * mix; }
        }
        meter = maxGR;
    }
private:
    double sr = 44100;
    OptoCell cell;
    GRFollower fol;
    float env = 0;
    Shaper sh;
    DCBlock dc;
};

// =====================================================================================  Saturation
class Saturation : public Module
{
public:
    void prepare (double s, int) override { sr = s; for (auto& f : tone) f.reset(); lp[0] = lp[1] = 0; }
    void process (juce::AudioBuffer<float>& b, int nch, const Context&, const float* v, bool) override
    {
        const int type = (int) v[0];
        const float dB = v[1] * 30.0f, g = dbToGain (dB), comp = dbToGain (-dB * (type == 2 ? 0.8f : 0.7f));
        const float mix = v[3], out = dbToGain (v[4]);
        tone[0].lowShelf (sr, 300, 0.7, -v[2] * 6.0); tone[1].lowShelf (sr, 300, 0.7, -v[2] * 6.0);
        tone[2].highShelf (sr, 3000, 0.7, v[2] * 6.0); tone[3].highShelf (sr, 3000, 0.7, v[2] * 6.0);
        const float lpc = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 14000.0f / (float) sr);
        for (int c = 0; c < nch; ++c)
        {
            auto* d = b.getWritePointer (c);
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const float x = d[i];
                float y;
                if (type == 0) { y = sh.run (x * g, c, 0); lp[c] += lpc * (y - lp[c]); y = lp[c]; }   // tape: rounded, soft top
                else if (type == 1) y = dc.process (sh.run (x * g, c, 1, 0.25), c);
                else y = sh.run (x * g, c, 2);
                y = tone[c + 2].process (tone[c].process (y * comp, 0), 0);
                d[i] = (x + mix * (y - x)) * out;
            }
        }
    }
private:
    double sr = 44100;
    BQ tone[4];
    float lp[2] {};
    Shaper sh;
    DCBlock dc;
};

// =====================================================================================  Doubler
class Doubler : public Module
{
public:
    void prepare (double s, int) override
    {
        sr = s;
        for (auto& l : line) l.prepare ((int) (sr * 0.09));
        for (auto& f : lpf) { f.lowpass (sr, 9000, 0.707); f.reset(); }
        ph[0] = 0; ph[1] = 0.37;
    }
    void process (juce::AudioBuffer<float>& b, int nch, const Context&, const float* v, bool) override
    {
        const float amt = v[0] * 0.8f, width = v[1], cents = v[2], baseMs = v[3];
        const float rates[2] { 0.53f, 0.71f };
        auto* L = b.getWritePointer (0);
        auto* R = nch > 1 ? b.getWritePointer (1) : nullptr;
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float m = R ? 0.5f * (L[i] + R[i]) : L[i];
            float vo[2];
            for (int k = 0; k < 2; ++k)
            {
                // delay modulation depth chosen so the peak pitch drift equals the Detune setting
                const float depthMs = cents * 0.000578f / (juce::MathConstants<float>::twoPi * rates[k]) * 1000.0f;
                const float d = (baseMs * (k == 0 ? 1.0f : 1.37f) + depthMs * std::sin (juce::MathConstants<float>::twoPi * ph[k])) * 0.001f * (float) sr;
                vo[k] = lpf[k].process (line[k].read (std::max (1.0f, d)), 0);
                line[k].push (m);
                ph[k] += rates[k] / (float) sr;
                if (ph[k] >= 1) ph[k] -= 1;
            }
            if (R)
            {
                L[i] += amt * (vo[0] * (0.5f + 0.5f * width) + vo[1] * (0.5f - 0.5f * width));
                R[i] += amt * (vo[1] * (0.5f + 0.5f * width) + vo[0] * (0.5f - 0.5f * width));
            }
            else L[i] += amt * 0.5f * (vo[0] + vo[1]);
        }
    }
private:
    double sr = 44100;
    wv::DelayBuffer line[2];
    BQ lpf[2];
    float ph[2] {};
};

// =====================================================================================  Delay
class Delay : public Module
{
public:
    void prepare (double s, int) override
    {
        sr = s;
        for (auto& l : line) l.prepare ((int) (sr * 2.6));
        for (auto& f : hp) f.reset();
        for (auto& f : lp) f.reset();
        cur = (float) (sr * 0.25); env = 0; wph = 0;
    }
    void process (juce::AudioBuffer<float>& b, int nch, const Context& ctx, const float* v, bool) override
    {
        static const float beats[7] { 1.0f, 0.5f, 0.75f, 0.25f, 1.5f, 2.0f, 1.0f / 3.0f };
        const float target = juce::jmin ((float) (beats[juce::jlimit (0, 6, (int) v[0])] * 60.0 / ctx.bpm * sr), (float) (sr * 2.5));
        const float fb = v[1], mix = v[2], duck = v[6], wow = v[7];
        const int mode = (int) v[3];
        for (auto& f : hp) f.highpass (sr, v[4], 0.707);
        for (auto& f : lp) f.lowpass (sr, v[5], 0.707);
        const float ea = coefMs (5, sr), er = coefMs (250, sr);
        auto* L = b.getWritePointer (0);
        auto* R = nch > 1 ? b.getWritePointer (1) : nullptr;
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            cur += (target - cur) * 0.0004f;
            wph += 0.55f / (float) sr; if (wph >= 1) wph -= 1;
            const float d = std::max (2.0f, cur + wow * 0.002f * (float) sr * std::sin (juce::MathConstants<float>::twoPi * wph));
            const float inL = L[i], inR = R ? R[i] : inL, mono = 0.5f * (inL + inR);
            const float lvl = std::max (std::abs (inL), std::abs (inR));
            env = lvl > env ? ea * env + (1 - ea) * lvl : er * env + (1 - er) * lvl;
            const float duckG = 1.0f - duck * juce::jlimit (0.0f, 1.0f, (gainToDb (env) + 45.0f) / 30.0f);

            const float o0 = line[0].read (d), o1 = line[1].read (d);
            auto filt = [&] (float x, int k) { return lp[k].process (hp[k].process (x, 0), 0); };
            float wl, wr;
            if (mode == 0)       // ping-pong
            {
                const float f0 = filt (o1, 0), f1 = filt (o0, 1);
                line[0].push (mono + fb * f0);
                line[1].push (fb * f1);
                wl = o0; wr = o1;
            }
            else if (mode == 1)  // stereo
            {
                line[0].push (inL + fb * filt (o0, 0));
                line[1].push (inR + fb * filt (o1, 1));
                wl = o0; wr = o1;
            }
            else                 // mono
            {
                line[0].push (mono + fb * filt (o0, 0));
                line[1].push (0.0f);
                wl = wr = o0;
            }
            const float w = mix * duckG;
            L[i] = inL + w * (R ? wl : 0.5f * (wl + wr));
            if (R) R[i] = inR + w * wr;
        }
    }
private:
    double sr = 44100;
    wv::DelayBuffer line[2];
    BQ hp[2], lp[2];
    float cur = 10000, env = 0, wph = 0;
};

// =====================================================================================  Reverb (8-line FDN)
class Reverb : public Module
{
public:
    void prepare (double s, int) override
    {
        sr = s;
        pre.prepare ((int) (sr * 0.36));
        for (auto& d : diff) d.prepare ((int) (sr * 0.03));
        for (auto& l : lines) l.prepare ((int) (sr * 0.16));
        for (auto& v : damp) v = 0;
        for (auto& f : outHP) f.reset();
        for (int k = 0; k < 4; ++k) mph[k] = 0.25f * (float) k;
    }

    void process (juce::AudioBuffer<float>& b, int nch, const Context&, const float* v, bool) override
    {
        static const float lens[4][8] = {
            { 13.7f, 17.3f, 19.9f, 23.1f, 26.3f, 29.9f, 33.1f, 37.7f },    // plate
            { 31.1f, 37.3f, 41.9f, 47.3f, 53.9f, 59.3f, 67.1f, 73.7f },    // hall
            { 7.3f, 9.1f, 11.3f, 13.1f, 15.7f, 17.9f, 19.7f, 23.3f },      // room
            { 17.1f, 19.7f, 23.9f, 27.1f, 31.3f, 34.9f, 39.1f, 43.7f } };  // chamber
        static const float diffMs[4] { 4.77f, 3.59f, 12.73f, 9.31f };
        static const float diffG[4] { 0.75f, 0.7f, 0.6f, 0.65f };
        static const float erTaps[6][2] { { 7.1f, 0.6f }, { 11.3f, 0.5f }, { 17.9f, 0.42f }, { 23.7f, 0.35f }, { 31.1f, 0.28f }, { 41.3f, 0.2f } };
        const int type = juce::jlimit (0, 3, (int) v[0]);
        const float decay = v[1], preMs = v[2], size = 0.6f + 0.8f * v[3];
        const float width = v[6], mix = v[7];
        const float dampC = std::exp (-juce::MathConstants<float>::twoPi * v[4] / (float) sr);
        for (auto& f : outHP) f.highpass (sr, v[5], 0.707);

        float len[8], g[8];
        for (int k = 0; k < 8; ++k)
        {
            len[k] = std::min (lens[type][k] * size * 0.001f * (float) sr, (float) sr * 0.15f);
            g[k] = std::pow (10.0f, -3.0f * len[k] / (decay * 1.15f * (float) sr));   // 1.15: offsets the extra loss from damping
        }
        const float modDepth = (type == 0 ? 0.6f : type == 1 ? 0.8f : 0.3f) * 0.001f * (float) sr;
        const float er = type >= 2 ? (type == 2 ? 1.0f : 0.5f) : 0.0f;
        const float preD = std::max (1.0f, preMs * 0.001f * (float) sr);

        auto* L = b.getWritePointer (0);
        auto* R = nch > 1 ? b.getWritePointer (1) : nullptr;
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float inL = L[i], inR = R ? R[i] : inL;
            float x = pre.read (preD);
            pre.push (0.5f * (inL + inR));

            float earlyL = 0, earlyR = 0;
            if (er > 0)
                for (auto& t : erTaps)
                {
                    earlyL += t[1] * pre.read (preD + t[0] * size * 0.001f * (float) sr);
                    earlyR += t[1] * pre.read (preD + t[0] * 1.13f * size * 0.001f * (float) sr);
                }

            for (int k = 0; k < 4; ++k)       // input diffusion
            {
                const float dd = diff[k].read (diffMs[k] * size * 0.001f * (float) sr);
                const float wv = x + diffG[type] * dd;
                x = dd - diffG[type] * wv;
                diff[k].push (wv);
            }

            float o[8], sum = 0;
            for (int k = 0; k < 8; ++k)
            {
                float d = len[k];
                if (k < 4) d += modDepth * std::sin (juce::MathConstants<float>::twoPi * mph[k]);
                float y = lines[k].read (std::max (2.0f, d));
                damp[k] = y + dampC * (damp[k] - y);           // high frequencies decay faster
                o[k] = damp[k] * g[k];
                sum += o[k];
            }
            for (int k = 0; k < 4; ++k) { mph[k] += (0.31f + 0.13f * (float) k) / (float) sr; if (mph[k] >= 1) mph[k] -= 1; }
            const float hh = sum * 0.25f;                      // Householder: o - 2/N * sum
            static const float inj[8] { 1, -1, 1, -1, 1, 1, -1, -1 };
            for (int k = 0; k < 8; ++k) lines[k].push (o[k] - hh + x * inj[k] * 0.35f);

            float wl = (o[0] - o[2] + o[4] - o[6] + o[1] + o[5]) * 0.45f + earlyL * er * 0.5f;
            float wr = (o[1] - o[3] + o[5] - o[7] + o[2] + o[6]) * 0.45f - earlyR * er * 0.5f;
            wl = outHP[0].process (wl, 0); wr = outHP[1].process (wr, 0);
            const float mid = 0.5f * (wl + wr), side = 0.5f * (wl - wr) * width;
            wl = mid + side; wr = mid - side;
            L[i] = inL + mix * (R ? wl : mid);
            if (R) R[i] = inR + mix * wr;
        }
    }
private:
    double sr = 44100;
    wv::DelayBuffer pre, diff[4], lines[8];
    float damp[8] {}, mph[4] {};
    BQ outHP[2];
};

// =====================================================================================  Chop modules
#if WALL_CHOP
class BenderMod : public Module
{
public:
    void prepare (double sr, int) override { bender.prepare (sr); }
    int latency() const override { return bender.getLatency(); }
    bool handlesBypass() const override { return true; }
    void process (juce::AudioBuffer<float>& b, int nch, const Context& ctx, const float* v, bool on) override
    {
        bender.setParams (v[1], v[2], v[3], (int) v[0] == 1, ctx.key);
        float* const* ch = b.getArrayOfWritePointers();
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            float fr[2] { ch[0][i], nch > 1 ? ch[1][i] : 0.0f };
            bender.processFrame (fr, std::min (nch, 2), on);
            for (int c = 0; c < std::min (nch, 2); ++c) ch[c][i] = fr[c];
        }
    }
    wc::Bender bender;
};

class StutterMod : public Module
{
public:
    void prepare (double sr, int) override { st.prepare (sr); }
    void process (juce::AudioBuffer<float>& b, int nch, const Context& ctx, const float* v, bool) override
    {
        static const double intervals[] { 0, 2, 4, 8 }, lengths[] { 0.5, 1, 2, 4, 8 }, slices[] { 1, 0.5, 0.25, 0.125, 1.0 / 3.0, 1.0 / 6.0 };
        float* const* ch = b.getArrayOfWritePointers();
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            float fr[2] { ch[0][i], nch > 1 ? ch[1][i] : 0.0f };
            st.process (fr, std::min (nch, 2), ctx.ppq + i * ctx.beatsPerSample, ctx.bpm, true,
                        intervals[(int) v[0]], lengths[(int) v[1]], slices[(int) v[2]], v[3], v[4], v[5]);
            for (int c = 0; c < std::min (nch, 2); ++c) ch[c][i] = fr[c];
        }
    }
    wc::Stutter st;
};

class GateMod : public Module
{
public:
    void prepare (double sr, int) override { gate.prepare (sr); }
    void process (juce::AudioBuffer<float>& b, int nch, const Context& ctx, const float* v, bool) override
    {
        static const double steps[] { 0.5, 0.25, 0.125 };
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float g = gate.gain (ctx.ppq + i * ctx.beatsPerSample, (int) v[0], steps[(int) v[1]], v[2], v[3]);
            for (int c = 0; c < nch; ++c) b.getWritePointer (c)[i] *= g;
        }
    }
    wc::Gate gate;
};

class FilterMod : public Module
{
public:
    void prepare (double sr, int) override { f.prepare (sr); }
    void process (juce::AudioBuffer<float>& b, int nch, const Context& ctx, const float* v, bool) override
    {
        static const double lfo[] { 4, 2, 1, 0.5, 0.25 };
        float* const* ch = b.getArrayOfWritePointers();
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            float fr[2] { ch[0][i], nch > 1 ? ch[1][i] : 0.0f };
            f.process (fr, std::min (nch, 2), ctx.ppq + i * ctx.beatsPerSample, (int) v[0], v[1], v[2], lfo[(int) v[3]], v[4]);
            for (int c = 0; c < std::min (nch, 2); ++c) ch[c][i] = fr[c];
        }
    }
    wc::SweepFilter f;
};

class TapeStopMod : public Module
{
public:
    void prepare (double sr, int) override { ts.prepare (sr); }
    bool handlesBypass() const override { return true; }
    void process (juce::AudioBuffer<float>& b, int nch, const Context&, const float* v, bool on) override
    {
        float* const* ch = b.getArrayOfWritePointers();
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            float fr[2] { ch[0][i], nch > 1 ? ch[1][i] : 0.0f };
            ts.process (fr, std::min (nch, 2), on, v[0]);
            for (int c = 0; c < std::min (nch, 2); ++c) ch[c][i] = fr[c];
        }
    }
    wc::TapeStop ts;
};
#endif

// =====================================================================================  factory
std::unique_ptr<Module> create (int type)
{
    if (type <= 0 || type >= (int) catalogue().size()) return nullptr;
    const juce::String id (catalogue()[(size_t) type].id);
    std::unique_ptr<Module> m;
    if (id == "tune") m = std::make_unique<TuneMod>();
    else if (id == "deess") m = std::make_unique<DeEss>();
    else if (id == "eq") m = std::make_unique<CleanEQ>();
    else if (id == "pultec") m = std::make_unique<PultecEQ>();
    else if (id == "tubeeq") m = std::make_unique<TubeEQ>();
    else if (id == "comp") m = std::make_unique<Comp>();
    else if (id == "fet") m = std::make_unique<FET>();
    else if (id == "opto") m = std::make_unique<Opto>();
    else if (id == "varimu") m = std::make_unique<VariMu>();
    else if (id == "sat") m = std::make_unique<Saturation>();
    else if (id == "double") m = std::make_unique<Doubler>();
    else if (id == "delay") m = std::make_unique<Delay>();
    else if (id == "reverb") m = std::make_unique<Reverb>();
   #if WALL_CHOP
    else if (id == "bender") m = std::make_unique<BenderMod>();
    else if (id == "stutter") m = std::make_unique<StutterMod>();
    else if (id == "gate") m = std::make_unique<GateMod>();
    else if (id == "filter") m = std::make_unique<FilterMod>();
    else if (id == "tapestop") m = std::make_unique<TapeStopMod>();
   #endif
    if (m) m->type = type;
    return m;
}

} // namespace mods
