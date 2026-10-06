#include "PresetManager.h"

#ifndef WALLVOX_PRESET_INDEX_URL
 #define WALLVOX_PRESET_INDEX_URL ""
#endif

// Built-in presets. Missing parameters fall back to their defaults; Key and Scale are never changed by presets.
static const char* const factoryPresets[] = {
R"P({"name":"Pop Lead","category":"Pop","params":{"hpf":90,"tuneAmt":0.5,"tuneSpeed":40,"mud":-3,"deess":0.5,"body":0,"presence":2.5,"air":4,"sat":0.15,"compThresh":-20,"compRatio":4,"compAttack":5,"compRelease":80,"double":0.15,"dlyMix":0.12,"dlyTime":1,"dlyFb":0.25,"revMix":0.18,"revSize":0.55,"revDamp":0.4}})P",
R"P({"name":"Pop Airy Ballad","category":"Pop","params":{"hpf":85,"tuneAmt":0.35,"tuneSpeed":60,"mud":-2,"deess":0.55,"body":0.5,"presence":1.5,"air":6,"sat":0.1,"compThresh":-22,"compRatio":3,"compAttack":10,"compRelease":120,"double":0.1,"dlyMix":0.1,"dlyTime":0,"dlyFb":0.3,"revMix":0.3,"revSize":0.8,"revDamp":0.3}})P",
R"P({"name":"EDM Topline","category":"EDM","params":{"hpf":120,"tuneAmt":0.85,"tuneSpeed":8,"mud":-4,"deess":0.6,"body":0,"presence":3,"air":6,"sat":0.25,"compThresh":-24,"compRatio":6,"compAttack":3,"compRelease":60,"double":0.35,"dlyMix":0.22,"dlyTime":2,"dlyFb":0.4,"revMix":0.3,"revSize":0.85,"revDamp":0.25}})P",
R"P({"name":"EDM Festival Chop","category":"EDM","params":{"hpf":150,"tuneAmt":1,"tuneSpeed":0,"mud":-4,"deess":0.6,"body":-1,"presence":4,"air":5,"sat":0.35,"compThresh":-26,"compRatio":8,"compAttack":1,"compRelease":50,"double":0.45,"dlyMix":0.25,"dlyTime":3,"dlyFb":0.35,"revMix":0.35,"revSize":0.9,"revDamp":0.2}})P",
R"P({"name":"Rap Upfront","category":"Rap","params":{"hpf":100,"tuneAmt":0.1,"tuneSpeed":80,"mud":-3,"deess":0.5,"body":1.5,"presence":3,"air":3,"sat":0.3,"compThresh":-22,"compRatio":5,"compAttack":8,"compRelease":70,"double":0.08,"dlyMix":0.06,"dlyTime":1,"dlyFb":0.15,"revMix":0.06,"revSize":0.3,"revDamp":0.6}})P",
R"P({"name":"Rap Melodic Trap","category":"Rap","params":{"hpf":110,"tuneAmt":1,"tuneSpeed":2,"mud":-3,"deess":0.6,"body":0.5,"presence":3.5,"air":5,"sat":0.2,"compThresh":-24,"compRatio":6,"compAttack":3,"compRelease":60,"double":0.2,"dlyMix":0.2,"dlyTime":1,"dlyFb":0.35,"revMix":0.15,"revSize":0.55,"revDamp":0.4}})P",
R"P({"name":"Rap Crunchy","category":"Rap","params":{"hpf":130,"tuneAmt":0.2,"tuneSpeed":40,"mud":-4,"deess":0.6,"body":-1,"presence":4,"air":3,"sat":0.55,"satType":2,"satMix":0.6,"compThresh":-24,"compRatio":6,"compAttack":3,"compRelease":60,"double":0.1,"dlyMix":0.08,"dlyTime":1,"dlyFb":0.2,"revMix":0.08,"revSize":0.35,"revDamp":0.6}})P",
R"P({"name":"Worship Lead","category":"Worship","params":{"hpf":90,"tuneAmt":0.4,"tuneSpeed":45,"mud":-2,"deess":0.55,"body":1,"presence":1.5,"air":3.5,"sat":0.1,"compThresh":-20,"compRatio":3,"compAttack":12,"compRelease":150,"double":0.12,"dlyMix":0.15,"dlyTime":0,"dlyFb":0.3,"revMix":0.35,"revSize":0.88,"revDamp":0.35}})P",
R"P({"name":"Worship Intimate","category":"Worship","params":{"hpf":80,"tuneAmt":0.3,"tuneSpeed":60,"mud":-1.5,"deess":0.5,"body":2,"presence":1,"air":2.5,"sat":0.08,"compThresh":-18,"compRatio":2.5,"compAttack":15,"compRelease":180,"double":0.05,"dlyMix":0.08,"dlyTime":2,"dlyFb":0.2,"revMix":0.22,"revSize":0.7,"revDamp":0.5}})P",
R"P({"name":"Stutter Fill 1/16","category":"Chop - Stutter","params":{"hpf":110,"tuneAmt":0.6,"tuneSpeed":20,"mud":-3,"deess":0.6,"presence":3,"air":5,"sat":0.2,"compThresh":-22,"compRatio":5,"compAttack":3,"compRelease":60,"double":0.2,"dlyMix":0.12,"dlyTime":1,"dlyFb":0.3,"revMix":0.25,"revSize":0.8,"revDamp":0.3,"onStutter":1,"stutInterval":2,"stutLength":1,"stutRate":2}})P",
R"P({"name":"Stutter 1/32 Roll","category":"Chop - Stutter","params":{"hpf":110,"tuneAmt":0.6,"tuneSpeed":20,"mud":-3,"deess":0.6,"presence":3,"air":5,"sat":0.2,"compThresh":-22,"compRatio":5,"compAttack":3,"compRelease":60,"double":0.2,"dlyMix":0.12,"dlyTime":1,"dlyFb":0.3,"revMix":0.25,"revSize":0.8,"revDamp":0.3,"onStutter":1,"stutInterval":2,"stutLength":2,"stutRate":3,"stutDecay":0.15}})P",
R"P({"name":"Stutter Pitch Drop","category":"Chop - Stutter","params":{"hpf":110,"tuneAmt":0.6,"tuneSpeed":20,"mud":-3,"deess":0.6,"presence":3,"air":5,"sat":0.2,"compThresh":-22,"compRatio":5,"compAttack":3,"compRelease":60,"double":0.2,"dlyMix":0.12,"dlyTime":1,"dlyFb":0.3,"revMix":0.25,"revSize":0.8,"revDamp":0.3,"onStutter":1,"stutInterval":3,"stutLength":3,"stutRate":1,"stutDrop":1.5,"stutDecay":0.2}})P",
R"P({"name":"Triplet Repeat","category":"Chop - Stutter","params":{"hpf":110,"tuneAmt":0.6,"tuneSpeed":20,"mud":-3,"deess":0.6,"presence":3,"air":5,"sat":0.2,"compThresh":-22,"compRatio":5,"compAttack":3,"compRelease":60,"double":0.2,"dlyMix":0.12,"dlyTime":1,"dlyFb":0.3,"revMix":0.25,"revSize":0.8,"revDamp":0.3,"onStutter":1,"stutInterval":2,"stutLength":2,"stutRate":5}})P",
R"P({"name":"Beat Repeat Hold (Always)","category":"Chop - Stutter","params":{"hpf":110,"tuneAmt":0.6,"tuneSpeed":20,"mud":-3,"deess":0.6,"presence":3,"air":5,"sat":0.2,"compThresh":-22,"compRatio":5,"compAttack":3,"compRelease":60,"double":0.2,"dlyMix":0.12,"dlyTime":1,"dlyFb":0.3,"revMix":0.25,"revSize":0.8,"revDamp":0.3,"onStutter":1,"stutInterval":0,"stutLength":2,"stutRate":2}})P",
R"P({"name":"Trance Gate","category":"Chop - Gate","params":{"hpf":110,"tuneAmt":0.6,"tuneSpeed":20,"mud":-3,"deess":0.6,"presence":3,"air":5,"sat":0.2,"compThresh":-22,"compRatio":5,"compAttack":3,"compRelease":60,"double":0.2,"dlyMix":0.12,"dlyTime":1,"dlyFb":0.3,"revMix":0.35,"revSize":0.8,"revDamp":0.3,"onGate":1,"gatePattern":2,"gateRate":1,"gateLen":0.6,"gateDepth":1}})P",
R"P({"name":"3-3-2 Gate","category":"Chop - Gate","params":{"hpf":110,"tuneAmt":0.6,"tuneSpeed":20,"mud":-3,"deess":0.6,"presence":3,"air":5,"sat":0.2,"compThresh":-22,"compRatio":5,"compAttack":3,"compRelease":60,"double":0.2,"dlyMix":0.12,"dlyTime":1,"dlyFb":0.3,"revMix":0.25,"revSize":0.8,"revDamp":0.3,"onGate":1,"gatePattern":3,"gateRate":1,"gateLen":0.7,"gateDepth":1}})P",
R"P({"name":"Offbeat Pump","category":"Chop - Gate","params":{"hpf":110,"tuneAmt":0.6,"tuneSpeed":20,"mud":-3,"deess":0.6,"presence":3,"air":5,"sat":0.2,"compThresh":-22,"compRatio":5,"compAttack":3,"compRelease":60,"double":0.2,"dlyMix":0.2,"dlyTime":2,"dlyFb":0.3,"revMix":0.25,"revSize":0.8,"revDamp":0.3,"onGate":1,"gatePattern":1,"gateRate":1,"gateLen":0.9,"gateDepth":0.9}})P",
R"P({"name":"1/16 Machine Chop","category":"Chop - Gate","params":{"hpf":110,"tuneAmt":0.6,"tuneSpeed":20,"mud":-3,"deess":0.6,"presence":3,"air":5,"sat":0.2,"compThresh":-22,"compRatio":5,"compAttack":3,"compRelease":60,"double":0.2,"dlyMix":0.12,"dlyTime":1,"dlyFb":0.3,"revMix":0.25,"revSize":0.8,"revDamp":0.3,"onGate":1,"gatePattern":0,"gateRate":1,"gateLen":0.45,"gateDepth":1}})P",
R"P({"name":"Build Up Gate","category":"Chop - Gate","params":{"hpf":110,"tuneAmt":0.6,"tuneSpeed":20,"mud":-3,"deess":0.6,"presence":3,"air":5,"sat":0.2,"compThresh":-22,"compRatio":5,"compAttack":3,"compRelease":60,"double":0.2,"dlyMix":0.12,"dlyTime":1,"dlyFb":0.3,"revMix":0.25,"revSize":0.8,"revDamp":0.3,"onGate":1,"gatePattern":4,"gateRate":2,"gateLen":0.5,"gateDepth":1,"onFilter":1,"filtType":1,"filtCutoff":300,"filtReso":0.4,"filtLfoRate":0,"filtLfoDepth":0.5}})P",
R"P({"name":"Chipmunk Chop","category":"Chop - Bender","params":{"hpf":110,"tuneAmt":0.6,"tuneSpeed":20,"mud":-3,"deess":0.6,"presence":3,"air":5,"sat":0.2,"compThresh":-22,"compRatio":5,"compAttack":3,"compRelease":60,"double":0.2,"dlyMix":0.12,"dlyTime":1,"dlyFb":0.3,"revMix":0.25,"revSize":0.8,"revDamp":0.3,"onBender":1,"bendPitch":7,"bendFormant":6,"onStutter":1,"stutInterval":2,"stutLength":0,"stutRate":3}})P",
R"P({"name":"Pitched Down Hook","category":"Chop - Bender","params":{"hpf":110,"tuneAmt":0.6,"tuneSpeed":20,"mud":-3,"deess":0.6,"presence":3,"air":5,"sat":0.2,"compThresh":-22,"compRatio":5,"compAttack":3,"compRelease":60,"double":0.2,"dlyMix":0.12,"dlyTime":1,"dlyFb":0.3,"revMix":0.25,"revSize":0.8,"revDamp":0.3,"onBender":1,"bendPitch":-5,"bendFormant":-4}})P",
R"P({"name":"Deep Demon","category":"Chop - Bender","params":{"hpf":110,"tuneAmt":0.6,"tuneSpeed":20,"mud":-3,"deess":0.6,"presence":3,"air":5,"sat":0.4,"compThresh":-22,"compRatio":5,"compAttack":3,"compRelease":60,"double":0.2,"dlyMix":0.12,"dlyTime":1,"dlyFb":0.3,"revMix":0.25,"revSize":0.8,"revDamp":0.3,"onBender":1,"bendPitch":-12,"bendFormant":-6,"satType":2,"satMix":0.4}})P",
R"P({"name":"Baby Voice","category":"Chop - Bender","params":{"hpf":110,"tuneAmt":0.6,"tuneSpeed":20,"mud":-3,"deess":0.6,"presence":3,"air":5,"sat":0.2,"compThresh":-22,"compRatio":5,"compAttack":3,"compRelease":60,"double":0.2,"dlyMix":0.12,"dlyTime":1,"dlyFb":0.3,"revMix":0.25,"revSize":0.8,"revDamp":0.3,"onBender":1,"bendPitch":5,"bendFormant":9}})P",
R"P({"name":"Robot Lead","category":"Chop - Bender","params":{"hpf":110,"tuneAmt":0.6,"tuneSpeed":20,"mud":-3,"deess":0.6,"presence":3,"air":5,"sat":0.2,"compThresh":-22,"compRatio":5,"compAttack":3,"compRelease":60,"double":0.2,"dlyMix":0.2,"dlyTime":1,"dlyFb":0.3,"revMix":0.25,"revSize":0.8,"revDamp":0.3,"onBender":1,"bendMode":1,"bendPitch":0,"bendFormant":0}})P",
R"P({"name":"Octave Up Lead","category":"Chop - Bender","params":{"hpf":110,"tuneAmt":0.6,"tuneSpeed":20,"mud":-3,"deess":0.6,"presence":3,"air":5,"sat":0.2,"compThresh":-22,"compRatio":5,"compAttack":3,"compRelease":60,"double":0.4,"dlyMix":0.12,"dlyTime":1,"dlyFb":0.3,"revMix":0.25,"revSize":0.8,"revDamp":0.3,"onBender":1,"bendPitch":12,"bendFormant":0,"bendMix":0.6}})P",
R"P({"name":"Filter Sweep Build","category":"Chop - Filter","params":{"hpf":110,"tuneAmt":0.6,"tuneSpeed":20,"mud":-3,"deess":0.6,"presence":3,"air":5,"sat":0.2,"compThresh":-22,"compRatio":5,"compAttack":3,"compRelease":60,"double":0.2,"dlyMix":0.12,"dlyTime":1,"dlyFb":0.3,"revMix":0.25,"revSize":0.8,"revDamp":0.3,"onFilter":1,"filtType":1,"filtCutoff":400,"filtReso":0.5,"filtLfoRate":0,"filtLfoDepth":0.6}})P",
R"P({"name":"Wobble Vox","category":"Chop - Filter","params":{"hpf":110,"tuneAmt":0.6,"tuneSpeed":20,"mud":-3,"deess":0.6,"presence":3,"air":5,"sat":0.2,"compThresh":-22,"compRatio":5,"compAttack":3,"compRelease":60,"double":0.2,"dlyMix":0.12,"dlyTime":1,"dlyFb":0.3,"revMix":0.25,"revSize":0.8,"revDamp":0.3,"onFilter":1,"filtType":0,"filtCutoff":1500,"filtReso":0.7,"filtLfoRate":3,"filtLfoDepth":0.6}})P",
R"P({"name":"Telephone Chop","category":"Chop - Filter","params":{"hpf":110,"tuneAmt":0.6,"tuneSpeed":20,"mud":-3,"deess":0.6,"presence":3,"air":5,"sat":0.2,"compThresh":-22,"compRatio":5,"compAttack":3,"compRelease":60,"double":0.2,"dlyMix":0.12,"dlyTime":1,"dlyFb":0.3,"revMix":0.1,"revSize":0.8,"revDamp":0.3,"onFilter":1,"filtType":2,"filtCutoff":1500,"filtReso":0.6,"onGate":1,"gatePattern":0,"gateRate":1,"gateLen":0.5,"gateDepth":1}})P",
R"P({"name":"Tape Stop Drop (press ON)","category":"Chop - FX","params":{"hpf":110,"tuneAmt":0.6,"tuneSpeed":20,"mud":-3,"deess":0.6,"presence":3,"air":5,"sat":0.2,"compThresh":-22,"compRatio":5,"compAttack":3,"compRelease":60,"double":0.2,"dlyMix":0.12,"dlyTime":1,"dlyFb":0.3,"revMix":0.25,"revSize":0.8,"revDamp":0.3,"tapeTime":1200}})P",
R"P({"name":"Festival Chop","category":"Chop - FX","params":{"hpf":110,"tuneAmt":0.6,"tuneSpeed":20,"mud":-3,"deess":0.6,"presence":3,"air":5,"sat":0.2,"compThresh":-22,"compRatio":5,"compAttack":3,"compRelease":60,"double":0.2,"dlyMix":0.25,"dlyTime":2,"dlyFb":0.45,"revMix":0.4,"revSize":0.92,"revDamp":0.3,"onBender":1,"bendPitch":12,"bendFormant":3,"bendMix":0.5,"onGate":1,"gatePattern":3,"gateRate":1,"gateLen":0.7,"gateDepth":1}})P"
};

static bool presetControlsParam (const juce::String& id) { return id != "key" && id != "scale"; }

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& s) : apvts (s) { refresh(); }

juce::File PresetManager::getUserDir()
{
    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    auto dir = base.getChildFile ("Application Support/WallChop/Presets");
   #else
    auto dir = base.getChildFile ("WallChop/Presets");
   #endif
    dir.createDirectory();
    return dir;
}

bool PresetManager::isValid (const juce::var& v)
{
    auto* obj = v.getDynamicObject();
    if (obj == nullptr) return false;
    const auto name = v["name"];
    if (! name.isString() || name.toString().trim().isEmpty() || name.toString().length() > 60) return false;
    auto* params = v["params"].getDynamicObject();
    if (params == nullptr) return false;
    for (auto& prop : params->getProperties())
        if (! (prop.value.isDouble() || prop.value.isInt() || prop.value.isInt64())) return false;
    return true;
}

void PresetManager::refresh()
{
    list.clear();
    for (auto* json : factoryPresets)
    {
        auto v = juce::JSON::parse (juce::String (json));
        if (isValid (v)) list.add ({ v["name"].toString(), v["category"].toString(), v["params"], true });
    }

    auto files = getUserDir().findChildFiles (juce::File::findFiles, false, "*.json");
    files.sort();
    for (auto& f : files)
    {
        if (f.getSize() > 65536) continue;
        auto v = juce::JSON::parse (f.loadFileAsString());
        if (! isValid (v)) continue;
        auto cat = v["category"].toString();
        list.add ({ v["name"].toString(), cat.isEmpty() ? juce::String ("User") : cat, v["params"], false });
    }
}

void PresetManager::apply (int index)
{
    if (! juce::isPositiveAndBelow (index, list.size())) return;
    const auto& pr = list.getReference (index);
    auto* values = pr.params.getDynamicObject();

    for (auto* param : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            const auto id = rp->getParameterID();
            if (! presetControlsParam (id)) continue;
            const juce::Identifier ident (id);
            const float v = (values != nullptr && values->hasProperty (ident))
                              ? (float) values->getProperty (ident)
                              : rp->convertFrom0to1 (rp->getDefaultValue());
            rp->setValueNotifyingHost (rp->convertTo0to1 (v));
        }
    currentName = pr.name;
}

bool PresetManager::saveCurrent (const juce::String& name)
{
    auto* params = new juce::DynamicObject();
    for (auto* param : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
            if (presetControlsParam (rp->getParameterID()))
                params->setProperty (rp->getParameterID(), rp->convertFrom0to1 (rp->getValue()));

    auto* obj = new juce::DynamicObject();
    obj->setProperty ("name", name.substring (0, 60));
    obj->setProperty ("category", "User");
    obj->setProperty ("params", juce::var (params));

    auto file = getUserDir().getChildFile (juce::File::createLegalFileName (name) + ".json");
    if (! file.replaceWithText (juce::JSON::toString (juce::var (obj)))) return false;
    refresh();
    currentName = name;
    return true;
}

juce::String PresetManager::importFile (const juce::File& f)
{
    if (! f.existsAsFile() || f.getSize() > 65536) return "That file isn't a Wall Chop preset.";
    const auto text = f.loadFileAsString();
    if (! isValid (juce::JSON::parse (text))) return "That file isn't a Wall Chop preset.";
    if (! getUserDir().getChildFile (juce::File::createLegalFileName (f.getFileName())).replaceWithText (text))
        return "Couldn't save the preset.";
    refresh();
    return {};
}

void PresetManager::downloadOnline (std::function<void (juce::String)> onDone)
{
    const juce::String indexUrl (WALLVOX_PRESET_INDEX_URL);
    auto finish = [onDone] (juce::String msg)
    {
        juce::MessageManager::callAsync ([onDone, msg] { onDone (msg); });
    };

    if (indexUrl.isEmpty()) { finish ("Online presets aren't set up in this build."); return; }

    juce::Thread::launch ([indexUrl, finish]
    {
        const auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                              .withConnectionTimeoutMs (8000);

        auto fetch = [&opts] (const juce::String& url) -> juce::String
        {
            if (auto s = juce::URL (url).createInputStream (opts))
                return s->readEntireStreamAsString();
            return {};
        };

        const auto indexText = fetch (indexUrl);
        auto index = juce::JSON::parse (indexText);
        auto* files = index["presets"].getArray();
        if (files == nullptr) { finish ("Couldn't reach the preset server. Check your internet."); return; }

        const auto base = indexUrl.upToLastOccurrenceOf ("/", true, false);
        const auto dir = getUserDir();
        int added = 0;

        for (auto& entry : *files)
        {
            const auto name = entry.toString();
            // Only plain file names like "worship-choir.json" are accepted
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
