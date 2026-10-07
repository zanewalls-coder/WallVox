#pragma once
#include "PluginProcessor.h"
#include "PresetManager.h"
#include "WallLook.h"

class VoxEditor;

// One module in the rack: drag to reorder, click to edit, power button, right-click menu
class Tile : public juce::Component
{
public:
    Tile (VoxEditor& e, int position, int type);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    int position, type;
    juce::ToggleButton power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt;
private:
    juce::Rectangle<int> closeArea() const { return { getWidth() - 22, 4, 18, 18 }; }
    VoxEditor& ed;
    bool dragging = false;
    juce::Point<int> dragOffset;
};

// Visual readout for the selected module (gain reduction, EQ curve or tuner)
class MeterView : public juce::Component
{
public:
    explicit MeterView (VoxProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    int slot = -1, type = 0;
    wl::Theme theme;
private:
    VoxProcessor& proc;
    float peakHold = 0, holdTimer = 0;
};

class VoxEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VoxEditor (VoxProcessor&);
    ~VoxEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    // used by tiles
    void select (int position);
    int selected = 0;
    void tileDragged (Tile&, int x);
    void tileDropped (Tile&, int x);
    void tileMenu (Tile&);
    void removeAt (int position);
    VoxProcessor& proc;
    wl::Look look;

private:
    void timerCallback() override;
    void rebuildRack();
    void rebuildPanel();
    void layoutRack();
    int dropIndexFor (int x) const;
    void showAddMenu();
    void rebuildPresetMenu();
    void stepPreset (int delta);
    void savePreset();
    void importPreset();
    void getMorePresets();
    juce::String chainSignature() const;

    wl::Scope scope;
    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, saveBtn { "SAVE" }, importBtn { "IMPORT" }, moreBtn { "GET MORE" };
    juce::Label status;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::TooltipWindow tooltips { this, 600 };

    // rack
    juce::Rectangle<int> rackArea, panelArea;
    std::vector<std::unique_ptr<Tile>> tiles;
    juce::TextButton addBtn { "+" };
    juce::String shownSignature;
    int dropMarker = -1;
    juce::Slider inKnob, outKnob;
    juce::Label inLabel, outLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inAtt, outAtt;

    // module editor
    struct Control
    {
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::ComboBox> combo;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
        std::unique_ptr<juce::Label> label;
        int k = 0;
    };
    std::vector<Control> controls;
    MeterView meter;
    int panelSlot = -2, panelType = -1;
    juce::String panelTitle, panelDesc;
    juce::Colour panelColour;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxEditor)
};
