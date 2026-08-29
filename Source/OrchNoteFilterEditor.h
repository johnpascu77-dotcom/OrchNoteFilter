#pragma once

#include <array>
#include <JuceHeader.h>
#include "OrchNoteFilterProcessor.h"

class OrchNoteFilterAudioProcessorEditor : public juce::AudioProcessorEditor,
                                           private juce::Timer
{
public:
    explicit OrchNoteFilterAudioProcessorEditor (OrchNoteFilterAudioProcessor&);
    ~OrchNoteFilterAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateStatus();
    void syncFieldPresetToCustom();

    OrchNoteFilterAudioProcessor& audioProcessor;

    juce::Label titleLabel, subtitleLabel, buildLabel;

    juce::ToggleButton enableButton;

    juce::Label fieldPresetLabel;
    juce::ComboBox fieldPresetBox;

    juce::Label pitchClassLabel;
    std::array<juce::TextButton, 12> pcButtons;

    juce::Label rootLabel;
    juce::Slider rootSlider;

    juce::Label foreignModeLabel;
    juce::ComboBox foreignModeBox;

    juce::Label constrainDirectionLabel;
    juce::ComboBox constrainDirectionBox;

    juce::Label shiftLabel;
    juce::Slider shiftSlider;

    juce::ToggleButton avoidSnapRepeatsButton;

    juce::Label fieldMorphLabel;
    juce::Slider fieldMorphSlider;
    juce::Label fieldWidthLabel;
    juce::Slider fieldWidthSlider;

    juce::Label probabilityLabel;
    juce::Slider probabilitySlider;

    juce::ToggleButton passKeyswitchesButton;
    juce::Label ksMinLabel, ksMaxLabel;
    juce::Slider ksMinSlider, ksMaxSlider;

    juce::ToggleButton ccControlButton;
    juce::Label ccChannelLabel;
    juce::Slider ccChannelSlider;

    juce::Label statusLabel;

    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<ButtonAttachment> enableAttachment;
    std::unique_ptr<ComboBoxAttachment> fieldPresetAttachment;
    std::array<std::unique_ptr<ButtonAttachment>, 12> pcAttachments;
    std::unique_ptr<SliderAttachment> rootAttachment;
    std::unique_ptr<ComboBoxAttachment> foreignModeAttachment;
    std::unique_ptr<ComboBoxAttachment> constrainDirectionAttachment;
    std::unique_ptr<SliderAttachment> shiftAttachment;
    std::unique_ptr<ButtonAttachment> avoidSnapRepeatsAttachment;
    std::unique_ptr<SliderAttachment> fieldMorphAttachment;
    std::unique_ptr<SliderAttachment> fieldWidthAttachment;
    std::unique_ptr<SliderAttachment> probabilityAttachment;
    std::unique_ptr<ButtonAttachment> passKeyswitchesAttachment;
    std::unique_ptr<SliderAttachment> ksMinAttachment;
    std::unique_ptr<SliderAttachment> ksMaxAttachment;
    std::unique_ptr<ButtonAttachment> ccControlAttachment;
    std::unique_ptr<SliderAttachment> ccChannelAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OrchNoteFilterAudioProcessorEditor)
};
