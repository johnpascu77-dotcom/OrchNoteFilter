#pragma once

#include <array>
#include <atomic>
#include <vector>
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
    int getActiveFieldSizeForUi() const; // number of enabled pitch classes

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
    std::atomic<float>* fieldMorphNotesParam = nullptr;
    std::atomic<float>* fieldWidthParam = nullptr;
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
    std::atomic<float>* ccMaskBaseNumberParam = nullptr;

    // Typed handles for the parameters CC control writes back, so incoming CCs
    // move the visible controls (and any host automation) rather than a hidden
    // override. Guarded writes only fire on an actual value change.
    juce::AudioParameterChoice* fieldPresetChoice = nullptr;
    juce::AudioParameterInt* fieldRootInt = nullptr;
    juce::AudioParameterChoice* foreignModeChoice = nullptr;
    juce::AudioParameterInt* scaleDegreeShiftInt = nullptr;
    juce::AudioParameterFloat* probabilityFloat = nullptr;
    std::array<juce::AudioParameterBool*, 12> pcBools { };

    juce::Random random;

    // Display-only: true once CC control has actually applied an incoming CC.
    std::atomic<bool> ccControlEngaged { false };

    std::atomic<int> lastPerfInputNote { -1 };
    std::atomic<int> lastPerfOutputNote { -1 };
    std::atomic<int> lastPerfAction { 0 };
    std::atomic<int> lastKsInputNote { -1 };
    std::atomic<int> lastKsOutputNote { -1 };
    std::atomic<int> lastControlCc { -1 };

    // One entry per sounding note-on that passed through. A list, not a slot
    // per (channel, input note): the wash source produces overlapping identical
    // input notes (the +6 Randomize collapses distinct MPL pitches onto one
    // value), so several can be live for the same (channel, input) at once and
    // each needs its own remembered output for a correctly-paired note-off.
    struct TrackedNote
    {
        int channel = 0;      // 1..16
        int inputNote = 0;    // 0..127
        int outputNote = -1;  // emitted pitch, or -1 if this note-on was dropped
    };
    std::vector<TrackedNote> activeNotes;

    // Per-channel last performance note in / out, for the "avoid snap repeats"
    // guard (Constrain mode).
    std::array<int, 16> lastSourceNotePerChannel { };
    std::array<int, 16> lastEmittedNotePerChannel { };

    // Broadcast motif mask arrives as two CCs (baseCc = low 7 pitch classes,
    // baseCc+1 = high 5); latch each half so either CC can rebuild the toggles.
    int maskLowBits { 0 };
    int maskHighBits { 0 };

    // Field Morph: when the field mask changes, blend old -> new over N note-ons
    // (ramping probability) instead of switching hard on the next note.
    std::array<bool, 12> lastSeenFieldMask { };
    bool lastSeenFieldValid { false };
    std::array<bool, 12> morphFromFieldMask { };
    int morphNotesRemaining { 0 };
    int morphNotesTotal { 0 };

    onft::FieldConfig buildFieldConfig() const;
    onft::FieldConfig buildFieldConfigWithMask (const std::array<bool, 12>& mask) const;
    int effectiveProbability() const;

    // The 12 pc toggles as a bare mask, and that mask after the Field Width
    // knob widens it toward the nearest scale (identity when the knob is 0).
    std::array<bool, 12> rawFieldMask() const;
    std::array<bool, 12> applyFieldWidth (const std::array<bool, 12>& mask) const;

    void resetNoteMap();
    void handleControlCc (const juce::MidiMessage& message);
    void handleNoteOn (const juce::MidiMessage& message, int samplePosition, juce::MidiBuffer& output);
    void handleNoteOff (const juce::MidiMessage& message, int samplePosition, juce::MidiBuffer& output);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OrchNoteFilterAudioProcessor)
};
