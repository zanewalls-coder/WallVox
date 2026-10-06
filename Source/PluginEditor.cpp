#include "PluginEditor.h"

namespace col
{
    const juce::Colour bg     (0xff13131b);
    const juce::Colour panel  (0xff1d1d29);
    const juce::Colour accent (0xff8a6bff);
    const juce::Colour text   (0xffe8e6f2);
    const juce::Colour dim    (0xff8d8aa3);
}

WallVoxLook::WallVoxLook()
{
    setColour (juce::ComboBox::backgroundColourId, col::panel.brighter (0.08f));
    setColour (juce::ComboBox::outlineColourId, col::accent.withAlpha (0.4f));
    setColour (juce::ComboBox::textColourId, col::text);
    setColour (juce::ComboBox::arrowColourId, col::accent);
    setColour (juce::PopupMenu::backgroundColourId, col::panel);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent);
    setColour (juce::TextButton::buttonColourId, col::panel.brighter (0.08f));
    setColour (juce::TextButton::textColourOffId, col::text);
    setColour (juce::Label::textColourId, col::text);
    setColour (juce::Slider::textBoxTextColourId, col::dim);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}

void WallVoxLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                    float start, float end, juce::Slider&)
{
    auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
    const float size = juce::jmin (r.getWidth(), r.getHeight());
    r = r.withSizeKeepingCentre (size, size);
    const auto c = r.getCentre();
    const float rad = size * 0.5f, angle = start + pos * (end - start);

    juce::Path track, value;
    track.addCentredArc (c.x, c.y, rad - 3, rad - 3, 0, start, end, true);
    value.addCentredArc (c.x, c.y, rad - 3, rad - 3, 0, start, angle, true);
    const juce::PathStrokeType stroke (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour (col::panel.brighter (0.25f)); g.strokePath (track, stroke);
    g.setColour (col::accent);                 g.strokePath (value, stroke);

    g.setColour (col::panel.brighter (0.15f));
    g.fillEllipse (r.reduced (9.0f));
    juce::Path tick;
    tick.addRoundedRectangle (-1.5f, -(rad - 11), 3.0f, rad * 0.45f, 1.5f);
    g.setColour (col::text);
    g.fillPath (tick, juce::AffineTransform::rotation (angle).translated (c));
}

WallVoxEditor::WallVoxEditor (WallVoxProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&look);

    sections = {
        { "TUNE",     { "tuneAmt", "tuneSpeed", "key", "scale" }, {} },
        { "CLEAN",    { "hpf", "mud", "deess" }, {} },
        { "TONE",     { "body", "presence", "air", "sat" }, {} },
        { "DYNAMICS", { "compThresh", "compRatio", "compAttack", "compRelease" }, {} },
        { "SPACE",    { "double", "dlyMix", "dlyTime", "dlyFb", "revMix", "revSize", "revDamp" }, {} },
        { "LEVEL",    { "inGain", "outGain" }, {} } };

    for (auto& s : sections)
        for (auto& id : s.ids)
        {
            auto c = std::make_unique<Control>();
            c->id = id;
            auto* param = proc.apvts.getParameter (id);
            c->label.setText (param->getName (20), juce::dontSendNotification);
            c->label.setJustificationType (juce::Justification::centred);
            c->label.setFont (juce::FontOptions (13.0f));
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
                c->slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag,
                                                            juce::Slider::TextBoxBelow);
                c->slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
                addAndMakeVisible (*c->slider);
                c->sAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, *c->slider);
            }
            controls.push_back (std::move (c));
        }

    presetBox.setTextWhenNothingSelected ("Choose a preset");
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0) proc.presets.apply (idx);
    };
    addAndMakeVisible (presetBox);

    saveBtn.onClick   = [this] { savePreset(); };
    importBtn.onClick = [this] { importPreset(); };
    moreBtn.onClick   = [this] { getMorePresets(); };
    for (auto* b : { &saveBtn, &importBtn, &moreBtn }) addAndMakeVisible (*b);

    status.setJustificationType (juce::Justification::centredRight);
    status.setColour (juce::Label::textColourId, col::dim);
    status.setFont (juce::FontOptions (13.0f));
    addAndMakeVisible (status);

    rebuildPresetMenu();
    setSize (980, 560);
}

WallVoxEditor::~WallVoxEditor() { setLookAndFeel (nullptr); }

void WallVoxEditor::rebuildPresetMenu()
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
        if (all[i].name == proc.presets.currentName)
        {
            presetBox.setSelectedId (i + 1, juce::dontSendNotification);
            break;
        }
}

void WallVoxEditor::savePreset()
{
    auto* w = new juce::AlertWindow ("Save Preset", "Name your preset:", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", proc.presets.currentName);
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<WallVoxEditor> safe (this);
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

void WallVoxEditor::importPreset()
{
    chooser = std::make_unique<juce::FileChooser> ("Import a WallVox preset", juce::File(), "*.json");
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

void WallVoxEditor::getMorePresets()
{
    moreBtn.setEnabled (false);
    status.setText ("Checking for new presets...", juce::dontSendNotification);
    juce::Component::SafePointer<WallVoxEditor> safe (this);
    PresetManager::downloadOnline ([safe] (juce::String msg)
    {
        if (safe == nullptr) return;
        safe->proc.presets.refresh();
        safe->rebuildPresetMenu();
        safe->status.setText (msg, juce::dontSendNotification);
        safe->moreBtn.setEnabled (true);
    });
}

void WallVoxEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);

    g.setColour (col::text);
    g.setFont (juce::FontOptions (30.0f, juce::Font::bold));
    g.drawText ("WALL", 24, 18, 90, 36, juce::Justification::centredLeft);
    g.setColour (col::accent);
    g.drawText ("VOX", 104, 18, 80, 36, juce::Justification::centredLeft);

    for (auto& s : sections)
    {
        g.setColour (col::panel);
        g.fillRoundedRectangle (s.area.toFloat(), 10.0f);
        g.setColour (col::accent);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (s.title, s.area.withHeight (28).reduced (14, 0), juce::Justification::centredLeft);
    }
}

void WallVoxEditor::resized()
{
    auto area = getLocalBounds().reduced (16);
    auto top = area.removeFromTop (60);
    top.removeFromLeft (190);
    moreBtn.setBounds (top.removeFromRight (150).withSizeKeepingCentre (150, 30));
    top.removeFromRight (8);
    importBtn.setBounds (top.removeFromRight (80).withSizeKeepingCentre (80, 30));
    top.removeFromRight (8);
    saveBtn.setBounds (top.removeFromRight (70).withSizeKeepingCentre (70, 30));
    top.removeFromRight (16);
    presetBox.setBounds (top.withSizeKeepingCentre (top.getWidth(), 32));
    status.setBounds (area.removeFromTop (20));
    area.removeFromTop (6);

    const int rowH = (area.getHeight() - 12) / 2;
    auto layoutRow = [this] (juce::Rectangle<int> row, int first, int count)
    {
        int total = 0;
        for (int i = first; i < first + count; ++i) total += sections[(size_t) i].ids.size();
        const int gap = 12;
        const float unit = (float) (row.getWidth() - gap * (count - 1)) / (float) total;
        for (int i = first; i < first + count; ++i)
        {
            auto& s = sections[(size_t) i];
            s.area = row.removeFromLeft ((int) (unit * (float) s.ids.size()));
            row.removeFromLeft (gap);
        }
    };
    layoutRow (area.removeFromTop (rowH), 0, 3);
    area.removeFromTop (12);
    layoutRow (area, 3, 3);

    size_t ci = 0;
    for (auto& s : sections)
    {
        auto inner = s.area.reduced (8).withTrimmedTop (22);
        const int w = inner.getWidth() / s.ids.size();
        for (int k = 0; k < s.ids.size(); ++k, ++ci)
        {
            auto& c = *controls[ci];
            auto cell = inner.removeFromLeft (w);
            c.label.setBounds (cell.removeFromTop (20));
            if (c.slider) c.slider->setBounds (cell.reduced (2));
            else          c.combo->setBounds (cell.withSizeKeepingCentre (cell.getWidth() - 8, 28));
        }
    }
}
