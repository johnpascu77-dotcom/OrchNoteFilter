#pragma once

#include <array>
#include <atomic>
#include <JuceHeader.h>

#include "OrchNoteFilterFieldLogic.h"

class OrchNoteFilterAudioProcessor final : public juce::AudioProcessor
{
public:
    OrchNoteFilterAudioProcessor();
    ~OrchNoteFilterAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getParameters() { return parameters; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Field-preset names, index 0 == "Custom". applyFieldPreset writes the 12
    // pitch-class parameters for a given index (>= 1); index 0 is a no-op.
    static juce::StringArray getFieldPresetNames();
    void applyFieldPresetFromUI (int presetIndex);

    // UI status readouts (message thread; plain int reads).
    int getLastPerfInputNoteForUi() const { return lastPerfInputNote.load(); }
    int getLastPerfOutputNoteForUi() const { return lastPerfOutputNote.load(); }
    int getLastPerfActionForUi() const { return lastPerfAction.load(); } // 0 none, 1 passed, 2 field, 3 dropped
    int getLastKsInputNoteForUi() const { return lastKsInputNote.load(); }
    int getLastKsOutputNoteForUi() const { return lastKsOutputNote.load(); }
    bool isCcControlActiveForUi() const;
    int getLastControlCcForUi() const { return lastControlCc.load(); }

private:
    juce::AudioProcessorValueTreeState parameters;

    std::atomic<float>* enableParam = nullptr;
    std::atomic<float>* fieldPresetParam = nullptr;
    std::array<std::atomic<float>*, 12> pcParams { };
    std::atomic<float>* fieldRootParam = nullptr;
    std::atomic<float>* foreignModeParam = nullptr;
    std::atomic<float>* constrainDirectionParam = nullptr;
    std::atomic<float>* scaleDegreeShiftParam = nullptr;
    std::atomic<float>* avoidSnapRepeatsParam = nullptr;
    std::atomic<float>* probabilityParam = nullptr;
    std::atomic<float>* passKeyswitchesParam = nullptr;
    std::atomic<float>* keyswitchMinParam = nullptr;
    std::atomic<float>* keyswitchMaxParam = nullptr;
    std::atomic<float>* ccControlEnableParam = nullptr;
    std::atomic<float>* ccChannelParam = nullptr;
    std::atomic<float>* ccRootNumberParam = nullptr;
    std::atomic<float>* ccShiftNumberParam = nullptr;
    std::atomic<float>* ccModeNumberParam = nullptr;
    std::atomic<float>* ccProbabilityNumberParam = nullptr;
    std::atomic<float>* ccFieldPresetNumberParam = nullptr;

    juce::Random random;

    // CC-driven overrides (used only while ccControlEnable is on).
    std::atomic<bool> ccControlEngaged { false };
    std::atomic<int> ccRoot { 0 };
    std::atomic<int> ccShift { 0 };
    std::atomic<int> ccMode { static_cast<int> (onft::ForeignMode::Constrain) };
    std::atomic<int> ccProbability { 100 };
    std::array<std::atomic<bool>, 12> ccFieldMask { };
    std::atomic<bool> ccFieldMaskValid { false };

    std::atomic<int> lastPerfInputNote { -1 };
    std::atomic<int> lastPerfOutputNote { -1 };
    std::atomic<int> lastPerfAction { 0 };
    std::atomic<int> lastKsInputNote { -1 };
    std::atomic<int> lastKsOutputNote { -1 };
    std::atomic<int> lastControlCc { -1 };

    // -1 untracked, -2 note-on was dropped, >=0 remembered output note.
    std::array<std::array<int, 128>, 16> activeNoteMap { };

    // Per-channel last performance note in / out, for the "avoid snap repeats"
    // guard (Constrain mode).
    std::array<int, 16> lastSourceNotePerChannel { };
    std::array<int, 16> lastEmittedNotePerChannel { };

    onft::FieldConfig buildFieldConfig() const;
    int effectiveProbability() const;

    void resetNoteMap();
    void handleControlCc (const juce::MidiMessage& message);
    void handleNoteOn (const juce::MidiMessage& message, int samplePosition, juce::MidiBuffer& output);
    void handleNoteOff (const juce::MidiMessage& message, int samplePosition, juce::MidiBuffer& output);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OrchNoteFilterAudioProcessor)
};
