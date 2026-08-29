#pragma once

#include <array>
#include <vector>

// Pure pitch-class field logic for OrchNoteFilter. No JUCE, no randomness, no
// keyswitch handling, no MIDI-buffer or note-tracking concerns - those live in
// the processor. This is the part the field-logic check tool exercises directly.
namespace onft
{
    enum class ForeignMode
    {
        Filter = 0,   // in-field: pass (+shift).  out-of-field: drop.
        Keep,         // in-field: pass (+shift).  out-of-field: pass unchanged.
        Constrain,    // in-field: pass (+shift).  out-of-field: snap to nearest in-field (+shift).
        Solo          // in-field: drop.           out-of-field: pass unchanged.
    };

    enum class ConstrainDirection
    {
        Nearest = 0,
        Up,
        Down
    };

    struct FieldConfig
    {
        // Authored as if Root == C. pitchClassAllowed[i] == true means scale
        // degree i (0 = C) is a member before the Root rotation is applied.
        std::array<bool, 12> pitchClassAllowed { };

        int root = 0;                    // 0..11, rotates the field upward
        ForeignMode foreignMode = ForeignMode::Constrain;
        ConstrainDirection constrainDirection = ConstrainDirection::Nearest;
        int scaleDegreeShift = 0;        // steps through the ordered field list

        // Constrain only: if the resolved output equals avoidNote (a pitch the
        // caller wants not repeated - typically the last note emitted on this
        // channel), step one field degree further so the constrained line keeps
        // moving instead of hammering one pitch. -1 disables.
        int avoidNote = -1;
    };

    struct FieldResult
    {
        bool play = true;                // false => drop this note
        int outputNote = 60;             // valid only when play == true
    };

    // True if no pitch class is enabled. An empty field passes every note
    // through unchanged regardless of mode (a safety, not a musical state).
    bool isEmptyField(const FieldConfig& config) noexcept;

    // Is the actual sounding pitch class (0..11) a member of the root-rotated
    // field?
    bool isInField(int pitchClass, const FieldConfig& config) noexcept;

    // Sorted ascending list of the sounding pitch classes (0..11) in the
    // root-rotated field. Empty only when isEmptyField() is true.
    std::vector<int> orderedFieldPitchClasses(const FieldConfig& config);

    // Resolve one input MIDI note (0..127) against the field.
    FieldResult resolveNote(int inputNote, const FieldConfig& config);

    // "Field Width": widen `field` toward the best-fitting scale drawn from
    // `candidateScales` (each a 12-bool pitch-class mask, any rotation - the
    // search tries all 12). width 0 returns `field` untouched; width 1 adds
    // every pitch class of the best-fit scale; in between adds that fraction of
    // the scale's extra notes, furthest-from-the-field first. The field's own
    // pitch classes are always kept, so the result is never smaller than the
    // input. Deterministic; safe to call per note.
    std::array<bool, 12> widenFieldToScale(const std::array<bool, 12>& field,
                                           float width,
                                           const std::vector<std::array<bool, 12>>& candidateScales);
}
