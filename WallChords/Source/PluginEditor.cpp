#include "PluginEditor.h"

namespace col
{
    const juce::Colour bg     (0xff15131b);
    const juce::Colour panel  (0xff201d28);
    const juce::Colour accent (0xffffb238);
    const juce::Colour text   (0xfff1ece2);
    const juce::Colour dim    (0xff9c95a8);
    const juce::Colour secCols[] { juce::Colour (0xff3d5a80), juce::Colour (0xff6d597a), juce::Colour (0xffb56576),
                                   juce::Colour (0xffe09f3e), juce::Colour (0xff2a9d8f), juce::Colour (0xff8d6a9f) };
}

// ------------------------------------------------------------------ look
WallChordsLook::WallChordsLook()
{
    setColour (juce::ComboBox::backgroundColourId, col::panel.brighter (0.1f));
    setColour (juce::ComboBox::outlineColourId, col::accent.withAlpha (0.35f));
    setColour (juce::ComboBox::textColourId, col::text);
    setColour (juce::ComboBox::arrowColourId, col::accent);
    setColour (juce::PopupMenu::backgroundColourId, col::panel);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
    setColour (juce::TextButton::buttonColourId, col::panel.brighter (0.1f));
    setColour (juce::TextButton::buttonOnColourId, col::accent);
    setColour (juce::TextButton::textColourOffId, col::text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    setColour (juce::Label::textColourId, col::text);
    setColour (juce::Slider::textBoxTextColourId, col::dim);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}

void WallChordsLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&)
{
    auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
    const float size = juce::jmin (r.getWidth(), r.getHeight());
    r = r.withSizeKeepingCentre (size, size);
    const auto c = r.getCentre();
    const float rad = size * 0.5f, ang = a0 + pos * (a1 - a0);
    juce::Path track, value;
    track.addCentredArc (c.x, c.y, rad - 3, rad - 3, 0, a0, a1, true);
    value.addCentredArc (c.x, c.y, rad - 3, rad - 3, 0, a0, ang, true);
    const juce::PathStrokeType st (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour (col::panel.brighter (0.3f)); g.strokePath (track, st);
    g.setColour (col::accent); g.strokePath (value, st);
    g.setColour (col::panel.brighter (0.18f)); g.fillEllipse (r.reduced (9.0f));
    juce::Path tick;
    tick.addRoundedRectangle (-1.5f, -(rad - 11), 3.0f, rad * 0.45f, 1.5f);
    g.setColour (col::text);
    g.fillPath (tick, juce::AffineTransform::rotation (ang).translated (c));
}

void WallChordsLook::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hover, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (2.0f);
    const bool on = b.getToggleState();
    const auto text = b.getButtonText();
    if (text.isEmpty())
    {
        g.setColour (on ? col::accent : col::panel.brighter (0.3f));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (on ? juce::Colours::black : col::dim);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (on ? "ON" : "OFF", r, juce::Justification::centred);
        return;
    }
    g.setColour (on ? col::accent.withAlpha (0.18f) : col::panel.brighter (hover ? 0.15f : 0.08f));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (on ? col::accent : col::dim.withAlpha (0.5f));
    g.drawRoundedRectangle (r, 8.0f, 1.2f);
    g.fillEllipse (r.getX() + 10, r.getCentreY() - 5, 10, 10);
    g.setColour (on ? col::text : col::dim);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (text, r.withTrimmedLeft (28), juce::Justification::centredLeft);
}

// ------------------------------------------------------------------ drag button
void DragButton::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 6 || ! makeFile) return;
    const auto f = makeFile();
    if (! f.existsAsFile()) return;
    dragging = true;
    juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this,
                                                               [safe = juce::Component::SafePointer<DragButton> (this)]
                                                               { if (safe != nullptr) safe->dragging = false; });
}

void DragButton::mouseUp (const juce::MouseEvent& e)
{
    juce::TextButton::mouseUp (e);
    if (dragging || e.getDistanceFromDragStart() >= 6 || ! makeFile) { dragging = false; return; }
    const auto f = makeFile();
    if (! f.existsAsFile()) return;
    chooser = std::make_unique<juce::FileChooser> ("Save MIDI", juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile (f.getFileName()), "*.mid");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [f] (const juce::FileChooser& fc)
                          {
                              const auto dest = fc.getResult();
                              if (dest != juce::File()) f.copyFileTo (dest.withFileExtension ("mid"));
                          });
}

// ------------------------------------------------------------------ timeline
void Timeline::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (col::panel);
    g.fillRoundedRectangle (r, 10.0f);
    if (song == nullptr || song->length <= 0.0) return;

    auto area = r.reduced (10.0f);
    const float secH = 26.0f;
    auto xOf = [&] (double beat) { return area.getX() + (float) (beat / song->length) * area.getWidth(); };

    for (size_t i = 0; i < song->sections.size(); ++i)
    {
        const auto& s = song->sections[i];
        juce::Rectangle<float> box (xOf (s.start), area.getY(), xOf (s.start + s.len) - xOf (s.start) - 2.0f, secH);
        g.setColour (col::secCols[i % 6].withAlpha (0.85f));
        g.fillRoundedRectangle (box, 5.0f);
        g.setColour (col::text);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawFittedText (s.name + "  " + juce::String ((int) (s.len / 4)) + " bars", box.toNearestInt().reduced (4, 0), juce::Justification::centredLeft, 1);
    }

    // chord lane
    const float laneY = area.getY() + secH + 8.0f, laneH = area.getBottom() - laneY;
    g.setFont (juce::FontOptions (11.5f));
    float lastTextEnd = -1.0f;
    for (const auto& sl : song->slots)
    {
        const float x0 = xOf (sl.start), x1 = xOf (sl.start + sl.len);
        g.setColour (col::panel.brighter (0.12f));
        g.fillRect (x0, laneY, juce::jmax (1.0f, x1 - x0 - 1.0f), laneH);
        if (x0 >= lastTextEnd)
        {
            const float tw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), sl.chord.name) + 6.0f;
            if (tw < (x1 - x0) * 2.2f)
            {
                g.setColour (col::text.withAlpha (0.9f));
                g.drawText (sl.chord.name, juce::Rectangle<float> (x0 + 3, laneY, tw, laneH), juce::Justification::centredLeft);
                lastTextEnd = x0 + tw;
            }
        }
    }

    const double ph = proc.getPlayhead();
    if (ph >= 0.0)
    {
        g.setColour (col::accent);
        g.fillRect (xOf (ph) - 1.0f, area.getY() - 4.0f, 2.0f, area.getHeight() + 8.0f);
    }
}

void Timeline::mouseDown (const juce::MouseEvent& e)
{
    if (song == nullptr) return;
    auto area = getLocalBounds().toFloat().reduced (10.0f);
    double beat = (e.position.x - area.getX()) / area.getWidth() * song->length;
    beat = juce::jlimit (0.0, song->length - 0.01, beat);
    for (auto& s : song->sections)   // snap to the start of the clicked section
        if (beat >= s.start && beat < s.start + s.len) { beat = s.start; break; }
    proc.startPreview (beat);
}

// ------------------------------------------------------------------ editor
juce::ComboBox& WallChordsEditor::addCombo (const char* paramId, const juce::String& label)
{
    auto box = std::make_unique<juce::ComboBox>();
    box->addItemList (proc.apvts.getParameter (paramId)->getAllValueStrings(), 1);
    addAndMakeVisible (*box);
    comboAtts.push_back (std::make_unique<CBA> (proc.apvts, paramId, *box));
    auto lab = std::make_unique<juce::Label> ("", label);
    lab->setFont (juce::FontOptions (12.0f, juce::Font::bold));
    lab->setColour (juce::Label::textColourId, col::dim);
    addAndMakeVisible (*lab);
    comboLabels.push_back (std::move (lab));
    combos.push_back (std::move (box));
    return *combos.back();
}

WallChordsEditor::WallChordsEditor (WallChordsProcessor& p) : AudioProcessorEditor (p), proc (p), timeline (p)
{
    setLookAndFeel (&look);

    for (int i = 0; i < 3; ++i)
    {
        genreBtn[i].setButtonText (th::genreNames[i].toUpperCase());
        genreBtn[i].setClickingTogglesState (false);
        genreBtn[i].onClick = [this, i] { proc.apvts.getParameter ("genre")->setValueNotifyingHost (proc.apvts.getParameter ("genre")->convertTo0to1 ((float) i)); };
        addAndMakeVisible (genreBtn[i]);
    }

    addCombo ("key", "KEY");
    addCombo ("mode", "MODE");
    addCombo ("section", "SONG PART");
    addCombo ("bars", "BARS");
    addCombo ("color", "CHORD COLOR");

    progLabel.setText ("PROGRESSION", juce::dontSendNotification);
    progLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    progLabel.setColour (juce::Label::textColourId, col::dim);
    addAndMakeVisible (progLabel);
    addAndMakeVisible (progBox);
    rebuildProgressionItems();

    generateBtn.onClick = [this] { proc.newIdea(); };
    previewBtn.onClick = [this] { if (proc.isPreviewing()) proc.stopPreview(); else proc.startPreview (0.0); };
    addAndMakeVisible (generateBtn);
    addAndMakeVisible (previewBtn);
    addAndMakeVisible (timeline);

    const juce::StringArray* styleLists[4] = { &th::chordStyles, &th::bassStyles, &th::leadStyles, &th::padStyles };
    const char* styleIds[4] = { "chordStyle", "bassStyle", "leadStyle", "padStyle" };
    for (int i = 0; i < 4; ++i)
    {
        addAndMakeVisible (partOn[i]);
        partOnAtt[i] = std::make_unique<BA> (proc.apvts, "on" + juce::String (i), partOn[i]);
        partLabel[i].setText (th::partNames[i].toUpperCase(), juce::dontSendNotification);
        partLabel[i].setFont (juce::FontOptions (15.0f, juce::Font::bold));
        addAndMakeVisible (partLabel[i]);
        partStyle[i].addItemList (*styleLists[i], 1);
        addAndMakeVisible (partStyle[i]);
        partStyleAtt[i] = std::make_unique<CBA> (proc.apvts, styleIds[i], partStyle[i]);
        drag[i] = std::make_unique<DragButton> ("DRAG MIDI");
        drag[i]->makeFile = [this, i] { return proc.writeMidiFile (1 << i); };
        drag[i]->setTooltip ("Drag onto a track in Logic or Ableton. Click to save the file instead.");
        addAndMakeVisible (*drag[i]);
    }
    dragAll.makeFile = [this] { return proc.writeMidiFile (0xF); };
    addAndMakeVisible (dragAll);

    const char* knobIds[7] = { "movement", "humanize", "strum", "swell", "dynamics", "liveIntensity", "volume" };
    const char* knobNames[7] = { "MOVEMENT", "HUMANIZE", "STRUM", "SWELL", "DYNAMICS", "LIVE ENERGY", "VOLUME" };
    for (int i = 0; i < 7; ++i)
    {
        knobs[i].setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        knobs[i].setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
        addAndMakeVisible (knobs[i]);
        knobAtts[i] = std::make_unique<SA> (proc.apvts, knobIds[i], knobs[i]);
        knobLabels[i].setText (knobNames[i], juce::dontSendNotification);
        knobLabels[i].setJustificationType (juce::Justification::centred);
        knobLabels[i].setFont (juce::FontOptions (12.0f, juce::Font::bold));
        knobLabels[i].setColour (juce::Label::textColourId, col::dim);
        addAndMakeVisible (knobLabels[i]);
    }

    followDaw.setButtonText ("Play with DAW");
    sound.setButtonText ("Built-in Sound");
    liveMode.setButtonText ("Live Chords (play your keyboard)");
    for (auto* b : { &followDaw, &sound, &liveMode }) addAndMakeVisible (*b);
    followAtt = std::make_unique<BA> (proc.apvts, "followDaw", followDaw);
    soundAtt  = std::make_unique<BA> (proc.apvts, "sound", sound);
    liveAtt   = std::make_unique<BA> (proc.apvts, "liveMode", liveMode);

    hint.setColour (juce::Label::textColourId, col::dim);
    hint.setFont (juce::FontOptions (13.0f));
    hint.setText ("Click a section to hear it. Drag parts onto MIDI tracks.",
                  juce::dontSendNotification);
    addAndMakeVisible (hint);
    liveChord.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    liveChord.setColour (juce::Label::textColourId, col::accent);
    liveChord.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (liveChord);

    setSize (1180, 620);
    startTimerHz (30);
}

WallChordsEditor::~WallChordsEditor() { stopTimer(); setLookAndFeel (nullptr); }

void WallChordsEditor::rebuildProgressionItems()
{
    const int genre = (int) proc.apvts.getRawParameterValue ("genre")->load();
    const int key = (int) proc.apvts.getRawParameterValue ("key")->load();
    const bool minor = (int) proc.apvts.getRawParameterValue ("mode")->load() == 1;
    const int color = (int) proc.apvts.getRawParameterValue ("color")->load();
    progAtt.reset();
    progBox.clear (juce::dontSendNotification);
    progBox.addItem ("Auto - picks the best fit for each section", 1);
    const auto& list = th::progressions (genre, minor, th::RoleMain);
    for (int i = 0; i < 8; ++i)
        progBox.addItem (i < list.size() ? th::progressionLabel (list[i], key, minor, genre, color) : juce::String ("-"), i + 2);
    for (int i = list.size(); i < 8; ++i) progBox.setItemEnabled (i + 2, false);
    progAtt = std::make_unique<CBA> (proc.apvts, "prog", progBox);
    progSignature = juce::String (genre) + ":" + juce::String (key) + ":" + juce::String ((int) minor) + ":" + juce::String (color);
}

void WallChordsEditor::timerCallback()
{
    const int genre = (int) proc.apvts.getRawParameterValue ("genre")->load();
    for (int i = 0; i < 3; ++i) genreBtn[i].setToggleState (i == genre, juce::dontSendNotification);

    const juce::String sig = juce::String (genre) + ":" + juce::String ((int) proc.apvts.getRawParameterValue ("key")->load()) + ":"
                           + juce::String ((int) proc.apvts.getRawParameterValue ("mode")->load()) + ":"
                           + juce::String ((int) proc.apvts.getRawParameterValue ("color")->load());
    if (sig != progSignature) rebuildProgressionItems();

    previewBtn.setButtonText (proc.isPreviewing() ? "STOP" : "PREVIEW");
    previewBtn.setToggleState (proc.isPreviewing(), juce::dontSendNotification);

    auto s = proc.getSong();
    if (s != shownSong) { shownSong = s; timeline.song = s; }
    timeline.repaint();

    const bool live = proc.apvts.getRawParameterValue ("liveMode")->load() > 0.5f;
    liveChord.setText (live ? proc.getLiveChordName() : juce::String(), juce::dontSendNotification);
}

void WallChordsEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    g.setColour (col::text);
    g.setFont (juce::FontOptions (30.0f, juce::Font::bold));
    g.drawText ("WALL", 24, 16, 90, 40, juce::Justification::centredLeft);
    g.setColour (col::accent);
    g.drawText ("CHORDS", 106, 16, 160, 40, juce::Justification::centredLeft);

    g.setColour (col::panel);
    g.fillRoundedRectangle (juce::Rectangle<float> (16, 316, 690, 236), 10.0f);
    g.fillRoundedRectangle (juce::Rectangle<float> (718, 316, 446, 236), 10.0f);
    g.setColour (col::accent);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("PARTS", 32, 322, 200, 22, juce::Justification::centredLeft);
    g.drawText ("FEEL", 734, 322, 200, 22, juce::Justification::centredLeft);
}

void WallChordsEditor::resized()
{
    // header
    generateBtn.setBounds (getWidth() - 176, 18, 160, 38);
    previewBtn.setBounds (getWidth() - 308, 18, 120, 38);
    for (int i = 0; i < 3; ++i) genreBtn[i].setBounds (290 + i * 120, 18, 112, 38);

    // settings row
    int x = 16;
    const int widths[5] = { 110, 110, 150, 90, 150 };
    for (size_t i = 0; i < combos.size(); ++i)
    {
        comboLabels[i]->setBounds (x, 72, widths[i], 18);
        combos[i]->setBounds (x, 92, widths[i], 30);
        x += widths[i] + 10;
    }
    progLabel.setBounds (x, 72, 200, 18);
    progBox.setBounds (x, 92, getWidth() - 16 - x, 30);

    timeline.setBounds (16, 138, getWidth() - 32, 164);
    liveChord.setBounds (650, 18, getWidth() - 316 - 650, 38);

    // parts
    for (int i = 0; i < 4; ++i)
    {
        const int y = 350 + i * 44;
        partOn[i].setBounds (30, y + 6, 46, 24);
        partLabel[i].setBounds (86, y, 110, 36);
        partStyle[i].setBounds (200, y + 3, 300, 30);
        drag[i]->setBounds (512, y + 3, 180, 30);
    }
    dragAll.setBounds (512, 350 + 4 * 44 + 4, 180, 32);

    // feel knobs
    for (int i = 0; i < 7; ++i)
    {
        const int col = i % 4, row = i / 4;
        knobLabels[i].setBounds (730 + col * 106, 348 + row * 100, 100, 16);
        knobs[i].setBounds (730 + col * 106, 364 + row * 100, 100, 80);
    }

    // bottom
    followDaw.setBounds (16, 566, 170, 36);
    sound.setBounds (194, 566, 170, 36);
    liveMode.setBounds (372, 566, 290, 36);
    hint.setBounds (674, 566, getWidth() - 690, 36);
}
