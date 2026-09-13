# Iron and Ember UI Design System

Standard of Iron's QML interface uses the `StandardOfIron.Design` resource module for shared colour, spacing, typography, motion, iconography, notifications, faction styling, accessibility-derived values, and reusable controls.

Typography asset rules are documented separately in [TYPOGRAPHY.md](TYPOGRAPHY.md). Input/accessibility behavior is documented in [ACCESSIBILITY.md](ACCESSIBILITY.md).

## Module packaging

The design system is published as a file-based QML resource module:

```text
design_resources.qrc   → :/StandardOfIron/Design/...
ui/qml/design/qmldir   → module StandardOfIron.Design
```

Screens import it with:

```qml
import StandardOfIron.Design 1.0 as Design
```

`ui/qml/design/qmldir` is the module manifest. The directory also contains the component gallery and the singleton token files consumed throughout the UI.

## Core singletons

| Singleton | Responsibility |
| --- | --- |
| `A11y` | QML-facing accessibility/scaling state |
| `Theme` | product colours and accessibility colour variants |
| `Metrics` | spacing, radii, borders, controls, touch targets |
| `Typography` | font families, pixel-size scale, weights, tracking |
| `Motion` | durations, easing, dwell timing |
| `Icons` | shared icon and painted-art lookups |
| `ActivityIcons` | activity/order-state icon vocabulary |
| `Numerals` | formatted numeral/readout presentation |
| `FactionTheme` | faction accent, heraldry, emblem and motto data |
| `Notifications` | product-wide notification queue |
| `UiSound` | UI interaction sound helpers |

QML components use these values instead of carrying independent copies of product metrics or colours.

## Accessibility source

`UiPreferences` is the persistent C++ preference object exposed to QML. It currently includes UI scale, reduced motion, high contrast, colour-vision mode, focus visibility, team patterns, edge-scroll options, camera-motion/effect settings, damage/economy number settings, commander input settings, display mode/vsync/FPS, and camera speed scales.

`A11y.qml` mirrors the subset of those preferences needed by design tokens. `Metrics`, `Typography`, `Motion`, and `Theme` derive their effective values from that state.

This gives scale/motion/contrast preferences one product-wide source instead of separate per-screen values.

## Typography

Text uses pixel sizes from `Design.Typography` rather than point sizes or arbitrary literals.

The type scale provides named rungs for captions, labels, body text, panel headings, screen titles, hero text, and glyph sizes. Lower text rungs respect `Typography.minimumSize`; pointer targets respect `Metrics.minTouchTarget`.

`scripts/check-typography.py` enforces the tokenized QML typography rules. See [TYPOGRAPHY.md](TYPOGRAPHY.md) for font-family ownership, bundled title-face rules, and glyph coverage.

## Colour and faction identity

`Theme.qml` provides the shared product palette and accessibility-adjusted semantic colours. `FactionTheme.qml` maps nation/faction identity to accents and heraldic presentation.

Faction identity changes the skin of common components rather than changing their layout contract. A button, panel, notification, or HUD region keeps the same interaction and geometry while its faction accent can change.

## Motion

`Motion.qml` centralizes animation durations and easing. Reduced-motion mode resolves motion timing through the same token layer so components do not need independent accessibility branches for every transition.

Reading/dwell time for notifications is distinct from decorative transition time; removing animation does not remove the time a message remains available to read.

## Icons

Product icons are resolved through `Icons.qml` and the related activity/numeral helpers rather than embedding arbitrary emoji in screens.

The icon system includes shared glyph marks and painted-art families. Activity icons encode order/activity state through shape and semantics in addition to colour, matching the accessibility contract.

## Command tooltips

`IronCommandTooltip` is the detailed explanation surface behind command-grid actions. It can present:

- action name and hotkey;
- summary text;
- rule/detail rows;
- live availability/status; and
- warning/refusal text.

Gameplay values shown in command explanations are passed from application/view-model action state rather than copied as independent QML balance constants.

## Notifications

`Design.Notifications` is the shared notification queue.

Notifications have ordered priority bands:

1. `critical`;
2. `urgent`;
3. `info`;
4. `ambient`.

Within one priority band, entries retain FIFO order.

A notification can carry a `channel`. Repeated pushes on the same channel are coalesced into the existing pending entry, with repeat count/priority handling performed by the queue. Sticky entries require explicit dismissal; ordinary entries use the dwell timing from `Motion`.

Screens publish notifications. A `NotificationHost` renders the active queue for the owning product shell.

## Coaching and hints

Persistent coaching overlays are managed by the C++ `UiHints` registry rather than by independent screen-local “seen” flags.

The registry distinguishes:

- whether a hint is enabled persistently; and
- whether it is armed/visible for the current session state.

Available operations include showing an enabled hint, explicitly revealing a hint, one-shot display, dismissal, persistent suppression, and selection-change handling.

`HintCard.qml` supplies the common shell for registered hints. The registry stores scope/persistence policy, while the screen supplies the world-state gate that says whether the hint is currently relevant.

Settings consume the hint catalogue so registered hint preferences can be exposed without duplicating the list in a separate UI model.

## Targeting feedback

Attack/interaction modes use the same design vocabulary as the rest of the HUD. Targeting feedback is gated by the active command mode so ordinary hover does not silently become a second targeting UI.

The command panel, world marker/cursor feedback, and tooltip/status text communicate the selected mode together.

## Reusable components

Reusable components live beneath `ui/qml/design/controls`, `surfaces`, `overlays`, and `layouts` and are exported through the design module manifest.

The component gallery (`ComponentGallery.qml` / `GalleryWindow.qml`) renders representative states for visual review. It is the reference surface for checking token changes, accessibility scales, faction skins, controls, notifications, and common surfaces without navigating through a full match.

## Qt Widgets tools

Arena and the map editor use the shared C++ theme/font support rather than QML components, but they read the same product palette and bundled font assets. QML and Widgets therefore use different widget implementations while sharing the same visual source data.

## Validation

The design system is covered by:

- QML tests under `tests/ui/qml/`;
- typography and glyph checks;
- C++ preference/theme/accessibility tests;
- component-gallery review; and
- source-policy checks that reject selected hard-coded presentation values.

The source of truth for a design token is the implementation under `ui/qml/design/` (or its C++ preference/theme source), not historical issue notes describing how the design system was introduced.
