#include "PluginEditor.h"

#if WALL_CHOP
 static const wl::Theme theme { juce::Colour (0xff39ff14), juce::Colour (0xff00e0c6) };
 static const char* word2 = "CHOP";
#else
 static const wl::Theme theme { juce::Colour (0xff9b5cff), juce::Colour (0xffff5e9c) };
 static const char* word2 = "VOX";
#endif

static const mods::ModuleInfo& info (int type)
{
    const auto& c = mods::catalogue();
    return c[(size_t) juce::jlimit (0, (int) c.size() - 1, type)];
}

// ======================================================================================= Tile
Tile::Tile (VoxEditor& e, int pos, int t) : position (pos), type (t), ed (e)
{
    power.setTooltip ("Turn this module on or off");
    addAndMakeVisible (power);
    powerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (ed.proc.apvts, chain::onId (pos), power);
    power.onStateChange = [this] { repaint(); };
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

void Tile::resized() { power.setBounds (8, getHeight() - 28, 22, 22); }

void Tile::paint (juce::Graphics& g)
{
    const auto& inf = info (type);
    const juce::Colour col (inf.colour);
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool sel = ed.selected == position, on = power.getToggleState(), hover = isMouseOver (true);

    g.setColour (juce::Colours::black.withAlpha (dragging ? 0.6f : 0.35f));
    g.fillRoundedRectangle (r.translated (0, dragging ? 6.0f : 2.0f), 10.0f);
    g.setGradientFill (juce::ColourGradient (wl::c::panelHi.brighter (sel ? 0.12f : hover ? 0.06f : 0.0f), r.getX(), r.getY(),
                                             wl::c::panel, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 10.0f);

    // colour strip
    auto strip = r.withHeight (4.0f).reduced (10.0f, 0).translated (0, 6);
    g.setColour (col.withAlpha (on ? 0.25f : 0.1f));
    g.fillRoundedRectangle (strip.expanded (2, 2), 3.0f);
    g.setColour (on ? col : col.withAlpha (0.35f));
    g.fillRoundedRectangle (strip, 2.0f);

    if (sel)
    {
        g.setColour (theme.a.withAlpha (0.25f));
        g.drawRoundedRectangle (r.expanded (1.5f), 11.0f, 3.0f);
        g.setColour (theme.a);
        g.drawRoundedRectangle (r, 10.0f, 1.4f);
    }
    else
    {
        g.setColour (wl::c::line);
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
    }

    g.setColour (on ? wl::c::text : wl::c::dim);
    g.setFont (juce::FontOptions (13.5f, juce::Font::bold));
    g.drawFittedText (juce::String (inf.name).toUpperCase(), getLocalBounds().reduced (8, 0).withTrimmedTop (16).withHeight (22),
                      juce::Justification::centred, 1, 0.7f);
    g.setColour (wl::c::dim.withAlpha (0.8f));
    g.setFont (juce::FontOptions (10.5f));
    g.drawText (juce::String (position + 1), getLocalBounds().withTrimmedLeft (34).withTrimmedRight (8).removeFromBottom (28),
                juce::Justification::centredLeft);

    if (hover && ! dragging)
    {
        const auto c = closeArea().toFloat();
        g.setColour (wl::c::dim);
        g.drawLine (c.getX() + 5, c.getY() + 5, c.getRight() - 5, c.getBottom() - 5, 1.5f);
        g.drawLine (c.getRight() - 5, c.getY() + 5, c.getX() + 5, c.getBottom() - 5, 1.5f);
    }
}

void Tile::mouseDown (const juce::MouseEvent& e)
{
    dragging = false;
    dragOffset = e.getPosition();
    if (e.mods.isPopupMenu()) ed.tileMenu (*this);
}

void Tile::mouseDrag (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    if (! dragging && e.getDistanceFromDragStart() < 5) return;
    if (! dragging) { dragging = true; toFront (false); }
    auto p = e.getEventRelativeTo (getParentComponent()).getPosition() - dragOffset;
    setTopLeftPosition (p.x, getY());
    ed.tileDragged (*this, getBounds().getCentreX());
    repaint();
}

void Tile::mouseUp (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    if (dragging) { dragging = false; ed.tileDropped (*this, getBounds().getCentreX()); return; }
    if (closeArea().contains (e.getPosition())) { ed.removeAt (position); return; }
    ed.select (position);
}

// ======================================================================================= MeterView
void MeterView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0d0f1c), r.getX(), r.getY(), juce::Colour (0xff161a2c), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 12.0f);
    g.setColour (wl::c::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);
    if (slot < 0) return;
    const auto& inf = info (type);
    auto area = r.reduced (16, 12);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (wl::c::dim);

    if (inf.meter == mods::MeterGR)
    {
        const float gr = juce::jlimit (0.0f, 24.0f, proc.getMeter (slot));
        if (gr >= peakHold) { peakHold = gr; holdTimer = 45; }
        else if (--holdTimer < 0) peakHold = std::max (gr, peakHold - 0.3f);
        g.drawText ("GAIN REDUCTION", area.removeFromTop (16), juce::Justification::centredLeft);
        g.setGradientFill (wl::grad (theme, area.getTopLeft(), area.getTopRight()));
        g.setFont (juce::FontOptions (42.0f, juce::Font::bold));
        g.drawText (gr < 0.05f ? juce::String ("0.0") : "-" + juce::String (gr, 1), area.removeFromTop (60), juce::Justification::centredLeft);
        auto bar = area.removeFromTop (22);
        const int segs = 24;
        const float sw = bar.getWidth() / segs;
        for (int i = 0; i < segs; ++i)
        {
            auto s = juce::Rectangle<float> (bar.getX() + i * sw + 1, bar.getY(), sw - 2, bar.getHeight());
            const bool lit = (float) i < gr;
            const auto c = theme.a.interpolatedWith (theme.b, (float) i / segs);
            if (lit) { g.setColour (c.withAlpha (0.25f)); g.fillRoundedRectangle (s.expanded (1.5f), 3.0f); }
            g.setColour (lit ? c : wl::c::panelHi);
            g.fillRoundedRectangle (s, 2.0f);
        }
        const float hx = bar.getX() + std::min ((float) segs, peakHold) * sw;
        g.setColour (wl::c::text.withAlpha (0.8f));
        g.fillRect (hx - 1, bar.getY() - 3, 2.0f, bar.getHeight() + 6);
        g.setColour (wl::c::dim);
        g.setFont (juce::FontOptions (10.5f));
        auto scale = area.removeFromTop (16);
        for (int db : { 0, 6, 12, 18, 24 })
            g.drawText (juce::String (db), juce::Rectangle<float> (bar.getX() + db * sw - 12, scale.getY() + 2, 24, 12), juce::Justification::centred);
    }
    else if (inf.meter == mods::MeterEQ)
    {
        g.drawText ("FREQUENCY RESPONSE", area.removeFromTop (16), juce::Justification::centredLeft);
        area.removeFromTop (6);
        auto plot = area;
        auto xOf = [&] (float f) { return plot.getX() + plot.getWidth() * std::log (f / 20.0f) / std::log (1000.0f); };
        auto yOf = [&] (float db) { return plot.getCentreY() - db / 18.0f * plot.getHeight() * 0.5f; };
        g.setColour (wl::c::line);
        for (float f : { 100.0f, 1000.0f, 10000.0f }) g.drawVerticalLine ((int) xOf (f), plot.getY(), plot.getBottom());
        for (float db : { -12.0f, -6.0f, 6.0f, 12.0f }) g.drawHorizontalLine ((int) yOf (db), plot.getX(), plot.getRight());
        g.setColour (wl::c::line.brighter (0.3f));
        g.drawHorizontalLine ((int) yOf (0), plot.getX(), plot.getRight());
        g.setColour (wl::c::dim.withAlpha (0.7f));
        g.setFont (juce::FontOptions (10.0f));
        for (auto [f, t] : { std::pair<float, const char*> { 100.0f, "100" }, { 1000.0f, "1k" }, { 10000.0f, "10k" } })
            g.drawText (t, juce::Rectangle<float> (xOf (f) + 3, plot.getBottom() - 13, 30, 12), juce::Justification::left);

        float v[mods::maxParams];
        for (int k = 0; k < mods::maxParams; ++k) v[k] = proc.realValue (slot, k);
        juce::Path curve, fill;
        for (int i = 0; i <= 240; ++i)
        {
            const float f = 20.0f * std::pow (1000.0f, (float) i / 240.0f);
            const float y = yOf (juce::jlimit (-18.0f, 18.0f, mods::eqResponseDb (type, v, f)));
            if (i == 0) { curve.startNewSubPath (xOf (f), y); fill.startNewSubPath (xOf (f), yOf (0)); }
            else curve.lineTo (xOf (f), y);
            fill.lineTo (xOf (f), y);
        }
        fill.lineTo (plot.getRight(), yOf (0));
        fill.closeSubPath();
        auto gr = wl::grad (theme, plot.getTopLeft(), plot.getTopRight());
        gr.multiplyOpacity (0.18f);
        g.setGradientFill (gr);
        g.fillPath (fill);
        g.setGradientFill (wl::grad (theme, plot.getTopLeft(), plot.getTopRight()));
        g.strokePath (curve, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved));
    }
    else if (inf.meter == mods::MeterPitch)
    {
        const int target = proc.getNote (slot);
        const int heard = juce::roundToInt (proc.getMeter2 (slot));
        const float cents = proc.getMeter (slot);
        g.drawText ("TUNED TO", area.removeFromTop (16), juce::Justification::centredLeft);
        g.setGradientFill (wl::grad (theme, area.getTopLeft(), area.getTopRight()));
        g.setFont (juce::FontOptions (46.0f, juce::Font::bold));
        g.drawText (wl::midiNoteName (target), area.removeFromTop (60), juce::Justification::centredLeft);
        g.setColour (wl::c::dim);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText ("YOUR VOICE  " + (heard >= 0 ? wl::midiNoteName (heard) + "  " + (cents >= 0 ? "+" : "") + juce::String (juce::roundToInt (cents)) + " cents" : juce::String ("--")),
                    area.removeFromTop (18), juce::Justification::centredLeft);
        auto bar = area.removeFromTop (26).reduced (0, 6);
        g.setColour (wl::c::panelHi);
        g.fillRoundedRectangle (bar, 4.0f);
        g.setColour (wl::c::line.brighter (0.4f));
        g.fillRect (bar.getCentreX() - 1, bar.getY() - 4, 2.0f, bar.getHeight() + 8);
        if (heard >= 0)
        {
            const float x = bar.getCentreX() + juce::jlimit (-50.0f, 50.0f, cents) / 50.0f * bar.getWidth() * 0.5f;
            g.setColour (theme.a.withAlpha (0.3f));
            g.fillEllipse (x - 9, bar.getCentreY() - 9, 18, 18);
            g.setColour (theme.a);
            g.fillEllipse (x - 5, bar.getCentreY() - 5, 10, 10);
        }
        g.setColour (wl::c::dim);
        g.setFont (juce::FontOptions (10.0f));
        g.drawText ("flat", bar.withY (bar.getBottom() + 4).withHeight (12), juce::Justification::left);
        g.drawText ("sharp", bar.withY (bar.getBottom() + 4).withHeight (12), juce::Justification::right);
    }
}

// ======================================================================================= Editor
VoxEditor::VoxEditor (VoxProcessor& p)
    : AudioProcessorEditor (p), proc (p), look (theme),
      scope (p.scope, theme, "OUTPUT"), meter (p)
{
    setLookAndFeel (&look);
    meter.theme = theme;
   #if WALL_CHOP
    scope.readoutCaption = "TEMPO";
    scope.readout = [this] { return juce::String (juce::roundToInt (proc.scope.bpm.load())); };
   #else
    scope.readoutCaption = "TUNED NOTE";
    scope.readout = [this] { return wl::midiNoteName (proc.scope.note.load()); };
   #endif
    addAndMakeVisible (scope);

    presetBox.setTextWhenNothingSelected ("Choose a preset");
    presetBox.setJustificationType (juce::Justification::centred);
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0) { proc.presets->apply (idx); selected = 0; shownSignature = {}; }
    };
    addAndMakeVisible (presetBox);
    prevBtn.onClick = [this] { stepPreset (-1); };
    nextBtn.onClick = [this] { stepPreset (1); };
    saveBtn.onClick = [this] { savePreset(); };
    importBtn.onClick = [this] { importPreset(); };
    moreBtn.onClick = [this] { getMorePresets(); };
    saveBtn.setTooltip ("Save this whole chain as a preset");
    moreBtn.setTooltip ("Download new presets from the internet");
    for (auto* b : { &prevBtn, &nextBtn, &saveBtn, &importBtn, &moreBtn }) addAndMakeVisible (*b);

    addBtn.setTooltip ("Add a module to the end of the chain");
    addBtn.onClick = [this] { showAddMenu(); };
    addAndMakeVisible (addBtn);

    for (auto* s : { &inKnob, &outKnob })
    {
        s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 15);
        s->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s->setDoubleClickReturnValue (true, 0.0);
        addAndMakeVisible (*s);
    }
    inAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "in", inKnob);
    outAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "out", outKnob);
    for (auto [l, t] : { std::pair<juce::Label*, const char*> { &inLabel, "INPUT" }, { &outLabel, "OUTPUT" } })
    {
        l->setText (t, juce::dontSendNotification);
        l->setJustificationType (juce::Justification::centred);
        l->setFont (juce::FontOptions (11.0f, juce::Font::bold));
        l->setColour (juce::Label::textColourId, wl::c::dim);
        addAndMakeVisible (*l);
    }

    addAndMakeVisible (meter);
    status.setColour (juce::Label::textColourId, wl::c::dim);
    status.setFont (juce::FontOptions (12.5f));
    status.setText ("Drag modules to reorder. Click + to add. Right-click a module for more options.", juce::dontSendNotification);
    addAndMakeVisible (status);

    rebuildPresetMenu();
    setSize (1140, 730);
    rebuildRack();
    rebuildPanel();
    startTimerHz (30);

   #if WALL_SNAPSHOT
    // capture the tuner, EQ and compressor panels, then quit
    auto shot = [] (juce::Component::SafePointer<VoxEditor> safe, int sel, juce::String suffix, bool quit)
    {
        if (safe == nullptr) return;
        safe->select (sel);
        juce::Timer::callAfterDelay (400, [safe, suffix, quit]
        {
            if (safe == nullptr) return;
            const auto img = safe->createComponentSnapshot (safe->getLocalBounds(), true, 1.0f);
            auto dir = juce::File (juce::SystemStats::getEnvironmentVariable ("WALL_SNAPSHOT_DIR", "/tmp"));
            dir.createDirectory();
            auto f = dir.getChildFile (juce::String (JucePlugin_Name).removeCharacters (" ") + suffix + ".png");
            f.deleteFile();
            juce::FileOutputStream os (f);
            juce::PNGImageFormat().writeImageToStream (img, os);
            os.flush();
            if (quit) juce::JUCEApplicationBase::quit();
        });
    };
    juce::Component::SafePointer<VoxEditor> me (this);
    juce::Timer::callAfterDelay (2000, [shot, me] { shot (me, 0, "", false); });
    juce::Timer::callAfterDelay (3000, [shot, me] { shot (me, 1, "_eq", false); });
    juce::Timer::callAfterDelay (4000, [shot, me] { shot (me, 2, "_comp", true); });
   #endif
}

VoxEditor::~VoxEditor() { stopTimer(); setLookAndFeel (nullptr); }

juce::String VoxEditor::chainSignature() const
{
    juce::String s;
    for (int i = 0; i < chain::numSlots; ++i) s << proc.slotType (i) << ",";
    return s;
}

// ---------------------------------------------------------------------- rack
void VoxEditor::rebuildRack()
{
    tiles.clear();
    const auto c = proc.getChain();
    for (int i = 0; i < (int) c.size(); ++i)
    {
        auto t = std::make_unique<Tile> (*this, i, c[(size_t) i].type);
        addAndMakeVisible (*t);
        tiles.push_back (std::move (t));
    }
    selected = juce::jlimit (0, juce::jmax (0, (int) c.size() - 1), selected);
    addBtn.setVisible ((int) c.size() < chain::numSlots);
    shownSignature = chainSignature();
    layoutRack();
    repaint();
}

void VoxEditor::layoutRack()
{
    const int n = (int) tiles.size(), gap = 10, addW = 52;
    const int avail = rackArea.getWidth() - addW - gap;
    const int w = n == 0 ? 0 : juce::jlimit (74, 150, (avail - gap * (n - 1)) / n);
    int x = rackArea.getX();
    for (auto& t : tiles)
    {
        t->setBounds (x, rackArea.getY(), w, rackArea.getHeight());
        x += w + gap;
    }
    addBtn.setBounds (x, rackArea.getY() + 8, addW, rackArea.getHeight() - 16);
}

int VoxEditor::dropIndexFor (int x) const
{
    int idx = 0;
    for (auto& t : tiles)
        if (! t->isMouseButtonDown() && t->getBounds().getCentreX() < x) ++idx;
    return idx;
}

void VoxEditor::tileDragged (Tile& t, int x)
{
    juce::ignoreUnused (t);
    dropMarker = dropIndexFor (x);
    repaint (rackArea.expanded (8));
}

void VoxEditor::tileDropped (Tile& t, int x)
{
    const int to = dropIndexFor (x);
    dropMarker = -1;
    const int from = t.position;
    if (to != from) { proc.moveModule (from, to); selected = to; }
    rebuildRack();
    rebuildPanel();
}

void VoxEditor::removeAt (int position)
{
    proc.removeModule (position);
    if (selected >= position && selected > 0) --selected;
    rebuildRack();
    rebuildPanel();
}

void VoxEditor::select (int position)
{
    selected = position;
    for (auto& t : tiles) t->repaint();
    rebuildPanel();
}

static void addModuleItems (juce::PopupMenu& m, int idOffset)
{
    juce::String lastGroup;
    const auto& cat = mods::catalogue();
    for (const char* group : { "Pitch", "Dynamics", "EQ", "Color", "Space", "Chop" })
    {
        bool header = false;
        for (int t = 1; t < (int) cat.size(); ++t)
        {
            if (juce::String (cat[(size_t) t].group) != group) continue;
            if (! header) { m.addSectionHeader (group); header = true; }
            m.addItem (t + idOffset, cat[(size_t) t].name);
        }
    }
}

void VoxEditor::showAddMenu()
{
    juce::PopupMenu m;
    addModuleItems (m, 0);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addBtn),
                     [safe = juce::Component::SafePointer<VoxEditor> (this)] (int r)
                     {
                         if (safe == nullptr || r <= 0) return;
                         const int pos = safe->proc.addModule (r);
                         if (pos >= 0) safe->selected = pos;
                         safe->rebuildRack();
                         safe->rebuildPanel();
                     });
}

void VoxEditor::tileMenu (Tile& t)
{
    const int pos = t.position;
    const int n = (int) tiles.size();
    juce::PopupMenu m, replace;
    addModuleItems (replace, 1000);
    m.addItem (1, "Duplicate", n < chain::numSlots);
    m.addItem (2, "Move Left", pos > 0);
    m.addItem (3, "Move Right", pos < n - 1);
    m.addSubMenu ("Replace With", replace);
    m.addSeparator();
    m.addItem (4, "Remove");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&t),
                     [safe = juce::Component::SafePointer<VoxEditor> (this), pos] (int r)
                     {
                         if (safe == nullptr || r <= 0) return;
                         auto& p = safe->proc;
                         if (r == 1) { p.duplicateModule (pos); safe->selected = pos + 1; }
                         else if (r == 2) { p.moveModule (pos, pos - 1); safe->selected = pos - 1; }
                         else if (r == 3) { p.moveModule (pos, pos + 1); safe->selected = pos + 1; }
                         else if (r == 4) { p.removeModule (pos); safe->selected = juce::jmax (0, pos - 1); }
                         else if (r > 1000) { p.replaceModule (pos, r - 1000); safe->selected = pos; }
                         safe->rebuildRack();
                         safe->rebuildPanel();
                     });
}

// ---------------------------------------------------------------------- module panel
void VoxEditor::rebuildPanel()
{
    controls.clear();
    const auto c = proc.getChain();
    const bool has = juce::isPositiveAndBelow (selected, (int) c.size());
    panelSlot = has ? selected : -1;
    panelType = has ? c[(size_t) selected].type : 0;
    meter.slot = panelSlot;
    meter.type = panelType;

    if (! has)
    {
        panelTitle = "YOUR CHAIN IS EMPTY";
        panelDesc = "Click + to add modules (tune, compressors, EQs, saturation, delay, reverb...) in any order, "
                    "or choose a preset to start from. Save your own chain with SAVE.";
        meter.setVisible (false);
        resized();
        repaint();
        return;
    }

    const auto& inf = info (panelType);
    panelTitle = juce::String (inf.name).toUpperCase();
    panelDesc = inf.description;
    panelColour = juce::Colour (inf.colour);
    meter.setVisible (inf.meter != mods::MeterNone);

    for (int k = 0; k < (int) inf.params.size(); ++k)
    {
        const auto spec = inf.params[(size_t) k];
        Control ctl;
        ctl.k = k;
        ctl.label = std::make_unique<juce::Label> ("", juce::String (spec.name).toUpperCase());
        ctl.label->setJustificationType (juce::Justification::centred);
        ctl.label->setFont (juce::FontOptions (11.5f, juce::Font::bold));
        ctl.label->setColour (juce::Label::textColourId, wl::c::dim);
        addAndMakeVisible (*ctl.label);
        auto* prm = proc.apvts.getParameter (chain::paramId (panelSlot, k));
        if (spec.isChoice())
        {
            ctl.combo = std::make_unique<juce::ComboBox>();
            ctl.combo->addItemList (spec.choices, 1);
            ctl.combo->setSelectedItemIndex (juce::roundToInt (spec.toValue (prm->getValue())), juce::dontSendNotification);
            ctl.combo->onChange = [prm, spec, box = ctl.combo.get()]
            {
                prm->beginChangeGesture();
                prm->setValueNotifyingHost (spec.toNorm ((float) box->getSelectedItemIndex()));
                prm->endChangeGesture();
            };
            addAndMakeVisible (*ctl.combo);
        }
        else
        {
            ctl.slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
            ctl.slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 84, 16);
            ctl.slider->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            addAndMakeVisible (*ctl.slider);
            ctl.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, chain::paramId (panelSlot, k), *ctl.slider);
            // set after attaching: the attachment installs the host's generic text functions
            ctl.slider->textFromValueFunction = [spec] (double v) { return spec.text (spec.toValue ((float) v)); };
            ctl.slider->valueFromTextFunction = [spec] (const juce::String& t)
            {
                float v = t.retainCharacters ("-0123456789.").getFloatValue();
                if (juce::String (spec.unit) == "Hz" && t.containsIgnoreCase ("k")) v *= 1000.0f;
                if (juce::String (spec.unit) == "%") v /= 100.0f;
                return (double) spec.toNorm (v);
            };
            ctl.slider->setDoubleClickReturnValue (true, spec.toNorm (spec.def));
            ctl.slider->updateText();
        }
        controls.push_back (std::move (ctl));
    }
    resized();
    repaint();
}

// ---------------------------------------------------------------------- presets
void VoxEditor::rebuildPresetMenu()
{
    presetBox.clear (juce::dontSendNotification);
    const auto& all = proc.presets->getAll();
    juce::StringArray cats;
    for (auto& pr : all) cats.addIfNotAlreadyThere (pr.category);
    for (auto& cat : cats)
    {
        presetBox.addSectionHeading (cat);
        for (int i = 0; i < all.size(); ++i)
            if (all[i].category == cat) presetBox.addItem (all[i].name, i + 1);
    }
    for (int i = 0; i < all.size(); ++i)
        if (all[i].name == proc.presets->currentName) { presetBox.setSelectedId (i + 1, juce::dontSendNotification); break; }
}

void VoxEditor::stepPreset (int delta)
{
    const int n = proc.presets->getAll().size();
    if (n == 0) return;
    const int cur = presetBox.getSelectedId() - 1;
    presetBox.setSelectedId ((cur < 0 ? 0 : ((cur + delta) % n + n) % n) + 1, juce::sendNotificationSync);
}

void VoxEditor::savePreset()
{
    auto* w = new juce::AlertWindow ("Save Chain", "Name your chain preset:", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", proc.presets->currentName == "Empty Chain" ? juce::String() : proc.presets->currentName);
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<VoxEditor> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
    {
        if (result != 1 || safe == nullptr) return;
        const auto name = w->getTextEditorContents ("name").trim();
        if (name.isEmpty()) return;
        const bool ok = safe->proc.presets->saveCurrent (name);
        safe->rebuildPresetMenu();
        safe->status.setText (ok ? "Saved \"" + name + "\"" : "Couldn't save the preset.", juce::dontSendNotification);
    }), true);
}

void VoxEditor::importPreset()
{
    chooser = std::make_unique<juce::FileChooser> ("Import a preset", juce::File(), "*.json");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (f == juce::File()) return;
        const auto err = proc.presets->importFile (f);
        rebuildPresetMenu();
        status.setText (err.isEmpty() ? "Imported " + f.getFileNameWithoutExtension() : err, juce::dontSendNotification);
    });
}

void VoxEditor::getMorePresets()
{
    moreBtn.setEnabled (false);
    status.setText ("Checking for new presets...", juce::dontSendNotification);
    juce::Component::SafePointer<VoxEditor> safe (this);
    PresetManager::downloadOnline ([safe] (juce::String msg)
    {
        if (safe == nullptr) return;
        safe->proc.presets->refresh();
        safe->rebuildPresetMenu();
        safe->status.setText (msg, juce::dontSendNotification);
        safe->moreBtn.setEnabled (true);
    });
}

// ---------------------------------------------------------------------- timer / paint / layout
void VoxEditor::timerCallback()
{
    if (chainSignature() != shownSignature && ! juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown())
    {
        rebuildRack();
        rebuildPanel();
    }
    // keep drop-downs in sync with automation / presets
    if (panelSlot >= 0)
    {
        const auto& inf = info (panelType);
        for (auto& ctl : controls)
            if (ctl.combo && ! ctl.combo->isPopupActive())
            {
                const int idx = juce::roundToInt (inf.params[(size_t) ctl.k].toValue (proc.apvts.getParameter (chain::paramId (panelSlot, ctl.k))->getValue()));
                if (idx != ctl.combo->getSelectedItemIndex()) ctl.combo->setSelectedItemIndex (idx, juce::dontSendNotification);
            }
    }
    if (meter.isVisible()) meter.repaint();
}

void VoxEditor::paint (juce::Graphics& g)
{
    wl::drawBackground (g, getLocalBounds(), 56.0f, theme, "WALL", word2);

    // rack label + connecting line between modules
    g.setColour (wl::c::dim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("YOUR CHAIN  -  signal flows left to right", rackArea.getX(), rackArea.getY() - 18, 400, 16, juce::Justification::centredLeft);
    if (! tiles.empty())
    {
        const float y = (float) rackArea.getCentreY();
        g.setGradientFill (wl::grad (theme, { (float) rackArea.getX(), y }, { (float) rackArea.getRight(), y }));
        g.setOpacity (0.35f);
        g.fillRect ((float) rackArea.getX(), y - 1.0f, (float) (addBtn.getX() - rackArea.getX()), 2.0f);
        g.setOpacity (1.0f);
    }
    if (dropMarker >= 0)
    {
        int x = rackArea.getX() - 5;
        int idx = 0;
        for (auto& t : tiles)
        {
            if (t->isMouseButtonDown()) continue;
            if (idx == dropMarker) { x = t->getX() - 5; break; }
            x = t->getRight() + 5;
            ++idx;
        }
        g.setColour (theme.a);
        g.fillRoundedRectangle ((float) x - 2, (float) rackArea.getY() - 4, 4.0f, (float) rackArea.getHeight() + 8, 2.0f);
    }

    // module panel
    wl::drawPanel (g, panelArea.toFloat(), {}, true, theme, 0.0f);
    auto head = panelArea.reduced (20, 14).removeFromTop (54);
    if (panelSlot >= 0)
    {
        g.setColour (panelColour);
        g.fillRoundedRectangle ((float) head.getX(), (float) head.getY() + 4, 5.0f, 22.0f, 2.5f);
    }
    g.setColour (wl::c::text);
    g.setFont (juce::FontOptions (19.0f, juce::Font::bold));
    g.drawText (panelTitle, head.removeFromTop (30).withTrimmedLeft (panelSlot >= 0 ? 14 : 0), juce::Justification::centredLeft);
    g.setColour (wl::c::dim);
    g.setFont (juce::FontOptions (13.0f));
    g.drawFittedText (panelDesc, head.withTrimmedRight (meter.isVisible() ? 360 : 0), juce::Justification::topLeft, 2);
}

void VoxEditor::resized()
{
    auto head = getLocalBounds().removeFromTop (56).reduced (16, 11);
    moreBtn.setBounds (head.removeFromRight (104));
    head.removeFromRight (8);
    importBtn.setBounds (head.removeFromRight (84));
    head.removeFromRight (8);
    saveBtn.setBounds (head.removeFromRight (70));
    auto centre = getLocalBounds().removeFromTop (56).withSizeKeepingCentre (420, 34);
    prevBtn.setBounds (centre.removeFromLeft (34));
    nextBtn.setBounds (centre.removeFromRight (34));
    presetBox.setBounds (centre.reduced (6, 0));

    scope.setBounds (16, 66, getWidth() - 32, 100);

    rackArea = { 16, 200, getWidth() - 32 - 196, 86 };
    inKnob.setBounds (getWidth() - 196, 178, 90, 92);
    outKnob.setBounds (getWidth() - 104, 178, 90, 92);
    inLabel.setBounds (getWidth() - 196, 270, 90, 16);
    outLabel.setBounds (getWidth() - 104, 270, 90, 16);
    layoutRack();

    panelArea = { 16, 300, getWidth() - 32, getHeight() - 300 - 36 };
    status.setBounds (getLocalBounds().removeFromBottom (32).reduced (18, 0));

    auto body = panelArea.reduced (20, 14).withTrimmedTop (64);
    if (meter.isVisible()) meter.setBounds (body.removeFromRight (340));
    body.removeFromRight (16);

    const int n = (int) controls.size();
    if (n == 0) return;
    const int cols = n <= 5 ? n : (n <= 6 ? 3 : 4);
    const int rows = (n + cols - 1) / cols;
    const int cw = juce::jmin (150, body.getWidth() / cols), ch = juce::jmin (160, body.getHeight() / rows);
    const int top = body.getY() + (body.getHeight() - ch * rows) / 2;
    for (int i = 0; i < n; ++i)
    {
        auto& ctl = controls[(size_t) i];
        auto cell = juce::Rectangle<int> (body.getX() + (i % cols) * cw, top + (i / cols) * ch, cw, ch).reduced (6, 2);
        ctl.label->setBounds (cell.removeFromBottom (18));
        if (ctl.slider)
        {
            const int sz = juce::jmin (cell.getWidth(), cell.getHeight());
            ctl.slider->setBounds (cell.withSizeKeepingCentre (juce::jmax (sz, 84), sz));
        }
        else ctl.combo->setBounds (cell.withSizeKeepingCentre (cell.getWidth() - 4, 32));
    }
}
