# Accessibility and Input Customization

Standard of Iron exposes accessibility and input options from **Settings** in both the main menu and pause menu. Every option has a default that reproduces the shipped presentation and controls, so accessibility features can be enabled deliberately without changing the experience for a fresh installation.

Settings persist through `App::Core::UserSettings` in the same INI file as the rest of the application, under `ui/` and `input/bindings`. They apply consistently across campaign, skirmish, and editor tools.

This page explains the architecture behind those preferences and the guarantees each option is designed to provide.

## Architecture

| Concern                                | Owner                                         |
| -------------------------------------- | --------------------------------------------- |
| Persisted values, validation, defaults | `app/core/user_settings.h`                    |
| Preference objects exposed to QML      | `ui/preferences.{h,cpp}` (`UiPreferences`)    |
| Key and mouse bindings                 | `ui/input_bindings.{h,cpp}` (`InputBindings`) |
| Team palettes and ring patterns        | `game/accessibility/team_identity.{h,cpp}`    |
| Camera motion scale                    | `game/accessibility/motion_settings.{h,cpp}`  |
| QML-side read model                    | `ui/qml/design/A11y.qml`                      |
| Rebinding UI                           | `ui/qml/ControlsBindingList.qml`              |

`game/accessibility/` is a dependency-free leaf library built as `accessibility_runtime`. Simulation and rendering code read accessibility state from that library, while UI preferences write to it.

This boundary allows a renderer to respond immediately to a preference change without depending on settings-file code or QML. It also keeps the accessibility rules available to non-UI systems without creating a dependency cycle.

## Input customization

### Rebinding commands

Every gameplay command in the input catalog can be rebound to a keyboard key or mouse button, with or without modifiers.

In **Settings → Controls**:

- select a command and press the desired chord;
- press `Backspace` to unbind it; or
- press `Esc` to cancel capture.

Bare modifiers such as `Shift` and `Alt` are valid bindings and are captured on key release. Other keys are captured on key press.

Bindings are stored as portable text such as `Ctrl+Shift+S`, `Mouse Right`, or `Up`. This keeps the settings file readable and avoids tying persisted preferences to Qt's internal key-code representation.

Only overrides are written. Commands that the player never changes continue to follow their catalog defaults, which means a future default change can reach players who have not explicitly customized that command.

### Binding contexts and conflicts

Every command belongs to a context: `rts`, `commander`, or `global`.

Two commands conflict only when their contexts overlap. This lets the commander use `Space` for Dodge while the army view uses `Space` for Pause. A `global` command overlaps every context.

When a new binding would collide with an existing command, the rebinding screen identifies the conflict before changing anything. If the player confirms the reassignment, the previous holder is unbound rather than leaving two commands competing for the same chord. Any conflict that remains is shown on the affected row.

Each command supports up to **two active chords**, a primary and an alternate. This is how camera panning can support both arrow keys and `WASD` without treating either as a secondary compatibility path.

Conflict checking covers both slots, including attempts to assign the same chord to both slots of one command. **Default** restores the original pair together.

### Contextual commands

One overlap is intentional. `rts.commander_rally` is a **contextual** command: it claims `R` only while a rally point can actually be placed. Outside that state, another command bound to `R` may receive the key normally.

Contextual commands are excluded from ordinary conflict reporting because their layered behavior is deliberate. Nothing else ships on `R` by default—the camera pitch commands use `Ctrl+Up` and `Ctrl+Down`—but players remain free to create their own layered binding.

### How a chord is resolved

Exact modifier matches take priority. `Ctrl+S` can therefore be bound independently from `S`.

If no exact match exists, an unmodified binding may still match while modifiers are held. This preserves common modifier-as-qualifier behavior—for example, holding `Shift` to speed up a camera pan without preventing the underlying pan command from being recognized.

Commander locomotion commands such as forward, back, strafe, turn, and sprint are held states. Rebinding changes which physical key activates the command, while the controller continues to receive the command's canonical key code through `InputBindings::canonical_key_for`.

### Command catalog

`InputBindings::catalog()` is the single source of truth for bindable gameplay commands. The settings UI groups them into:

- **System** — open menu, switch between army and commander, quick save, quick load, pause;
- **Camera** — four pan directions, two rotations, two tilt directions, two zoom directions, reset, first/third-person toggle;
- **Selection** — select a unit or drag a selection box, select all troops;
- **Orders** — move, attack-move, stop, attack, patrol, guard, hold position, commander rally placement;
- **Commander movement** — forward, back, two strafes, two turns, sprint, dodge, jump; and
- **Commander combat** — attack, block, cycle locked target, special action, vanguard rush, second wind, commanding aura, rally nearby troops.

## Visual accessibility

### Interface scale

`UiPreferences.uiScale` ranges from 75% to 200%.

Both UI metric systems respond to it: the design-system tokens in `Design.Metrics` and `Design.Typography`, together with the older `Theme` spacing values. The settings panel itself is sized in scaled pixels and is limited to 90% of the window, ensuring that the control used to change scale remains reachable at every supported value.

All product font sizes come from `Design.Typography`, so the scale applies consistently to campaign screens, menus, and the in-match HUD.

`scripts/check-typography.py` rejects hard-coded font sizes that bypass the token system. `tests/ui/qml/tst_gameplay_typography.qml` measures the production gameplay panel at 75%, 100%, and 200% to verify that the setting reaches the actual in-match UI.

The smallest typography token is clamped by `Design.Typography.minimumSize` at 12 px. Costs, counters, and objective text therefore remain legible at the minimum interface scale rather than shrinking without limit. `Design.Metrics.minTouchTarget` provides the corresponding floor for pointer targets.

### Team identity without relying on hue

Team identity is carried through two independent visual channels: **palette** and **ring pattern**.

#### Palette

Selecting a colour-vision mode replaces the team palette with one designed to remain separable under that deficiency.

Protanopia and deuteranopia share a palette that avoids green and emphasizes a blue-to-orange axis. Tritanopia uses a palette designed around a different separation. Both distribute relative luminance across the four main team slots so teams remain ordered even when hue information is reduced.

`tests/ui/team_identity_test.cpp` enforces a minimum luminance gap of `0.12` between every pair.

The standard mode retains the game's default palette.

Only colours assigned automatically follow accessibility palette changes. If a map or lobby explicitly assigns a colour, that colour is treated as part of the authored player identity and is left unchanged.

Because the palette is resolved on read, changing colour-vision mode recolours an active match immediately rather than only affecting the next one.

#### Selection-ring patterns

Every team slot also receives a distinct selection-ring pattern: solid, dashed, double ring, notched, dotted, or chevron.

Teams beyond the fourth may reuse a colour but do not reuse the same shape. Patterns are enabled automatically whenever a colour-vision mode is active and can also be enabled independently through **Team ring patterns**.

The result is that team identity never depends on colour alone.

### Order state without relying on hue

`IronActivityIcon` communicates both the activity a unit is performing and the state of that order in the selection panel and over the unit.

The four order states—active, queued, unavailable, and interrupted—use three redundant channels:

- **Shape:** queued orders add a filled corner chevron, interrupted orders add pause bars, unavailable orders add a cross, and active orders have no corner mark.
- **Weight:** non-active states use a thicker marker border, preserving the distinction in greyscale.
- **Words:** every marker exposes an `Accessible.name` and `Accessible.description` containing the activity, state, group headcount when relevant, and a short explanation of the state. The same wording is used for mouse tooltips.

Gathering icons also combine their material tone with explicit labels such as “Cutting timber”, “Quarrying stone”, and “Mining iron”. Resource identity is therefore not encoded by colour alone.

### Contrast and focus

Two additional preferences improve UI readability and keyboard navigation:

- **High contrast** increases separation between panels and text.
- **Always show keyboard focus** keeps focus outlines visible even after pointer interaction.

## Motion and screen effects

### Reduce motion

**Reduce motion** disables transitions and idle animations across the UI.

### Camera motion

**Camera motion**, from 0% to 100%, scales movement the camera introduces automatically while following the commander, including head bob, breathing, and strafe lean.

It never scales movement explicitly requested by the player, so changing the preference does not reduce camera control authority.

### Screen effects

**Screen effects**, from 0% to 100%, scales full-screen overlays such as the damage vignette, low-health pulse, and guard glow. At 0%, those effects are removed rather than merely dimmed.

### Damage numbers

**Damage numbers** toggles floating combat feedback for damage taken or dealt and health restored by healers, commander auras, or repair crews.

When hidden, the presentation layer continues draining the engine's event queue. Turning the display off therefore does not allow events to accumulate in the background.

## Camera accessibility

### Edge scrolling

**Edge scrolling** controls whether moving the pointer to a screen edge pans the camera. Hover tracking remains active when the option is off; disabling edge scrolling changes camera behavior, not pointer targeting.

### Edge-scroll speed

**Edge scroll speed**, from 25% to 200%, scales both pan speed and the width of the edge trigger band. A lower setting therefore makes the camera move more slowly and reduces accidental panning from a pointer resting near the edge.

## Test coverage

| File                                           | Covers                                                                                   |
| ---------------------------------------------- | ---------------------------------------------------------------------------------------- |
| `tests/ui/input_bindings_test.cpp`             | catalog coverage, chord round-tripping, conflict detection, context scoping, persistence |
| `tests/ui/team_identity_test.cpp`              | palette distinctness, luminance separation, pattern assignment                           |
| `tests/ui/preferences_test.cpp`                | defaults, clamping, corrupt-value fallback, propagation to accessibility runtime          |
| `tests/render/selection_ring_pattern_test.cpp` | one drawable, upward-facing, visibly distinct mesh per pattern                           |

The accessibility architecture is designed around redundancy and shared sources of truth: controls remain customizable without ambiguous conflicts, important visual states use more than colour, motion can be reduced without weakening control, and UI scaling applies to the same tokens the production interface already uses.
