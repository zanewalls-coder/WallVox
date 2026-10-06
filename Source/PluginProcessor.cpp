#include "PluginProcessor.h"
#include "PluginEditor.h"

using APF = juce::AudioParameterFloat;
using APC = juce::AudioParameterChoice;

static std::unique_ptr<APF> fp (const char* id, const char* name, float lo, float hi, float def,
                                const char* unit = "", float skewCentre = 0.0f)
{
    juce::NormalisableRange<float> r (lo, hi);
    if (skewCentre > 0.0f) r.setSkewForCentre (skewCentre);
    return std::make_unique<APF> (juce::ParameterID { id, 1 }, name, r, def,
                                  juce::AudioParameterFloatAttributes().withLabel (unit));
}

juce::AudioProcessorValueTreeState::ParameterLayout WallVoxProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add (fp ("inGain", "Input", -24, 24, 0, "dB"));
    l.add (fp ("hpf", "Low Cut", 20, 400, 90, "Hz", 100));
    l.add (fp ("tuneAmt", "Tune", 0, 1, 0.5f));
    l.add (fp ("tuneSpeed", "Speed", 0, 200, 40, "ms", 40));
    l.add (std::make_unique<APC> (juce::ParameterID { "key", 1 }, "Key",
             juce::StringArray { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" }, 0));
    l.add (std::make_unique<APC> (juce::ParameterID { "scale", 1 }, "Scale",
             juce::StringArray { "Chromatic", "Major", "Minor" }, 0));
    l.add (fp ("mud", "Mud", -12, 6, -3, "dB"));
    l.add (fp ("deess", "De-Ess", 0, 1, 0.5f));
    l.add (fp ("body", "Body", -6, 6, 0, "dB"));
    l.add (fp ("presence", "Presence", -6, 12, 2.5f, "dB"));
    l.add (fp ("air", "Air", -6, 12, 4, "dB"));
    l.add (fp ("sat", "Warmth", 0, 1, 0.15f));
    l.add (fp ("compThresh", "Threshold", -40, 0, -20, "dB"));
    l.add (fp ("compRatio", "Ratio", 1, 20, 4, ":1", 4));
    l.add (fp ("compAttack", "Attack", 0.1f, 100, 5, "ms", 10));
    l.add (fp ("compRelease", "Release", 10, 500, 80, "ms", 100));
    l.add (fp ("double", "Doubler", 0, 1, 0.15f));
    l.add (fp ("dlyMix", "Delay", 0, 1, 0.12f));
    l.add (std::make_unique<APC> (juce::ParameterID { "dlyTime", 1 }, "Delay Time",
             juce::StringArray { "1/4", "1/8", "1/8 Dotted", "1/16", "1/4 Dotted" }, 1));
    l.add (fp ("dlyFb", "Feedback", 0, 0.9f, 0.25f));
    l.add (fp ("revMix", "Reverb", 0, 1, 0.18f));
    l.add (fp ("revSize", "Size", 0, 1, 0.55f));
    l.add (fp ("revDamp", "Damping", 0, 1, 0.4f));
    l.add (fp ("outGain", "Output", -24, 24, 0, "dB"));
    return l;
}

WallVoxProcessor::WallVoxProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "WallVox", createLayout()),
      presets (apvts)
{
    presets.apply (0);
}

bool WallVoxProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return l.getMainInputChannelSet() == out;
}

void WallVoxProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    tuner.prepare (sr, 2);
    setLatencySamples (tuner.getLatency());

    juce::dsp::ProcessSpec spec { sr, (juce::uint32) samplesPerBlock, 2 };
    comp.prepare (spec);
    limiter.prepare (spec);
    limiter.setThreshold (-0.5f);
    limiter.setRelease (60.0f);

    for (int c = 0; c < 2; ++c)
    {
        dblLine[c].prepare ((int) (sr * 0.05));
        dlyLine[c].prepare ((int) (sr * 2.1));
        dblPhase[c] = c * 0.37f;
    }
    dlySamples = (float) (sr * 0.25);

    reverb.setSampleRate (sr);
    reverb.reset();
    revBuf.setSize (2, samplesPerBlock);

    for (auto* b : { &hpf, &mud, &deessSplit, &deessSide, &body, &presence, &air, &dlyHp, &dlyLp })
        b->reset();
    deessEnv = 0.0f;
    updateFilters();
}

void WallVoxProcessor::updateFilters()
{
    hpf.highpass (sr, p ("hpf"));
    mud.peak (sr, 300.0, 1.0, p ("mud"));
    deessSplit.highpass (sr, 5500.0);
    deessSide.highpass (sr, 6500.0);
    body.lowShelf (sr, 180.0, p ("body"));
    presence.peak (sr, 3500.0, 0.8, p ("presence"));
    air.highShelf (sr, 10000.0, p ("air"));
    dlyHp.highpass (sr, 300.0);
    dlyLp.lowpass (sr, 5000.0);
}

void WallVoxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nc = juce::jmin (2, getTotalNumOutputChannels(), buffer.getNumChannels());
    if (nc == 0 || n == 0) return;
    float* const* ch = buffer.getArrayOfWritePointers();

    updateFilters();
    tuner.setParams (p ("tuneAmt"), p ("tuneSpeed"), (int) p ("key"), (int) p ("scale"));
    buffer.applyGain (juce::Decibels::decibelsToGain (p ("inGain")));

    // ---- Stage 1: clean-up, tuning, de-essing ----
    const float deessAmt = p ("deess");
    const float deessThr = -18.0f - 24.0f * deessAmt;
    const float atk = std::exp (-1.0f / (0.001f * (float) sr));
    const float rel = std::exp (-1.0f / (0.06f * (float) sr));

    for (int i = 0; i < n; ++i)
    {
        float fr[2] {};
        for (int c = 0; c < nc; ++c) fr[c] = hpf.process (ch[c][i], c);
        tuner.processFrame (fr, nc);

        float side = 0.0f;
        for (int c = 0; c < nc; ++c)
        {
            fr[c] = mud.process (fr[c], c);
            side += deessSide.process (fr[c], c);
        }
        const float lvl = std::abs (side / (float) nc);
        deessEnv = lvl > deessEnv ? atk * deessEnv + (1 - atk) * lvl : rel * deessEnv + (1 - rel) * lvl;
        const float over = juce::Decibels::gainToDecibels (deessEnv, -120.0f) - deessThr;
        const float g = (deessAmt > 0.0f && over > 0.0f)
                          ? juce::Decibels::decibelsToGain (-juce::jmin (12.0f, over * 0.7f)) : 1.0f;

        for (int c = 0; c < nc; ++c)
            ch[c][i] = fr[c] + deessSplit.process (fr[c], c) * (g - 1.0f);
    }

    // ---- Stage 2: compression ----
    const float thr = p ("compThresh"), ratio = p ("compRatio");
    comp.setThreshold (thr);
    comp.setRatio (ratio);
    comp.setAttack (p ("compAttack"));
    comp.setRelease (p ("compRelease"));
    {
        juce::dsp::AudioBlock<float> block (buffer);
        auto sub = block.getSubsetChannelBlock (0, (size_t) nc);
        comp.process (juce::dsp::ProcessContextReplacing<float> (sub));
    }
    buffer.applyGain (juce::Decibels::decibelsToGain (-thr * (1.0f - 1.0f / ratio) * 0.5f));

    // ---- Stage 3: tone, saturation, doubler, delay ----
    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) bpm = juce::jlimit (40.0, 300.0, *b);
    static const float beats[] = { 1.0f, 0.5f, 0.75f, 0.25f, 1.5f };
    const float targetDly = juce::jmin ((float) (beats[(int) p ("dlyTime")] * 60.0 / bpm * sr), (float) (sr * 2.0));

    const float sat = p ("sat"), k = 1.0f + sat * 5.0f, normK = 1.0f / std::tanh (k);
    const float dbl = p ("double"), dlyMix = p ("dlyMix"), fb = p ("dlyFb");
    const float dblRate[2] = { 0.40f / (float) sr, 0.33f / (float) sr };
    const float dblBase[2] = { 11.0f, 16.0f };
    const float msToS = 0.001f * (float) sr;

    revBuf.setSize (2, n, false, false, true);

    for (int i = 0; i < n; ++i)
    {
        float x[2] {}, mono = 0.0f;
        for (int c = 0; c < nc; ++c)
        {
            float v = air.process (presence.process (body.process (ch[c][i], c), c), c);
            v += sat * (std::tanh (k * v) * normK - v);
            x[c] = v;
            mono += v;
        }
        mono /= (float) nc;

        // Doubler: two slowly modulated short delays, one per side
        for (int c = 0; c < nc; ++c)
        {
            const float d = (dblBase[c] + 1.5f * std::sin (juce::MathConstants<float>::twoPi * dblPhase[c])) * msToS;
            const float wet = dblLine[c].read (d);
            dblLine[c].push (mono);
            dblPhase[c] += dblRate[c];
            if (dblPhase[c] >= 1.0f) dblPhase[c] -= 1.0f;
            x[c] += dbl * 0.7f * wet;
        }

        // Tempo-synced (ping-pong when stereo) delay
        dlySamples += (targetDly - dlySamples) * 0.0005f;
        if (nc == 2)
        {
            const float oL = dlyLine[0].read (dlySamples), oR = dlyLine[1].read (dlySamples);
            const float fL = dlyLp.process (dlyHp.process (oR, 0), 0);
            const float fR = dlyLp.process (dlyHp.process (oL, 1), 1);
            dlyLine[0].push (mono + fb * fL);
            dlyLine[1].push (fb * fR);
            revBuf.setSample (0, i, x[0]);
            revBuf.setSample (1, i, x[1]);
            ch[0][i] = x[0] + dlyMix * oL;
            ch[1][i] = x[1] + dlyMix * oR;
        }
        else
        {
            const float o = dlyLine[0].read (dlySamples);
            dlyLine[0].push (mono + fb * dlyLp.process (dlyHp.process (o, 0), 0));
            revBuf.setSample (0, i, x[0]);
            ch[0][i] = x[0] + dlyMix * o;
        }
    }

    // ---- Stage 4: reverb ----
    juce::Reverb::Parameters rp;
    rp.roomSize = 0.5f + 0.48f * p ("revSize");
    rp.damping = p ("revDamp");
    rp.wetLevel = 1.0f;
    rp.dryLevel = 0.0f;
    rp.width = 1.0f;
    reverb.setParameters (rp);
    if (nc == 2) reverb.processStereo (revBuf.getWritePointer (0), revBuf.getWritePointer (1), n);
    else         reverb.processMono (revBuf.getWritePointer (0), n);
    const float revMix = p ("revMix") * 0.5f;
    for (int c = 0; c < nc; ++c)
        buffer.addFrom (c, 0, revBuf, c, 0, n, revMix);

    // ---- Stage 5: output ----
    buffer.applyGain (juce::Decibels::decibelsToGain (p ("outGain")));
    juce::dsp::AudioBlock<float> block (buffer);
    auto sub = block.getSubsetChannelBlock (0, (size_t) nc);
    limiter.process (juce::dsp::ProcessContextReplacing<float> (sub));
}

void WallVoxProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto st = apvts.copyState();
    st.setProperty ("preset", presets.currentName, nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, dest);
}

void WallVoxProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto st = juce::ValueTree::fromXml (*xml);
            presets.currentName = st.getProperty ("preset").toString();
            apvts.replaceState (st);
        }
}

juce::AudioProcessorEditor* WallVoxProcessor::createEditor() { return new WallVoxEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WallVoxProcessor(); }
