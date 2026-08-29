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
                int out = applyScaleDegreeShift(base, fieldPCs, config.scaleDegreeShift);

                if (config.avoidNote >= 0 && out == config.avoidNote)
                {
                    const int step = config.constrainDirection == ConstrainDirection::Down ? -1 : 1;
                    out = applyScaleDegreeShift(out, fieldPCs, step);
                }

                return { true, out };
            }

            case ForeignMode::Solo:
                if (inField)
                    return { false, note };
                return { true, note };
        }

        return { true, note };
    }

    std::array<bool, 12> widenFieldToScale(const std::array<bool, 12>& field,
                                           float width,
                                           const std::vector<std::array<bool, 12>>& candidateScales)
    {
        if (width < 0.0f) width = 0.0f;
        if (width > 1.0f) width = 1.0f;
        if (width <= 0.0f || candidateScales.empty())
            return field;

        int fieldCount = 0;
        for (bool b : field)
            fieldCount += b ? 1 : 0;

        if (fieldCount == 0 || fieldCount >= 12)
            return field; // nothing to anchor to, or already full

        // Best fit over every rotation of every candidate scale: minimise the
        // field notes the scale fails to cover, then minimise the scale's own
        // extra notes (tightest superset wins).
        std::array<bool, 12> best { };
        int bestScore = -1;

        for (const auto& scale : candidateScales)
        {
            int scaleCount = 0;
            for (bool b : scale)
                scaleCount += b ? 1 : 0;

            if (scaleCount == 0)
                continue;

            for (int rot = 0; rot < 12; ++rot)
            {
                std::array<bool, 12> rotated { };
                for (int i = 0; i < 12; ++i)
                    rotated[static_cast<size_t>(i)] = scale[static_cast<size_t>(mod12(i - rot))];

                int missing = 0;
                int extra = 0;
                for (int i = 0; i < 12; ++i)
                {
                    const bool inField = field[static_cast<size_t>(i)];
                    const bool inScale = rotated[static_cast<size_t>(i)];
                    if (inField && ! inScale) ++missing;
                    if (! inField && inScale) ++extra;
                }

                const int score = missing * 100 + extra;
                if (bestScore < 0 || score < bestScore)
                {
                    bestScore = score;
                    best = rotated;
                }
            }
        }

        if (bestScore < 0)
            return field;

        // Pitch classes the best-fit scale adds, ordered furthest-first by
        // semitone distance to the nearest field member - gentle extensions
        // (9ths, 6ths) come in before semitone neighbours - tie-break ascending.
        struct Extra { int pc; int dist; };
        std::array<Extra, 12> extras { };
        int numExtras = 0;

        for (int i = 0; i < 12; ++i)
        {
            if (field[static_cast<size_t>(i)] || ! best[static_cast<size_t>(i)])
                continue;

            int nearest = 12;
            for (int j = 0; j < 12; ++j)
            {
                if (! field[static_cast<size_t>(j)])
                    continue;
                const int raw = std::abs(i - j);
                nearest = std::min(nearest, std::min(raw, 12 - raw));
            }

            extras[static_cast<size_t>(numExtras++)] = { i, nearest };
        }

        std::sort(extras.begin(), extras.begin() + numExtras,
                  [](const Extra& a, const Extra& b)
                  { return a.dist != b.dist ? a.dist > b.dist : a.pc < b.pc; });

        int include = static_cast<int>(width * static_cast<float>(numExtras) + 0.5f);
        if (include > numExtras) include = numExtras;

        std::array<bool, 12> out = field;
        for (int k = 0; k < include; ++k)
            out[static_cast<size_t>(extras[static_cast<size_t>(k)].pc)] = true;

        return out;
    }
}
