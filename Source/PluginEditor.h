#pragma once
#include "PluginProcessor.h"

class WallVoxLook : public juce::LookAndFeel_V4
{
public:
    WallVoxLook();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

class WallVoxEditor : public juce::AudioProcessorEditor
{
public:
    explicit WallVoxEditor (WallVoxProcessor&);
    ~WallVoxEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Control
    {
        juce::String id;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::ComboBox> combo;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> cAtt;
        juce::Label label;
    };
    struct Section
    {
        juce::String title, toggleId;
        juce::StringArray ids;
        juce::Rectangle<int> area;
        std::unique_ptr<juce::ToggleButton> toggle;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> tAtt;
    };
    void updateSectionDimming();

    void rebuildPresetMenu();
    void savePreset();
    void importPreset();
    void getMorePresets();

    WallVoxProcessor& proc;
    WallVoxLook look;
    std::vector<Section> sections;
    std::vector<std::unique_ptr<Control>> controls;

    juce::ComboBox presetBox;
    juce::TextButton saveBtn { "Save" }, importBtn { "Import" }, moreBtn { "Get More Presets" };
    juce::Label status;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WallVoxEditor)
};
