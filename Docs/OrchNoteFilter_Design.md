# OrchNoteFilter — Design (Phase 1A)

Date: 2026-08-28
Status: building.
Repo: `C:\AudioDev\Repos\OrchNoteFilter`. Plugin code `Onft`, VST3, MIDI effect.

---

## 1. Purpose

Per-instrument **pitch-class field** filter. Sits in each orchestral track's chain and constrains the
(often stochastically scattered) note stream to an allowed set of pitch classes — any scale, any
exotic mode, or an arbitrary atonal set. The harmonic-identity half of the Orch family, distinct
from OrchNoteMapper (instrument-range correctness) and OrchGate (participation).

`ORCH_SYSTEM_ROADMAP.md §6` (OrchNoteFilter) and §7 (OrchFieldConductor). Inspired by the old
Notomizor VST2 and by Bitwig's Key Filter+, but: **arbitrary pitch-class sets**, **CC-native field
control** (so the narrative arc can drive harmony the way it now drives orchestration), and
family-consistent keyswitch / note-off handling.

## 2. Chain position

```
Note source (MPL / clip) → [Randomize] → OrchNoteFilter → OrchNoteMapper → OrchGate → instrument
```

Filter pitch class first (octave-invariant), then range-map. So OrchNoteMapper's fold/clamp never
pushes a note back out of the field.

## 3. Foreign-note handling (from Key Filter+, adopted wholesale)

For an incoming note, pitch class `p`, tested against the field (rotated by Field Root):

| Mode | in-field notes | out-of-field notes |
|---|---|---|
| **Filter** | pass (+ shift) | **drop** |
| **Keep** | pass (+ shift) | pass **unchanged** |
| **Constrain** | pass (+ shift) | **snap to nearest in-field** (+ shift) |
| **Solo** | **drop** | pass **unchanged** |

- **Filter** thins the stream where a note lands out of field — and because upstream Randomize is
  independent per track, *where* the holes fall is different per instrument every bar. Free
  emergent rhythmic counterpoint. Field width becomes a density lever.
- **Constrain** keeps density constant, everything locked to the field. The dense-texture default.
- **Solo** plays the field's *complement* — set some instruments to Solo for harmonic divergence
  against the ensemble (the roadmap's "Complement" idea, for free).
- **Keep** lets the raw out-of-field notes poke through as color / tension.

**Constrain Direction**: Nearest / Up / Down — which in-field pitch a foreign note snaps to.

## 4. Scale-Degree Shift

`±N` (Phase 1A range: −12..+12). A *diatonic* transpose: moves in-field and Constrain-corrected
notes by N steps through the ordered field list, wrapping with octave add/subtract. Does **not**
apply to Keep's or Solo's raw foreign notes (undefined for non-members — matches Key Filter+).

Pairs with OrchNoteMapper: OrchNoteMapper walks the register window chromatically, OrchNoteFilter
shifts within the field diatonically. Both automatable, complementary melodic controls.

## 5. Field

- **12 pitch-class toggles** — the authored field, as if Root = C.
- **Field Root** 0..11 — rotates the field. A note pc `p` is in-field iff `toggle[((p - root) mod 12)]`.
- **Field Preset** (choice) — selecting one writes the 12 toggles (chromatic, the 7 modes,
  whole-tone, octatonic H-W, octatonic W-H, major/minor pentatonic, and two atonal cells:
  all-interval tetrachord `{0,1,4,6}` and `{0,1,6,7}`). Editing a toggle by hand sets it to
  "Custom". Same pattern as OrchNoteMapper's instrument presets.
- If the field ends up empty (no toggles on), the filter passes everything through unchanged
  regardless of mode — a safety, not a musical state.

## 6. Probability

`0..100`. Per note-on, roll `random < probability`: hit → the note goes through the field logic;
miss → it passes raw. 0 = filter inert, 100 = every note processed. Lets you dial between a strict
field and a loose chromatic haze.

## 7. Keyswitch passthrough

`Pass Keyswitches` on/off, `KS Min` / `KS Max` (default 0..35). A note in the KS range bypasses the
filter entirely (no membership test, no shift). Same contract as OrchGate / OrchNoteMapper, so the
hand-drawn C-1 keyswitch note survives.

## 8. Note-off safety

`activeNoteMap[16][128]`, values: `-1` untracked, `-2` note-on was dropped, `>=0` remembered output
note. Note-on records what it emitted; note-off emits the remembered note (or is consumed if the
on was dropped, or passes unchanged if untracked). Reset on `prepareToPlay` and all-notes-off.
Mirrors OrchNoteMapper.

## 9. CC control

Undefined-CC block, clear of MPL (20-64), OrchConductor output (20-54), and the MC bridge (102-104):

| CC | Target | Mapping |
|---|---|---|
| `105` | Field Preset | value → preset index (clamped) |
| `106` | Field Root | value → 0..11 |
| `107` | Scale-Degree Shift | value → −12..+12 (64 = 0) |
| `108` | Foreign-Note Mode | banded → Filter / Keep / Constrain / Solo |
| `109` | Probability | value → 0..100 |

**CC Channel** parameter: `0` = any channel, `1..16` = that channel only. Built in from the start —
OrchGate's channel-agnostic CC listening caused a real collision with MPL's Rate CC; OrchNoteFilter
should be able to isolate its control channel per template.

Incoming CC on a listened number is applied and **passed through** (control-path CCs are harmless
downstream; this plugin is not a control-path terminus like OrchConductor).

## 10. Parameters (APVTS-free, raw juce::AudioParameter* like OrchGate)

`enable` (bool) · `fieldPreset` (choice) · `pc0..pc11` (12 bool) · `fieldRoot` (int 0..11) ·
`foreignMode` (choice: Filter/Keep/Constrain/Solo) · `constrainDirection` (choice: Nearest/Up/Down) ·
`scaleDegreeShift` (int −12..12) · `probability` (float 0..100) · `passKeyswitches` (bool) ·
`ksMin` (int 0..127) · `ksMax` (int 0..127) · `ccChannel` (int 0..16) ·
`ccFieldPreset` / `ccRoot` / `ccShift` / `ccMode` / `ccProbability` (int 0..127, 0 = off/unassigned —
actually default to the §9 numbers, with 0 meaning "ignore").

State: XML via `copyXmlToBinary`, same as OrchGate.

## 11. Out of scope for Phase 1A

Field morphing (probability-weighted A→B transition), OrchFieldConductor (the global companion),
per-note-history "avoid repeats", chord-tone weighting, microtonal / non-12-EDO. All noted in the
roadmap; none block Phase 1A.

## 12. Endgame

Once the field is CC-selectable: OrchConductor's narrative lane points gain a `pitchField`;
OrchConductor emits CC105 alongside CC20-54, driven by the same `Narrative Position`. One automation
lane evolves orchestration + density + harmonic field together. Later still: MC emits the pitch
classes its MotifEngine is actually writing, so the Randomize wash tracks the structural voices
automatically.
