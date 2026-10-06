#include "PluginEditor.h"

namespace col
{
    const juce::Colour panel  = wl::c::panel;
    const juce::Colour accent = chordsTheme.a;
    const juce::Colour text   = wl::c::text;
    const juce::Colour dim    = wl::c::dim;
    // one colour per part in the note roll: chords, bass, lead, pad
    const juce::Colour partCols[] { juce::Colour (0xffffb238), juce::Colour (0xffff5f3d), juce::Colour (0xffffe08a), juce::Colour (0xffc77dff) };
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
void Timeline::renderRoll (juce::Rectangle<float> area)
{
    rollImage = juce::Image (juce::Image::ARGB, juce::jmax (1, getWidth()), juce::jmax (1, getHeight()), true);
    if (song == nullptr || song->notes.empty()) return;
    juce::Graphics g (rollImage);
    int lo = 127, hi = 0;
    for (auto& n : song->notes) { lo = juce::jmin (lo, n.pitch); hi = juce::jmax (hi, n.pitch); }
    hi = juce::jmax (hi, lo + 12);
    const float noteH = juce::jlimit (1.5f, 4.0f, area.getHeight() / (float) (hi - lo + 1));
    for (int pass = 0; pass < 2; ++pass)
        for (auto& n : song->notes)
        {
            const float x0 = area.getX() + (float) (n.beat / song->length) * area.getWidth();
            const float x1 = area.getX() + (float) ((n.beat + n.len) / song->length) * area.getWidth();
            const float y = area.getBottom() - (float) (n.pitch - lo) / (float) (hi - lo) * (area.getHeight() - noteH) - noteH;
            const auto c = col::partCols[n.part].withAlpha (pass == 0 ? 0.12f : 0.35f + 0.6f * n.vel / 127.0f);
            g.setColour (c);
            if (pass == 0) g.fillRoundedRectangle (x0 - 1.5f, y - 2.5f, juce::jmax (2.0f, x1 - x0) + 3.0f, noteH + 5.0f, 2.0f);
            else g.fillRect (x0, y, juce::jmax (1.0f, x1 - x0 - 0.5f), noteH);
        }
}

void Timeline::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0d0f1c), r.getX(), r.getY(), juce::Colour (0xff171a2e), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 14.0f);
    g.setColour (wl::c::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 14.0f, 1.0f);
    if (song == nullptr || song->length <= 0.0) return;

    auto area = r.reduced (14.0f, 12.0f);
    const float secH = 24.0f, chordH = 20.0f;
    auto xOf = [&] (double beat) { return area.getX() + (float) (beat / song->length) * area.getWidth(); };

    // section chips
    for (size_t i = 0; i < song->sections.size(); ++i)
    {
        const auto& s = song->sections[i];
        juce::Rectangle<float> box (xOf (s.start), area.getY(), xOf (s.start + s.len) - xOf (s.start) - 3.0f, secH);
        g.setColour (wl::c::panelHi);
        g.fillRoundedRectangle (box, 6.0f);
        g.setGradientFill (wl::grad (chordsTheme, box.getTopLeft(), box.getTopRight()));
        g.fillRoundedRectangle (box.removeFromBottom (2.5f), 1.2f);
        g.setColour (col::text);
        g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
        g.drawFittedText (s.name.toUpperCase() + "  " + juce::String ((int) (s.len / 4)),
                          juce::Rectangle<float> (xOf (s.start), area.getY(), box.getWidth(), secH - 2).toNearestInt().reduced (6, 0),
                          juce::Justification::centredLeft, 1);
    }

    // note roll
    auto roll = area.withTrimmedTop (secH + 6).withTrimmedBottom (chordH + 6);
    if (! rollImage.isValid()) renderRoll (roll);
    g.drawImageAt (rollImage, 0, 0);

    // chord lane: each section's progression written under it
    auto lane = area.removeFromBottom (chordH);
    g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    for (const auto& sec : song->sections)
    {
        juce::StringArray names;
        for (const auto& sl : song->slots)
            if (sl.start >= sec.start && sl.start < sec.start + sec.len && names.size() < 5 && ! names.contains (sl.chord.name))
                names.add (sl.chord.name);
        auto box = juce::Rectangle<float> (xOf (sec.start), lane.getY(), xOf (sec.start + sec.len) - xOf (sec.start) - 3.0f, lane.getHeight());
        g.setColour (wl::c::panel);
        g.fillRoundedRectangle (box, 4.0f);
        g.setColour (col::text.withAlpha (0.9f));
        g.drawFittedText (names.joinIntoString (" "), box.toNearestInt().reduced (5, 0), juce::Justification::centredLeft, 1, 0.8f);
    }

    const double ph = proc.getPlayhead();
    if (ph >= 0.0)
    {
        const float x = xOf (ph);
        g.setColour (chordsTheme.a.withAlpha (0.18f));
        g.fillRect (x - 6.0f, area.getY() - 4.0f, 12.0f, r.getHeight() - 16.0f);
        g.setColour (chordsTheme.a);
        g.fillRect (x - 1.0f, area.getY() - 4.0f, 2.0f, r.getHeight() - 16.0f);
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
    liveChord.setJustificationType (juce::Justification::centred);
    
    addAndMakeVisible (liveChord);

    setSize (1180, 644);
    startTimerHz (30);

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
    if (s != shownSong) { shownSong = s; timeline.setSong (s); }
    timeline.repaint();

    const bool live = proc.apvts.getRawParameterValue ("liveMode")->load() > 0.5f;
    liveChord.setText (live ? proc.getLiveChordName() : juce::String(), juce::dontSendNotification);
}

void WallChordsEditor::paint (juce::Graphics& g)
{
    wl::drawBackground (g, getLocalBounds(), 56.0f, chordsTheme, "WALL", "CHORDS");
    wl::drawPanel (g, juce::Rectangle<float> (16, 352, 690, 236), "PARTS", true, chordsTheme, 32.0f, 16.0f);
    wl::drawPanel (g, juce::Rectangle<float> (718, 352, (float) getWidth() - 734, 236), "FEEL", true, chordsTheme, 32.0f, 16.0f);
}

void WallChordsEditor::resized()
{
    // header: genre tabs in the middle, actions on the right
    generateBtn.setBounds (getWidth() - 146, 11, 130, 34);
    previewBtn.setBounds (getWidth() - 256, 11, 102, 34);
    for (int i = 0; i < 3; ++i) genreBtn[i].setBounds (250 + i * 112, 11, 106, 34);
    liveChord.setBounds (590, 11, getWidth() - 256 - 600, 34);

    // settings row
    int x = 16;
    const int widths[5] = { 104, 104, 140, 84, 140 };
    for (size_t i = 0; i < combos.size(); ++i)
    {
        comboLabels[i]->setBounds (x, 66, widths[i], 18);
        combos[i]->setBounds (x, 86, widths[i], 30);
        x += widths[i] + 10;
    }
    progLabel.setBounds (x, 66, 200, 18);
    progBox.setBounds (x, 86, getWidth() - 16 - x, 30);

    timeline.setBounds (16, 130, getWidth() - 32, 208);

    // parts
    for (int i = 0; i < 4; ++i)
    {
        const int y = 390 + i * 42;
        partOn[i].setBounds (28, y + 6, 24, 24);
        partLabel[i].setBounds (62, y, 120, 36);
        partStyle[i].setBounds (186, y + 3, 312, 30);
        drag[i]->setBounds (510, y + 3, 182, 30);
    }
    dragAll.setBounds (510, 390 + 4 * 42, 182, 30);

    // feel knobs
    const int kw = (getWidth() - 734 - 16) / 4;
    for (int i = 0; i < 7; ++i)
    {
        const int cx = 726 + (i % 4) * kw, cy = 392 + (i / 4) * 96;
        knobs[i].setBounds (cx, cy, kw, 74);
        knobLabels[i].setBounds (cx, cy + 74, kw, 16);
    }

    // footer
    followDaw.setBounds (16, 600, 170, 32);
    sound.setBounds (194, 600, 170, 32);
    liveMode.setBounds (372, 600, 300, 32);
    hint.setBounds (680, 600, getWidth() - 696, 32);
}
