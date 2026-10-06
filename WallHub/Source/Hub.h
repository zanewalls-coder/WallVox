#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "WallLook.h"

#ifndef HUB_MANIFEST_URL
 #define HUB_MANIFEST_URL "https://github.com/zanewalls-coder/WallVox/releases/download/latest/manifest.json"
#endif
#ifndef HUB_VERSION
 #define HUB_VERSION 0
#endif

namespace hub
{
inline const wl::Theme theme { juce::Colour (0xff4cc9f0), juce::Colour (0xff7b61ff) };

struct PluginInfo
{
    juce::String id, name, tagline, url, presetsUrl, presetDir;
    juce::StringArray bundles;
    juce::Colour colour;
    int version = 0;
};

class Card;

class HubComponent : public juce::Component, private juce::Timer
{
public:
    HubComponent();
    ~HubComponent() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    // called by cards
    void install (Card&);
    void remove (Card&);
    void syncPresets (Card&);
    int installedVersion (const juce::String& id) const;
    bool isInstalled (const PluginInfo&) const;
    void setStatus (const juce::String& s) { status.setText (s, juce::dontSendNotification); }

private:
    void timerCallback() override;
    void refresh();
    void applyManifest (const juce::var& m);
    void updateAll();
    void updateHub();
    void recordInstalled (const juce::String& id, int version);

    wl::Look look { theme };
    juce::TextButton refreshBtn { "CHECK FOR UPDATES" }, updateAllBtn { "UPDATE ALL" }, hubUpdateBtn { "UPDATE HUB" };
    juce::Viewport viewport;
    juce::Component list;
    std::vector<std::unique_ptr<Card>> cards;
    juce::Label status;
    juce::String hubUrl;
    int hubLatest = 0;
    bool loading = false;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
};

class Card : public juce::Component
{
public:
    Card (HubComponent& owner, PluginInfo i);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refreshState();

    PluginInfo info;
    std::shared_ptr<std::atomic<float>> progress = std::make_shared<std::atomic<float>> (-1.0f);   // -1 = idle
    bool busy = false;
    juce::TextButton actionBtn, presetsBtn { "GET PRESETS" }, removeBtn { "REMOVE" };

private:
    HubComponent& hubRef;
    juce::String versionText;
};

} // namespace hub
