#include "PluginEditor.h"

namespace col
{
    const juce::Colour text = wl::c::text, dim = wl::c::dim, panel = wl::c::panel;
    const juce::Colour kind[3] { juce::Colour (0xffffb238), juce::Colour (0xffff6fa8), juce::Colour (0xff4cc9f0) };
}

// ================================================================ drag button
void DragButton::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 6 || ! makeFile) return;
    const auto f = makeFile();
    if (! f.existsAsFile()) return;
    dragging = true;
    juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this,
        [safe = juce::Component::SafePointer<DragButton> (this)] { if (safe != nullptr) safe->dragging = false; });
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

// ================================================================ loop view
void LoopView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0d0f1c), r.getX(), r.getY(), juce::Colour (0xff171a2e), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 14.0f);
    g.setColour (wl::c::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 14.0f, 1.0f);
    if (loop == nullptr) return;

    const auto& s = loop->song;
    auto area = r.reduced (16.0f, 12.0f);
    auto infoRow = area.removeFromTop (20);
    auto lane = area.removeFromBottom (24);
    area.removeFromBottom (6);
    auto xOf = [&] (double b) { return area.getX() + (float) (b / s.length) * area.getWidth(); };

    g.setColour (col::dim);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText (loop->info, infoRow, juce::Justification::centredLeft);

    // bar grid
    for (int bar = 0; bar <= (int) (s.length / 4); ++bar)
    {
        g.setColour (wl::c::line.withAlpha (bar == 0 ? 0.0f : 0.8f));
        g.drawVerticalLine ((int) xOf (bar * 4.0), area.getY(), area.getBottom());
        g.setColour (col::dim.withAlpha (0.6f));
        g.setFont (juce::FontOptions (10.5f));
        if (bar < (int) (s.length / 4)) g.drawText (juce::String (bar + 1), juce::Rectangle<float> (xOf (bar * 4.0) + 4, area.getY(), 30, 14), juce::Justification::topLeft);
    }

    int lo = 127, hi = 0;
    for (auto& n : s.notes) { lo = std::min (lo, n.pitch); hi = std::max (hi, n.pitch); }
    if (lo > hi) return;
    lo -= 2; hi += 2;
    const float rowH = juce::jlimit (2.0f, 9.0f, (area.getHeight() - 14) / (float) (hi - lo + 1));
    auto yOf = [&] (int pitch) { return area.getBottom() - (pitch - lo + 1) * rowH; };

    for (int pass = 0; pass < 2; ++pass)
        for (auto& n : s.notes)
        {
            const bool mainPart = n.part == eng::PChords;
            if ((pass == 0) == mainPart) continue;   // backing first, main on top
            auto rect = juce::Rectangle<float> (xOf (n.beat), yOf (n.pitch), juce::jmax (2.0f, xOf (n.beat + n.len) - xOf (n.beat) - 1.0f), rowH - 1.0f);
            if (mainPart)
            {
                g.setColour (chordsTheme.a.withAlpha (0.18f));
                g.fillRoundedRectangle (rect.expanded (2.0f, 2.0f), 3.0f);
                g.setGradientFill (wl::grad (chordsTheme, rect.getTopLeft(), rect.getBottomRight()));
                g.setOpacity (0.45f + 0.55f * n.vel / 127.0f);
                g.fillRoundedRectangle (rect, 2.0f);
                g.setOpacity (1.0f);
            }
            else
            {
                g.setColour (col::dim.withAlpha (0.22f));
                g.fillRoundedRectangle (rect, 2.0f);
            }
        }

    // chord names
    g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
    for (auto& c : loop->chords)
    {
        auto box = juce::Rectangle<float> (xOf (c.start) + 1, lane.getY(), xOf (c.start + c.len) - xOf (c.start) - 3, lane.getHeight());
        g.setColour (wl::c::panelHi);
        g.fillRoundedRectangle (box, 5.0f);
        g.setColour (col::text);
        g.drawFittedText (c.chord.name, box.toNearestInt().reduced (6, 0), juce::Justification::centredLeft, 1, 0.7f);
    }

    const double ph = proc.getPlayhead();
    if (ph >= 0.0)
    {
        const float x = xOf (ph);
        g.setColour (chordsTheme.a.withAlpha (0.18f));
        g.fillRect (x - 5.0f, area.getY(), 10.0f, area.getHeight() + 30.0f);
        g.setColour (chordsTheme.a);
        g.fillRect (x - 1.0f, area.getY(), 2.0f, area.getHeight() + 30.0f);
    }
}

// ================================================================ editor
juce::ComboBox& WallChordsEditor::addCombo (const char* paramId, const juce::String& label)
{
    auto box = std::make_unique<juce::ComboBox>();
    box->addItemList (proc.apvts.getParameter (paramId)->getAllValueStrings(), 1);
    addAndMakeVisible (*box);
    comboAtts.push_back (std::make_unique<CBA> (proc.apvts, paramId, *box));
    auto lab = std::make_unique<juce::Label> ("", label);
    lab->setFont (juce::FontOptions (11.5f, juce::Font::bold));
    lab->setColour (juce::Label::textColourId, col::dim);
    addAndMakeVisible (*lab);
    comboLabels.push_back (std::move (lab));
    combos.push_back (std::move (box));
    return *combos.back();
}

WallChordsEditor::WallChordsEditor (WallChordsProcessor& p) : AudioProcessorEditor (p), proc (p), view (p)
{
    setLookAndFeel (&look);

    for (int i = 0; i < 3; ++i)
    {
        modeBtn[i].setButtonText (gen::modeNames[i].toUpperCase());
        modeBtn[i].onClick = [this, i]
        { auto* prm = proc.apvts.getParameter ("mode"); prm->setValueNotifyingHost (prm->convertTo0to1 ((float) i)); };
        addAndMakeVisible (modeBtn[i]);
    }
    prevBtn.onClick = [this] { proc.stepHistory (-1); };
    nextBtn.onClick = [this] { proc.stepHistory (1); };
    newBtn.onClick = [this] { proc.newIdea(); };
    previewBtn.onClick = [this] { if (proc.isPreviewing()) proc.stopPreview(); else proc.startPreview(); };
    prevBtn.setTooltip ("Previous idea");
    nextBtn.setTooltip ("Next idea");
    for (auto* b : { &prevBtn, &nextBtn, &newBtn, &previewBtn }) addAndMakeVisible (*b);
    historyLabel.setJustificationType (juce::Justification::centred);
    historyLabel.setColour (juce::Label::textColourId, col::dim);
    historyLabel.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (historyLabel);

    // library panel
    libList.setRowHeight (48);
    libList.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    libList.setOutlineThickness (0);
    addAndMakeVisible (libList);
    addBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Add MIDI or audio loops", juce::File(), "*.mid;*.midi;*.wav;*.aif;*.aiff;*.mp3;*.m4a;*.flac;*.ogg");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::canSelectMultipleItems,
                              [this] (const juce::FileChooser& fc)
                              {
                                  juce::StringArray paths;
                                  for (auto& f : fc.getResults()) paths.add (f.getFullPathName());
                                  if (! paths.isEmpty()) proc.addToLibrary (paths);
                              });
    };
    folderBtn.onClick = [] { lib::Library::folder().getChildFile ("files").revealToUser(); };
    for (auto* b : { &addBtn, &folderBtn }) addAndMakeVisible (*b);
    libStatus.setColour (juce::Label::textColourId, col::dim);
    libStatus.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (libStatus);

    addAndMakeVisible (view);

    // generator controls
    addCombo ("source", "LEARN FROM");
    addCombo ("bars", "LENGTH");
    addCombo ("key", "KEY");
    addCombo ("scale", "SCALE");
    rhythmBox = &addCombo ("rhythm", "CHORD RHYTHM");
    instLabel.setText ("SOUND", juce::dontSendNotification);
    instLabel.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    instLabel.setColour (juce::Label::textColourId, col::dim);
    addAndMakeVisible (instLabel);
    instBox.addItemList (smp::instrumentNames(), 1);
    addAndMakeVisible (instBox);
    addCombo ("octave", "OCTAVE");
    attachInstrumentCombo();

    const char* knobIds[5] = { "similarity", "complexity", "humanize", "strum", "volume" };
    const char* knobNames[5] = { "SIMILARITY", "COMPLEXITY", "HUMANIZE", "STRUM", "VOLUME" };
    for (int i = 0; i < 5; ++i)
    {
        knobs[i].setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        knobs[i].setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
        knobs[i].setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (knobs[i]);
        knobAtts[i] = std::make_unique<SA> (proc.apvts, knobIds[i], knobs[i]);
        knobLabels[i].setText (knobNames[i], juce::dontSendNotification);
        knobLabels[i].setJustificationType (juce::Justification::centred);
        knobLabels[i].setFont (juce::FontOptions (11.5f, juce::Font::bold));
        knobLabels[i].setColour (juce::Label::textColourId, col::dim);
        addAndMakeVisible (knobLabels[i]);
    }
    knobs[0].setTooltip ("High = stays close to your samples. Low = new ideas in the same style.");

    dragMain.makeFile = [this] { return proc.writeMidiFile (1 << eng::PChords); };
    dragChords.makeFile = [this] { return proc.writeMidiFile (1 << eng::PPad); };
    dragMain.setTooltip ("Drag onto a MIDI track in Logic or Ableton. Click to save the file.");
    dragChords.setTooltip ("The backing chords as their own MIDI clip");
    for (auto* b : { &dragMain, &dragChords }) addAndMakeVisible (*b);

    backing.setButtonText ("Backing Chords");
    followDaw.setButtonText ("Play with DAW");
    sound.setButtonText ("Built-in Sound");
    liveMode.setButtonText ("Live Chords");
    for (auto* b : { &backing, &followDaw, &sound, &liveMode }) addAndMakeVisible (*b);
    backingAtt = std::make_unique<BA> (proc.apvts, "backing", backing);
    followAtt  = std::make_unique<BA> (proc.apvts, "followDaw", followDaw);
    soundAtt   = std::make_unique<BA> (proc.apvts, "sound", sound);
    liveAtt    = std::make_unique<BA> (proc.apvts, "liveMode", liveMode);

    soundsBtn.setToggleState (true, juce::dontSendNotification);
    soundsBtn.setTooltip ("Downloads recorded piano, strings, harp, vibes, organ and upright bass (free, public domain)");
    soundsBtn.onClick = [this]
    {
        juce::Component::SafePointer<WallChordsEditor> safe (this);
        proc.downloadSoundPack ([safe] (juce::String msg) { if (safe != nullptr) safe->status.setText (msg, juce::dontSendNotification); });
    };
    addAndMakeVisible (soundsBtn);
    status.setColour (juce::Label::textColourId, col::dim);
    status.setFont (juce::FontOptions (12.5f));
    addAndMakeVisible (status);
    liveChord.setColour (juce::Label::textColourId, chordsTheme.a);
    liveChord.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    liveChord.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (liveChord);

    refreshLibrary();
    setSize (1240, 760);
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

void WallChordsEditor::attachInstrumentCombo()
{
    const auto id = proc.instrumentParamId();
    if (id == instParam) return;
    instParam = id;
    instAtt.reset();
    instAtt = std::make_unique<CBA> (proc.apvts, id, instBox);
}

bool WallChordsEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files) if (lib::Library::isSupported (f)) return true;
    return false;
}

void WallChordsEditor::filesDropped (const juce::StringArray& files, int, int)
{
    dragOver = false;
    proc.addToLibrary (files);
    status.setText ("Learning from " + juce::String (files.size()) + " file" + (files.size() == 1 ? "" : "s") + "...", juce::dontSendNotification);
    repaint();
}

void WallChordsEditor::refreshLibrary()
{
    libItems = proc.library.snapshot();
    std::stable_sort (libItems.begin(), libItems.end(), [] (const lib::Entry& a, const lib::Entry& b)
                      { return a.kind != b.kind ? a.kind < b.kind : a.name.compareIgnoreCase (b.name) < 0; });
    libList.updateContent();
    libList.repaint();
    int counts[3] {};
    for (auto& e : libItems) counts[e.kind]++;
    juce::String s;
    s << counts[0] << " chords  -  " << counts[1] << " melodies  -  " << counts[2] << " bass";
    const int pending = proc.library.pendingJobs();
    if (pending > 0) s = "Analysing " + juce::String (pending) + " file" + (pending == 1 ? "" : "s") + "...";
    libStatus.setText (s, juce::dontSendNotification);
}

void WallChordsEditor::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool)
{
    if (! juce::isPositiveAndBelow (row, (int) libItems.size())) return;
    const auto& e = libItems[(size_t) row];
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (2.0f, 3.0f);
    g.setColour (wl::c::bg0.withAlpha (0.55f));
    g.fillRoundedRectangle (r, 8.0f);
    const float alpha = e.enabled ? 1.0f : 0.4f;

    auto badge = r.removeFromLeft (64).reduced (6, 10);
    g.setColour (col::kind[e.kind].withAlpha (0.18f * alpha));
    g.fillRoundedRectangle (badge, 5.0f);
    g.setColour (col::kind[e.kind].withAlpha (alpha));
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText (juce::String (lib::kindName (e.kind)).toUpperCase(), badge, juce::Justification::centred);

    auto del = r.removeFromRight (28);
    g.setColour (col::dim);
    g.setFont (juce::FontOptions (16.0f));
    g.drawText (juce::String::fromUTF8 ("\xc3\x97"), del, juce::Justification::centred);   // ×

    auto tog = r.removeFromRight (28).withSizeKeepingCentre (16, 16);
    g.setColour (e.enabled ? chordsTheme.a : wl::c::line.brighter (0.3f));
    g.drawEllipse (tog, 1.5f);
    if (e.enabled) g.fillEllipse (tog.reduced (4));

    g.setColour (col::text.withAlpha (alpha));
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (e.name, r.removeFromTop (r.getHeight() * 0.52f).withTrimmedLeft (4), juce::Justification::bottomLeft, true);
    g.setColour (col::dim.withAlpha (alpha));
    g.setFont (juce::FontOptions (11.5f));
    g.drawText (e.describe() + (e.source == "audio" ? "  -  audio" : "  -  MIDI"), r.withTrimmedLeft (4), juce::Justification::topLeft, true);
}

void WallChordsEditor::listBoxItemClicked (int row, const juce::MouseEvent& ev)
{
    if (! juce::isPositiveAndBelow (row, (int) libItems.size())) return;
    const auto e = libItems[(size_t) row];
    const int w = libList.getVisibleRowWidth();
    if (ev.x > w - 32)
    {
        proc.library.remove (e.id);
        proc.regenerate();
    }
    else if (ev.x > w - 62)
    {
        proc.library.setEnabled (e.id, ! e.enabled);
        proc.regenerate();
    }
}

void WallChordsEditor::timerCallback()
{
    const int mode = (int) proc.apvts.getRawParameterValue ("mode")->load();
    for (int i = 0; i < 3; ++i) modeBtn[i].setToggleState (i == mode, juce::dontSendNotification);
    attachInstrumentCombo();
    if (rhythmBox != nullptr) rhythmBox->setEnabled (mode == gen::MChords);
    dragChords.setEnabled (mode != gen::MChords && proc.apvts.getRawParameterValue ("backing")->load() > 0.5f);

    previewBtn.setButtonText (proc.isPreviewing() ? "STOP" : "PREVIEW");
    previewBtn.setToggleState (proc.isPreviewing(), juce::dontSendNotification);
    historyLabel.setText (proc.historyLabel(), juce::dontSendNotification);

    if (proc.library.changeCount() != lastLibChange || proc.library.pendingJobs() != lastPending)
    {
        lastLibChange = proc.library.changeCount();
        lastPending = proc.library.pendingJobs();
        refreshLibrary();
        if (lastPending == 0 && status.getText().startsWith ("Learning")) status.setText ("Library updated.", juce::dontSendNotification);
    }

    auto l = proc.getLoop();
    if (l != shownLoop) { shownLoop = l; view.loop = l; }
    view.repaint();

    const float dl = proc.downloadProgress->load();
    const bool installed = smp::soundPackInstalled();
    soundsBtn.setVisible (! installed || dl >= 0.0f);
    soundsBtn.setEnabled (dl < 0.0f);
    soundsBtn.setButtonText (dl >= 0.0f ? "DOWNLOADING " + juce::String (juce::roundToInt (dl * 100)) + "%" : juce::String ("GET REAL INSTRUMENTS"));

    const bool live = proc.apvts.getRawParameterValue ("liveMode")->load() > 0.5f;
    liveChord.setText (live ? proc.getLiveChordName() : juce::String(), juce::dontSendNotification);
}

void WallChordsEditor::paint (juce::Graphics& g)
{
    wl::drawBackground (g, getLocalBounds(), 56.0f, chordsTheme, "WALL", "CHORDS");
    wl::drawPanel (g, juce::Rectangle<float> (16, 68, 340, (float) getHeight() - 68 - 60), "YOUR LIBRARY", true, chordsTheme, 32.0f, 16.0f);
    wl::drawPanel (g, juce::Rectangle<float> (368, 326, (float) getWidth() - 384, 314), "GENERATE", true, chordsTheme, 32.0f, 16.0f);

    // drop hint
    auto hint = juce::Rectangle<float> (30, 110, 312, 64);
    g.setColour (dragOver ? chordsTheme.a.withAlpha (0.15f) : wl::c::bg0.withAlpha (0.4f));
    g.fillRoundedRectangle (hint, 10.0f);
    const float dash[] { 6.0f, 4.0f };
    juce::Path border;
    border.addRoundedRectangle (hint, 10.0f);
    juce::Path dashed;
    juce::PathStrokeType (1.2f).createDashedStroke (dashed, border, dash, 2);
    g.setColour (dragOver ? chordsTheme.a : wl::c::line.brighter (0.4f));
    g.fillPath (dashed);
    g.setColour (dragOver ? col::text : col::dim);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText ("Drop MIDI or audio loops here", hint.withTrimmedBottom (20), juce::Justification::centredBottom);
    g.setFont (juce::FontOptions (11.5f));
    g.drawText ("They're sorted into chords, melodies and bass", hint.withTrimmedTop (34), juce::Justification::centredTop);
}

void WallChordsEditor::paintOverChildren (juce::Graphics& g)
{
    if (! dragOver) return;
    g.setColour (chordsTheme.a.withAlpha (0.08f));
    g.fillAll();
    g.setColour (chordsTheme.a);
    g.drawRect (getLocalBounds(), 3);
}

void WallChordsEditor::resized()
{
    // header
    for (int i = 0; i < 3; ++i) modeBtn[i].setBounds (260 + i * 112, 11, 106, 34);
    newBtn.setBounds (getWidth() - 136, 11, 120, 34);
    previewBtn.setBounds (getWidth() - 244, 11, 100, 34);
    nextBtn.setBounds (getWidth() - 290, 11, 38, 34);
    historyLabel.setBounds (getWidth() - 392, 11, 100, 34);
    prevBtn.setBounds (getWidth() - 430, 11, 38, 34);
    liveChord.setBounds (600, 11, getWidth() - 430 - 610, 34);

    // library
    addBtn.setBounds (30, 184, 150, 30);
    folderBtn.setBounds (190, 184, 152, 30);
    libList.setBounds (24, 222, 324, getHeight() - 222 - 92);
    libStatus.setBounds (30, getHeight() - 92, 312, 24);

    // display
    view.setBounds (368, 68, getWidth() - 384, 246);

    // generate panel
    const int px = 384, py = 366;
    const int widths[6] = { 134, 96, 74, 92, 140, 66 };
    int x = px;
    for (size_t i = 0; i < combos.size(); ++i)
    {
        if (i == 5)   // sound selector sits before octave
        {
            instLabel.setBounds (x, py, 168, 16);
            instBox.setBounds (x, py + 18, 168, 30);
            x += 176;
        }
        comboLabels[i]->setBounds (x, py, widths[i], 16);
        combos[i]->setBounds (x, py + 18, widths[i], 30);
        x += widths[i] + 8;
    }
    for (int i = 0; i < 5; ++i)
    {
        knobs[i].setBounds (px + i * 104, py + 70, 100, 92);
        knobLabels[i].setBounds (px + i * 104, py + 162, 100, 16);
    }
    dragMain.setBounds (getWidth() - 224, py + 82, 192, 40);
    dragChords.setBounds (getWidth() - 224, py + 130, 192, 34);
    status.setBounds (px, py + 196, getWidth() - px - 40, 24);

    // footer
    const int fy = getHeight() - 48;
    backing.setBounds (16, fy, 170, 34);
    followDaw.setBounds (194, fy, 160, 34);
    sound.setBounds (362, fy, 160, 34);
    liveMode.setBounds (530, fy, 150, 34);
    soundsBtn.setBounds (getWidth() - 236, fy, 220, 34);
}
