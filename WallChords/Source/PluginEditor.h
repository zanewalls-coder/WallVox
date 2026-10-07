#pragma once
#include "PluginProcessor.h"
#include "WallLook.h"

inline const wl::Theme chordsTheme { juce::Colour (0xffffb238), juce::Colour (0xffff5f3d) };

// Drag onto a DAW track as a MIDI clip; click to save the file instead
class DragButton : public juce::TextButton
{
public:
    explicit DragButton (const juce::String& text) : juce::TextButton (text) {}
    std::function<juce::File()> makeFile;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    bool dragging = false;
    std::unique_ptr<juce::FileChooser> chooser;
};

// Piano-roll view of the generated loop
class LoopView : public juce::Component
{
public:
    explicit LoopView (WallChordsProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    std::shared_ptr<const gen::Loop> loop;
private:
    WallChordsProcessor& proc;
};

class WallChordsEditor : public juce::AudioProcessorEditor,
                         public juce::FileDragAndDropTarget,
                         private juce::ListBoxModel,
                         private juce::Timer
{
public:
    explicit WallChordsEditor (WallChordsProcessor&);
    ~WallChordsEditor() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; repaint(); }
    void filesDropped (const juce::StringArray&, int, int) override;

private:
    // library list
    int getNumRows() override { return (int) libItems.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;

    void timerCallback() override;
    void refreshLibrary();
    void attachInstrumentCombo();
    juce::ComboBox& addCombo (const char* paramId, const juce::String& label);

    using CBA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SA  = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA  = juce::AudioProcessorValueTreeState::ButtonAttachment;

    WallChordsProcessor& proc;
    wl::Look look { chordsTheme };

    juce::TextButton modeBtn[3];
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, newBtn { "NEW IDEA" }, previewBtn { "PREVIEW" };
    juce::Label historyLabel;

    juce::ListBox libList { "library", this };
    juce::TextButton addBtn { "ADD FILES" }, folderBtn { "SHOW FOLDER" };
    juce::Label libStatus;
    std::vector<lib::Entry> libItems;
    int lastLibChange = -1, lastPending = -1;
    bool dragOver = false;
    std::unique_ptr<juce::FileChooser> chooser;

    LoopView view;
    std::vector<std::unique_ptr<juce::ComboBox>> combos;
    std::vector<std::unique_ptr<juce::Label>> comboLabels;
    std::vector<std::unique_ptr<CBA>> comboAtts;
    juce::ComboBox instBox;
    juce::Label instLabel;
    std::unique_ptr<CBA> instAtt;
    juce::String instParam;
    juce::ComboBox* rhythmBox = nullptr;

    juce::Slider knobs[5];
    juce::Label knobLabels[5];
    std::unique_ptr<SA> knobAtts[5];

    DragButton dragMain { "DRAG MIDI" }, dragChords { "DRAG CHORDS" };
    juce::ToggleButton backing, followDaw, sound, liveMode;
    std::unique_ptr<BA> backingAtt, followAtt, soundAtt, liveAtt;
    juce::TextButton soundsBtn { "GET REAL INSTRUMENTS" };
    juce::Label status, liveChord;
    std::shared_ptr<const gen::Loop> shownLoop;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WallChordsEditor)
};
