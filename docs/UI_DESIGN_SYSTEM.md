# The Iron and Ember Design System

Standard of Iron ships four front ends — the main menu, the in-battle HUD, the campaign
flow and the editor tools — and for a long time each one styled itself. This document
describes the shared QML design system introduced for issue #1082, what it guarantees,
and how to build a screen on top of it.

## What we'll cover

1. The aesthetic the tokens encode
2. How the module is packaged and imported
3. The token singletons and who owns which value
4. The type ladder and its legibility floor
5. Accessibility: one preference, every surface
6. Faction identity as a skin, never a layout
7. The single notification queue
8. Iconography, and why emoji are banned
9. Reviewing a screen with a screenshot
10. The component gallery
11. How the system is tested
12. How the Qt Widgets tools stay in the same product

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

## Type

One ladder, in pixels, for every screen in the product:

| Rung         | px at scale 1.0 | Used for                                     |
| ------------ | --------------- | -------------------------------------------- |
| `caption`    | 13              | Costs, counters, queue indices, hotkey chips |
| `label`      | 15              | Field labels, button text, list rows         |
| `body`       | 16              | Running prose, tooltips, descriptions        |
| `bodyLarge`  | 19              | Emphasised body, panel intros                |
| `subheading` | 21              | Section headings inside a panel              |
| `heading`    | 24              | Panel titles                                 |
| `title`      | 32              | Screen titles                                |
| `hero`       | 40              | Menu and outcome display text                |
| `glyphSmall` | 28              | Icon glyphs standing in for missing art      |
| `glyph`      | 44              | Unit card glyphs                             |
| `glyphLarge` | 56              | Large recruit card glyphs                    |

Rules, in order of how often they are broken:

- **Always `font.pixelSize`, never `font.pointSize`.** Points resolve against the screen's
  reported DPI, so a point-sized label and a pixel-sized token drift apart on the same
  panel. The ladder used to exist twice — pixels here, points on `Theme` — and the point
  copy rendered a third larger, which is how the gameplay HUD ended up smaller than the
  menus around it. `Theme` no longer carries font sizes.
- **`caption` through `heading` carry a legibility floor** (`Typography.minimumSize`, 12px).
  A player who drops the interface scale to 0.75 still gets readable costs and counters;
  the bottom rungs converge on the floor rather than shrinking past it, the same way
  `Metrics.minTouchTarget` refuses to go under 32px.
- **For a size that is genuinely computed** — a damage number that ramps with severity, a
  menu title that halves on a narrow window — use `Typography.scaled(px)` (floored) or
  `Typography.display(px)` (unfloored). Both follow the interface scale. A bare number does
  not, and `scripts/check-typography.py` rejects it.
- **A `Control` is not the thing that draws its label.** `Button.font` is inherited default
  state; the `contentItem: Text` under it is what renders, and its own `font.pixelSize`
  survives. Set the size there, and measure there — a test that reads `button.font` is
  reading a value no player ever sees.
- **If the text scales, the box around it has to scale too.** A label on the ladder inside a
  `height: 16` pill is legible at 1.0 and spills at 2.0. Size those containers from the
  content (`implicitHeight + padding`) or from `A11y.scaled()`.

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

A third rule applies to anything that explains itself:

- **An explanation a mouse can reach, a keyboard can reach.** `IronCommandButton` takes
  `Qt.TabFocus` and opens its `IronCommandTooltip` on `hovered || showFocusRing`, so hover,
  Tab and controller navigation all surface the same panel. An order that cannot be given
  keeps its tooltip: the refusal is printed under the rules rather than instead of them.

## Explaining an order

`IronCommandTooltip` is the panel behind every button in the order grid. It is not a
one-line hint: it carries the order's name and hotkey, a sentence of summary, a list of
term/text rules — scope, how to give it, how to cancel it, which troops may obey — a live
`status` line, and a `warning` line when the selection cannot take the order.

The numbers in those rules are not written by hand. `App::Core::get_action_states()` puts a
`detail` map on each action (guard radius, hold's range and damage bonuses, the patrol
waypoint the next click sets, the selected commander's aura radius, duration and cooldown),
and `HUDBottom` formats that map into the rules. A balance change moves the tooltip with
it; there is no second copy of the truth to forget.

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

## Attack mode targeting feedback

Picking a target used to be a cursor change and nothing else: the player could not tell
which of the shapes on screen the click would actually accept. Attack mode now layers
three signals on top of the cursor, and all three are gated on the player having
_asked_ for attack mode — `CursorMode::Attack`, which only the HUD `Attack` command (and
its `A` hotkey) sets. Ordinary hovering, and the right-click attack that works from the
normal cursor, draw nothing new. The command panel already names the mode in its banner
("Attack order"), so the mode is legible in both places.

- **Every eligible target carries a quiet hostile ring** — red, `TeamPattern::Chevron`,
  low alpha. Only entities the order would accept get one: enemy units and enemy
  buildings, never your own troops, never an ally, never wildlife, never a corpse.
- **The hovered target's ring turns loud** — brighter, `DoubleRing`, pulsing, with the
  attack glyph floating above it.
- **An invalid hover is answered, not ignored** — a muted grey `Dashed` ring plus the
  blocked glyph on the shape under the cursor, and a label beside the cursor that says
  why: _Cannot attack ally_, _Cannot attack this target_, _Selection cannot attack_.

The ring pattern and the floating glyph are the shape channel: the hostile, hovered and
blocked states differ in outline shape and iconography before they differ in hue, so the
feedback survives every colour-vision mode.

`Game::Systems::collect_attack_target_highlights` (`game/systems/attack_targeting.h`) is
the whole rule set, and it is a pure function over the world so the eligibility matrix is
unit-tested rather than eyeballed. Fog is part of that matrix: a target the player cannot
currently see is not highlighted, which is also why the set is rebuilt every frame instead
of cached on entry to attack mode — visibility, health and position all move underneath it.
Two bounds keep a large battle cheap: markers are collected within
`k_attack_highlight_max_distance` of the camera's ground focus and capped at
`k_attack_highlight_max_markers`, nearest first, with the hovered target always kept.

The collection runs in `GameEngine::update`, which the GL view drives from the render
thread, so the markers reach `render_attack_target_markers` on the thread that owns them.
The cursor label is the exception: it is a QML-facing property on `ActivityViewModel`, so
it is posted to that object's thread with a queued call and only when the text changes.

## Projectile range rings

Selecting a ranged unit draws its projectile reach on the ground. The ring is
built from segments sampled on the terrain surface, so it climbs a hill and
disappears behind one instead of slicing through it, and it is drawn with the
depth buffer like any other world geometry.

- **The radius is the number combat fires with**, not a second copy of it.
  `Game::Systems::resolve_attack_range` reads the unit's `AttackComponent` and
  applies `hold_mode_range_multiplier` — the same function
  `apply_hold_mode_bonuses` calls inside the attack processor — so an archer that
  goes to Hold grows its ring in the tick its reach actually grows. Anything that
  writes the component (an upgrade, an aura, a scenario override) moves the ring
  with it, because the ring is recomputed every frame from the component.
- **Weapon classes differ in line pattern, not hue**: bows dashed, siege ticked,
  arcane dotted. A weapon with a minimum firing distance also draws an inner
  dashed ring in the warning colour, marking the dead zone it cannot shoot into.
- **The focused unit gets the loud ring** — thicker line, higher alpha. A single
  selection is always focused; in a group only the hovered unit is, so a mixed
  selection reads as one bright boundary over quiet ones.
- **A group stays legible.** Rings that would sit on top of each other (same
  weapon class, radius within 5%, centres within 35% of the radius) collapse into
  one, and the set is capped at `k_attack_range_max_rings`, keeping the longest
  reaches. The focused ring is never dropped.
- **Only your own units.** The collector filters to the local owner, so an
  enemy's reach is never revealed.

The ring states distance and nothing else — it is not a line-of-sight claim. What
the cursor label adds, while a hostile target is hovered in attack mode, is the
verdict that the combat check itself returns: _In range_, _Too close_ (inside a
minimum), _Out of range_, or _No firing line_ when the target sits inside the ring
and the attacker still cannot shoot it — a unit locked in melee, or a structure
whose contact geometry refuses. Each verdict carries its own glyph so the state
survives a colour-vision mode.

`render_attack_range_rings` is called from both the game's frame coordinator and
the Arena viewport off the same collector, which is what lets the
`range_indicator_*` Arena scenarios assert the shipped behaviour rather than a
harness copy of it.

The rings themselves are not their own renderer: they are ground markers, the
same command and shader that draw selection rings and attack-target outlines —
see "Ground markers: one ring system" in `RENDERING_ARCHITECTURE.md`. That is
where the terrain-following, the dash/tick patterns and the focus pulse come
from, and why every highlight on the ground costs one instanced draw between
them.

## Unit details: one readout, three surfaces

Issue #1239 asked for a readable unit panel. The hard part was not the panel; it was
that the numbers on a recruit card and the numbers on a details panel are the same
numbers and had no shared source. So the rule is:

**`App::Economy::unit_profile(type, nation)` is the only assembled unit readout.**
`unit_production_info` delegates to it, `ActivityViewModel::unit_profile` exposes it to
QML, and `unit_profile_test.cpp` pins the shared keys equal across every recruitable
unit. A surface that wants a stat asks that function; nothing recomputes one.

What it assembles:

- Stats and derived DPS from `TroopProfile`, with the primary attack chosen by whichever
  of melee/ranged actually does more per second, so a card never advertises a swordsman's
  vestigial bow.
- Costs, population and build time from the same profile.
- Role labels from `Game::Formation::role_tag_label`. The 18 `RoleTag` ids had no
  human-readable form anywhere; the label table sits next to the id table in
  `formation_roles.cpp` so a new role cannot be added without an obvious empty slot
  beside it.
- Lore — role, strengths, weaknesses, history — from `assets/data/troops/base.json`,
  with per-nation `history` overrides, because the nations rename nearly every unit and
  the history follows the name rather than the stat block. Strengths and weaknesses are
  written from the counter table in `UNIT_BALANCE.md`, so they teach the counters the
  game implements; when a counter moves, the prose moves with it.
- Documented abilities, which are **display only**. `TroopProfile::abilities` drives real
  behaviour (`skeleton_archer.cpp` keys cursed arrows on it) and is still populated only
  by nation variants. `documented_abilities` is the parallel list the panel reads, and a
  regression test asserts a Roman skeleton archer gains no ability from it.

`UnitInspectPanel.qml` is the detail surface, hosted in `HUD.qml` on the
`EconomyHelpPanel` pattern (scrim, centred `IronPanel`, `ScrollView`, Escape, and a
`close_requested` signal it never answers itself). It is keyed on **(type, nation)**, not
on a live entity, so the same panel opens from a selected unit and from a recruit card;
live order availability from `get_action_states()` is layered on only when a selection is
open, because a recruit card has no selection to report a status for.

The compact surfaces are the selection summary — an info button and a three-stat strip,
both gated on the readout knowing the unit, so wildlife offers nothing rather than an
empty panel — and `RecruitCard.qml`, the one card the recruit grid repeats.

## The battle report

`BattleReportLayout` is the after-action screen, opened from the outcome banner's
**Battle Report** action. `BattleSummary.qml` is the only part that knows about `game`:
it reads `owner_info` and `get_player_stats`, and hands the layout a plain array of
armies. That split is why the report can be rendered and asserted in
`design_system_qml_tests` without a running match.

The screen is a scoreboard, not a stack of per-player cards. Cards forced a reader to
scan sideways to compare two numbers, and they could not survive more than two players
without running off the viewport. One table, one row per army, ranked by score, puts
every comparison on a single horizontal line:

- a **commander strip** — four tiles for the local army, each with a meter reading the
  value against the best army in that column, so "did I lead the field" is one glance;
- the **table**, with the local row raised, badged `YOU` and outlined, and every row
  carrying a faction-coloured stripe plus a score-share gradient behind it;
- a pinned footer, so the actions are reachable no matter how long the field is.

Two things give way when there is no room, in this order: the optional columns
(`Trained`, `Villages`) fold away below a narrow sheet width, and the commander strip
drops below a shallow viewport, because every figure in it is repeated in the table row.
The decision is taken from the viewport and the army count alone — never from the laid
out content — so it cannot oscillate against the layout it feeds.

### Roman numerals have a legibility ceiling

`Numerals.roman` will spell any figure up to 19999, and for a score that reads
`MMMMMMMMMMMMMMMMMMMCCLX` — twenty-three glyphs that overran the card, the sibling card
and most of the screen. The ceiling that matters is not the one where the algorithm
stops, it is the one where a reader stops counting M's:

- `Numerals.legibleRomanMax` is **3999**, the last figure the subtractive notation writes
  without repeating a glyph four times.
- `Numerals.legible(value)` writes roman below the ceiling and grouped arabic above it.
- `Numerals.needsArabic(values)` asks the question for a whole column, and
  `Numerals.tally(value, arabic)` writes one cell in the answer. **A column of figures is
  compared, so all of its cells use one system** — a table that mixed `CXLVIII` and
  `12 300` in one column would be worse than either.
- Group separators are non-breaking spaces, so a grouped figure never wraps mid-number.
- `Numerals.span(seconds)` is the duration form. The colon form (`Numerals.clock`) turns
  a play time into `N:XXX:XXXIV`, which reads as three unrelated numerals; `span` names
  the units instead — `XXXm XXXIVs`.

Counts the player actually accumulates in a match — kills, losses, recruits, villages —
stay roman in every ordinary game. The score, which multiplies them, is the figure that
crosses the ceiling, and it is the one that turns arabic.

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
  Widgets tools render them
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

These icons belong to the panels — the selection summary, the roster chips, the
order bar. They are deliberately **not** used over the battlefield. What a unit
is doing is drawn in the world by the 3D activity indicators described in
`RENDERING_ARCHITECTURE.md`; a screen-space badge layer over the same units
existed alongside it for a while and the two disagreed about height, ownership
and grouping. One system owns the overhead read, and it is the 3D one.

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
  target floor, the type legibility floor, reduced motion, high contrast, colour-vision
  repainting
- `tst_gameplay_typography.qml` — walks the real production panel at the minimum, default
  and maximum interface scale: nothing renders under the legibility floor, every label
  grows when the scale doubles, and no label outgrows the box drawn around it
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
- `tst_battle_report.qml` — no figure ever spills its cell, a column keeps one numeral
  system, the sheet stays inside every viewport and scale, and the report folds its
  optional columns and its commander strip in that order
- `tst_battle_summary.qml` — the `game` → armies mapping: neutrals excluded, ranking,
  verdicts, the local commander's colour and faction, and a match it cannot read

They run headless via `ctest -R design_system_qml`, and from `make test`.

On the C++ side: `preferences_test.cpp` covers the persistence layer including
clamping and corrupted values, `selection_grouping_test.cpp` covers the HUD
roster, `icon_resources_test.cpp` keeps the icon registry and the shipped files
in step, `icon_art_test.cpp` holds the vector catalogue to its legibility
contract, and `widget_theme_test.cpp` pins the widget
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
