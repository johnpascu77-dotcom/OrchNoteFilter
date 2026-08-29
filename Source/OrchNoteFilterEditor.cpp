#include "OrchNoteFilterEditor.h"

namespace
{
    const juce::Colour kBackground = juce::Colour::fromRGB (18, 24, 31);
    const juce::Colour kAccent     = juce::Colour::fromRGB (95, 200, 245);
    const juce::Colour kFieldOff   = juce::Colour::fromRGB (40, 50, 60);

    void styleLabel (juce::Label& label, float size, bool bold = false)
    {
        label.setColour (juce::Label::textColourId, juce::Colours::white);
        label.setFont (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
    }

    void styleBox (juce::ComboBox& box)
    {
        box.setColour (juce::ComboBox::backgroundColourId, juce::Colour::fromRGB (28, 36, 46));
        box.setColour (juce::ComboBox::textColourId, juce::Colours::white);
        box.setColour (juce::ComboBox::outlineColourId, kAccent);
    }

    void styleSlider (juce::Slider& slider)
    {
        slider.setColour (juce::Slider::backgroundColourId, juce::Colour::fromRGB (28, 36, 46));
        slider.setColour (juce::Slider::trackColourId, kAccent);
        slider.setColour (juce::Slider::thumbColourId, juce::Colours::white);
        slider.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
        slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour::fromRGB (28, 36, 46));
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour::fromRGB (70, 85, 95));
    }

    const char* kPcNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
}

OrchNoteFilterAudioProcessorEditor::OrchNoteFilterAudioProcessorEditor (OrchNoteFilterAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setResizable (true, true);
    setResizeLimits (600, 560, 1100, 940);
    setSize (720, 664);

    auto& params = audioProcessor.getParameters();

    titleLabel.setText ("OrchNoteFilter", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centred);
    styleLabel (titleLabel, 26.0f, true);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("Pitch-Class Field Filter", juce::dontSendNotification);
    subtitleLabel.setJustificationType (juce::Justification::centred);
    styleLabel (subtitleLabel, 13.0f);
    addAndMakeVisible (subtitleLabel);

    buildLabel.setText ("Build: Phase 1A", juce::dontSendNotification);
    buildLabel.setJustificationType (juce::Justification::centred);
    buildLabel.setColour (juce::Label::textColourId, juce::Colour::fromRGB (140, 160, 180));
    buildLabel.setFont (juce::FontOptions (11.0f));
    addAndMakeVisible (buildLabel);

    enableButton.setButtonText ("Enable");
    enableButton.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    addAndMakeVisible (enableButton);
    enableAttachment = std::make_unique<ButtonAttachment> (params, "enable", enableButton);

    fieldPresetLabel.setText ("Field Preset", juce::dontSendNotification);
    styleLabel (fieldPresetLabel, 13.0f, true);
    addAndMakeVisible (fieldPresetLabel);

    {
        int id = 1;
        for (const auto& name : OrchNoteFilterAudioProcessor::getFieldPresetNames())
            fieldPresetBox.addItem (name, id++);
    }
    styleBox (fieldPresetBox);
    addAndMakeVisible (fieldPresetBox);
    fieldPresetAttachment = std::make_unique<ComboBoxAttachment> (params, "fieldPreset", fieldPresetBox);
    fieldPresetBox.onChange = [this]
    {
        const int index = fieldPresetBox.getSelectedItemIndex(); // 0 == Custom
        if (index >= 1)
            audioProcessor.applyFieldPresetFromUI (index);
    };

    pitchClassLabel.setText ("Pitch Classes (Root = C)", juce::dontSendNotification);
    styleLabel (pitchClassLabel, 13.0f, true);
    addAndMakeVisible (pitchClassLabel);

    for (int i = 0; i < 12; ++i)
    {
        auto& button = pcButtons[static_cast<size_t> (i)];
        button.setButtonText (kPcNames[i]);
        button.setClickingTogglesState (true);
        button.setColour (juce::TextButton::buttonColourId, kFieldOff);
        button.setColour (juce::TextButton::buttonOnColourId, kAccent);
        button.setColour (juce::TextButton::textColourOffId, juce::Colour::fromRGB (150, 160, 170));
        button.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        addAndMakeVisible (button);
        pcAttachments[static_cast<size_t> (i)] =
            std::make_unique<ButtonAttachment> (params, "pc" + juce::String (i), button);
        button.onClick = [this] { syncFieldPresetToCustom(); };
    }

    rootLabel.setText ("Field Root", juce::dontSendNotification);
    styleLabel (rootLabel, 13.0f, true);
    addAndMakeVisible (rootLabel);
    rootSlider.setSliderStyle (juce::Slider::IncDecButtons);
    rootSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 60, 22);
    rootSlider.textFromValueFunction = [] (double v)
    {
        return juce::String (kPcNames[juce::jlimit (0, 11, (int) v)]);
    };
    styleSlider (rootSlider);
    addAndMakeVisible (rootSlider);
    rootAttachment = std::make_unique<SliderAttachment> (params, "fieldRoot", rootSlider);

    foreignModeLabel.setText ("Foreign Notes", juce::dontSendNotification);
    styleLabel (foreignModeLabel, 13.0f, true);
    addAndMakeVisible (foreignModeLabel);
    foreignModeBox.addItemList ({ "Filter", "Keep", "Constrain", "Solo" }, 1);
    styleBox (foreignModeBox);
    addAndMakeVisible (foreignModeBox);
    foreignModeAttachment = std::make_unique<ComboBoxAttachment> (params, "foreignMode", foreignModeBox);

    constrainDirectionLabel.setText ("Constrain Dir", juce::dontSendNotification);
    styleLabel (constrainDirectionLabel, 13.0f, true);
    addAndMakeVisible (constrainDirectionLabel);
    constrainDirectionBox.addItemList ({ "Nearest", "Up", "Down" }, 1);
    styleBox (constrainDirectionBox);
    addAndMakeVisible (constrainDirectionBox);
    constrainDirectionAttachment = std::make_unique<ComboBoxAttachment> (params, "constrainDirection", constrainDirectionBox);

    shiftLabel.setText ("Scale Degree Shift", juce::dontSendNotification);
    styleLabel (shiftLabel, 13.0f, true);
    addAndMakeVisible (shiftLabel);
    shiftSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    shiftSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 22);
    styleSlider (shiftSlider);
    addAndMakeVisible (shiftSlider);
    shiftAttachment = std::make_unique<SliderAttachment> (params, "scaleDegreeShift", shiftSlider);

    avoidSnapRepeatsButton.setButtonText ("Avoid Snap Repeats (Constrain)");
    avoidSnapRepeatsButton.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    addAndMakeVisible (avoidSnapRepeatsButton);
    avoidSnapRepeatsAttachment = std::make_unique<ButtonAttachment> (params, "avoidSnapRepeats", avoidSnapRepeatsButton);

    probabilityLabel.setText ("Probability", juce::dontSendNotification);
    styleLabel (probabilityLabel, 13.0f, true);
    addAndMakeVisible (probabilityLabel);
    probabilitySlider.setSliderStyle (juce::Slider::LinearHorizontal);
    probabilitySlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 55, 22);
    probabilitySlider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v)) + "%"; };
    styleSlider (probabilitySlider);
    addAndMakeVisible (probabilitySlider);
    probabilityAttachment = std::make_unique<SliderAttachment> (params, "probability", probabilitySlider);

    passKeyswitchesButton.setButtonText ("Pass Keyswitches");
    passKeyswitchesButton.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    addAndMakeVisible (passKeyswitchesButton);
    passKeyswitchesAttachment = std::make_unique<ButtonAttachment> (params, "passKeyswitches", passKeyswitchesButton);

    ksMinLabel.setText ("KS Min", juce::dontSendNotification);
    styleLabel (ksMinLabel, 12.0f);
    addAndMakeVisible (ksMinLabel);
    ksMinSlider.setSliderStyle (juce::Slider::IncDecButtons);
    ksMinSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 50, 22);
    styleSlider (ksMinSlider);
    addAndMakeVisible (ksMinSlider);
    ksMinAttachment = std::make_unique<SliderAttachment> (params, "keyswitchMin", ksMinSlider);

    ksMaxLabel.setText ("KS Max", juce::dontSendNotification);
    styleLabel (ksMaxLabel, 12.0f);
    addAndMakeVisible (ksMaxLabel);
    ksMaxSlider.setSliderStyle (juce::Slider::IncDecButtons);
    ksMaxSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 50, 22);
    styleSlider (ksMaxSlider);
    addAndMakeVisible (ksMaxSlider);
    ksMaxAttachment = std::make_unique<SliderAttachment> (params, "keyswitchMax", ksMaxSlider);

    ccControlButton.setButtonText ("CC Control (105 preset / 106 root / 107 shift / 108 mode / 109 prob)");
    ccControlButton.setColour (juce::ToggleButton::textColourId, juce::Colour::fromRGB (205, 220, 230));
    addAndMakeVisible (ccControlButton);
    ccControlAttachment = std::make_unique<ButtonAttachment> (params, "ccControlEnable", ccControlButton);

    ccChannelLabel.setText ("CC Channel (0 = any)", juce::dontSendNotification);
    styleLabel (ccChannelLabel, 12.0f);
    addAndMakeVisible (ccChannelLabel);
    ccChannelSlider.setSliderStyle (juce::Slider::IncDecButtons);
    ccChannelSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 50, 22);
    styleSlider (ccChannelSlider);
    addAndMakeVisible (ccChannelSlider);
    ccChannelAttachment = std::make_unique<SliderAttachment> (params, "ccChannel", ccChannelSlider);

    statusLabel.setJustificationType (juce::Justification::centred);
    statusLabel.setColour (juce::Label::textColourId, juce::Colour::fromRGB (245, 195, 90));
    statusLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    addAndMakeVisible (statusLabel);

    updateStatus();
    startTimerHz (12);
}

OrchNoteFilterAudioProcessorEditor::~OrchNoteFilterAudioProcessorEditor()
{
    stopTimer();
}

void OrchNoteFilterAudioProcessorEditor::syncFieldPresetToCustom()
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
            audioProcessor.getParameters().getParameter ("fieldPreset")))
    {
        if (choice->getIndex() != 0)
        {
            choice->beginChangeGesture();
            choice->setValueNotifyingHost (0.0f);
            choice->endChangeGesture();
        }
    }
}

void OrchNoteFilterAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBackground);
    auto bounds = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (kAccent);
    g.drawRoundedRectangle (bounds, 8.0f, 2.0f);
}

void OrchNoteFilterAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (24, 18);

    titleLabel.setBounds (area.removeFromTop (32));
    subtitleLabel.setBounds (area.removeFromTop (18));
    buildLabel.setBounds (area.removeFromTop (16));
    area.removeFromTop (10);

    enableButton.setBounds (area.removeFromTop (26).removeFromLeft (120));
    area.removeFromTop (8);

    {
        auto row = area.removeFromTop (28);
        fieldPresetLabel.setBounds (row.removeFromLeft (110));
        fieldPresetBox.setBounds (row.removeFromLeft (320));
    }
    area.removeFromTop (8);

    pitchClassLabel.setBounds (area.removeFromTop (20));
    {
        auto row = area.removeFromTop (34);
        const int w = row.getWidth() / 12;
        for (int i = 0; i < 12; ++i)
            pcButtons[static_cast<size_t> (i)].setBounds (row.removeFromLeft (w).reduced (2));
    }
    area.removeFromTop (10);

    {
        auto row = area.removeFromTop (28);
        rootLabel.setBounds (row.removeFromLeft (90));
        rootSlider.setBounds (row.removeFromLeft (140));
        row.removeFromLeft (20);
        foreignModeLabel.setBounds (row.removeFromLeft (110));
        foreignModeBox.setBounds (row.removeFromLeft (130));
    }
    area.removeFromTop (8);

    {
        auto row = area.removeFromTop (28);
        constrainDirectionLabel.setBounds (row.removeFromLeft (110));
        constrainDirectionBox.setBounds (row.removeFromLeft (130));
    }
    area.removeFromTop (8);

    {
        auto row = area.removeFromTop (28);
        shiftLabel.setBounds (row.removeFromLeft (150));
        shiftSlider.setBounds (row.removeFromLeft (row.getWidth()));
    }
    area.removeFromTop (4);
    avoidSnapRepeatsButton.setBounds (area.removeFromTop (24).removeFromLeft (280));
    area.removeFromTop (6);
    {
        auto row = area.removeFromTop (28);
        probabilityLabel.setBounds (row.removeFromLeft (150));
        probabilitySlider.setBounds (row.removeFromLeft (row.getWidth()));
    }
    area.removeFromTop (12);

    passKeyswitchesButton.setBounds (area.removeFromTop (26).removeFromLeft (200));
    area.removeFromTop (4);
    {
        auto row = area.removeFromTop (28);
        ksMinLabel.setBounds (row.removeFromLeft (60));
        ksMinSlider.setBounds (row.removeFromLeft (120));
        row.removeFromLeft (16);
        ksMaxLabel.setBounds (row.removeFromLeft (60));
        ksMaxSlider.setBounds (row.removeFromLeft (120));
    }
    area.removeFromTop (12);

    ccControlButton.setBounds (area.removeFromTop (26));
    area.removeFromTop (4);
    {
        auto row = area.removeFromTop (28);
        ccChannelLabel.setBounds (row.removeFromLeft (150));
        ccChannelSlider.setBounds (row.removeFromLeft (120));
    }

    auto footer = getLocalBounds().reduced (24, 0);
    footer.removeFromBottom (14);
    statusLabel.setBounds (footer.removeFromBottom (24));
}

void OrchNoteFilterAudioProcessorEditor::timerCallback()
{
    const bool constrainMode = foreignModeBox.getSelectedItemIndex() == 2;
    constrainDirectionBox.setEnabled (constrainMode);
    constrainDirectionLabel.setEnabled (constrainMode);

    updateStatus();
}

void OrchNoteFilterAudioProcessorEditor::updateStatus()
{
    const int in = audioProcessor.getLastPerfInputNoteForUi();
    const int out = audioProcessor.getLastPerfOutputNoteForUi();
    const int action = audioProcessor.getLastPerfActionForUi();

    const char* actionText = action == 1 ? "passed"
                           : action == 2 ? "field"
                           : action == 3 ? "DROPPED"
                           : "-";

    juce::String text;
    if (in >= 0)
    {
        text << "Perf: In " << in << " -> "
             << (action == 3 ? juce::String ("(dropped)") : juce::String (out))
             << "  " << actionText;
    }
    else
    {
        text << "Perf: no notes yet";
    }

    text << "   |   field: " << audioProcessor.getActiveFieldSizeForUi() << " pc";

    const int ksIn = audioProcessor.getLastKsInputNoteForUi();
    if (ksIn >= 0)
        text << "   |   KS: In " << ksIn << " -> " << audioProcessor.getLastKsOutputNoteForUi();

    if (audioProcessor.isCcControlActiveForUi())
        text << "   |   CC driving (last CC" << audioProcessor.getLastControlCcForUi() << ")";
    else if (audioProcessor.getParameters().getRawParameterValue ("ccControlEnable")->load() >= 0.5f)
        text << "   |   CC armed";

    statusLabel.setText (text, juce::dontSendNotification);
}
