#pragma once
#include "PluginProcessor.h"
#include "WallLook.h"

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
    void stepPreset (int delta);
    void savePreset();
    void importPreset();
    void getMorePresets();
    bool sectionOn (const Section& s) const { return s.toggle == nullptr || s.toggle->getToggleState(); }

    WallVoxProcessor& proc;
    wl::Look look;
    std::vector<Section> sections;
    std::vector<std::vector<int>> rows;
    std::vector<std::unique_ptr<Control>> controls;

    wl::Scope scope;
    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" };
    juce::TextButton saveBtn { "SAVE" }, importBtn { "IMPORT" }, moreBtn { "GET MORE" };
    juce::Label status;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WallVoxEditor)
};
