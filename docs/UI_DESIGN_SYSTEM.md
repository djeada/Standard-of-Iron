# The Iron and Ember Design System

Standard of Iron ships four front ends — the main menu, the in-battle HUD, the campaign
flow and the editor tools — and for a long time each one styled itself. This document
describes the shared QML design system introduced for issue #1082, what it guarantees,
and how to build a screen on top of it.

## What we'll cover

1. The aesthetic the tokens encode
2. How the module is packaged and imported
3. The token singletons and who owns which value
4. Accessibility: one preference, every surface
5. Faction identity as a skin, never a layout
6. The single notification queue
7. Iconography, and why emoji are banned
8. Reviewing a screen with a screenshot
9. The component gallery
10. How the system is tested
11. How the Qt Widgets tools stay in the same product

## The aesthetic: "Iron and Ember"

Dark, serious, ancient and military — heroic rather than evil. The material vocabulary is
weathered iron, charcoal, leather, parchment, stone, bronze, smoke and restrained
firelight. Motifs come from heraldry: shields, standards, laurels, waves, elephants,
fortifications and geometric military patterns.

Explicitly out of scope: demonic imagery, gore, skull decoration, excessive spikes, and
gothic typography that is hard to read at a glance during a battle.

Motion follows the same restraint. A transition exists to explain a state change; it never
decorates.

## Packaging: a file-based QML module at `:/StandardOfIron/Design`

The design system is a plain resource module rather than a `qt_add_qml_module` target:

```
design_resources.qrc   →   :/StandardOfIron/Design/...
ui/qml/design/qmldir   →   module StandardOfIron.Design
```

`main.cpp` already puts `qrc:/` on the engine's import path, so any screen reaches it with

```qml
import StandardOfIron.Design 1.0 as Design
```

Why not `qt_add_qml_module`? That generator republishes every file as a type of the
enclosing module, which would expose the QML `Theme` token singleton as a plain
`StandardOfIron.Theme` type — colliding with the C++ `Theme` singleton of the same name and
silently dropping the `pragma Singleton` declarations. Shipping the module as resources
keeps one authoritative `qmldir` and behaves identically on Qt 5 and Qt 6.

`ui/qml/design/qmldir` is the single manifest. Files live in `controls/`, `surfaces/`,
`overlays/` and `layouts/` for readability, but every type is published flat, so one import
gives a screen the whole library.

## Tokens

| Singleton       | Owns                                                                    |
| --------------- | ----------------------------------------------------------------------- |
| `A11y`          | User accessibility preferences; the root every other token derives from |
| `Theme`         | Colour, including the high-contrast and colour-vision variants          |
| `Metrics`       | Spacing, radius, border width, control and touch-target sizing          |
| `Typography`    | Font families, the type scale, weights and tracking                     |
| `Motion`        | Durations, easing curves and notification dwell times                   |
| `Icons`         | Every icon in the product: glyph marks and the painted art families     |
| `FactionTheme`  | Per-faction accent, emblem, heraldic glyph and motto                    |
| `Notifications` | The product-wide priority queue                                         |
| `UiSound`       | Hover / activate / warning cues                                         |

The base palette lives in `ui/theme.h` so the Qt Widgets tools (arena, map editor) and the
QML product cannot drift apart: `Theme.qml` reads `StandardOfIron.Theme` and layers the
accessibility variants on top of it.

Screens should reference tokens, never literals. `Metrics.space12`, not `12`.

## Accessibility

`UiPreferences` (`ui/preferences.h`) is the persistent store, backed by the same
`QSettings` file as graphics and audio. It exposes five preferences:

- `uiScale` — clamped to `[0.75, 2.0]`
- `reducedMotion`
- `highContrast`
- `colorVisionMode` — `none`, `protanopia`, `deuteranopia`, `tritanopia`
- `alwaysShowFocus`

`A11y` mirrors those into QML, and `Metrics`, `Typography`, `Motion` and `Theme` all derive
from it. That is the whole mechanism: a player moves one slider in Settings and the
campaign, the HUD and the gallery resize together, because none of them hold their own
copy of the value.

Two rules the components follow:

- **Hue is never the only signal.** Colour-vision modes repaint the status tokens, but a
  checkbox also draws a tick, a notification also draws a priority glyph, and a selected
  tab also draws an underline.
- **Touch targets have a floor.** `Metrics.minTouchTarget` never drops below 32px even at
  the smallest scale.

Reduced motion collapses durations to zero rather than removing bindings, so state still
lands in the right place. Notification dwell times are deliberately _not_ affected — that
is reading time, not motion.

## Faction identity

`FactionTheme` maps a nation id (`roman_republic`, `carthage`, `iron_sepulcher`) to an
accent, a deep accent, a heraldic glyph, an emblem source and a motto. Unknown ids fall
back to a neutral skin rather than failing.

Identity is a _skin_: the same panels, the same zones, the same control sizes. Only the
accent and heraldry change, so a player who has learned one faction's HUD already knows
the others.

`GameShell.faction` is the single place the active faction is published. `Main.qml` binds it
to `game.local_player_nation`, which resolves through `NationRegistry` for the local owner.

## Notifications

Every transient message goes through one priority-ordered queue so a "unit ready" toast can
never bury a "commander lost" banner.

```qml
Design.Notifications.critical(qsTr("Commander lost"), { "channel": "commander" })
Design.Notifications.info(text, { "channel": "mission-announcement" })
```

Priorities, highest first: `critical`, `urgent`, `info`, `ambient`. Within a band the order
is first-in-first-out.

`channel` groups repeats. Pushing the same channel again bumps a counter on the pending
entry instead of adding a second row, and a repeat may _escalate_ the priority but never
downgrade it. Entries marked `sticky` wait for an explicit dismissal; everything else times
out on `Motion.dwellFor(priority)`.

Screens push. They do not render toasts. A single `NotificationHost` per product shell
renders whatever is current — `Main.qml` mounts one in the top-right of the battle view.

## Iconography

`Icons` is the only place an icon is named. It holds two families that are not
interchangeable:

- **glyphs** — text marks for world-space feedback, disclosure and inline
  affordances, plus a fallback for every command and unit
- **artwork** — the painted PNG families, resolved by `command()`, `resource()`,
  `status()` and `unit(type, nation)`

`unit()` follows a fixed fallback chain: shared art → nation-agnostic art → the
nation's family → the default family. It never returns an empty source for a
type it knows, so a roster row cannot render a hole.

### Vector command and activity icons

The order bar and the activity readouts are drawn from vector outlines, not from
bitmaps. The originals were 30×30 PNGs: soft at 1× and mush on a high-DPI panel,
and impossible to retint for a disabled or interrupted state.

The geometry lives in one place, `ui/icon_art.cpp`, as paths over a 24×24 design
grid, and is published three ways:

- `Core.IconArt` (a C++ singleton in `StandardOfIron.Core`) hands QML flattened,
  0..1-normalised polylines, which `IronVectorIcon` strokes onto a `Canvas`
- `Ui::IconArt::paint()` draws the same shapes with `QPainter`, which is how the
  arena viewport shows activity badges over the battlefield
- `IronCommandButton` prefers the vector drawing and only falls back to the
  legacy bitmap, then to a font glyph, when a drawing does not exist

Every shape names a _tone_ rather than a colour — `ink`, `metal`, `edge`, `ember`
plus the material tones `timber`, `stone`, `iron` and `gold`. That is what lets
one drawing serve an enabled button, a disabled one, and a small monochrome mark
over a unit, and it is why the gathering icons can identify the resource they
yield without a second set of artwork.

`Design.ActivityIcons` is the semantic half: activity id → drawing, label,
tooltip and resource. It is the only place a player-facing activity string is
written, and `IronActivityIcon` is the only control that renders one.

Two rules are enforced by tests rather than by review:

- **No emoji.** The shipped font has no glyph for them, so they render as empty
  boxes. `tst_glyph_coverage.qml` probes every registered mark against the real
  font metrics — nothing else can catch a tofu glyph, because it is invisible to
  layout and to logic.
- **The icon directory ships whole.** CMake globs `assets/visuals/icons/*.png`
  instead of listing them, and `icon_resources_test.cpp` checks the registry and
  the directory agree in both directions: no name that cannot resolve, no file
  that nothing can request.
- **A vector icon still reads when it is small.** `icon_art_test.cpp` rasterises
  every drawing at 16 px and 96 px and asserts the painted fraction of the tile
  stays in a sane band and barely changes between the two. An icon that vanishes,
  fills the tile, or is clipped by its own bounds fails the build.

## Reviewing a screen

```
standard_of_iron --screenshot shot.png --screenshot-view hud
map_editor --screenshot editor.png
```

`--screenshot-view` takes `menu`, `skirmish`, `campaign`, `settings`, `load`,
`save`, `briefing` or `hud`, and works with `--component-gallery` too. The
capture renders one frame at 1600×900 and exits; modals are suppressed so a
startup error can never sit on top of the surface being reviewed.

This is how the defects that no test covered were found: emoji rendering as
boxes, an order bar clipped by its zone, a slider collapsing to zero height, a
skirmish panel rendered indigo by an `#RRGGBBAA` literal that Qt reads as
`#AARRGGBB`, and Qt's platform-styled dialog chrome showing as a white bar.

## The component gallery

```
standard_of_iron --component-gallery
```

opens `GalleryWindow.qml`: every published control, the faction skins and the notification
priorities, with live scale, reduced-motion, high-contrast and colour-vision controls in the
toolbar. It is living documentation — if a screen needs something that is not on this page,
the component belongs in the library first.

## Testing

`tests/ui/qml/` holds QtQuickTest cases that run against the real shipped module:

- `tst_design_tokens.qml` — scale monotonicity, UI scale propagation and clamping, touch
  target floor, reduced motion, high contrast, colour-vision repainting
- `tst_component_library.qml` — every published type instantiates; accessible names;
  focus-ring behaviour; scale response
- `tst_notifications.qml` — priority ordering, FIFO within a band, channel collapsing and
  escalation, dismissal, host rendering
- `tst_faction_theme.qml` — every shipped faction has a distinct identity, unknown ids fall
  back safely
- `tst_iconography.qml` — every command, resource, status and unit resolves for
  every shipped nation, and unknown keys degrade instead of breaking
- `tst_glyph_coverage.qml` — every glyph exists in the shipped font
- `tst_command_button.qml` — availability, partial coverage, armed and active
  states stay distinguishable
- `tst_activity_icons.qml` — every activity has a drawing, a label and a tooltip;
  gathering activities name their resource; the four order states stay distinct
  by shape as well as by colour; unknown ids degrade to idle
- `tst_selection_summary.qml` — the presentation switches at the right army size
- `tst_campaign_flow.qml` — objective states, briefing sections, outcome kinds

They run headless via `ctest -R design_system_qml`, and from `make test`.

On the C++ side: `preferences_test.cpp` covers the persistence layer including
clamping and corrupted values, `selection_grouping_test.cpp` covers the HUD
roster, `icon_resources_test.cpp` keeps the icon registry and the shipped files
in step, `icon_art_test.cpp` holds the vector catalogue to its legibility
contract, `activity_markers_test.cpp` covers which units earn an overhead marker
and how crowded ones are merged, and `widget_theme_test.cpp` pins the widget
vocabulary the tools use and proves the shared accessibility settings reach
them.

## The Qt Widgets tools

The arena and the map editor are Widgets applications, not QML, so they cannot
use the component library directly. They are unified through `ui/theme.h` and
`ui/widget_shell.h` instead:

- `Theme::widgetStyleSheet()` covers the whole widget vocabulary the tools use
  and derives from the same tokens as the QML side.
- It reads `UiPreferences`, so the UI scale and high-contrast settings a player
  picks in the game apply to the tools, and `UiShell::apply` keeps a running tool
  in sync without a restart.
- `UiShell::prepare_tool_window` gives both windows the same menu bar / workspace
  / status bar structure, and drops dock animation under reduced motion.

Anything a tool paints itself — the editor canvas, for instance — takes its
chrome colours from `Theme` too. Only content colours are authored locally.

## Adding a component

1. Add the `.qml` under `ui/qml/design/controls/` (or the matching folder).
2. Declare it in `ui/qml/design/qmldir` and `design_resources.qrc` — both, or it will not
   ship.
3. Reference tokens only; no literal colours, sizes or durations.
4. Set `Accessible.name`, and `Accessible.description` when a disabled state needs an
   explanation.
5. Add it to `ComponentGallery.qml` and to the instantiation table in
   `tst_component_library.qml`.
