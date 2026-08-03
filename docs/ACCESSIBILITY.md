# Accessibility and input customization

Everything here is reachable from **Settings** in the pause menu and the main
menu. Every option has a value that reproduces the game exactly as it ships, and
that value is the default, so a fresh install looks and plays as it always has.

Settings persist to the same INI the rest of the game uses
(`App::Core::UserSettings`), under `ui/` and `input/bindings`. They apply to the
campaign, skirmish, and the editor tools alike.

## Where the pieces live

| Concern                                | Owner                                         |
| -------------------------------------- | --------------------------------------------- |
| Persisted values, validation, defaults | `app/core/user_settings.h`                    |
| Preference objects exposed to QML      | `ui/preferences.{h,cpp}` (`UiPreferences`)    |
| Key and mouse bindings                 | `ui/input_bindings.{h,cpp}` (`InputBindings`) |
| Team palettes and ring patterns        | `game/accessibility/team_identity.{h,cpp}`    |
| Camera motion scale                    | `game/accessibility/motion_settings.{h,cpp}`  |
| QML-side read model                    | `ui/qml/design/A11y.qml`                      |
| Rebinding UI                           | `ui/qml/ControlsBindingList.qml`              |

`game/accessibility/` is a dependency-free leaf library
(`accessibility_runtime`). The simulation and the renderer read from it; the UI
preferences write to it. Neither end depends on the other, which is what lets a
preference reach a renderer that has no way to open a settings file.

## Input customization

### Rebinding

Every gameplay command in the catalog is rebindable to a key or a mouse button,
with or without modifiers. Select a command in **Settings → Controls**, then
press the chord. `Backspace` unbinds the command, `Esc` cancels the capture.
Bare modifiers (`Shift`, `Alt`) are legitimate bindings and are captured on
release; every other key is captured on press.

Chords are stored as portable text — `Ctrl+Shift+S`, `Mouse Right`, `Up` — so
the settings file stays readable and survives Qt key-code changes. Only
overrides are written; a command left alone follows its catalog default, which
is what lets a changed default reach players who never touched that command.

### Conflicts

A command belongs to a **context**: `rts`, `commander`, or `global`. Two
commands only conflict when their contexts overlap, which is why the commander
can keep `Space` for Dodge while the army view keeps it for Pause. `global`
overlaps everything.

Assigning a chord another command already holds is refused, and the rebinding
screen names the command that would be broken before anything is written.
Confirming the reassignment unbinds the previous holder rather than leaving two
commands fighting over one key. Any conflict that still exists is flagged
against the affected row.

One overlap is deliberate. `rts.commander_rally` is a **contextual** command: it
shares `R` with `rts.camera_orbit_left`, claims the key only while a rally can
actually be placed, and otherwise lets the camera keep it. Contextual commands
are excluded from conflict reporting because the layering is the intended
behaviour, not a collision.

### Resolution rules

When a key arrives, an exact modifier match wins — so `Ctrl+S` can be bound
separately from `S`. Failing that, an unmodified binding matches even while
modifiers are held, which is what keeps `Shift` working as the "move faster"
qualifier on camera pans instead of breaking them.

Commander locomotion (`W`/`A`/`S`/`D`, turn, sprint) is a held state. Rebinding
changes which physical key the player presses; the controller keeps receiving
the key code the command has always meant, via
`InputBindings::canonical_key_for`.

### Command catalog

`InputBindings::catalog()` is the single source of truth. Categories as shown in
the UI:

- **System** — open menu, switch between army and commander, quick save, quick
  load, pause.
- **Camera** — pan (4), rotate (2), orbit (2), toggle first/third person.
- **Selection** — select unit or drag a box, select all troops.
- **Orders** — move or attack-move to cursor, stop, attack, move, patrol, guard,
  hold position, place commander rally flag.
- **Commander movement** — forward, back, strafe (2), turn (2), sprint, dodge,
  jump.
- **Commander combat** — attack, block, cycle locked target, special action,
  vanguard rush, second wind, commanding aura, rally nearby troops.

## Visual accessibility

### Interface scale

`UiPreferences.uiScale` runs from 75% to 200%. Both metric systems follow it:
the design-system tokens (`Design.Metrics`, `Design.Typography`) and the older
`Theme` spacing and font sizes, which are no longer constant. The settings panel
itself is sized in scaled pixels and takes up to 90% of the window, so the
screen where the scale is changed stays reachable at every step.

### Team identity without hue

Two independent channels carry team identity, so neither has to work alone.

**Palette.** Selecting a colour vision mode swaps the team palette for one
chosen to stay readable under that deficiency. Protanopia and deuteranopia share
a palette that drops green entirely and leans on the blue-to-orange axis;
tritanopia gets one built the other way round. Both spread relative luminance
across the four team slots so the sides stay in a readable order even with hue
removed completely — `tests/ui/team_identity_test.cpp` asserts a minimum
luminance gap of 0.12 between every pair.

The standard palette keeps the colours the game has always used.

**Pattern.** Each team slot also gets its own selection-ring shape: solid,
dashed, double ring, notched, dotted, chevron. Teams beyond the fourth wrap onto
a repeated colour but never onto a repeated shape. Ring patterns turn on
automatically whenever a colour vision mode is selected, and can be turned on
independently via **Team ring patterns**.

Only owners whose colour was assigned automatically follow the palette. A colour
a map or a lobby chose explicitly is that owner's identity and is left alone.
Because the lookup happens on read, changing the mode recolours a running match
rather than only the next one.

### Order state without hue

The activity a unit is on, and how that order is going, are reported by
`IronActivityIcon` in the selection panel and over the unit itself. The four
order states — active, queued, unavailable, interrupted — never rely on colour
alone:

- **Shape.** A queued order carries a filled chevron in the corner of the
  medallion, an interrupted one a pair of paused bars, an unavailable one a
  cross. An active order carries no corner mark at all.
- **Weight.** Anything other than an active order also thickens the medallion
  border, so the difference survives a greyscale screenshot.
- **Words.** Every marker exposes `Accessible.name` (the activity and its state)
  and `Accessible.description` (the same, plus the headcount when the marker
  stands for a group, plus what the state means). The same text is the tooltip,
  so a mouse user and a screen-reader user get the same sentence.

Gathering icons identify their resource by both the material tone of the drawing
and the label — "Cutting timber", "Quarrying stone", "Mining iron" — rather than
by tone alone.

### Contrast and focus

- **High contrast** raises panel and text contrast.
- **Always show keyboard focus** keeps the focus outline visible after clicking.

## Motion and effects

- **Reduce motion** removes transitions and idle animations across every screen.
- **Camera motion** (0–100%) scales the movement the camera adds on its own —
  head bob, breathing, strafe lean — while leading the commander. It never
  limits movement the player asked for, so control is unaffected.
- **Screen effects** (0–100%) scales the full-screen tints: the damage vignette,
  the low-health pulse, the guard glow. At zero they are gone rather than dimmed.
- **Damage numbers** toggles the floating damage readout. The layer keeps
  draining the engine's event queue while hidden, so turning it off costs
  nothing and never lets a backlog build.

## Camera control

- **Edge scrolling** toggles camera panning from the screen edge. Hover tracking
  stays live with it off: the option is about the camera moving on its own, not
  about the cursor losing what it is pointing at.
- **Edge scroll speed** (25–200%) scales both the pan speed and the width of the
  trigger band, so a lower setting also means the camera will not creep from a
  cursor parked near the edge.

## Tests

| File                                           | Covers                                                                                   |
| ---------------------------------------------- | ---------------------------------------------------------------------------------------- |
| `tests/ui/input_bindings_test.cpp`             | catalog coverage, chord round-tripping, conflict detection, context scoping, persistence |
| `tests/ui/team_identity_test.cpp`              | palette distinctness, luminance separation, pattern assignment                           |
| `tests/ui/preferences_test.cpp`                | defaults, clamping, corrupt-value fallback, propagation to the accessibility runtime     |
| `tests/render/selection_ring_pattern_test.cpp` | one drawable, upward-facing, visibly distinct mesh per pattern                           |
