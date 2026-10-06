#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

// Small built-in sounds so you can audition the generated parts without routing anything.
namespace ps
{
enum Timbre { Keys, Pluck, Saw, Bass, Lead, Pad };

struct Shared { std::atomic<int> timbre { Keys }; float mod = 0.7f, expr = 1.0f; };

struct Sound : juce::SynthesiserSound
{
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

class Voice : public juce::SynthesiserVoice
{
public:
    explicit Voice (Shared& s) : shared (s) { ks.assign (8192, 0.0f); }

    bool canPlaySound (juce::SynthesiserSound*) override { return true; }

    void startNote (int note, float vel, juce::SynthesiserSound*, int) override
    {
        sr = getSampleRate();
        timbre = shared.timbre.load();
        freq = 440.0 * std::pow (2.0, (note - 69) / 12.0);
        velocity = vel;
        for (auto& p : ph) p = juce::Random::getSystemRandom().nextDouble();
        phase = modPhase = 0.0;
        lp = 0.0f;
        smoothMod = shared.mod;
        smoothExpr = shared.expr;

        juce::ADSR::Parameters a;
        switch (timbre)
        {
            case Keys:  a = { 0.002f, 1.6f, 0.25f, 0.35f }; break;
            case Pluck: a = { 0.001f, 0.1f, 1.0f, 0.15f }; break;
            case Saw:   a = { 0.003f, 0.35f, 0.45f, 0.18f }; break;
            case Bass:  a = { 0.004f, 0.4f, 0.8f, 0.08f }; break;
            case Lead:  a = { 0.004f, 0.25f, 0.7f, 0.15f }; break;
            default:    a = { 0.45f, 0.5f, 0.9f, 1.1f }; break;
        }
        env.setSampleRate (sr);
        env.setParameters (a);
        env.noteOn();

        if (timbre == Pluck)
        {
            ksLen = juce::jlimit (2, (int) ks.size() - 1, (int) std::round (sr / freq));
            float last = 0.0f;
            for (int i = 0; i < ksLen; ++i)
            {
                const float noise = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
                last += (noise - last) * (0.25f + 0.6f * vel);   // brighter when played harder
                ks[(size_t) i] = last;
            }
            ksPos = 0;
            ksDamp = 0.9985f - (float) juce::jlimit (0.0, 0.02, freq / 60000.0);
        }
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff) env.noteOff();
        else { env.reset(); clearCurrentNote(); }
    }

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& out, int start, int num) override
    {
        if (! isVoiceActive()) return;
        const double inc = freq / sr;
        for (int i = 0; i < num; ++i)
        {
            smoothMod += (shared.mod - smoothMod) * 0.0015f;
            smoothExpr += (shared.expr - smoothExpr) * 0.004f;
            float s = 0.0f;
            switch (timbre)
            {
                case Keys:
                {
                    const float decay = env.getNextSample();
                    const double m = std::sin (modPhase * juce::MathConstants<double>::twoPi) * (0.6 + 2.2 * velocity) * decay;
                    s = (float) std::sin ((phase + m * 0.16) * juce::MathConstants<double>::twoPi) * decay * 0.5f;
                    modPhase += inc; modPhase -= std::floor (modPhase);
                    break;
                }
                case Pluck:
                {
                    const int nxt = (ksPos + 1) % ksLen;
                    const float y = 0.5f * (ks[(size_t) ksPos] + ks[(size_t) nxt]) * ksDamp;
                    s = ks[(size_t) ksPos] * env.getNextSample() * 0.8f;
                    ks[(size_t) ksPos] = y;
                    ksPos = nxt;
                    break;
                }
                case Bass:
                {
                    const float sine = (float) std::sin (phase * juce::MathConstants<double>::twoPi);
                    s = std::tanh (sine * 1.8f) * 0.55f * env.getNextSample();
                    break;
                }
                default:
                {
                    // detuned saws through a soft low-pass
                    const int voices = timbre == Lead ? 2 : 3;
                    float acc = 0.0f;
                    for (int v = 0; v < voices; ++v)
                    {
                        const double d = 1.0 + (v - 1) * (timbre == Pad ? 0.006 : 0.004);
                        ph[v] += inc * d; ph[v] -= std::floor (ph[v]);
                        acc += saw (ph[v], inc * d);
                    }
                    if (timbre == Lead) acc += (phase < 0.5 ? 0.4f : -0.4f);
                    acc /= (float) voices;
                    float cutoff = timbre == Pad ? 400.0f + 4500.0f * smoothMod
                                 : timbre == Saw ? 1500.0f + 6000.0f * velocity : 3500.0f;
                    const float a = 1.0f - std::exp (-2.0f * 3.14159f * cutoff / (float) sr);
                    lp += (acc - lp) * a;
                    s = lp * env.getNextSample() * (timbre == Pad ? 0.35f * (0.35f + 0.65f * smoothMod) * smoothExpr : 0.4f);
                    break;
                }
            }
            phase += inc; phase -= std::floor (phase);
            s *= 0.25f + 0.75f * velocity;
            for (int c = 0; c < out.getNumChannels(); ++c) out.addSample (c, start + i, s);
            if (! env.isActive()) { clearCurrentNote(); break; }
        }
    }

private:
    static float saw (double p, double dt)
    {
        float v = (float) (2.0 * p - 1.0);
        if (p < dt) { const double t = p / dt; v -= (float) (t + t - t * t - 1.0); }
        else if (p > 1.0 - dt) { const double t = (p - 1.0) / dt; v -= (float) (t * t + t + t + 1.0); }
        return v;
    }

    Shared& shared;
    juce::ADSR env;
    double sr = 44100, freq = 440, phase = 0, modPhase = 0, ph[3] {};
    float velocity = 1, lp = 0, smoothMod = 0.7f, smoothExpr = 1.0f, ksDamp = 0.996f;
    int timbre = Keys, ksLen = 100, ksPos = 0;
    std::vector<float> ks;
};

class Synth
{
public:
    Synth()
    {
        for (int i = 0; i < 16; ++i) synth.addVoice (new Voice (shared));
        synth.addSound (new Sound());
    }
    void prepare (double sr) { synth.setCurrentPlaybackSampleRate (sr); }
    void render (juce::AudioBuffer<float>& b, const juce::MidiBuffer& m, int n) { synth.renderNextBlock (b, m, 0, n); }
    void allOff() { synth.allNotesOff (0, false); }
    Shared shared;
private:
    juce::Synthesiser synth;
};
} // namespace ps
