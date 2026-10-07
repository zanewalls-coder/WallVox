#include "Hub.h"

namespace hub
{
// ------------------------------------------------------------------ helpers (background threads)
static int run (const juce::StringArray& args, int timeoutMs = 180000)
{
    juce::ChildProcess p;
    if (! p.start (args, 0)) return -1;
    p.waitForProcessToFinish (timeoutMs);
    return (int) p.getExitCode();
}

static juce::String fetchText (const juce::String& url)
{
    const auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                          .withConnectionTimeoutMs (15000).withNumRedirectsToFollow (6);
    if (auto s = juce::URL (url).createInputStream (opts)) return s->readEntireStreamAsString();
    return {};
}

static bool download (const juce::String& url, const juce::File& dest, std::shared_ptr<std::atomic<float>> progress)
{
    const auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                          .withConnectionTimeoutMs (15000).withNumRedirectsToFollow (6);
    auto in = juce::URL (url).createInputStream (opts);
    if (in == nullptr) return false;
    dest.deleteFile();
    juce::FileOutputStream out (dest);
    if (! out.openedOk()) return false;
    const auto total = in->getTotalLength();
    juce::HeapBlock<char> buf (65536);
    juce::int64 done = 0;
    for (;;)
    {
        const int n = in->read (buf, 65536);
        if (n <= 0) break;
        out.write (buf, (size_t) n);
        done += n;
        progress->store (total > 0 ? 0.85f * (float) done / (float) total : 0.4f);
    }
    out.flush();
    return done > 0;
}

static juce::File pluginFolder (const juce::String& bundle)
{
    auto lib = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Library/Audio/Plug-Ins");
    return lib.getChildFile (bundle.endsWithIgnoreCase (".component") ? "Components" : "VST3");
}

static juce::File appData()
{
    auto d = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Application Support/WallHub");
    d.createDirectory();
    return d;
}

static juce::File presetFolder (const juce::String& name)
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
             .getChildFile ("Application Support").getChildFile (name).getChildFile ("Presets");
}

static bool validPreset (const juce::var& v)
{
    return v.getDynamicObject() != nullptr && v["name"].isString() && v["params"].getDynamicObject() != nullptr;
}

static juce::String installWorker (const PluginInfo& info, std::shared_ptr<std::atomic<float>> progress)
{
    auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("WallHub").getChildFile (info.id);
    tmp.deleteRecursively();
    tmp.createDirectory();
    const auto zip = tmp.getChildFile ("package.zip"), x = tmp.getChildFile ("x");

    if (! download (info.url, zip, progress)) return "Download failed. Check your internet connection.";
    progress->store (0.9f);
    if (run ({ "/usr/bin/ditto", "-x", "-k", zip.getFullPathName(), x.getFullPathName() }) != 0) return "Couldn't unpack the download.";

    for (auto& bundle : info.bundles)
    {
        const auto found = x.findChildFiles (juce::File::findDirectories, true, bundle);
        if (found.isEmpty()) return "The download was missing " + bundle + ".";
        auto folder = pluginFolder (bundle);
        folder.createDirectory();
        auto dest = folder.getChildFile (bundle);
        if (dest.exists()) dest.deleteRecursively();
        if (run ({ "/usr/bin/ditto", found[0].getFullPathName(), dest.getFullPathName() }) != 0) return "Couldn't copy " + bundle + ".";
        run ({ "/usr/bin/xattr", "-cr", dest.getFullPathName() });   // clear the "downloaded from internet" flag
    }
    run ({ "/usr/bin/killall", "-9", "AudioComponentRegistrar" });   // make Logic notice the new AU
    tmp.deleteRecursively();
    progress->store (1.0f);
    return {};
}

static juce::String presetWorker (const PluginInfo& info)
{
    auto index = juce::JSON::parse (fetchText (info.presetsUrl));
    auto* files = index["presets"].getArray();
    if (files == nullptr) return "Couldn't reach the preset library.";
    const auto base = info.presetsUrl.upToLastOccurrenceOf ("/", true, false);
    auto dir = presetFolder (info.presetDir);
    dir.createDirectory();
    int added = 0;
    for (auto& e : *files)
    {
        const auto name = e.toString();
        if (! name.endsWith (".json") || name != juce::File::createLegalFileName (name) || name.containsAnyOf ("/\\ ") || name.contains ("..")) continue;
        auto target = dir.getChildFile (name);
        if (target.existsAsFile()) continue;
        const auto text = fetchText (base + name);
        if (text.length() > 65536 || ! validPreset (juce::JSON::parse (text))) continue;
        if (target.replaceWithText (text)) ++added;
    }
    return added == 0 ? info.name + " already has every preset." : juce::String (added) + " new preset" + (added == 1 ? "" : "s") + " added to " + info.name + ".";
}

// ------------------------------------------------------------------ hub component
HubComponent::HubComponent()
{
    setLookAndFeel (&look);
    refreshBtn.onClick = [this] { refresh(); };
    updateAllBtn.onClick = [this] { updateAll(); };
    hubUpdateBtn.onClick = [this] { updateHub(); };
    hubUpdateBtn.setToggleState (true, juce::dontSendNotification);
    hubUpdateBtn.setVisible (false);
    for (auto* b : { &refreshBtn, &updateAllBtn }) addAndMakeVisible (*b);
    addChildComponent (hubUpdateBtn);

    viewport.setViewedComponent (&list, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    status.setColour (juce::Label::textColourId, wl::c::dim);
    status.setFont (juce::FontOptions (13.0f));
    addAndMakeVisible (status);

    setSize (860, 640);
    refresh();
    startTimerHz (20);

   #if WALL_SNAPSHOT
    juce::Timer::callAfterDelay (6000, [safe = juce::Component::SafePointer<juce::Component> (this)]
    {
        if (safe == nullptr) return;
        const auto img = safe->createComponentSnapshot (safe->getLocalBounds(), true, 1.0f);
        auto dir = juce::File (juce::SystemStats::getEnvironmentVariable ("WALL_SNAPSHOT_DIR", "/tmp"));
        dir.createDirectory();
        auto f = dir.getChildFile ("WallHub.png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (img, os);
        os.flush();
        juce::JUCEApplicationBase::quit();
    });
   #endif
}

HubComponent::~HubComponent()
{
    alive->store (false);
    setLookAndFeel (nullptr);
}

void HubComponent::refresh()
{
    if (loading) return;
    loading = true;
    setStatus ("Checking for plugins and updates...");
    juce::Component::SafePointer<HubComponent> safe (this);
    juce::Thread::launch ([safe]
    {
        const auto text = fetchText (HUB_MANIFEST_URL);
        juce::MessageManager::callAsync ([safe, text]
        {
            if (safe == nullptr) return;
            safe->loading = false;
            const auto m = juce::JSON::parse (text);
            if (m["plugins"].getArray() == nullptr) { safe->setStatus ("Couldn't reach the plugin library. Check your internet and try again."); return; }
            safe->applyManifest (m);
        });
    });
}

void HubComponent::applyManifest (const juce::var& m)
{
    hubUrl = m["hub"]["url"].toString();
    hubLatest = (int) m["hub"]["version"];
    hubUpdateBtn.setVisible (hubLatest > HUB_VERSION && hubUrl.isNotEmpty());

    cards.clear();
    for (auto& p : *m["plugins"].getArray())
    {
        PluginInfo info;
        info.id = p["id"].toString();
        info.name = p["name"].toString();
        info.tagline = p["tagline"].toString();
        info.url = p["url"].toString();
        info.presetsUrl = p["presets"].toString();
        info.presetDir = p["presetDir"].toString();
        info.version = (int) p["version"];
        info.colour = juce::Colour::fromString ("ff" + p["color"].toString().removeCharacters ("#"));
        if (auto* b = p["bundles"].getArray()) for (auto& x : *b) info.bundles.add (x.toString());
        if (info.id.isEmpty() || info.url.isEmpty() || info.bundles.isEmpty()) continue;
        auto card = std::make_unique<Card> (*this, info);
        list.addAndMakeVisible (*card);
        cards.push_back (std::move (card));
    }
    resized();
    int updates = 0;
    for (auto& c : cards) if (isInstalled (c->info) && installedVersion (c->info.id) < c->info.version) ++updates;
    setStatus (juce::String (cards.size()) + " plugins available" + (updates > 0 ? "  -  " + juce::String (updates) + " update" + (updates == 1 ? "" : "s") + " ready" : juce::String ("  -  everything is up to date")));
}

int HubComponent::installedVersion (const juce::String& id) const
{
    const auto v = juce::JSON::parse (appData().getChildFile ("installed.json").loadFileAsString());
    return (int) v[juce::Identifier (id)];
}

bool HubComponent::isInstalled (const PluginInfo& info) const
{
    for (auto& b : info.bundles) if (pluginFolder (b).getChildFile (b).exists()) return true;
    return false;
}

void HubComponent::recordInstalled (const juce::String& id, int version)
{
    auto f = appData().getChildFile ("installed.json");
    auto v = juce::JSON::parse (f.loadFileAsString());
    if (v.getDynamicObject() == nullptr) v = juce::var (new juce::DynamicObject());
    v.getDynamicObject()->setProperty (id, version);
    f.replaceWithText (juce::JSON::toString (v));
}

void HubComponent::install (Card& card)
{
    if (card.busy) return;
    card.busy = true;
    card.progress->store (0.0f);
    card.refreshState();
    setStatus ("Installing " + card.info.name + "...");
    juce::Component::SafePointer<Card> safeCard (&card);
    juce::Component::SafePointer<HubComponent> safe (this);
    const auto info = card.info;
    const auto progress = card.progress;
    juce::Thread::launch ([=]
    {
        const auto err = installWorker (info, progress);
        juce::MessageManager::callAsync ([=]
        {
            if (safe == nullptr) return;
            if (err.isEmpty())
            {
                safe->recordInstalled (info.id, info.version);
                safe->setStatus (info.name + " installed. Restart Logic or Ableton to see it.");
            }
            else safe->setStatus (info.name + ": " + err);
            if (safeCard != nullptr) { safeCard->busy = false; safeCard->progress->store (-1.0f); safeCard->refreshState(); }
        });
    });
}

void HubComponent::remove (Card& card)
{
    juce::Component::SafePointer<Card> safeCard (&card);
    juce::Component::SafePointer<HubComponent> safe (this);
    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Remove " + card.info.name + "?",
        "The plugin files will be moved to the Trash. Your presets are kept.", "Remove", "Cancel", this,
        juce::ModalCallbackFunction::create ([safe, safeCard] (int r)
        {
            if (r != 1 || safe == nullptr || safeCard == nullptr) return;
            for (auto& b : safeCard->info.bundles)
            {
                auto f = pluginFolder (b).getChildFile (b);
                if (f.exists()) f.moveToTrash();
            }
            safe->recordInstalled (safeCard->info.id, 0);
            safe->setStatus (safeCard->info.name + " removed.");
            safeCard->refreshState();
        }));
}

void HubComponent::syncPresets (Card& card)
{
    setStatus ("Getting presets for " + card.info.name + "...");
    juce::Component::SafePointer<HubComponent> safe (this);
    const auto info = card.info;
    juce::Thread::launch ([=]
    {
        const auto msg = presetWorker (info);
        juce::MessageManager::callAsync ([=] { if (safe != nullptr) safe->setStatus (msg); });
    });
}

void HubComponent::updateAll()
{
    int started = 0;
    for (auto& c : cards)
        if (isInstalled (c->info) && installedVersion (c->info.id) < c->info.version && ! c->busy) { install (*c); ++started; }
    if (started == 0) setStatus ("Everything is up to date.");
}

void HubComponent::updateHub()
{
    setStatus ("Updating Wall Hub...");
    hubUpdateBtn.setEnabled (false);
    juce::Component::SafePointer<HubComponent> safe (this);
    const auto url = hubUrl;
    juce::Thread::launch ([=]
    {
        auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("WallHubSelf");
        tmp.deleteRecursively();
        tmp.createDirectory();
        auto zip = tmp.getChildFile ("hub.zip"), x = tmp.getChildFile ("x");
        auto prog = std::make_shared<std::atomic<float>> (0.0f);
        juce::String err;
        if (! download (url, zip, prog) || run ({ "/usr/bin/ditto", "-x", "-k", zip.getFullPathName(), x.getFullPathName() }) != 0)
            err = "Couldn't download the Hub update.";
        const auto apps = x.findChildFiles (juce::File::findDirectories, true, "Wall Hub.app");
        if (err.isEmpty() && apps.isEmpty()) err = "The Hub update was incomplete.";

        juce::MessageManager::callAsync ([=]
        {
            if (safe == nullptr) return;
            if (err.isNotEmpty()) { safe->setStatus (err); safe->hubUpdateBtn.setEnabled (true); return; }
            // swap the app after we quit, then reopen it
            const auto current = juce::File::getSpecialLocation (juce::File::currentApplicationFile);
            auto script = tmp.getChildFile ("swap.sh");
            script.replaceWithText ("#!/bin/bash\nsleep 1\nrm -rf \"" + current.getFullPathName() + "\"\n/usr/bin/ditto \""
                                    + apps[0].getFullPathName() + "\" \"" + current.getFullPathName() + "\"\n/usr/bin/xattr -cr \""
                                    + current.getFullPathName() + "\"\nopen \"" + current.getFullPathName() + "\"\n");
            std::system (("nohup /bin/bash \"" + script.getFullPathName() + "\" >/dev/null 2>&1 &").toRawUTF8());
            juce::JUCEApplicationBase::quit();
        });
    });
}

void HubComponent::timerCallback()
{
    for (auto& c : cards) if (c->busy) c->repaint();
}

void HubComponent::paint (juce::Graphics& g)
{
    wl::drawBackground (g, getLocalBounds(), 56.0f, theme, "WALL", "HUB");
    g.setColour (wl::c::dim);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("YOUR PLUGINS", 24, 66, 300, 22, juce::Justification::centredLeft);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("Hub v" + juce::String (HUB_VERSION), getLocalBounds().removeFromBottom (34).reduced (20, 0), juce::Justification::centredRight);
}

void HubComponent::resized()
{
    updateAllBtn.setBounds (getWidth() - 136, 11, 120, 34);
    refreshBtn.setBounds (getWidth() - 316, 11, 172, 34);
    hubUpdateBtn.setBounds (getWidth() - 452, 11, 128, 34);
    viewport.setBounds (16, 92, getWidth() - 32, getHeight() - 92 - 40);
    status.setBounds (16, getHeight() - 36, getWidth() - 140, 30);
    const int cardH = 116, gap = 12;
    list.setSize (viewport.getWidth() - 12, juce::jmax (viewport.getHeight(), (int) cards.size() * (cardH + gap)));
    for (size_t i = 0; i < cards.size(); ++i) cards[i]->setBounds (0, (int) i * (cardH + gap), list.getWidth(), cardH);
}

// ------------------------------------------------------------------ card
Card::Card (HubComponent& owner, PluginInfo i) : info (std::move (i)), hubRef (owner)
{
    actionBtn.onClick = [this] { hubRef.install (*this); };
    presetsBtn.onClick = [this] { hubRef.syncPresets (*this); };
    removeBtn.onClick = [this] { hubRef.remove (*this); };
    for (auto* b : { &actionBtn, &presetsBtn, &removeBtn }) addAndMakeVisible (*b);
    refreshState();
}

void Card::refreshState()
{
    const bool installed = hubRef.isInstalled (info);
    const int have = hubRef.installedVersion (info.id);
    if (busy) { actionBtn.setButtonText ("WORKING..."); actionBtn.setEnabled (false); }
    else if (! installed) { actionBtn.setButtonText ("INSTALL"); actionBtn.setEnabled (true); }
    else if (have < info.version) { actionBtn.setButtonText ("UPDATE"); actionBtn.setEnabled (true); }
    else { actionBtn.setButtonText ("UP TO DATE"); actionBtn.setEnabled (false); }
    actionBtn.setToggleState (actionBtn.isEnabled() && ! busy, juce::dontSendNotification);
    presetsBtn.setVisible (installed && info.presetsUrl.isNotEmpty());
    removeBtn.setVisible (installed && ! busy);

    versionText = installed ? (have > 0 ? "Installed v" + juce::String (have) : juce::String ("Installed")) : juce::String ("Not installed");
    versionText << "   |   Latest v" << info.version;
    repaint();
}

void Card::paint (juce::Graphics& g)
{
    const wl::Theme t { info.colour, info.colour.withRotatedHue (0.08f) };
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    wl::drawPanel (g, r, {}, true, t, 0.0f);

    // glowing colour strip + mark
    auto strip = r.removeFromLeft (6).reduced (0, 14);
    g.setColour (info.colour.withAlpha (0.25f));
    g.fillRoundedRectangle (strip.expanded (3, 3).translated (10, 0), 4.0f);
    g.setColour (info.colour);
    g.fillRoundedRectangle (strip.translated (10, 0), 3.0f);

    auto text = getLocalBounds().toFloat().withTrimmedLeft (36).withTrimmedRight (330).reduced (0, 18);
    // solid colour on purpose: on macOS a gradient-filled string containing a space floods the whole card
    g.setColour (info.colour.brighter (0.15f));
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText (info.name, text.removeFromTop (30), juce::Justification::centredLeft);
    g.setColour (wl::c::text.withAlpha (0.85f));
    g.setFont (juce::FontOptions (14.0f));
    g.drawText (info.tagline, text.removeFromTop (22), juce::Justification::centredLeft);
    g.setColour (wl::c::dim);
    g.setFont (juce::FontOptions (12.5f));
    g.drawText (versionText, text.removeFromTop (22), juce::Justification::centredLeft);

    const float p = progress->load();
    if (busy && p >= 0.0f)
    {
        auto bar = getLocalBounds().toFloat().reduced (36, 0).removeFromBottom (12).withHeight (4);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (bar, 2.0f);
        g.setGradientFill (wl::grad (t, bar.getTopLeft(), bar.getTopRight()));
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.02f, 1.0f, p)), 2.0f);
    }
}

void Card::resized()
{
    auto r = getLocalBounds().reduced (18, 0);
    auto right = r.removeFromRight (300);
    actionBtn.setBounds (right.removeFromRight (140).withSizeKeepingCentre (140, 38));
    right.removeFromRight (8);
    auto small = right.withSizeKeepingCentre (right.getWidth(), 76);
    presetsBtn.setBounds (small.removeFromTop (34));
    small.removeFromTop (8);
    removeBtn.setBounds (small.removeFromTop (34));
}

} // namespace hub
