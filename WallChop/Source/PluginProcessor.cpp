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

juce::AudioProcessorValueTreeState::ParameterLayout WallChopProcessor::createLayout()
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
    l.add (fp ("sat", "Drive", 0, 1, 0.15f));
    l.add (std::make_unique<APC> (juce::ParameterID { "satType", 1 }, "Sat Type",
             juce::StringArray { "Warm", "Tube", "Crunch" }, 0));
    l.add (fp ("satMix", "Sat Mix", 0, 1, 1.0f));
    for (auto* id : { "onTune", "onClean", "onTone", "onSat", "onDyn", "onDelay", "onSpace" })
        l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 },
                 juce::String (id).fromFirstOccurrenceOf ("on", false, false) + " On", true));
    // Chop sections start switched off
    for (auto* id : { "onBender", "onStutter", "onGate", "onFilter", "onTapeStop" })
        l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 },
                 juce::String (id).fromFirstOccurrenceOf ("on", false, false) + " On", false));

    l.add (fp ("bendPitch", "Pitch", -12, 12, 0, "st"));
    l.add (fp ("bendFormant", "Formant", -12, 12, 0, "st"));
    l.add (fp ("bendMix", "Bend Mix", 0, 1, 1));
    l.add (std::make_unique<APC> (juce::ParameterID { "bendMode", 1 }, "Mode", juce::StringArray { "Shift", "Robot" }, 0));

    l.add (std::make_unique<APC> (juce::ParameterID { "stutInterval", 1 }, "Repeat",
             juce::StringArray { "Always", "Every 1/2 Bar", "Every Bar", "Every 2 Bars" }, 2));
    l.add (std::make_unique<APC> (juce::ParameterID { "stutLength", 1 }, "Length",
             juce::StringArray { "1/8", "1/4", "1/2", "1 Bar", "2 Bars" }, 1));
    l.add (std::make_unique<APC> (juce::ParameterID { "stutRate", 1 }, "Slice",
             juce::StringArray { "1/4", "1/8", "1/16", "1/32", "1/8 T", "1/16 T" }, 2));
    l.add (fp ("stutDrop", "Pitch Drop", 0, 12, 0, "st"));
    l.add (fp ("stutDecay", "Decay", 0, 1, 0));
    l.add (fp ("stutMix", "Stutter Mix", 0, 1, 1));

    l.add (std::make_unique<APC> (juce::ParameterID { "gatePattern", 1 }, "Pattern", wc::Gate::patternNames(), 2));
    l.add (std::make_unique<APC> (juce::ParameterID { "gateRate", 1 }, "Gate Rate",
             juce::StringArray { "1/8", "1/16", "1/32" }, 1));
    l.add (fp ("gateLen", "Gate Length", 0.1f, 1, 0.6f));
    l.add (fp ("gateDepth", "Gate Depth", 0, 1, 1));

    l.add (std::make_unique<APC> (juce::ParameterID { "filtType", 1 }, "Filter Type",
             juce::StringArray { "Low Pass", "High Pass", "Band Pass" }, 0));
    l.add (fp ("filtCutoff", "Cutoff", 20, 20000, 2000, "Hz", 1000));
    l.add (fp ("filtReso", "Resonance", 0, 1, 0.3f));
    l.add (std::make_unique<APC> (juce::ParameterID { "filtLfoRate", 1 }, "Sweep Rate",
             juce::StringArray { "1 Bar", "1/2", "1/4", "1/8", "1/16" }, 0));
    l.add (fp ("filtLfoDepth", "Sweep", 0, 1, 0));

    l.add (fp ("tapeTime", "Stop Time", 100, 3000, 900, "ms", 800));
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

WallChopProcessor::WallChopProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "WallChop", createLayout()),
      presets (apvts)
{
    presets.apply (0);
}

bool WallChopProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return l.getMainInputChannelSet() == out;
}

void WallChopProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    tuner.prepare (sr, 2);
    bender.prepare (sr);
    stutter.prepare (sr);
    tapeStop.prepare (sr);
    gate.prepare (sr);
    sweep.prepare (sr);
    setLatencySamples (tuner.getLatency() + bender.getLatency());

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

void WallChopProcessor::updateFilters()
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

void WallChopProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nc = juce::jmin (2, getTotalNumOutputChannels(), buffer.getNumChannels());
    if (nc == 0 || n == 0) return;
    float* const* ch = buffer.getArrayOfWritePointers();

    updateFilters();
    auto on = [this] (const char* id) { return p (id) > 0.5f; };
    const bool onClean = on ("onClean"), onTone = on ("onTone"), onSat = on ("onSat"),
               onDelay = on ("onDelay"), onSpace = on ("onSpace");
    const bool onBender = on ("onBender");
    bender.setParams (p ("bendPitch"), p ("bendFormant"), p ("bendMix"), (int) p ("bendMode") == 1, (int) p ("key"));
    tuner.setParams (on ("onTune") ? p ("tuneAmt") : 0.0f, p ("tuneSpeed"), (int) p ("key"), (int) p ("scale"));
    buffer.applyGain (juce::Decibels::decibelsToGain (p ("inGain")));

    // ---- Stage 1: clean-up, tuning, de-essing ----
    const float deessAmt = p ("deess");
    const float deessThr = -18.0f - 24.0f * deessAmt;
    const float atk = std::exp (-1.0f / (0.001f * (float) sr));
    const float rel = std::exp (-1.0f / (0.06f * (float) sr));

    for (int i = 0; i < n; ++i)
    {
        float fr[2] {};
        for (int c = 0; c < nc; ++c) fr[c] = onClean ? hpf.process (ch[c][i], c) : ch[c][i];
        tuner.processFrame (fr, nc);
        bender.processFrame (fr, nc, onBender);

        if (! onClean)
        {
            for (int c = 0; c < nc; ++c) ch[c][i] = fr[c];
            continue;
        }

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
    if (on ("onDyn"))
    {
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
    }

    // ---- Chop stage: stutter, tape stop, gate, filter (tempo-synced) ----
    double bpm = 120.0, ppq = freePpq;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = juce::jlimit (40.0, 300.0, *b);
            if (pos->getIsPlaying())
                if (auto q = pos->getPpqPosition()) ppq = *q;
        }
    const double beatsPerSample = bpm / 60.0 / sr;
    freePpq = ppq + n * beatsPerSample;
    ppq -= getLatencySamples() * beatsPerSample;   // align with the audio that reaches this stage

    {
        static const double intervals[] = { 0, 2, 4, 8 }, lengths[] = { 0.5, 1, 2, 4, 8 },
                            slices[] = { 1, 0.5, 0.25, 0.125, 1.0 / 3.0, 1.0 / 6.0 },
                            gateSteps[] = { 0.5, 0.25, 0.125 }, lfoBeats[] = { 4, 2, 1, 0.5, 0.25 };
        const bool onStut = on ("onStutter"), onGate = on ("onGate"), onFilt = on ("onFilter"), onTape = on ("onTapeStop");
        const double sInt = intervals[(int) p ("stutInterval")], sLen = lengths[(int) p ("stutLength")],
                     sSlice = slices[(int) p ("stutRate")];
        const float sDrop = p ("stutDrop"), sDecay = p ("stutDecay"), sMix = p ("stutMix");
        const int gPat = (int) p ("gatePattern");
        const double gStep = gateSteps[(int) p ("gateRate")];
        const float gLen = p ("gateLen"), gDepth = p ("gateDepth");
        const int fType = (int) p ("filtType");
        const float fCut = p ("filtCutoff"), fRes = p ("filtReso"), fDepth = p ("filtLfoDepth");
        const double fLfo = lfoBeats[(int) p ("filtLfoRate")];
        const float tTime = p ("tapeTime");

        for (int i = 0; i < n; ++i)
        {
            const double q = ppq + i * beatsPerSample;
            float fr[2] {};
            for (int c = 0; c < nc; ++c) fr[c] = ch[c][i];
            stutter.process (fr, nc, q, bpm, onStut, sInt, sLen, sSlice, sDrop, sDecay, sMix);
            tapeStop.process (fr, nc, onTape, tTime);
            const float gg = gate.gain (q, gPat, gStep, gLen, onGate ? gDepth : 0.0f);
            for (int c = 0; c < nc; ++c) fr[c] *= gg;
            if (onFilt) sweep.process (fr, nc, q, fType, fCut, fRes, fLfo, fDepth);
            for (int c = 0; c < nc; ++c) ch[c][i] = fr[c];
        }
    }

    // ---- Stage 3: tone, saturation, doubler, delay ----
    static const float beats[] = { 1.0f, 0.5f, 0.75f, 0.25f, 1.5f };
    const float targetDly = juce::jmin ((float) (beats[(int) p ("dlyTime")] * 60.0 / bpm * sr), (float) (sr * 2.0));

    const float sat = p ("sat"), satMix = p ("satMix");
    const int satType = (int) p ("satType");
    const float k = 1.0f + sat * (satType == 2 ? 40.0f : 5.0f), normK = 1.0f / std::tanh (k);
    const float tubeBias = 0.25f, tubeOff = std::tanh (k * tubeBias);
    const float crunchMakeup = 1.0f / std::sqrt (k);
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
            float v = ch[c][i];
            if (onTone) v = air.process (presence.process (body.process (v, c), c), c);
            if (onSat && sat > 0.0f)
            {
                float y;
                if (satType == 0)      y = v + sat * (std::tanh (k * v) * normK - v);               // warm tape
                else if (satType == 1) y = v + sat * ((std::tanh (k * (v + tubeBias)) - tubeOff) * normK - v); // tube (even harmonics)
                else                   y = juce::jlimit (-0.7f, 0.7f, k * v) * crunchMakeup * 1.4f;  // hard-clip crunch
                v += satMix * (y - v);
            }
            x[c] = v;
            mono += v;
        }
        mono /= (float) nc;
        const float dblIn = onSpace ? mono : 0.0f, dlyIn = onDelay ? mono : 0.0f;

        // Doubler: two slowly modulated short delays, one per side
        for (int c = 0; c < nc; ++c)
        {
            const float d = (dblBase[c] + 1.5f * std::sin (juce::MathConstants<float>::twoPi * dblPhase[c])) * msToS;
            const float wet = dblLine[c].read (d);
            dblLine[c].push (dblIn);
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
            dlyLine[0].push (dlyIn + fb * fL);
            dlyLine[1].push (fb * fR);
            revBuf.setSample (0, i, onSpace ? x[0] : 0.0f);
            revBuf.setSample (1, i, onSpace ? x[1] : 0.0f);
            ch[0][i] = x[0] + dlyMix * oL;
            ch[1][i] = x[1] + dlyMix * oR;
        }
        else
        {
            const float o = dlyLine[0].read (dlySamples);
            dlyLine[0].push (dlyIn + fb * dlyLp.process (dlyHp.process (o, 0), 0));
            revBuf.setSample (0, i, onSpace ? x[0] : 0.0f);
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

void WallChopProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto st = apvts.copyState();
    st.setProperty ("preset", presets.currentName, nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, dest);
}

void WallChopProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto st = juce::ValueTree::fromXml (*xml);
            presets.currentName = st.getProperty ("preset").toString();
            apvts.replaceState (st);
        }
}

juce::AudioProcessorEditor* WallChopProcessor::createEditor() { return new WallChopEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WallChopProcessor(); }
