#include "OrchNoteFilterFieldLogic.h"

#include <algorithm>

namespace onft
{
    namespace
    {
        int mod12(int value) noexcept
        {
            return ((value % 12) + 12) % 12;
        }

        int clampNote(int note) noexcept
        {
            return std::clamp(note, 0, 127);
        }

        int floorDiv(int a, int b) noexcept
        {
            int q = a / b;
            if ((a % b != 0) && ((a < 0) != (b < 0)))
                --q;
            return q;
        }

        // Move an in-field note by config.scaleDegreeShift steps through the
        // ordered field, wrapping with octave add/subtract. `note` must already
        // be an in-field pitch.
        int applyScaleDegreeShift(int note, const std::vector<int>& fieldPCs, int shift)
        {
            if (shift == 0 || fieldPCs.empty())
                return note;

            const int pc = mod12(note);
            const int octave = floorDiv(note, 12);

            const auto it = std::find(fieldPCs.begin(), fieldPCs.end(), pc);
            if (it == fieldPCs.end())
                return note; // defensive: caller guarantees membership

            const int index = static_cast<int>(std::distance(fieldPCs.begin(), it));
            const int size = static_cast<int>(fieldPCs.size());

            const int total = index + shift;
            const int octaveShift = floorDiv(total, size);
            const int newIndex = total - octaveShift * size;

            return clampNote(12 * (octave + octaveShift) + fieldPCs[static_cast<size_t>(newIndex)]);
        }

        int snapToField(int note, const FieldConfig& config, ConstrainDirection direction)
        {
            // The field is non-empty here, so a member is always reachable
            // within an octave in at least one direction.
            auto inField = [&config](int candidate)
            {
                return candidate >= 0 && candidate <= 127 && isInField(mod12(candidate), config);
            };

            if (direction == ConstrainDirection::Up)
            {
                for (int d = 0; d <= 12; ++d)
                    if (inField(note + d))
                        return note + d;
                for (int d = 1; d <= 12; ++d)
                    if (inField(note - d))
                        return note - d;
                return clampNote(note);
            }

            if (direction == ConstrainDirection::Down)
            {
                for (int d = 0; d <= 12; ++d)
                    if (inField(note - d))
                        return note - d;
                for (int d = 1; d <= 12; ++d)
                    if (inField(note + d))
                        return note + d;
                return clampNote(note);
            }

            // Nearest: expand symmetrically; on a tie prefer the lower note.
            if (inField(note))
                return note;

            for (int d = 1; d <= 12; ++d)
            {
                if (inField(note - d))
                    return note - d;
                if (inField(note + d))
                    return note + d;
            }

            return clampNote(note);
        }
    }

    bool isEmptyField(const FieldConfig& config) noexcept
    {
        for (bool allowed : config.pitchClassAllowed)
            if (allowed)
                return false;
        return true;
    }

    bool isInField(int pitchClass, const FieldConfig& config) noexcept
    {
        const int degree = mod12(pitchClass - config.root);
        return config.pitchClassAllowed[static_cast<size_t>(degree)];
    }

    std::vector<int> orderedFieldPitchClasses(const FieldConfig& config)
    {
        std::vector<int> result;
        result.reserve(12);
        for (int pc = 0; pc < 12; ++pc)
            if (isInField(pc, config))
                result.push_back(pc);
        return result;
    }

    FieldResult resolveNote(int inputNote, const FieldConfig& config)
    {
        const int note = clampNote(inputNote);

        if (isEmptyField(config))
            return { true, note };

        const int pc = mod12(note);
        const bool inField = isInField(pc, config);
        const auto fieldPCs = orderedFieldPitchClasses(config);

        switch (config.foreignMode)
        {
            case ForeignMode::Filter:
                if (inField)
                    return { true, applyScaleDegreeShift(note, fieldPCs, config.scaleDegreeShift) };
                return { false, note };

            case ForeignMode::Keep:
                if (inField)
                    return { true, applyScaleDegreeShift(note, fieldPCs, config.scaleDegreeShift) };
                return { true, note };

            case ForeignMode::Constrain:
            {
                const int base = inField ? note
                                         : snapToField(note, config, config.constrainDirection);
                return { true, applyScaleDegreeShift(base, fieldPCs, config.scaleDegreeShift) };
            }

            case ForeignMode::Solo:
                if (inField)
                    return { false, note };
                return { true, note };
        }

        return { true, note };
    }
}
