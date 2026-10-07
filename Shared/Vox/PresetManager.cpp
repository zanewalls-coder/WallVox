#include "PresetManager.h"
#include "FactoryPresets.h"

#ifndef WALLVOX_PRESET_INDEX_URL
 #define WALLVOX_PRESET_INDEX_URL ""
#endif

PresetManager::PresetManager (VoxProcessor& p) : proc (p) { refresh(); }

juce::File PresetManager::getUserDir()
{
    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if WALL_CHOP
    const juce::String name ("WallChop");
   #else
    const juce::String name ("WallVox");
   #endif
   #if JUCE_MAC
    auto dir = base.getChildFile ("Application Support").getChildFile (name).getChildFile ("Presets");
   #else
    auto dir = base.getChildFile (name).getChildFile ("Presets");
   #endif
    dir.createDirectory();
    return dir;
}

bool PresetManager::isValid (const juce::var& v)
{
    if (v.getDynamicObject() == nullptr) return false;
    const auto name = v["name"];
    if (! name.isString() || name.toString().trim().isEmpty() || name.toString().length() > 60) return false;
    if (auto* c = v["chain"].getArray())
    {
        if (c->size() > chain::numSlots) return false;
        for (auto& m : *c)
            if (m.getDynamicObject() == nullptr || ! m["type"].isString()) return false;
        return true;
    }
    auto* params = v["params"].getDynamicObject();   // v1
    if (params == nullptr) return false;
    for (auto& prop : params->getProperties())
        if (! (prop.value.isDouble() || prop.value.isInt() || prop.value.isInt64())) return false;
    return true;
}

// ---------------------------------------------------------------------------- conversion
static chain::SlotData makeSlot (const juce::String& id, const juce::var& params, bool on = true)
{
    const int t = mods::indexOf (id);
    if (t <= 0) return {};
    auto d = chain::defaultsFor (t);
    d.on = on;
    const auto& ps = mods::catalogue()[(size_t) t].params;
    if (auto* obj = params.getDynamicObject())
        for (size_t k = 0; k < ps.size(); ++k)
        {
            const juce::Identifier key (ps[k].name);
            if (! obj->hasProperty (key)) continue;
            const auto val = obj->getProperty (key);
            float real;
            if (val.isString() && ps[k].isChoice()) real = (float) juce::jmax (0, ps[k].choices.indexOf (val.toString(), true));
            else real = (float) val;
            d.norm[k] = ps[k].toNorm (real);
        }
    return d;
}

static juce::var obj (std::initializer_list<std::pair<const char*, juce::var>> items)
{
    auto* o = new juce::DynamicObject();
    for (auto& [k, v] : items) o->setProperty (k, v);
    return juce::var (o);
}

// v1 presets (one fixed chain with on/off sections) -> equivalent modular chain
static chain::Chain fromV1 (const juce::var& prm)
{
    auto num = [&] (const char* k, float def) { return prm.hasProperty (k) ? (float) prm[k] : def; };
    auto on = [&] (const char* k) { return num (k, 1.0f) > 0.5f; };
    chain::Chain c;
    if (on ("onTune") && num ("tuneAmt", 0.5f) > 0.01f)
        c.push_back (makeSlot ("tune", obj ({ { "Speed", num ("tuneSpeed", 40) }, { "Amount", num ("tuneAmt", 0.5f) }, { "Natural", 0.2f } })));
    if (on ("onClean") || on ("onTone"))
        c.push_back (makeSlot ("eq", obj ({ { "Low Cut", num ("hpf", 90) }, { "Low", num ("body", 0) }, { "Low Mid Freq", 300.0f },
                                             { "Low Mid", num ("mud", -3) }, { "High Mid Freq", 3500.0f }, { "High Mid", num ("presence", 2.5f) },
                                             { "High", num ("air", 4) * 0.5f }, { "Air", num ("air", 4) * 0.6f } })));
    if (on ("onDyn"))
    {
        const float thr = num ("compThresh", -20), ratio = num ("compRatio", 4);
        c.push_back (makeSlot ("comp", obj ({ { "Threshold", thr }, { "Ratio", ratio }, { "Attack", num ("compAttack", 5) },
                                               { "Release", num ("compRelease", 80) }, { "Knee", 6.0f }, { "Makeup", -thr * (1 - 1 / ratio) * 0.5f } })));
    }
    if (on ("onClean") && num ("deess", 0.5f) > 0.01f)
        c.push_back (makeSlot ("deess", obj ({ { "Threshold", -18 - 24 * num ("deess", 0.5f) }, { "Range", 10.0f } })));
    if (on ("onSat") && num ("sat", 0.15f) > 0.01f)
        c.push_back (makeSlot ("sat", obj ({ { "Type", num ("satType", 0) }, { "Drive", num ("sat", 0.15f) }, { "Mix", num ("satMix", 1) } })));
   #if WALL_CHOP
    if (num ("onBender", 0) > 0.5f)
        c.push_back (makeSlot ("bender", obj ({ { "Mode", num ("bendMode", 0) }, { "Pitch", num ("bendPitch", 0) }, { "Formant", num ("bendFormant", 0) }, { "Mix", num ("bendMix", 1) } })));
    if (num ("onStutter", 0) > 0.5f)
        c.push_back (makeSlot ("stutter", obj ({ { "Repeat", num ("stutInterval", 2) }, { "Length", num ("stutLength", 1) }, { "Slice", num ("stutRate", 2) },
                                                  { "Pitch Drop", num ("stutDrop", 0) }, { "Decay", num ("stutDecay", 0) }, { "Mix", num ("stutMix", 1) } })));
    if (num ("onGate", 0) > 0.5f)
        c.push_back (makeSlot ("gate", obj ({ { "Pattern", num ("gatePattern", 2) }, { "Rate", num ("gateRate", 1) }, { "Length", num ("gateLen", 0.6f) }, { "Depth", num ("gateDepth", 1) } })));
    if (num ("onFilter", 0) > 0.5f)
        c.push_back (makeSlot ("filter", obj ({ { "Type", num ("filtType", 0) }, { "Cutoff", num ("filtCutoff", 2000) }, { "Resonance", num ("filtReso", 0.3f) },
                                                 { "Sweep Rate", num ("filtLfoRate", 0) }, { "Sweep", num ("filtLfoDepth", 0) } })));
   #endif
    if (on ("onSpace") && num ("double", 0.15f) > 0.01f)
        c.push_back (makeSlot ("double", obj ({ { "Amount", num ("double", 0.15f) } })));
    if (on ("onDelay") && num ("dlyMix", 0.12f) > 0.005f)
        c.push_back (makeSlot ("delay", obj ({ { "Time", num ("dlyTime", 1) }, { "Feedback", num ("dlyFb", 0.25f) }, { "Mix", num ("dlyMix", 0.12f) } })));
    if (on ("onSpace") && num ("revMix", 0.18f) > 0.005f)
        c.push_back (makeSlot ("reverb", obj ({ { "Decay", 0.8f + 4.0f * num ("revSize", 0.55f) }, { "Size", num ("revSize", 0.55f) },
                                                 { "High Cut", 12000 - 8000 * num ("revDamp", 0.4f) }, { "Mix", num ("revMix", 0.18f) * 0.6f } })));
    if ((int) c.size() > chain::numSlots) c.resize ((size_t) chain::numSlots);
    return c;
}

chain::Chain PresetManager::toChain (const juce::var& v, float& inDb, float& outDb)
{
    inDb = v.hasProperty ("in") ? (float) v["in"] : 0.0f;
    outDb = v.hasProperty ("out") ? (float) v["out"] : 0.0f;
    if (auto* arr = v["chain"].getArray())
    {
        chain::Chain c;
        for (auto& m : *arr)
        {
            auto d = makeSlot (m["type"].toString(), m["params"], m.hasProperty ("on") ? (bool) m["on"] : true);
            if (d.type > 0 && (int) c.size() < chain::numSlots) c.push_back (d);
        }
        return c;
    }
    const auto prm = v["params"];
    if (prm.hasProperty ("inGain")) inDb = (float) prm["inGain"];
    if (prm.hasProperty ("outGain")) outDb = (float) prm["outGain"];
    return fromV1 (prm);
}

// ---------------------------------------------------------------------------- list / apply / save
void PresetManager::refresh()
{
    list.clear();
    const auto factory = juce::JSON::parse (juce::String::fromUTF8 (FactoryPresets::factory_presets_json, FactoryPresets::factory_presets_jsonSize));
    if (auto* arr = factory.getArray())
        for (auto& v : *arr)
            if (isValid (v)) list.add ({ v["name"].toString(), v["category"].toString(), v, true });

    auto files = getUserDir().findChildFiles (juce::File::findFiles, false, "*.json");
    files.sort();
    for (auto& f : files)
    {
        if (f.getSize() > 65536) continue;
        auto v = juce::JSON::parse (f.loadFileAsString());
        if (! isValid (v)) continue;
        auto cat = v["category"].toString();
        list.add ({ v["name"].toString(), cat.isEmpty() ? juce::String ("User") : cat, v, false });
    }
}

void PresetManager::apply (int index)
{
    if (! juce::isPositiveAndBelow (index, list.size())) return;
    const auto& pr = list.getReference (index);
    float in = 0, out = 0;
    const auto c = toChain (pr.data, in, out);
    proc.setChain (c);
    if (auto* p = proc.apvts.getParameter ("in")) p->setValueNotifyingHost (p->convertTo0to1 (in));
    if (auto* p = proc.apvts.getParameter ("out")) p->setValueNotifyingHost (p->convertTo0to1 (out));
    currentName = pr.name;
}

void PresetManager::applyDefault()
{
    for (int i = 0; i < list.size(); ++i)
        if (list[i].factory && list[i].category != "Start") { apply (i); return; }
    if (! list.isEmpty()) apply (0);
}

bool PresetManager::saveCurrent (const juce::String& name)
{
    juce::Array<juce::var> items;
    for (auto& d : proc.getChain())
    {
        const auto& info = mods::catalogue()[(size_t) d.type];
        auto* params = new juce::DynamicObject();
        for (size_t k = 0; k < info.params.size(); ++k)
        {
            const auto& ps = info.params[k];
            const float v = ps.toValue (d.norm[k]);
            if (ps.isChoice()) params->setProperty (ps.name, ps.choices[juce::roundToInt (v)]);
            else params->setProperty (ps.name, std::round (v * 1000.0f) / 1000.0f);
        }
        items.add (obj ({ { "type", juce::String (info.id) }, { "on", d.on }, { "params", juce::var (params) } }));
    }
    auto root = obj ({ { "name", name.substring (0, 60) }, { "category", "User" },
                       { "in", proc.apvts.getParameter ("in")->convertFrom0to1 (proc.apvts.getParameter ("in")->getValue()) },
                       { "out", proc.apvts.getParameter ("out")->convertFrom0to1 (proc.apvts.getParameter ("out")->getValue()) },
                       { "chain", items } });
    auto file = getUserDir().getChildFile (juce::File::createLegalFileName (name) + ".json");
    if (! file.replaceWithText (juce::JSON::toString (root))) return false;
    refresh();
    currentName = name;
    return true;
}

juce::String PresetManager::importFile (const juce::File& f)
{
    if (! f.existsAsFile() || f.getSize() > 65536) return "That file isn't a preset.";
    const auto text = f.loadFileAsString();
    if (! isValid (juce::JSON::parse (text))) return "That file isn't a preset.";
    if (! getUserDir().getChildFile (juce::File::createLegalFileName (f.getFileName())).replaceWithText (text))
        return "Couldn't save the preset.";
    refresh();
    return {};
}

void PresetManager::downloadOnline (std::function<void (juce::String)> onDone)
{
    const juce::String indexUrl (WALLVOX_PRESET_INDEX_URL);
    auto finish = [onDone] (juce::String msg) { juce::MessageManager::callAsync ([onDone, msg] { onDone (msg); }); };
    if (indexUrl.isEmpty()) { finish ("Online presets aren't set up in this build."); return; }

    juce::Thread::launch ([indexUrl, finish]
    {
        const auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress).withConnectionTimeoutMs (8000);
        auto fetch = [&opts] (const juce::String& url) -> juce::String
        {
            if (auto s = juce::URL (url).createInputStream (opts)) return s->readEntireStreamAsString();
            return {};
        };
        auto index = juce::JSON::parse (fetch (indexUrl));
        auto* files = index["presets"].getArray();
        if (files == nullptr) { finish ("Couldn't reach the preset server. Check your internet."); return; }

        const auto base = indexUrl.upToLastOccurrenceOf ("/", true, false);
        const auto dir = getUserDir();
        int added = 0;
        for (auto& entry : *files)
        {
            const auto name = entry.toString();
            if (! name.endsWith (".json") || name != juce::File::createLegalFileName (name)
                || name.containsAnyOf ("/\\ ") || name.contains ("..")) continue;
            auto target = dir.getChildFile (name);
            if (target.existsAsFile()) continue;
            const auto text = fetch (base + name);
            if (text.length() > 65536 || ! isValid (juce::JSON::parse (text))) continue;
            if (target.replaceWithText (text)) ++added;
        }
        finish (added == 0 ? juce::String ("You already have every online preset.")
                           : juce::String (added) + " new preset" + (added == 1 ? "" : "s") + " added!");
    });
}
