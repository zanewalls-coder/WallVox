#include "PluginEditor.h"

// ---- per-plugin identity -------------------------------------------------------------
#define PLUGIN_THEME  wl::Theme { juce::Colour (0xff39ff14), juce::Colour (0xff00e0c6) }
#define PLUGIN_WORD2  "CHOP"
#define PLUGIN_W      1300
#define PLUGIN_H      860
#define SCOPE_H       130

static void buildLayout (std::function<void (const char*, const char*, juce::StringArray)> add,
                         std::vector<std::vector<int>>& rows)
{
    add ("TUNE",       "onTune",  { "tuneAmt", "tuneSpeed", "key", "scale" });
    add ("CLEAN",      "onClean", { "hpf", "mud", "deess" });
    add ("TONE",       "onTone",  { "body", "presence", "air" });
    add ("SATURATION", "onSat",   { "satType", "sat", "satMix" });
    add ("DYNAMICS",   "onDyn",   { "compThresh", "compRatio", "compAttack", "compRelease" });
    add ("VOCAL BENDER", "onBender", { "bendMode", "bendPitch", "bendFormant", "bendMix" });
    add ("STUTTER",    "onStutter", { "stutInterval", "stutLength", "stutRate", "stutDrop", "stutDecay", "stutMix" });
    add ("TRANCE GATE", "onGate", { "gatePattern", "gateRate", "gateLen", "gateDepth" });
    add ("TAPE STOP",  "onTapeStop", { "tapeTime" });
    add ("FILTER",     "onFilter", { "filtType", "filtCutoff", "filtReso", "filtLfoRate", "filtLfoDepth" });
    add ("DELAY",      "onDelay", { "dlyTime", "dlyMix", "dlyFb" });
    add ("SPACE",      "onSpace", { "double", "revMix", "revSize", "revDamp" });
    add ("LEVEL",      "",        { "inGain", "outGain" });
    rows = { { 0, 1, 2, 3, 4 }, { 5, 6, 7, 8 }, { 9, 10, 11, 12 } };
}

static void setupScope (wl::Scope& s, wl::ScopeData& d)
{
    s.readoutCaption = "TEMPO";
    s.readout = [&d] { return juce::String (juce::roundToInt (d.bpm.load())); };
}
// --------------------------------------------------------------------------------------

WallChopEditor::WallChopEditor (WallChopProcessor& p)
    : AudioProcessorEditor (p), proc (p), look (PLUGIN_THEME), scope (p.scope, PLUGIN_THEME, "OUTPUT")
{
    setLookAndFeel (&look);
    setupScope (scope, proc.scope);
    addAndMakeVisible (scope);

    buildLayout ([this] (const char* title, const char* toggleId, juce::StringArray ids)
    {
        Section s;
        s.title = title;
        s.toggleId = toggleId;
        s.ids = ids;
        sections.push_back (std::move (s));
    }, rows);

    for (auto& s : sections)
    {
        if (s.toggleId.isEmpty()) continue;
        s.toggle = std::make_unique<juce::ToggleButton>();
        s.toggle->setTooltip ("Turn " + s.title.toLowerCase() + " on or off");
        addAndMakeVisible (*s.toggle);
        s.tAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, s.toggleId, *s.toggle);
        s.toggle->onStateChange = [this] { updateSectionDimming(); repaint(); };
    }

    for (auto& s : sections)
        for (auto& id : s.ids)
        {
            auto c = std::make_unique<Control>();
            c->id = id;
            auto* param = proc.apvts.getParameter (id);
            c->label.setText (param->getName (20).toUpperCase(), juce::dontSendNotification);
            c->label.setJustificationType (juce::Justification::centred);
            c->label.setFont (juce::FontOptions (11.5f, juce::Font::bold));
            c->label.setColour (juce::Label::textColourId, wl::c::dim);
            addAndMakeVisible (c->label);

            if (dynamic_cast<juce::AudioParameterChoice*> (param) != nullptr)
            {
                c->combo = std::make_unique<juce::ComboBox>();
                c->combo->addItemList (param->getAllValueStrings(), 1);
                addAndMakeVisible (*c->combo);
                c->cAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, *c->combo);
            }
            else
            {
                c->slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
                c->slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 16);
                c->slider->setPopupDisplayEnabled (false, false, nullptr);
                addAndMakeVisible (*c->slider);
                c->sAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, *c->slider);
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
                    c->slider->setDoubleClickReturnValue (true, rp->convertFrom0to1 (rp->getDefaultValue()));
            }
            controls.push_back (std::move (c));
        }

    presetBox.setTextWhenNothingSelected ("Choose a preset");
    presetBox.setJustificationType (juce::Justification::centred);
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0) proc.presets.apply (idx);
    };
    addAndMakeVisible (presetBox);
    prevBtn.onClick = [this] { stepPreset (-1); };
    nextBtn.onClick = [this] { stepPreset (1); };
    for (auto* b : { &prevBtn, &nextBtn }) addAndMakeVisible (*b);

    saveBtn.onClick   = [this] { savePreset(); };
    importBtn.onClick = [this] { importPreset(); };
    moreBtn.onClick   = [this] { getMorePresets(); };
    moreBtn.setTooltip ("Download new presets from the internet");
    for (auto* b : { &saveBtn, &importBtn, &moreBtn }) addAndMakeVisible (*b);

    status.setJustificationType (juce::Justification::centredLeft);
    status.setColour (juce::Label::textColourId, wl::c::dim);
    status.setFont (juce::FontOptions (12.5f));
    status.setText ("Double-click any knob to reset it.", juce::dontSendNotification);
    addAndMakeVisible (status);

    rebuildPresetMenu();
    updateSectionDimming();
    setSize (PLUGIN_W, PLUGIN_H);

   #if WALL_SNAPSHOT
    juce::Timer::callAfterDelay (2500, [safe = juce::Component::SafePointer<juce::Component> (this)]
    {
        if (safe == nullptr) return;
        const auto img = safe->createComponentSnapshot (safe->getLocalBounds(), true, 1.0f);
        auto dir = juce::File (juce::SystemStats::getEnvironmentVariable ("WALL_SNAPSHOT_DIR", "/tmp"));
        dir.createDirectory();
        auto f = dir.getChildFile (juce::String (JucePlugin_Name).removeCharacters (" ") + ".png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (img, os);
        os.flush();
        juce::JUCEApplicationBase::quit();
    });
   #endif
}

WallChopEditor::~WallChopEditor() { setLookAndFeel (nullptr); }

void WallChopEditor::updateSectionDimming()
{
    size_t ci = 0;
    for (auto& s : sections)
    {
        const bool on = sectionOn (s);
        for (int k = 0; k < s.ids.size(); ++k, ++ci)
        {
            auto& c = *controls[ci];
            c.label.setAlpha (on ? 1.0f : 0.4f);
            if (c.slider) c.slider->setAlpha (on ? 1.0f : 0.45f);
            else          c.combo->setAlpha (on ? 1.0f : 0.4f);
        }
    }
}

void WallChopEditor::rebuildPresetMenu()
{
    presetBox.clear (juce::dontSendNotification);
    const auto& all = proc.presets.getAll();
    juce::StringArray cats;
    for (auto& pr : all) cats.addIfNotAlreadyThere (pr.category);
    for (auto& cat : cats)
    {
        presetBox.addSectionHeading (cat);
        for (int i = 0; i < all.size(); ++i)
            if (all[i].category == cat) presetBox.addItem (all[i].name, i + 1);
    }
    for (int i = 0; i < all.size(); ++i)
        if (all[i].name == proc.presets.currentName) { presetBox.setSelectedId (i + 1, juce::dontSendNotification); break; }
}

void WallChopEditor::stepPreset (int delta)
{
    const int n = proc.presets.getAll().size();
    if (n == 0) return;
    const int cur = presetBox.getSelectedId() - 1;
    const int next = cur < 0 ? 0 : ((cur + delta) % n + n) % n;
    presetBox.setSelectedId (next + 1, juce::sendNotificationSync);
}

void WallChopEditor::savePreset()
{
    auto* w = new juce::AlertWindow ("Save Preset", "Name your preset:", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", proc.presets.currentName);
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<WallChopEditor> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
    {
        if (result != 1 || safe == nullptr) return;
        const auto name = w->getTextEditorContents ("name").trim();
        if (name.isEmpty()) return;
        const bool ok = safe->proc.presets.saveCurrent (name);
        safe->rebuildPresetMenu();
        safe->status.setText (ok ? "Saved \"" + name + "\"" : "Couldn't save the preset.", juce::dontSendNotification);
    }), true);
}

void WallChopEditor::importPreset()
{
    chooser = std::make_unique<juce::FileChooser> ("Import a preset", juce::File(), "*.json");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (f == juce::File()) return;
        const auto err = proc.presets.importFile (f);
        rebuildPresetMenu();
        status.setText (err.isEmpty() ? "Imported " + f.getFileNameWithoutExtension() : err, juce::dontSendNotification);
    });
}

void WallChopEditor::getMorePresets()
{
    moreBtn.setEnabled (false);
    status.setText ("Checking for new presets...", juce::dontSendNotification);
    juce::Component::SafePointer<WallChopEditor> safe (this);
    PresetManager::downloadOnline ([safe] (juce::String msg)
    {
        if (safe == nullptr) return;
        safe->proc.presets.refresh();
        safe->rebuildPresetMenu();
        safe->status.setText (msg, juce::dontSendNotification);
        safe->moreBtn.setEnabled (true);
    });
}

void WallChopEditor::paint (juce::Graphics& g)
{
    const auto theme = look.theme;
    wl::drawBackground (g, getLocalBounds(), 56.0f, theme, "WALL", PLUGIN_WORD2);
    for (auto& s : sections)
        wl::drawPanel (g, s.area.toFloat(), s.title, sectionOn (s), theme, 32.0f, s.toggle ? 40.0f : 14.0f);

    g.setColour (wl::c::dim.withAlpha (0.6f));
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("v1.2", getLocalBounds().removeFromBottom (28).reduced (18, 0), juce::Justification::centredRight);
}

void WallChopEditor::resized()
{
    // header
    auto head = getLocalBounds().removeFromTop (56).reduced (16, 11);
    moreBtn.setBounds (head.removeFromRight (104));
    head.removeFromRight (8);
    importBtn.setBounds (head.removeFromRight (84));
    head.removeFromRight (8);
    saveBtn.setBounds (head.removeFromRight (70));
    auto centre = getLocalBounds().removeFromTop (56).withSizeKeepingCentre (400, 34);
    prevBtn.setBounds (centre.removeFromLeft (34));
    nextBtn.setBounds (centre.removeFromRight (34));
    presetBox.setBounds (centre.reduced (6, 0));

    scope.setBounds (16, 68, getWidth() - 32, SCOPE_H);
    status.setBounds (getLocalBounds().removeFromBottom (28).reduced (18, 0));

    // section rows
    auto area = juce::Rectangle<int> (16, 68 + SCOPE_H + 14, getWidth() - 32, getHeight() - (68 + SCOPE_H + 14) - 32);
    const int gap = 12, nRows = (int) rows.size();
    const int rowH = (area.getHeight() - gap * (nRows - 1)) / nRows;
    auto cellWeight = [this] (const juce::String& id)
    { return dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)) != nullptr ? 1.45f : 1.0f; };
    auto weight = [&] (int i)
    {
        float w = 0.0f;
        for (auto& id : sections[(size_t) i].ids) w += cellWeight (id);
        return juce::jmax (2.0f, w);
    };

    for (int r = 0; r < nRows; ++r)
    {
        auto row = area.removeFromTop (rowH);
        area.removeFromTop (gap);
        float total = 0.0f;
        for (int i : rows[(size_t) r]) total += weight (i);
        const float unit = (float) (row.getWidth() - gap * ((int) rows[(size_t) r].size() - 1)) / total;
        for (int i : rows[(size_t) r])
        {
            sections[(size_t) i].area = row.removeFromLeft ((int) (unit * weight (i)));
            row.removeFromLeft (gap);
        }
    }

    size_t ci = 0;
    for (auto& s : sections)
    {
        if (s.toggle) s.toggle->setBounds (s.area.getX() + 10, s.area.getY() + 5, 22, 22);
        auto inner = s.area.withTrimmedTop (38).reduced (6, 6);
        float sum = 0.0f;
        for (auto& id : s.ids) sum += cellWeight (id);
        const float unitW = (float) inner.getWidth() / sum;
        for (int k = 0; k < s.ids.size(); ++k, ++ci)
        {
            auto& c = *controls[ci];
            auto cell = inner.removeFromLeft ((int) (unitW * cellWeight (s.ids[k])));
            c.label.setBounds (cell.removeFromBottom (18));
            if (c.slider) c.slider->setBounds (cell.withSizeKeepingCentre (juce::jmin (cell.getWidth(), cell.getHeight() + 6), cell.getHeight()));
            else          c.combo->setBounds (cell.withSizeKeepingCentre (cell.getWidth() - 6, 30));
        }
    }
}
