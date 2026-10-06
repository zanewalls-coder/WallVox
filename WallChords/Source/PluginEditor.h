#pragma once
#include "PluginProcessor.h"
#include "WallLook.h"

inline const wl::Theme chordsTheme { juce::Colour (0xffffb238), juce::Colour (0xffff5f3d) };

// A button you can drag straight into Logic / Ableton as a MIDI clip (click = save to a folder)
class DragButton : public juce::TextButton
{
public:
    DragButton (const juce::String& text) : juce::TextButton (text) {}
    std::function<juce::File()> makeFile;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    bool dragging = false;
    std::unique_ptr<juce::FileChooser> chooser;
};

class Timeline : public juce::Component
{
public:
    explicit Timeline (WallChordsProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void resized() override { rollImage = {}; }
    void setSong (std::shared_ptr<const eng::Song> s) { song = std::move (s); rollImage = {}; }
    std::shared_ptr<const eng::Song> song;
private:
    void renderRoll (juce::Rectangle<float> area);
    WallChordsProcessor& proc;
    juce::Image rollImage;
};

class WallChordsEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit WallChordsEditor (WallChordsProcessor&);
    ~WallChordsEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void rebuildProgressionItems();
    juce::ComboBox& addCombo (const char* paramId, const juce::String& label);

    using CBA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SA  = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA  = juce::AudioProcessorValueTreeState::ButtonAttachment;

    WallChordsProcessor& proc;
    wl::Look look { chordsTheme };

    juce::TextButton genreBtn[3];
    juce::TextButton generateBtn { "NEW IDEA" }, previewBtn { "PREVIEW" };
    std::vector<std::unique_ptr<juce::ComboBox>> combos;
    std::vector<std::unique_ptr<juce::Label>> comboLabels;
    std::vector<std::unique_ptr<CBA>> comboAtts;
    juce::ComboBox progBox;
    juce::Label progLabel;
    std::unique_ptr<CBA> progAtt;
    juce::String progSignature;

    Timeline timeline;
    juce::ToggleButton partOn[4];
    juce::Label partLabel[4];
    juce::ComboBox partStyle[4];
    std::unique_ptr<DragButton> drag[4];
    DragButton dragAll { "DRAG ALL PARTS" };
    std::unique_ptr<BA> partOnAtt[4];
    std::unique_ptr<CBA> partStyleAtt[4];

    juce::Slider knobs[7];
    juce::Label knobLabels[7];
    std::unique_ptr<SA> knobAtts[7];

    juce::ToggleButton followDaw, sound, liveMode;
    std::unique_ptr<BA> followAtt, soundAtt, liveAtt;
    juce::Label hint, liveChord;
    std::shared_ptr<const eng::Song> shownSong;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WallChordsEditor)
};
