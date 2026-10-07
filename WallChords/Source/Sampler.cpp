#include "Sampler.h"

namespace smp
{
juce::File soundsFolder()
{
    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    return base.getChildFile ("Application Support/WallChords/Sounds");
   #else
    return base.getChildFile ("WallChords/Sounds");
   #endif
}

bool soundPackInstalled()
{
    return soundsFolder().getChildFile ("grand_piano/instrument.json").existsAsFile();
}

std::shared_ptr<const Instrument> load (const juce::File& dir, juce::AudioFormatManager& fm)
{
    const auto v = juce::JSON::parse (dir.getChildFile ("instrument.json").loadFileAsString());
    auto* zones = v["zones"].getArray();
    if (zones == nullptr) return nullptr;

    auto inst = std::make_shared<Instrument>();
    inst->name = v["name"].toString();
    if (v.hasProperty ("attack"))  inst->attack = (float) v["attack"];
    if (v.hasProperty ("release")) inst->release = (float) v["release"];
    if (v.hasProperty ("gain"))    inst->gain = (float) v["gain"];
    inst->loop = (bool) v["loop"];

    for (auto& z : *zones)
    {
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (dir.getChildFile (z["file"].toString())));
        if (r == nullptr) continue;
        Zone zone;
        zone.root = (int) z["root"]; zone.lo = (int) z["lo"]; zone.hi = (int) z["hi"];
        zone.vlo = z.hasProperty ("vlo") ? (int) z["vlo"] : 0;
        zone.vhi = z.hasProperty ("vhi") ? (int) z["vhi"] : 127;
        zone.rate = r->sampleRate;
        const int len = (int) juce::jmin<juce::int64> (r->lengthInSamples, (juce::int64) (r->sampleRate * 20));
        zone.data.setSize ((int) juce::jmin (2u, r->numChannels), len + 1);
        zone.data.clear();
        r->read (&zone.data, 0, len, 0, true, zone.data.getNumChannels() > 1);

        if (inst->loop && len > (int) (zone.rate * 1.0))
        {
            // sustained instruments: loop the steady part with a pre-baked crossfade
            zone.loopStart = (int) (len * 0.4);
            zone.loopEnd = (int) (len * 0.88);
            const int xf = juce::jmin ((int) (zone.rate * 0.12), (zone.loopEnd - zone.loopStart) / 2, zone.loopStart);
            for (int ch = 0; ch < zone.data.getNumChannels(); ++ch)
            {
                auto* d = zone.data.getWritePointer (ch);
                for (int i = 0; i < xf; ++i)
                {
                    const float a = (float) i / (float) xf;
                    const int dst = zone.loopEnd - xf + i, src = zone.loopStart - xf + i;
                    d[dst] = d[dst] * (1.0f - a) + d[src] * a;
                }
            }
        }
        inst->zones.push_back (std::move (zone));
    }
    if (inst->zones.empty()) return nullptr;
    return inst;
}

// ---------------------------------------------------------------- voice
void Voice::startNote (int note, float vel, juce::SynthesiserSound*, int)
{
    {
        const juce::SpinLock::ScopedTryLockType tl (lock);
        if (tl.isLocked()) playing = inst;
    }
    zone = nullptr;
    if (playing == nullptr) { clearCurrentNote(); return; }

    const int v = juce::jlimit (1, 127, (int) (vel * 127.0f));
    int bestDist = 1000;
    for (auto& z : playing->zones)
    {
        const bool velOk = v >= z.vlo && v <= z.vhi;
        const int dist = (note >= z.lo && note <= z.hi ? 0 : 100 + std::abs (note - z.root)) + (velOk ? 0 : 50);
        if (dist < bestDist) { bestDist = dist; zone = &z; }
    }
    if (zone == nullptr) { clearCurrentNote(); return; }

    pos = 0;
    step = std::pow (2.0, (note - zone->root) / 12.0) * zone->rate / getSampleRate();
    gain = playing->gain * (0.15f + 0.85f * std::pow (vel, 1.4f)) * 0.5f;
    env.setSampleRate (getSampleRate());
    env.setParameters ({ playing->attack, 0.0f, 1.0f, playing->release });
    env.noteOn();
}

void Voice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff) env.noteOff();
    else { env.reset(); clearCurrentNote(); zone = nullptr; }
}

void Voice::renderNextBlock (juce::AudioBuffer<float>& out, int start, int num)
{
    if (zone == nullptr || ! isVoiceActive()) return;
    const auto& d = zone->data;
    const int len = d.getNumSamples() - 1;
    const int chans = d.getNumChannels();
    const bool looping = playing->loop && zone->loopEnd > zone->loopStart;

    for (int i = 0; i < num; ++i)
    {
        if (looping && pos >= zone->loopEnd) pos -= (zone->loopEnd - zone->loopStart);
        if (pos >= len) { clearCurrentNote(); zone = nullptr; return; }
        const int i0 = (int) pos;
        const float fr = (float) (pos - i0);
        const float e = env.getNextSample() * gain;
        for (int c = 0; c < out.getNumChannels(); ++c)
        {
            const auto* src = d.getReadPointer (juce::jmin (c, chans - 1));
            out.addSample (c, start + i, (src[i0] + fr * (src[i0 + 1] - src[i0])) * e);
        }
        pos += step;
        if (! env.isActive()) { clearCurrentNote(); zone = nullptr; return; }
    }
}

// ---------------------------------------------------------------- player
Player::Player()
{
    for (int i = 0; i < 32; ++i) sampler.addVoice (new Voice (current, lock));
    sampler.addSound (new ps::Sound());
}

void Player::prepare (double sr)
{
    sampler.setCurrentPlaybackSampleRate (sr);
    synthFallback.prepare (sr);
}

void Player::setInstrument (int index, std::shared_ptr<const Instrument> sampled)
{
    const auto& defs = instruments();
    const auto& d = defs[(size_t) juce::jlimit (0, (int) defs.size() - 1, index)];
    synthFallback.shared.timbre = d.fallback;
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        current = sampled;
    }
    useSampler = sampled != nullptr;
}

void Player::render (juce::AudioBuffer<float>& b, const juce::MidiBuffer& m, int n)
{
    if (useSampler.load()) sampler.renderNextBlock (b, m, 0, n);
    else synthFallback.render (b, m, n);
}

void Player::allOff()
{
    sampler.allNotesOff (0, true);
    synthFallback.allOff();
}

} // namespace smp
