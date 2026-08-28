#include "OrchNoteFilterFieldLogic.h"

#include <array>
#include <iostream>
#include <string>

namespace
{
    int failures = 0;

    void check (bool condition, const std::string& label)
    {
        if (condition)
        {
            std::cout << "[PASS] " << label << "\n";
        }
        else
        {
            std::cerr << "[FAIL] " << label << "\n";
            ++failures;
        }
    }

    void checkNote (int got, int expected, const std::string& label)
    {
        check (got == expected, label + " (got " + std::to_string (got)
                                + ", expected " + std::to_string (expected) + ")");
    }

    onft::FieldConfig cMajor()
    {
        onft::FieldConfig c;
        for (int pc : { 0, 2, 4, 5, 7, 9, 11 })
            c.pitchClassAllowed[static_cast<size_t> (pc)] = true;
        return c;
    }
}

int main()
{
    std::cout << "OrchNoteFilterFieldLogicCheck\n----------------------------\n";

    // Empty field: passes everything unchanged regardless of mode.
    {
        onft::FieldConfig empty;
        empty.foreignMode = onft::ForeignMode::Filter;
        const auto r = onft::resolveNote (61, empty);
        check (r.play && r.outputNote == 61, "empty field passes note unchanged");
    }

    // Membership + root rotation.
    {
        auto c = cMajor();
        check (onft::isInField (0, c) && ! onft::isInField (1, c), "C in C-major, C# not");
        c.root = 2; // D major
        check (onft::isInField (2, c) && onft::isInField (6, c) && ! onft::isInField (0, c),
               "root=2 rotates field to D major (C# in, C out)");
    }

    // Filter mode: in-field passes, out-of-field drops.
    {
        auto c = cMajor();
        c.foreignMode = onft::ForeignMode::Filter;
        checkNote (onft::resolveNote (60, c).outputNote, 60, "Filter: C60 passes");
        check (! onft::resolveNote (61, c).play, "Filter: C#61 dropped");
    }

    // Keep mode: out-of-field passes unchanged.
    {
        auto c = cMajor();
        c.foreignMode = onft::ForeignMode::Keep;
        const auto r = onft::resolveNote (61, c);
        check (r.play && r.outputNote == 61, "Keep: C#61 passes unchanged");
    }

    // Solo mode: only out-of-field passes.
    {
        auto c = cMajor();
        c.foreignMode = onft::ForeignMode::Solo;
        check (! onft::resolveNote (60, c).play, "Solo: C60 (in field) dropped");
        const auto r = onft::resolveNote (61, c);
        check (r.play && r.outputNote == 61, "Solo: C#61 (out of field) passes");
    }

    // Constrain: nearest / up / down.
    {
        auto c = cMajor();
        c.foreignMode = onft::ForeignMode::Constrain;

        c.constrainDirection = onft::ConstrainDirection::Nearest;
        checkNote (onft::resolveNote (61, c).outputNote, 60, "Constrain Nearest: C#61 -> C60 (tie prefers lower)");
        checkNote (onft::resolveNote (66, c).outputNote, 65, "Constrain Nearest: F#66 -> F65");

        c.constrainDirection = onft::ConstrainDirection::Up;
        checkNote (onft::resolveNote (61, c).outputNote, 62, "Constrain Up: C#61 -> D62");

        c.constrainDirection = onft::ConstrainDirection::Down;
        checkNote (onft::resolveNote (61, c).outputNote, 60, "Constrain Down: C#61 -> C60");
    }

    // Scale-degree shift within the field (C major, 7 degrees).
    {
        auto c = cMajor();
        c.foreignMode = onft::ForeignMode::Constrain;

        c.scaleDegreeShift = 1;
        checkNote (onft::resolveNote (60, c).outputNote, 62, "shift +1: C60 -> D62");
        checkNote (onft::resolveNote (71, c).outputNote, 72, "shift +1: B71 -> C72 (octave wrap up)");

        c.scaleDegreeShift = -1;
        checkNote (onft::resolveNote (60, c).outputNote, 59, "shift -1: C60 -> B59 (octave wrap down)");

        c.scaleDegreeShift = 7;
        checkNote (onft::resolveNote (60, c).outputNote, 72, "shift +7: C60 -> C72 (one octave)");

        c.scaleDegreeShift = -7;
        checkNote (onft::resolveNote (60, c).outputNote, 48, "shift -7: C60 -> C48");
    }

    // Avoid-snap-repeats: two different inputs that would both land on D62.
    {
        auto c = cMajor();
        c.foreignMode = onft::ForeignMode::Constrain;
        c.constrainDirection = onft::ConstrainDirection::Nearest;

        // C#61 -> C60, D#63 -> D62 or E64. Force the collision case: avoidNote
        // says "don't repeat 60", so C#61 must move off C60.
        c.avoidNote = 60;
        check (onft::resolveNote (61, c).outputNote != 60,
               "avoidNote: C#61 does not repeat C60");

        c.avoidNote = -1;
        checkNote (onft::resolveNote (61, c).outputNote, 60, "avoidNote off: C#61 -> C60 as usual");
    }

    // Whole-tone field, shift is even-spaced.
    {
        onft::FieldConfig wt;
        for (int pc : { 0, 2, 4, 6, 8, 10 })
            wt.pitchClassAllowed[static_cast<size_t> (pc)] = true;
        wt.foreignMode = onft::ForeignMode::Constrain;
        wt.scaleDegreeShift = 1;
        checkNote (onft::resolveNote (60, wt).outputNote, 62, "whole-tone shift +1: C60 -> D62");
        checkNote (onft::resolveNote (70, wt).outputNote, 72, "whole-tone shift +1: A#70 -> C72 (wrap)");
    }

    std::cout << "----------------------------\n";
    if (failures == 0)
    {
        std::cout << "[PASS] OrchNoteFilterFieldLogicCheck passed.\n";
        return 0;
    }

    std::cerr << "[FAIL] " << failures << " failure(s).\n";
    return 1;
}
