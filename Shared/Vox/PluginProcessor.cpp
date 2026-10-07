#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PresetManager.h"

chain::SlotData chain::defaultsFor (int type)
{
    SlotData d;
    d.type = type;
    if (type > 0 && type < (int) mods::catalogue().size())
    {
        const auto& ps = mods::catalogue()[(size_t) type].params;
        for (size_t k = 0; k < ps.size() && k < (size_t) mods::maxParams; ++k) d.norm[k] = ps[k].toNorm (ps[k].def);
    }
    return d;
}

juce::AudioProcessorValueTreeState::ParameterLayout VoxProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    auto gainParam = [&] (const char* id, const char* name)
    {
        l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 2 }, name, juce::NormalisableRange<float> (-24, 24), 0.0f,
               juce::AudioParameterFloatAttributes().withStringFromValueFunction (wl::valueToText ("dB", -24, 24))
                                                    .withValueFromStringFunction (wl::textToValue ("dB", -24, 24))));
    };
    gainParam ("in", "Input");
    gainParam ("out", "Output");
    const auto names = mods::typeNames();
    for (int s = 0; s < chain::numSlots; ++s)
    {
        const juce::String n = "Slot " + juce::String (s + 1);
        auto group = std::make_unique<juce::AudioProcessorParameterGroup> ("slot" + juce::String (s), n, " | ");
        group->addChild (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { chain::typeId (s), 2 }, n + " Module", names, 0));
        group->addChild (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { chain::onId (s), 2 }, n + " On", true));
        for (int k = 0; k < mods::maxParams; ++k)
            group->addChild (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { chain::paramId (s, k), 2 },
                             n + " Control " + juce::String (k + 1), juce::NormalisableRange<float> (0, 1), 0.5f));
        l.add (std::move (group));
    }
    return l;
}

VoxProcessor::VoxProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "WallChain", createLayout())
{
    for (int s = 0; s < chain::numSlots; ++s)
    {
        meters[s] = 0; meters2[s] = 0; notes[s] = -1;
        typeParams[s] = apvts.getRawParameterValue (chain::typeId (s));
        onParams[s] = apvts.getRawParameterValue (chain::onId (s));
        for (int k = 0; k < mods::maxParams; ++k) valueParams[s][k] = apvts.getRawParameterValue (chain::paramId (s, k));
        apvts.addParameterListener (chain::typeId (s), this);
    }
    presets = std::make_unique<PresetManager> (*this);
    presets->applyDefault();
}

VoxProcessor::~VoxProcessor()
{
    for (int s = 0; s < chain::numSlots; ++s) apvts.removeParameterListener (chain::typeId (s), this);
    cancelPendingUpdate();
}

bool VoxProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return l.getMainInputChannelSet() == out;
}

void VoxProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    maxBlock = juce::jmax (64, samplesPerBlock);
    dryBuf.setSize (2, maxBlock);
    {
        const juce::SpinLock::ScopedLockType sl (chainLock);
        for (auto& m : owned) if (m) m->prepare (sr, maxBlock);
    }
    syncModules();
    limGain = 1.0f;
}

// ------------------------------------------------------------------------------------ chain editing
void VoxProcessor::setParam (const juce::String& id, float norm)
{
    if (auto* prm = apvts.getParameter (id))
        if (std::abs (prm->getValue() - norm) > 1.0e-6f) prm->setValueNotifyingHost (norm);
}

int VoxProcessor::slotType (int s) const { return juce::roundToInt (typeParams[s]->load()); }

float VoxProcessor::realValue (int s, int k) const
{
    const int t = slotType (s);
    const auto& ps = mods::catalogue()[(size_t) juce::jlimit (0, (int) mods::catalogue().size() - 1, t)].params;
    return k < (int) ps.size() ? ps[(size_t) k].toValue (valueParams[s][k]->load()) : 0.0f;
}

chain::Chain VoxProcessor::getChain() const
{
    chain::Chain c;
    for (int s = 0; s < chain::numSlots; ++s)
    {
        const int t = slotType (s);
        if (t <= 0) continue;
        chain::SlotData d;
        d.type = t;
        d.on = onParams[s]->load() > 0.5f;
        for (int k = 0; k < mods::maxParams; ++k) d.norm[k] = valueParams[s][k]->load();
        c.push_back (d);
    }
    return c;
}

void VoxProcessor::setChain (const chain::Chain& wanted)
{
    // reuse existing module instances of the same type so moved modules keep their sound/tails
    std::unique_ptr<mods::Module> fresh[chain::numSlots];
    int source[chain::numSlots];
    bool used[chain::numSlots] {};
    for (int i = 0; i < chain::numSlots; ++i)
    {
        source[i] = -1;
        const int t = i < (int) wanted.size() ? wanted[(size_t) i].type : 0;
        if (t <= 0) continue;
        for (int j = 0; j < chain::numSlots; ++j)
            if (! used[j] && owned[j] && owned[j]->type == t) { used[j] = true; source[i] = j; break; }
        if (source[i] < 0)
        {
            fresh[i] = mods::create (t);
            if (fresh[i]) fresh[i]->prepare (sr, maxBlock);
        }
    }

    for (int i = 0; i < chain::numSlots; ++i)
    {
        const chain::SlotData d = i < (int) wanted.size() ? wanted[(size_t) i] : chain::SlotData {};
        auto* tp = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (chain::typeId (i)));
        setParam (chain::typeId (i), tp->convertTo0to1 ((float) d.type));
        setParam (chain::onId (i), d.on ? 1.0f : 0.0f);
        for (int k = 0; k < mods::maxParams; ++k) setParam (chain::paramId (i, k), d.type > 0 ? d.norm[k] : 0.5f);
    }

    std::vector<std::unique_ptr<mods::Module>> graveyard;
    {
        const juce::SpinLock::ScopedLockType sl (chainLock);
        std::unique_ptr<mods::Module> next[chain::numSlots];
        for (int i = 0; i < chain::numSlots; ++i)
            next[i] = source[i] >= 0 ? std::move (owned[source[i]]) : std::move (fresh[i]);
        for (int j = 0; j < chain::numSlots; ++j)
            if (owned[j]) graveyard.push_back (std::move (owned[j]));
        for (int i = 0; i < chain::numSlots; ++i)
        {
            owned[i] = std::move (next[i]);
            lastOn[i] = i < (int) wanted.size() && wanted[(size_t) i].on;
        }
    }
    graveyard.clear();
    updateLatency();
}

int VoxProcessor::addModule (int type)
{
    auto c = getChain();
    if ((int) c.size() >= chain::numSlots) return -1;
    c.push_back (chain::defaultsFor (type));
    setChain (c);
    return (int) c.size() - 1;
}

void VoxProcessor::removeModule (int pos)
{
    auto c = getChain();
    if (! juce::isPositiveAndBelow (pos, (int) c.size())) return;
    c.erase (c.begin() + pos);
    setChain (c);
}

void VoxProcessor::moveModule (int from, int to)
{
    auto c = getChain();
    if (! juce::isPositiveAndBelow (from, (int) c.size())) return;
    to = juce::jlimit (0, (int) c.size() - 1, to);
    if (from == to) return;
    auto item = c[(size_t) from];
    c.erase (c.begin() + from);
    c.insert (c.begin() + to, item);
    setChain (c);
}

void VoxProcessor::duplicateModule (int pos)
{
    auto c = getChain();
    if (! juce::isPositiveAndBelow (pos, (int) c.size()) || (int) c.size() >= chain::numSlots) return;
    c.insert (c.begin() + pos + 1, c[(size_t) pos]);
    setChain (c);
}

void VoxProcessor::replaceModule (int pos, int type)
{
    auto c = getChain();
    if (! juce::isPositiveAndBelow (pos, (int) c.size())) return;
    c[(size_t) pos] = chain::defaultsFor (type);
    setChain (c);
}

void VoxProcessor::syncModules()
{
    for (int s = 0; s < chain::numSlots; ++s)
    {
        const int want = slotType (s);
        const int have = owned[s] ? owned[s]->type : 0;
        if (want == have) continue;
        auto fresh = mods::create (want);
        if (fresh) fresh->prepare (sr, maxBlock);
        std::unique_ptr<mods::Module> old;
        {
            const juce::SpinLock::ScopedLockType sl (chainLock);
            old = std::move (owned[s]);
            owned[s] = std::move (fresh);
            lastOn[s] = onParams[s]->load() > 0.5f;
        }
    }
    updateLatency();
}

void VoxProcessor::updateLatency()
{
    int total = 0;
    for (auto& m : owned) if (m) total += m->latency();
    if (total != getLatencySamples()) setLatencySamples (total);
}

// ------------------------------------------------------------------------------------ audio
void VoxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nc = juce::jmin (2, getTotalNumOutputChannels(), buffer.getNumChannels());
    if (n == 0 || nc == 0) return;

    buffer.applyGain (juce::Decibels::decibelsToGain (p ("in")));
    const float inPeak = wl::peakOf (buffer, nc);

    mods::Context ctx;
    ctx.sampleRate = sr;
    double ppq = freePpq;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) ctx.bpm = juce::jlimit (40.0, 300.0, *b);
            ctx.playing = pos->getIsPlaying();
            if (ctx.playing) if (auto q = pos->getPpqPosition()) ppq = *q;
        }
    ctx.beatsPerSample = ctx.bpm / 60.0 / sr;
    freePpq = ppq + n * ctx.beatsPerSample;
    ctx.ppq = ppq - getLatencySamples() * ctx.beatsPerSample;

    int tunerNote = -1;
    {
        const juce::SpinLock::ScopedLockType sl (chainLock);
        // the first Tune module's key drives key-aware modules (e.g. Robot mode)
        for (int s = 0; s < chain::numSlots; ++s)
            if (owned[s] && juce::String (mods::catalogue()[(size_t) owned[s]->type].id) == "tune") { ctx.key = (int) realValue (s, 2); break; }

        for (int s = 0; s < chain::numSlots; ++s)
        {
            auto* m = owned[s].get();
            if (m == nullptr || m->type != slotType (s)) continue;
            const bool on = onParams[s]->load() > 0.5f;
            float v[mods::maxParams];
            const auto& ps = mods::catalogue()[(size_t) m->type].params;
            for (int k = 0; k < mods::maxParams; ++k) v[k] = k < (int) ps.size() ? ps[(size_t) k].toValue (valueParams[s][k]->load()) : 0.0f;

            if (m->handlesBypass()) m->process (buffer, nc, ctx, v, on);
            else if (on && lastOn[s]) m->process (buffer, nc, ctx, v, true);
            else if (on != lastOn[s] && n <= dryBuf.getNumSamples())
            {
                // click-free switch: crossfade between dry and processed over this block
                for (int c = 0; c < nc; ++c) dryBuf.copyFrom (c, 0, buffer, c, 0, n);
                m->process (buffer, nc, ctx, v, true);
                for (int c = 0; c < nc; ++c)
                {
                    auto* w = buffer.getWritePointer (c);
                    const auto* d = dryBuf.getReadPointer (c);
                    for (int i = 0; i < n; ++i)
                    {
                        const float r = (float) i / (float) n, a = on ? r : 1.0f - r;
                        w[i] = d[i] + a * (w[i] - d[i]);
                    }
                }
            }
            else if (on) m->process (buffer, nc, ctx, v, true);
            lastOn[s] = on;
            meters[s] = m->meter.load();
            meters2[s] = m->meter2.load();
            notes[s] = m->note.load();
            if (tunerNote < 0 && m->note.load() >= 0 && on) tunerNote = m->note.load();
        }
    }

    buffer.applyGain (juce::Decibels::decibelsToGain (p ("out")));

    {   // a module changed its latency (e.g. Tune engine switch): tell the host from the message thread
        int total = 0;
        for (auto& m : owned) if (m) total += m->latency();
        if (total != getLatencySamples() && ! isUpdatePending()) triggerAsyncUpdate();
    }

    // transparent safety limiter at -0.3 dBFS
    const float ceiling = 0.966f, rel = std::exp (-1.0f / (0.08f * (float) sr));
    for (int i = 0; i < n; ++i)
    {
        float pk = 0;
        for (int c = 0; c < nc; ++c) pk = std::max (pk, std::abs (buffer.getSample (c, i)));
        const float want = pk * limGain > ceiling ? ceiling / pk : 1.0f;
        limGain = want < limGain ? want : rel * limGain + (1 - rel) * std::min (1.0f, want);
        if (limGain < 0.9999f) for (int c = 0; c < nc; ++c) buffer.setSample (c, i, buffer.getSample (c, i) * limGain);
    }

    scope.push (inPeak, wl::peakOf (buffer, nc));
    scope.note = tunerNote;
    scope.bpm = (float) ctx.bpm;
}

// ------------------------------------------------------------------------------------ state
void VoxProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto st = apvts.copyState();
    st.setProperty ("preset", presets->currentName, nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, dest);
}

void VoxProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto st = juce::ValueTree::fromXml (*xml);
            presets->currentName = st.getProperty ("preset").toString();
            apvts.replaceState (st);
            if (juce::MessageManager::existsAndIsCurrentThread()) syncModules();
            else triggerAsyncUpdate();
        }
}

juce::AudioProcessorEditor* VoxProcessor::createEditor() { return new VoxEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VoxProcessor(); }
