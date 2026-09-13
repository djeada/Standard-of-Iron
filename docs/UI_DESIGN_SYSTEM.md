# Iron and Ember UI Design System

Standard of Iron's interface uses the `StandardOfIron.Design` QML module as the shared presentation layer for colour, spacing, typography, motion, iconography, faction styling, notifications, hints, sound hooks, accessibility-derived values, and reusable controls.

The design system exists to keep the product coherent across battle HUDs, menus, mission screens, settings, save/load panels, overlays, and tooling. Screens are expected to compose shared design primitives and live application state rather than carrying their own independent visual language.

Typography asset rules are documented in [TYPOGRAPHY.md](TYPOGRAPHY.md). Input and accessibility behavior is documented in [ACCESSIBILITY.md](ACCESSIBILITY.md).

## Module packaging

The module is shipped as a file-based QML resource module:

```text
design_resources.qrc   → :/StandardOfIron/Design/...
ui/qml/design/qmldir   → module StandardOfIron.Design
```

Consumers import it with:

```qml
import StandardOfIron.Design 1.0 as Design
```

The `qmldir` file is the module manifest. It exports the shared singleton tokens and reusable components under `ui/qml/design/`.

The module boundary matters because QML tooling, runtime imports, tests, and generated type information all need to agree about what the design package contains.

## Design-system layers

The module can be understood in four layers.

### 1. Preferences and accessibility state

Persistent user preferences come from C++ through `UiPreferences`.

### 2. Tokens

Singletons such as `Theme`, `Metrics`, `Typography`, and `Motion` derive effective product values from preferences and accessibility state.

### 3. Reusable components

Controls, surfaces, overlays, and layouts consume the tokens and define interaction/presentation behavior.

### 4. Product screens

HUDs, menus, mission screens, settings, save/load panels, and other product views assemble those components around live view-model data.

The intended direction is:

```text
UiPreferences / app state
        │
        ▼
A11y + design singletons
        │
        ▼
shared components
        │
        ▼
product screens
```

A screen should not need to restate the product's base radius, animation timing, title font, warning colour, or touch-target minimum.

## Core singletons

| Singleton       | Responsibility                                             |
| --------------- | ---------------------------------------------------------- |
| `A11y`          | QML-facing accessibility and scaling state                 |
| `Theme`         | product colours and accessibility-aware semantic colours   |
| `Metrics`       | spacing, radii, borders, control dimensions, touch targets |
| `Typography`    | font families, pixel sizes, scale, weights, tracking       |
| `Motion`        | durations, easing, dwell timing                            |
| `Icons`         | shared glyph and painted-art lookup                        |
| `ActivityIcons` | action/activity-state icon vocabulary                      |
| `Numerals`      | numeric/readout presentation helpers                       |
| `FactionTheme`  | faction accent, heraldry, emblem, motto data               |
| `Notifications` | product-wide notification queue                            |
| `UiSound`       | UI interaction-sound helpers                               |

These singletons provide one place for product-wide decisions. They are not gameplay authorities: a theme token can describe how a warning looks, but it does not decide whether a gameplay action is legal.

## Persistent preference source

`UiPreferences` is the persistent C++ preference object exposed to QML.

The current preference surface includes values for:

- interface scale;
- reduced motion;
- high contrast;
- colour-vision mode;
- focus visibility;
- team patterns;
- edge scrolling;
- camera motion/effects;
- damage/economy-number presentation;
- commander input;
- display mode;
- VSync;
- FPS display; and
- camera speed scales.

This is a broader contract than a handful of appearance toggles. The design system reads the subset that affects visual tokens, while product screens/controllers use the rest for the relevant interaction or display behavior.

## Accessibility derivation

`A11y.qml` mirrors the preference state needed by the design layer.

`Metrics`, `Typography`, `Motion`, and `Theme` then derive effective values from that state.

For example:

- UI scale affects geometry and typography;
- reduced motion changes transition behavior;
- high-contrast and colour-vision settings affect semantic colour resolution;
- focus visibility affects keyboard-focus presentation; and
- team-pattern preferences add non-colour identity cues.

This keeps accessibility behavior product-wide. A new screen using the shared tokens inherits the same scaling and contrast rules instead of requiring a bespoke accessibility implementation.

## Metrics

`Metrics.qml` owns reusable geometry such as:

- spacing steps;
- padding;
- panel/control sizes;
- radii;
- border widths;
- icon dimensions; and
- minimum pointer/touch target sizes.

The value of tokenizing these measurements is not only visual consistency. It also makes layout respond coherently to interface scale.

A screen that hard-codes a local 28 px button while the rest of the UI derives control height from scaled metrics can become both visually inconsistent and harder to use at large interface scales.

## Typography

Text uses pixel-size values from `Design.Typography` instead of arbitrary point sizes or screen-local literals.

The type scale provides named rungs for:

- captions;
- small labels;
- body text;
- strong labels;
- panel headings;
- screen titles;
- hero/display text; and
- icon/glyph sizes.

Lower text rungs respect `Typography.minimumSize`, and interactive geometry respects `Metrics.minTouchTarget`.

Typography is therefore a combination of font family, scale, minimum legibility, tracking, and context—not simply a font-size constant.

`scripts/check-typography.py` enforces the tokenized typography rules in QML. See [TYPOGRAPHY.md](TYPOGRAPHY.md) for bundled font ownership and glyph coverage.

## Title and body faces

The product ships its own display face for title/brand usage and a bundled text fallback for broader script coverage.

The design system resolves those families through `Typography` instead of allowing screens to choose arbitrary host fonts.

That is especially important for screenshots, releases, localization, and promo rendering: the product should not reflow simply because the operating system happens to have a different font installed.

## Theme and semantic colour

`Theme.qml` provides the shared product palette and semantic colours.

Screens should prefer semantic intent—such as emphasis, warning, danger, success, panel surface, text hierarchy, or disabled state—over embedding raw colour literals.

The semantic layer allows accessibility variants to change the effective value while the component retains its meaning.

For example, a “danger” action can remain the danger semantic whether the active palette changes for high contrast or colour-vision accessibility.

## Faction identity

`FactionTheme.qml` maps faction/nation identity to accent, heraldry, emblem, and motto presentation.

Faction styling changes the skin of shared components rather than their interaction contract.

A button remains a button; a notification remains a notification; a mission card keeps the same spacing and hierarchy. Faction identity changes emphasis and branding without requiring each nation to maintain a second component library.

This is important for accessibility as well: nation identity is not supposed to be communicated only through hue. Heraldry, iconography, text, and team-pattern systems provide additional channels.

## Motion

`Motion.qml` owns durations, easing, and dwell behavior.

Motion has at least two distinct purposes:

- **decorative/transition motion**, which can be reduced; and
- **readability timing**, such as how long a notification remains visible.

Reduced-motion mode can shorten or eliminate decorative movement without also making text disappear immediately.

Components should therefore consume `Motion` tokens rather than assuming that “reduced motion” means every timeout becomes zero.

## Icon system

Product icons are resolved through `Icons.qml`, `ActivityIcons.qml`, and the related numeral/art helpers.

The design system avoids arbitrary emoji or platform-dependent symbols in product screens.

Shared icon families provide:

- command/action symbols;
- resource/activity marks;
- faction/product glyphs;
- status/readout symbols; and
- painted-art assets where a text glyph is not sufficient.

Activity icons encode meaning through shape/semantics in addition to colour so they remain useful under colour-vision adjustments.

## Numerals and readouts

`Numerals` centralizes product-specific numeric presentation.

This is useful for interfaces that mix Roman-themed visual language with ordinary numeric quantities. Screens can present counts and labels consistently without reimplementing formatting rules in each panel.

Gameplay values still come from application/simulation state; `Numerals` controls representation, not the value itself.

## Reusable component taxonomy

Reusable components live under design subdirectories for controls, surfaces, overlays, and layouts.

A useful mental model is:

- **controls** — interactive elements such as buttons, toggles, selectors, sliders;
- **surfaces** — cards, panels, frames, containers;
- **overlays** — notifications, hints, tooltips, transient feedback;
- **layouts** — repeated structural arrangements and spacing helpers.

Screens should assemble these primitives before introducing a local one-off component with nearly identical behavior.

## State model for controls

A reusable control should make its important states visually distinct and accessible.

Typical state dimensions include:

- enabled vs disabled;
- hovered;
- pressed;
- keyboard-focused;
- selected/checked;
- destructive/warning emphasis; and
- unavailable with an explanation.

The product frequently needs to explain _why_ a gameplay action is unavailable, so disabled styling alone is not enough for command surfaces.

## Command tooltips

`IronCommandTooltip` is the detailed explanation surface used by command-grid actions.

It can present:

- action name;
- hotkey;
- short summary;
- rule/detail rows;
- current availability/status; and
- refusal/warning text.

The critical data rule is that gameplay numbers and refusal reasons come from application/view-model action state rather than copied QML balance constants.

A tooltip may explain “missing 15 stone,” but the amount should come from the same gameplay/economy source that will accept or reject the command.

## Availability and refusal states

The UI distinguishes an unavailable command from an unexplained disabled control.

Where the application can expose a reason, the command surface can show the reason directly in tooltip/status text.

This pattern is used throughout gameplay UX because a refusal such as:

- no valid target;
- missing resource;
- wrong unit type;
- production cap reached; or
- placement blocked

is actionable information for the player.

The design system provides the presentation vocabulary; simulation/application logic provides the reason.

## Notifications

`Design.Notifications` is the shared product-wide notification queue.

Notifications use ordered priority bands:

1. `critical`;
2. `urgent`;
3. `info`;
4. `ambient`.

Within one band, entries retain FIFO order.

This keeps a low-priority ambient notice from displacing a combat-critical warning merely because it was pushed one frame later.

## Notification channels and coalescing

A notification can carry a `channel`.

Repeated pushes on the same channel are coalesced into the existing pending entry, with repeat count and priority handling performed by the queue.

Channel coalescing is important for high-frequency events. A repeated warning should not fill the entire notification stack with identical cards if the product can instead update one existing entry.

Sticky notifications require explicit dismissal. Ordinary entries use dwell timing from `Motion`.

## Notification rendering

Product code publishes notifications; `NotificationHost` renders the current queue for the owning shell.

That separation means gameplay/application code does not need to know where on screen a notification card sits or how it animates.

It publishes message semantics and priority; the design system presents them.

## Hints and coaching

Persistent coaching is managed through the C++ `UiHints` registry rather than a collection of unrelated screen-local “seen” booleans.

The registry distinguishes:

- whether a hint is persistently enabled; and
- whether it is currently armed/visible for the live session state.

Supported operations include:

- showing an enabled hint;
- explicitly revealing a hint;
- one-shot display;
- dismissal;
- persistent suppression; and
- selection-change handling.

`HintCard.qml` is the common presentation shell.

The registry owns identity/persistence policy. The product screen owns the world-state condition that decides whether the hint is relevant right now.

## Settings integration for hints

Settings consume the registered hint catalogue rather than maintaining a separate hard-coded list.

This keeps “what hints exist” consistent between runtime coaching and preference controls.

A newly registered hint can therefore become configurable without requiring another independently maintained settings model.

## Targeting feedback

Attack and interaction modes use the same design language as the command UI.

World cursor/marker feedback, command-panel state, tooltip status, and selected mode are meant to agree.

Targeting visuals are gated by active command mode so ordinary hover does not become a second implicit attack/interaction mode.

The world feedback communicates the simulation/application decision; it does not independently decide target legality.

## Selection and inspection surfaces

Selection/inspection UI follows the same hierarchy rules as command panels:

- identity first;
- current health/status/activity;
- important target/relationship information;
- available commands; and
- explanatory secondary detail.

Large selections can use grouping/aggregation, but the design system still supplies consistent typography, spacing, state treatment, and interaction components.

## Battle HUD composition

The battle HUD combines several independent subsystems:

- resources/economy;
- objectives/waves;
- selection/inspection;
- command actions;
- notifications;
- hints/coaching;
- minimap/camera; and
- pause/speed/utility controls.

The design system keeps these regions visually related while allowing each subsystem to own its live data.

The HUD should not become a monolithic QML object that reimplements resource, combat, formation, or mission rules merely because it displays all of them.

## Menus and setup screens

The same tokens and controls are used outside battle for:

- campaign/mission selection;
- skirmish setup;
- settings;
- save/load;
- pause/menu flows; and
- briefing/results screens.

Using one module across gameplay and menu UI prevents the product from having a “battle visual language” and a different “menu visual language” with inconsistent control metrics and interaction states.

## Right-to-left layout

Arabic uses right-to-left interface support.

Reusable components should avoid assuming that left-to-right placement is universal. Text alignment, directional layout, ordering, and icon relationships need to cooperate with the product's RTL behavior.

The design system provides common building blocks, while individual screens remain responsible for using mirroring-safe layout patterns where needed.

## Focus and keyboard interaction

`UiPreferences` includes focus-visibility behavior, and reusable controls should expose keyboard focus consistently.

This is especially important because gameplay commands, menus, settings, and dialogs mix mouse and keyboard use.

A control that can receive keyboard activation should not hide its focus state merely because the default pointer workflow did not need it.

## Team identity

Team identity can use both colour and pattern.

The team-pattern preference exists so rings/markers can communicate identity without relying solely on hue. Shared components and world overlays should consume the application/design identity state rather than inventing local team colours.

## Damage and economy numbers

`UiPreferences` carries presentation preferences for damage/economy numbers.

These options affect whether/how certain feedback is shown, not the authoritative values themselves. The simulation still owns damage and resources; UI settings decide whether those values are surfaced in a particular visual form.

## UI sound

`UiSound` centralizes shared UI interaction-sound helpers.

Buttons/screens should use the common interaction sound path where appropriate instead of choosing unrelated assets or playback behavior locally.

Gameplay audio remains owned by the gameplay/audio systems; `UiSound` is for interface interaction feedback.

## Component gallery

`ComponentGallery.qml` and `GalleryWindow.qml` provide a dedicated review surface for the design system.

The gallery is useful for checking:

- token changes;
- control states;
- accessibility scale;
- high-contrast/color variants;
- faction skins;
- notifications;
- hints;
- typography; and
- common surfaces

without navigating through a full mission.

A shared gallery is especially valuable for regression review because it exposes many component states together.

## Qt Widgets tooling

Arena and the map editor use Qt Widgets rather than the QML control set.

They still use shared C++ theme/font support and bundled product font assets so they remain visually related to the main application.

The implementation widgets differ, but the product palette/font sources remain shared where the toolkit boundary permits it.

## Design-data boundary

The design system can present gameplay state but does not own it.

Examples:

- a resource counter reads resource values; it does not compute harvesting;
- a production button displays cost/refusal data; it does not decide affordability;
- a formation card displays plan state; it does not place slots;
- a save progress indicator shows save stages; it does not serialize the world;
- a target marker visualizes a valid target; it does not define hostility.

This boundary is what keeps QML from becoming a shadow gameplay implementation.

## Source-policy expectations

Several repository checks protect presentation consistency.

Examples include:

- typography token checks;
- QML tests;
- preference/accessibility tests;
- source-policy checks against selected hard-coded values; and
- component-level regression tests.

The exact checks evolve with the code, but the architectural expectation remains: shared presentation rules should live in shared design/pref sources, not be copied into screens.

## Testing

The UI/design system is covered by:

- QML tests under `tests/ui/qml/`;
- C++ preference/theme/accessibility tests;
- typography and glyph checks;
- interaction/input tests;
- component-gallery review; and
- integrated gameplay/UI scenarios.

QML tests are particularly useful for stateful components where visual correctness depends on live bindings, focus, selection, or accessibility settings rather than static appearance alone.

## Authoring guidance

When adding or changing a screen, the preferred sequence is:

1. identify whether a token already expresses the needed colour/spacing/type/motion value;
2. identify whether a reusable component already expresses the interaction;
3. obtain gameplay values/refusal reasons from a view model or application source;
4. make accessibility/RTL behavior inherit from shared state;
5. add component/screen tests for binding-heavy behavior; and
6. add gallery coverage when the change defines a reusable visual state.

A local component is justified when it represents a genuinely screen-specific concept, not simply because recreating a shared button with local rectangles/text feels faster.

## Architectural invariants

The current UI design system depends on these invariants:

- persistent preferences have a shared C++ source;
- design tokens derive effective appearance from those preferences;
- screens consume shared tokens/components instead of local copies;
- gameplay rules stay outside QML;
- unavailable actions expose reasons when the application can provide them;
- accessibility preferences affect the whole product through shared state;
- faction/team identity is not colour-only; and
- presentation caches/animations do not become simulation authority.

## Source map

| Concern                | Source                                |
| ---------------------- | ------------------------------------- |
| Design module          | `ui/qml/design/`                      |
| Module manifest        | `ui/qml/design/qmldir`                |
| QML resources          | `design_resources.qrc`                |
| Persistent preferences | `ui/preferences.h` and implementation |
| Design tests           | `tests/ui/qml/` and related C++ tests |
| Typography checks      | `scripts/check-typography.py`         |
| Font architecture      | [TYPOGRAPHY.md](TYPOGRAPHY.md)        |
| Input/accessibility    | [ACCESSIBILITY.md](ACCESSIBILITY.md)  |

The design system is defined by the current `StandardOfIron.Design` module and the preference/application state that feeds it. Historical issue notes about how individual controls were introduced are not part of the present design contract.
