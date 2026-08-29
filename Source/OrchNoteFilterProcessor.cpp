#include "OrchNoteFilterProcessor.h"
#include "OrchNoteFilterEditor.h"

namespace
{
    constexpr int kNoteUntracked = -1;
    constexpr int kNoteDropped = -2;

    // Field-preset masks, authored as if Root == C. Index order must match
    // getFieldPresetNames() (offset by 1 - index 0 there is "Custom").
    struct NamedField { const char* name; std::array<bool, 12> mask; };

    std::array<bool, 12> maskFromPcs (std::initializer_list<int> pcs)
    {
        std::array<bool, 12> m { };
        for (int pc : pcs)
            m[static_cast<size_t> (((pc % 12) + 12) % 12)] = true;
        return m;
    }

    const std::vector<NamedField>& namedFields()
    {
        static const std::vector<NamedField> fields = {
            { "Chromatic",              maskFromPcs ({ 0,1,2,3,4,5,6,7,8,9,10,11 }) },
            { "Major (Ionian)",         maskFromPcs ({ 0,2,4,5,7,9,11 }) },
            { "Natural Minor (Aeolian)",maskFromPcs ({ 0,2,3,5,7,8,10 }) },
            { "Dorian",                 maskFromPcs ({ 0,2,3,5,7,9,10 }) },
            { "Phrygian",               maskFromPcs ({ 0,1,3,5,7,8,10 }) },
            { "Lydian",                 maskFromPcs ({ 0,2,4,6,7,9,11 }) },
            { "Mixolydian",             maskFromPcs ({ 0,2,4,5,7,9,10 }) },
            { "Locrian",                maskFromPcs ({ 0,1,3,5,6,8,10 }) },
            { "Whole Tone",             maskFromPcs ({ 0,2,4,6,8,10 }) },
            { "Octatonic H-W",          maskFromPcs ({ 0,1,3,4,6,7,9,10 }) },
            { "Octatonic W-H",          maskFromPcs ({ 0,2,3,5,6,8,9,11 }) },
            { "Major Pentatonic",       maskFromPcs ({ 0,2,4,7,9 }) },
            { "Minor Pentatonic",       maskFromPcs ({ 0,3,5,7,10 }) },
            { "All-Interval Tetrachord {0,1,4,6}", maskFromPcs ({ 0,1,4,6 }) },
            { "Set {0,1,6,7}",          maskFromPcs ({ 0,1,6,7 }) },
        };
        return fields;
    }

    int bandedForeignMode (int ccValue)
    {
        const int v = juce::jlimit (0, 127, ccValue);
        if (v < 32)  return static_cast<int> (onft::ForeignMode::Filter);
        if (v < 64)  return static_cast<int> (onft::ForeignMode::Keep);
        if (v < 96)  return static_cast<int> (onft::ForeignMode::Constrain);
        return static_cast<int> (onft::ForeignMode::Solo);
    }
}

OrchNoteFilterAudioProcessor::OrchNoteFilterAudioProcessor()
    : AudioProcessor (BusesProperties()),
      parameters (*this, nullptr, "OrchNoteFilterParameters", createParameterLayout())
{
    enableParam = parameters.getRawParameterValue ("enable");
    fieldPresetParam = parameters.getRawParameterValue ("fieldPreset");
    for (int i = 0; i < 12; ++i)
        pcParams[static_cast<size_t> (i)] = parameters.getRawParameterValue ("pc" + juce::String (i));
    fieldRootParam = parameters.getRawParameterValue ("fieldRoot");
    foreignModeParam = parameters.getRawParameterValue ("foreignMode");
    constrainDirectionParam = parameters.getRawParameterValue ("constrainDirection");
    scaleDegreeShiftParam = parameters.getRawParameterValue ("scaleDegreeShift");
    avoidSnapRepeatsParam = parameters.getRawParameterValue ("avoidSnapRepeats");
    fieldMorphNotesParam = parameters.getRawParameterValue ("fieldMorphNotes");
    probabilityParam = parameters.getRawParameterValue ("probability");
    passKeyswitchesParam = parameters.getRawParameterValue ("passKeyswitches");
    keyswitchMinParam = parameters.getRawParameterValue ("keyswitchMin");
    keyswitchMaxParam = parameters.getRawParameterValue ("keyswitchMax");
    ccControlEnableParam = parameters.getRawParameterValue ("ccControlEnable");
    ccChannelParam = parameters.getRawParameterValue ("ccChannel");
    ccRootNumberParam = parameters.getRawParameterValue ("ccRootNumber");
    ccShiftNumberParam = parameters.getRawParameterValue ("ccShiftNumber");
    ccModeNumberParam = parameters.getRawParameterValue ("ccModeNumber");
    ccProbabilityNumberParam = parameters.getRawParameterValue ("ccProbabilityNumber");
    ccFieldPresetNumberParam = parameters.getRawParameterValue ("ccFieldPresetNumber");
    ccMaskBaseNumberParam = parameters.getRawParameterValue ("ccMaskBaseNumber");

    fieldPresetChoice = dynamic_cast<juce::AudioParameterChoice*> (parameters.getParameter ("fieldPreset"));
    fieldRootInt = dynamic_cast<juce::AudioParameterInt*> (parameters.getParameter ("fieldRoot"));
    foreignModeChoice = dynamic_cast<juce::AudioParameterChoice*> (parameters.getParameter ("foreignMode"));
    scaleDegreeShiftInt = dynamic_cast<juce::AudioParameterInt*> (parameters.getParameter ("scaleDegreeShift"));
    probabilityFloat = dynamic_cast<juce::AudioParameterFloat*> (parameters.getParameter ("probability"));
    for (int i = 0; i < 12; ++i)
        pcBools[static_cast<size_t> (i)] =
            dynamic_cast<juce::AudioParameterBool*> (parameters.getParameter ("pc" + juce::String (i)));

    resetNoteMap();
}

juce::StringArray OrchNoteFilterAudioProcessor::getFieldPresetNames()
{
    juce::StringArray names;
    names.add ("Custom");
    for (const auto& field : namedFields())
        names.add (field.name);
    return names;
}

juce::AudioProcessorValueTreeState::ParameterLayout OrchNoteFilterAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "enable", 1 }, "Enable", true));

    params.push_back (std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "fieldPreset", 1 }, "Field Preset", getFieldPresetNames(), 1)); // Chromatic

    const std::array<const char*, 12> pcNames {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    for (int i = 0; i < 12; ++i)
        params.push_back (std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { "pc" + juce::String (i), 1 },
            juce::String ("PC ") + pcNames[static_cast<size_t> (i)],
            true));

    params.push_back (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "fieldRoot", 1 }, "Field Root", 0, 11, 0));

    params.push_back (std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "foreignMode", 1 }, "Foreign Note Mode",
        juce::StringArray { "Filter", "Keep", "Constrain", "Solo" },
        static_cast<int> (onft::ForeignMode::Constrain)));

    params.push_back (std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "constrainDirection", 1 }, "Constrain Direction",
        juce::StringArray { "Nearest", "Up", "Down" }, 0));

    params.push_back (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "scaleDegreeShift", 1 }, "Scale Degree Shift", -12, 12, 0));

    params.push_back (std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "avoidSnapRepeats", 1 }, "Avoid Snap Repeats", true));

    params.push_back (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "fieldMorphNotes", 1 }, "Field Morph (notes)", 0, 128, 0));

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "probability", 1 }, "Probability",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f));

    params.push_back (std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "passKeyswitches", 1 }, "Pass Keyswitches", true));
    params.push_back (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "keyswitchMin", 1 }, "Keyswitch Min", 0, 127, 0));
    params.push_back (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "keyswitchMax", 1 }, "Keyswitch Max", 0, 127, 35));

    params.push_back (std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "ccControlEnable", 1 }, "CC Control", false));
    params.push_back (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "ccChannel", 1 }, "CC Channel (0 = any)", 0, 16, 0));
    params.push_back (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "ccFieldPresetNumber", 1 }, "CC# Field Preset", 0, 127, 105));
    params.push_back (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "ccRootNumber", 1 }, "CC# Root", 0, 127, 106));
    params.push_back (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "ccShiftNumber", 1 }, "CC# Shift", 0, 127, 107));
    params.push_back (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "ccModeNumber", 1 }, "CC# Mode", 0, 127, 108));
    params.push_back (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "ccProbabilityNumber", 1 }, "CC# Probability", 0, 127, 109));

    // Motif pitch-class mask: 12 consecutive CCs from this base, one per pitch
    // class (>= 64 = on). 0 = off. Lets MC broadcast the exact pitch classes its
    // MotifEngine is writing so the wash tracks the structural voices.
    params.push_back (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "ccMaskBaseNumber", 1 }, "CC# Mask Base (0 = off)", 0, 116, 110));

    return { params.begin(), params.end() };
}

void OrchNoteFilterAudioProcessor::applyFieldPresetFromUI (int presetIndex)
{
    if (presetIndex < 1 || presetIndex > static_cast<int> (namedFields().size()))
        return;

    const auto& mask = namedFields()[static_cast<size_t> (presetIndex - 1)].mask;

    for (int i = 0; i < 12; ++i)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterBool*> (parameters.getParameter ("pc" + juce::String (i))))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (mask[static_cast<size_t> (i)] ? 1.0f : 0.0f);
            p->endChangeGesture();
        }
    }
}

void OrchNoteFilterAudioProcessor::prepareToPlay (double, int)
{
    resetNoteMap();
    ccControlEngaged.store (false);
}

void OrchNoteFilterAudioProcessor::releaseResources() {}

bool OrchNoteFilterAudioProcessor::isBusesLayoutSupported (const BusesLayout&) const { return true; }

void OrchNoteFilterAudioProcessor::resetNoteMap()
{
    for (auto& channel : activeNoteMap)
        channel.fill (kNoteUntracked);

    lastSourceNotePerChannel.fill (-1);
    lastEmittedNotePerChannel.fill (-1);

    lastSeenFieldValid = false;
    morphNotesRemaining = 0;
}

int OrchNoteFilterAudioProcessor::effectiveProbability() const
{
    return probabilityParam != nullptr
        ? juce::jlimit (0, 100, juce::roundToInt (probabilityParam->load()))
        : 100;
}

onft::FieldConfig OrchNoteFilterAudioProcessor::buildFieldConfig() const
{
    // CC control writes the parameters directly (handleControlCc), so this only
    // ever reads params - no hidden override path.
    onft::FieldConfig config;

    for (int i = 0; i < 12; ++i)
        config.pitchClassAllowed[static_cast<size_t> (i)] =
            pcParams[static_cast<size_t> (i)] != nullptr && pcParams[static_cast<size_t> (i)]->load() >= 0.5f;

    config.root = fieldRootParam != nullptr ? juce::jlimit (0, 11, juce::roundToInt (fieldRootParam->load())) : 0;

    const int mode = foreignModeParam != nullptr ? juce::jlimit (0, 3, juce::roundToInt (foreignModeParam->load())) : 2;
    config.foreignMode = static_cast<onft::ForeignMode> (mode);

    const int dir = constrainDirectionParam != nullptr
        ? juce::jlimit (0, 2, juce::roundToInt (constrainDirectionParam->load())) : 0;
    config.constrainDirection = static_cast<onft::ConstrainDirection> (dir);

    config.scaleDegreeShift = scaleDegreeShiftParam != nullptr
        ? juce::jlimit (-12, 12, juce::roundToInt (scaleDegreeShiftParam->load())) : 0;

    return config;
}

onft::FieldConfig OrchNoteFilterAudioProcessor::buildFieldConfigWithMask (const std::array<bool, 12>& mask) const
{
    auto config = buildFieldConfig();
    config.pitchClassAllowed = mask;
    return config;
}

void OrchNoteFilterAudioProcessor::handleControlCc (const juce::MidiMessage& message)
{
    const int cc = message.getControllerNumber();
    const int value = juce::jlimit (0, 127, message.getControllerValue());

    const auto readNum = [] (std::atomic<float>* p, int fallback)
    {
        return p != nullptr ? juce::jlimit (0, 127, juce::roundToInt (p->load())) : fallback;
    };

    const int ccRootN = readNum (ccRootNumberParam, 106);
    const int ccShiftN = readNum (ccShiftNumberParam, 107);
    const int ccModeN = readNum (ccModeNumberParam, 108);
    const int ccProbN = readNum (ccProbabilityNumberParam, 109);
    const int ccPresetN = readNum (ccFieldPresetNumberParam, 105);
    const int ccMaskBase = juce::jlimit (0, 116, readNum (ccMaskBaseNumberParam, 110));

    const auto setInt = [] (juce::AudioParameterInt* p, int target)
    {
        if (p != nullptr && p->get() != target)
            *p = target;
    };
    const auto setChoice = [] (juce::AudioParameterChoice* p, int index)
    {
        if (p != nullptr && p->getIndex() != index)
            *p = index;
    };

    bool matched = false;

    if (ccRootN != 0 && cc == ccRootN)
    {
        setInt (fieldRootInt, juce::roundToInt (value / 127.0f * 11.0f));
        matched = true;
    }
    else if (ccShiftN != 0 && cc == ccShiftN)
    {
        setInt (scaleDegreeShiftInt, juce::roundToInt (value / 127.0f * 24.0f) - 12);
        matched = true;
    }
    else if (ccModeN != 0 && cc == ccModeN)
    {
        setChoice (foreignModeChoice, bandedForeignMode (value));
        matched = true;
    }
    else if (ccProbN != 0 && cc == ccProbN)
    {
        const float target = value / 127.0f * 100.0f;
        if (probabilityFloat != nullptr && ! juce::approximatelyEqual (probabilityFloat->get(), target))
            *probabilityFloat = target;
        matched = true;
    }
    else if (ccPresetN != 0 && cc == ccPresetN)
    {
        const int count = static_cast<int> (namedFields().size());
        const int index = juce::jlimit (0, count - 1, juce::roundToInt (value / 127.0f * (count - 1)));
        const auto& mask = namedFields()[static_cast<size_t> (index)].mask;

        // Move the visible preset selector and the 12 toggles together.
        setChoice (fieldPresetChoice, index + 1); // +1: choice index 0 is "Custom"
        for (int i = 0; i < 12; ++i)
        {
            auto* p = pcBools[static_cast<size_t> (i)];
            const bool want = mask[static_cast<size_t> (i)];
            if (p != nullptr && p->get() != want)
                *p = want;
        }
        matched = true;
    }
    else if (ccMaskBase != 0 && cc >= ccMaskBase && cc <= ccMaskBase + 11)
    {
        // One pitch class of a broadcast motif mask.
        const int pcIndex = cc - ccMaskBase;
        const bool want = value >= 64;

        if (auto* p = pcBools[static_cast<size_t> (pcIndex)]; p != nullptr && p->get() != want)
        {
            *p = want;
            setChoice (fieldPresetChoice, 0); // the mask defines the field, not a named preset
        }
        matched = true;
    }

    if (matched)
    {
        ccControlEngaged.store (true);
        lastControlCc.store (cc);
    }
}

void OrchNoteFilterAudioProcessor::handleNoteOn (const juce::MidiMessage& message, int samplePosition, juce::MidiBuffer& output)
{
    const int channel = juce::jlimit (1, 16, message.getChannel());
    const int inputNote = juce::jlimit (0, 127, message.getNoteNumber());
    const auto ch = static_cast<size_t> (channel - 1);
    const auto in = static_cast<size_t> (inputNote);

    auto emitPerf = [&] (int outNote, int action)
    {
        activeNoteMap[ch][in] = outNote;
        if (outNote == inputNote)
            output.addEvent (message, samplePosition);
        else
            output.addEvent (juce::MidiMessage::noteOn (channel, outNote, message.getVelocity()), samplePosition);

        lastPerfInputNote.store (inputNote);
        lastPerfOutputNote.store (outNote);
        lastPerfAction.store (action);
        lastSourceNotePerChannel[ch] = inputNote;
        lastEmittedNotePerChannel[ch] = outNote;
    };

    const bool enabled = enableParam != nullptr && enableParam->load() >= 0.5f;
    if (! enabled)
    {
        emitPerf (inputNote, 1);
        return;
    }

    const bool passKs = passKeyswitchesParam != nullptr && passKeyswitchesParam->load() >= 0.5f;
    const int ksMinRaw = keyswitchMinParam != nullptr ? juce::roundToInt (keyswitchMinParam->load()) : 0;
    const int ksMaxRaw = keyswitchMaxParam != nullptr ? juce::roundToInt (keyswitchMaxParam->load()) : 35;
    const int ksMin = juce::jlimit (0, 127, juce::jmin (ksMinRaw, ksMaxRaw));
    const int ksMax = juce::jlimit (0, 127, juce::jmax (ksMinRaw, ksMaxRaw));

    if (passKs && inputNote >= ksMin && inputNote <= ksMax)
    {
        activeNoteMap[ch][in] = inputNote;
        output.addEvent (message, samplePosition);
        lastKsInputNote.store (inputNote);
        lastKsOutputNote.store (inputNote);
        return;
    }

    const int probability = effectiveProbability();
    const bool processThisNote = probability >= 100
        || (probability > 0 && random.nextFloat() * 100.0f < static_cast<float> (probability));

    if (! processThisNote)
    {
        emitPerf (inputNote, 1);
        return;
    }

    // Read the current field mask and detect a change since the last note.
    std::array<bool, 12> currentMask { };
    for (int i = 0; i < 12; ++i)
        currentMask[static_cast<size_t> (i)] =
            pcParams[static_cast<size_t> (i)] != nullptr && pcParams[static_cast<size_t> (i)]->load() >= 0.5f;

    if (! lastSeenFieldValid)
    {
        lastSeenFieldMask = currentMask;
        lastSeenFieldValid = true;
    }
    else if (currentMask != lastSeenFieldMask)
    {
        const int morphNotes = fieldMorphNotesParam != nullptr
            ? juce::jlimit (0, 128, juce::roundToInt (fieldMorphNotesParam->load())) : 0;

        if (morphNotes > 0)
        {
            // Keep the pre-change field as the "from" side; if a morph is
            // already running, retarget without resetting its origin.
            if (morphNotesRemaining <= 0)
                morphFromFieldMask = lastSeenFieldMask;

            morphNotesRemaining = morphNotes;
            morphNotesTotal = morphNotes;
        }

        lastSeenFieldMask = currentMask;
    }

    onft::FieldConfig config;

    if (morphNotesRemaining > 0)
    {
        const float progress = 1.0f - static_cast<float> (morphNotesRemaining)
                                    / static_cast<float> (juce::jmax (1, morphNotesTotal));
        const bool useNew = random.nextFloat() < progress;
        config = buildFieldConfigWithMask (useNew ? currentMask : morphFromFieldMask);
        --morphNotesRemaining;
    }
    else
    {
        config = buildFieldConfigWithMask (currentMask);
    }

    const bool avoidRepeats = avoidSnapRepeatsParam != nullptr && avoidSnapRepeatsParam->load() >= 0.5f;
    if (avoidRepeats && inputNote != lastSourceNotePerChannel[ch])
        config.avoidNote = lastEmittedNotePerChannel[ch];

    const auto result = onft::resolveNote (inputNote, config);

    // Record the source even when the note is dropped, so "input differs" stays
    // meaningful against the previous *sounding* note.
    lastSourceNotePerChannel[ch] = inputNote;

    if (! result.play)
    {
        activeNoteMap[ch][in] = kNoteDropped;
        lastPerfInputNote.store (inputNote);
        lastPerfOutputNote.store (-1);
        lastPerfAction.store (3);
        return;
    }

    const int outNote = juce::jlimit (0, 127, result.outputNote);
    activeNoteMap[ch][in] = outNote;
    output.addEvent (juce::MidiMessage::noteOn (channel, outNote, message.getVelocity()), samplePosition);
    lastPerfInputNote.store (inputNote);
    lastPerfOutputNote.store (outNote);
    lastPerfAction.store (outNote == inputNote ? 1 : 2);
    lastEmittedNotePerChannel[ch] = outNote;
}

void OrchNoteFilterAudioProcessor::handleNoteOff (const juce::MidiMessage& message, int samplePosition, juce::MidiBuffer& output)
{
    const int channel = juce::jlimit (1, 16, message.getChannel());
    const int inputNote = juce::jlimit (0, 127, message.getNoteNumber());
    const auto ch = static_cast<size_t> (channel - 1);
    const auto in = static_cast<size_t> (inputNote);

    const int remembered = activeNoteMap[ch][in];
    activeNoteMap[ch][in] = kNoteUntracked;

    if (remembered == kNoteDropped)
        return; // matching note-on was dropped; consume the note-off

    if (remembered >= 0)
    {
        output.addEvent (juce::MidiMessage::noteOff (channel, remembered, message.getVelocity()), samplePosition);
        return;
    }

    // Untracked: pass through unchanged (avoid a stuck note if inserted mid-hold).
    output.addEvent (message, samplePosition);
}

void OrchNoteFilterAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    buffer.clear();

    const bool ccControlOn = ccControlEnableParam != nullptr && ccControlEnableParam->load() >= 0.5f;
    if (! ccControlOn)
        ccControlEngaged.store (false);

    const int ccChannel = ccChannelParam != nullptr
        ? juce::jlimit (0, 16, juce::roundToInt (ccChannelParam->load())) : 0;

    juce::MidiBuffer output;

    for (const auto metadata : midiMessages)
    {
        const auto message = metadata.getMessage();
        const int samplePosition = metadata.samplePosition;

        if (message.isController())
        {
            if (ccControlOn && (ccChannel == 0 || message.getChannel() == ccChannel))
                handleControlCc (message);

            output.addEvent (message, samplePosition); // control CCs pass through
            continue;
        }

        if (message.isNoteOn())
        {
            handleNoteOn (message, samplePosition, output);
            continue;
        }

        if (message.isNoteOff())
        {
            handleNoteOff (message, samplePosition, output);
            continue;
        }

        if (message.isAllNotesOff() || message.isAllSoundOff())
        {
            resetNoteMap();
            output.addEvent (message, samplePosition);
            continue;
        }

        output.addEvent (message, samplePosition);
    }

    midiMessages.swapWith (output);
}

bool OrchNoteFilterAudioProcessor::isCcControlActiveForUi() const
{
    return (ccControlEnableParam != nullptr && ccControlEnableParam->load() >= 0.5f)
        && ccControlEngaged.load();
}

int OrchNoteFilterAudioProcessor::getActiveFieldSizeForUi() const
{
    int count = 0;
    for (auto* p : pcParams)
        if (p != nullptr && p->load() >= 0.5f)
            ++count;
    return count;
}

juce::AudioProcessorEditor* OrchNoteFilterAudioProcessor::createEditor()
{
    return new OrchNoteFilterAudioProcessorEditor (*this);
}

bool OrchNoteFilterAudioProcessor::hasEditor() const { return true; }
const juce::String OrchNoteFilterAudioProcessor::getName() const { return JucePlugin_Name; }
bool OrchNoteFilterAudioProcessor::acceptsMidi() const { return true; }
bool OrchNoteFilterAudioProcessor::producesMidi() const { return true; }
bool OrchNoteFilterAudioProcessor::isMidiEffect() const { return true; }
double OrchNoteFilterAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int OrchNoteFilterAudioProcessor::getNumPrograms() { return 1; }
int OrchNoteFilterAudioProcessor::getCurrentProgram() { return 0; }
void OrchNoteFilterAudioProcessor::setCurrentProgram (int) {}
const juce::String OrchNoteFilterAudioProcessor::getProgramName (int) { return {}; }
void OrchNoteFilterAudioProcessor::changeProgramName (int, const juce::String&) {}

void OrchNoteFilterAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = parameters.copyState(); state.isValid())
        if (auto xml = std::unique_ptr<juce::XmlElement> (state.createXml()))
            copyXmlToBinary (*xml, destData);
}

void OrchNoteFilterAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = std::unique_ptr<juce::XmlElement> (getXmlFromBinary (data, sizeInBytes)))
        if (xml->hasTagName (parameters.state.getType()))
            parameters.replaceState (juce::ValueTree::fromXml (*xml));

    resetNoteMap();
    ccControlEngaged.store (false);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OrchNoteFilterAudioProcessor();
}
