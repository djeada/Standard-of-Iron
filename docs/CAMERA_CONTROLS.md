# Camera Controls

The RTS camera supports nine ways to move or restore the view. The same control definitions feed the in-battle legend and field manual, while automated tests cover the underlying geometry and input rules. This page explains the complete control model and ends with the manual regression pass needed for layouts and interactions that unit tests cannot fully reproduce.

## The nine camera controls

| Control      | How                                                           | Implementation                                 |
| ------------ | ------------------------------------------------------------- | ---------------------------------------------- |
| Edge scroll  | Push the cursor into a screen edge                            | `ui/qml/Main.qml`, `edge_scroll_overlay`       |
| Keyboard pan | Arrow keys **or** `WASD`; Shift for a double step             | `rts.camera_pan_*` in `ui/input_bindings.cpp`  |
| Drag pan     | Hold the right mouse button and drag                          | `ui/qml/GameView.qml`, `renderArea` mouse area |
| Zoom         | Mouse wheel, or `PgUp` / `PgDown`                             | `rts.camera_zoom_*`                            |
| Rotate       | `Q` / `E`; Shift for a larger step                            | `rts.camera_rotate_*`                          |
| Tilt         | `R` / `F` or `Ctrl+Up` / `Ctrl+Down`; Shift for a larger step | `rts.camera_tilt_*`                            |
| Minimap jump | Left-click or drag the minimap                                | `ui/qml/HUDTop.qml`, `minimapMouse`            |
| Follow       | Button in the top bar                                         | `ui/qml/HUDTop.qml`                            |
| Reset        | `Home`, or the Reset button in the top bar                    | `rts.camera_reset`                             |

`ui/qml/CameraGuide.qml` is the single descriptive list used by every help surface: the compact in-battle legend in `CameraLegend.qml`, the Camera tab in `HelpPanel.qml`, and the live edge-scroll status shown by both.

Adding or renaming a camera control should therefore start in `CameraGuide.qml` rather than by duplicating text in several interfaces.

### Follow keeps the viewing angle

`Camera::update_follow` eases the look-at point and places the eye at `target + follow offset` on every frame, so the viewing angle never changes while following. Snapping the target while only the eye eased made any jump in the selection's centre (select-all reaching a far builder, a unit dying at the edge of a group) swing the camera down towards the horizon. `CameraFollowSystem::snap_to_selection` likewise carries the eye with the target. The top-bar Follow button is bound to `following_selection`, so it goes dark when Reset turns following off.

## Pan, rotate, and tilt are different operations

The three basic camera motions should remain distinct in both code and player-facing language:

- **Pan** moves the camera's ground target without changing viewing angle.
- **Rotate** changes yaw, swinging the camera around the target on the horizontal plane. Runtime owners are `Camera::yaw` and `CameraService::yaw`.
- **Tilt** changes pitch, raising the view toward overhead or lowering it toward the horizon. Runtime owner is `CameraService::tilt`.

`Camera::orbit(yaw, pitch)` remains the lower-level two-axis primitive used by both rotation and tilt.

Tilt is bound to `R` / `F` and, as the primary chords, `Ctrl+Up` / `Ctrl+Down`. `R` is shared with the commander rally action, which is contextual: while a rally flag can be placed, `R` places it and the camera stays still; otherwise `R` tilts up. The formation planner, which used to sit on `F`, is on `V`.

### The pitch sign convention

`CameraService::tilt` accepts a direction where **positive means raise the camera**, but it passes the opposite sign to the lower-level orbit function.

That inversion is intentional. The camera's pitch describes the elevation of the **view direction**: a near-overhead view is around `-85`, while a near-level view is around `-5`. Raising the camera therefore moves the numerical pitch downward.

Tests assert the camera's height above its target rather than reading pitch directly, which protects the intended user-facing direction from sign confusion.

## Two chords per command

`InputBindings` stores a **primary** and **alternate** chord for each action through `InputBindings::Slot`. This is how keyboard panning can support both the arrow keys and `WASD` as first-class bindings.

Alternate bindings are persisted under the action ID with an `|alt` suffix, rebound from their own button in **Settings → Controls**, and resolved by `actions_for_key` alongside the primary chord.

Using `WASD` for camera pan required moving conflicting RTS commands: **Attack** uses `C` and **Stop** uses `Z`. Within one context, a key must have one unambiguous meaning.

Saved keymaps survive action renames. Migration maps stored chords according to **behavior**, not merely old labels. For example, the former `rts.camera_orbit_left` action actually lowered the camera, so its binding migrates to `rts.camera_tilt_down`. Preserving the physical key while reversing the action would be worse than dropping the binding.

## Reset framing follows the map

Every map authors a camera view appropriate to its scale. Small maps and large battlefields therefore need different reset distances.

`Game::reset_framing` in `game/camera_framing.h` derives reset framing from the map's authored camera:

- use one third of the authored distance;
- never go closer than 24 units; and
- never go farther out than the map itself requested.

Authored tilt and yaw are preserved.

`GameConfig::camera_reset_framing` applies this rule and falls back to the built-in default only when no map is loaded.

The same framing function is consumed by:

- camp focus during load in `app/session/level_orchestrator.cpp`;
- the Reset command through `CameraService::snap_to_entity`; and
- the skirmish opening shot through `SkirmishRuntimeCoordinator::center_camera_on_local_forces`.

Sharing one calculation prevents opening framing and reset framing from drifting apart.

## Camera speed settings

Keyboard pan, wheel zoom, and `Q` / `E` rotation each have a user-adjustable speed scale from one quarter to three times the designed pace.

Values persist through `App::Core::UserSettings` and are propagated to the atomics in `game/render_bridge/camera_speeds.h`. `CameraService::move`, `CameraService::zoom`, and `CameraService::yaw` read those values on every call.

Drag pan, minimap jumps, and tilt remain unscaled because their displacement is already determined by the gesture itself. Applying another multiplier would make the same physical motion encode two layers of speed.

## Edge-scroll geometry

The edge-scroll calculation lives in `ui/edge_scroll.cpp` so it can be tested independently by `tests/ui/edge_scroll_test.cpp`.

Its important rules are:

- the default trigger band is 26 logical px on **every** side;
- band width scales with both edge-scroll sensitivity and interface scale, with a 10 px minimum;
- push increases toward the edge on the same square curve for both axes;
- movement begins at `k_entry_push` (35%) as soon as the cursor enters the band;
- diagonal corner push is clamped to the magnitude of one edge, avoiding a `sqrt(2)` speed increase; and
- unknown cursor coordinates (`-1`), zero-sized surfaces, or coordinates outside the surface produce no movement.

Scaling by interface size keeps the target usable on high-resolution displays. A 26-logical-pixel band should not collapse into a tiny physical sliver while every other UI target doubles at 200% scale.

Horizontal and vertical edges deliberately use the same geometry and response curve so top and bottom scrolling feel as strong as left and right scrolling.

## Where the edge-scroll cursor comes from

`EdgeScroll.cursorIn(item)` reads `QCursor::pos()` and maps it into the edge-scroll overlay. A 16 ms timer in the overlay polls that position.

The system intentionally does **not** depend on the overlay's own hover events.

The full-screen edge overlay sits below the HUD (`z: 0.5` versus the HUD's `z: 1`) so HUD controls retain hover states and tooltips. In Qt, hover delivery stops at the first item that accepts the event. An overlay above the HUD would swallow HUD interaction; an overlay below it would fail to receive pointer movement whenever a HUD element was under the cursor.

Polling the platform cursor solves both sides of the problem: HUD controls receive their own events, while edge scrolling can still see every physical screen edge.

## The minimap suppresses edge scroll explicitly

The minimap is itself a camera-control surface, so edge scrolling must not compete with it.

Suppression is based on state rather than a hand-tuned geometric margin:

- `HUD.blocks_edge_scroll()` returns true for the minimap rectangle; and
- `mainWindow.edge_scroll_disabled` remains true during a minimap drag through `hud.minimap_drag_active`, even if the pointer leaves the minimap while dragging.

This lets the edge band be sized for usability rather than constrained by a fragile one-pixel clearance around the minimap.

## What suppresses edge scrolling

`mainWindow.edge_scroll_disabled` is a **derived** property:

```qml
readonly property bool edge_scroll_disabled: gameViewItem.camera_pan_active
    || !mainWindow.active || mainWindow.overlay_active
    || hud.commander_rpg_mode || hud.minimap_drag_active
```

`camera_pan_active` is also derived from live input state:

```text
renderArea.key_pan_count > 0 || renderArea.mouse_pan_active
```

Keeping these values derived prevents them from becoming latched when input is interrupted. For example, a modal opening or the application losing focus during a right-drag must not leave edge scrolling disabled for the rest of the session.

Polling the platform cursor means the overlay no longer learns suppression indirectly by losing hover events. Every state that should block edge scrolling therefore needs to be represented explicitly in the derived condition.

The overlay timer is likewise controlled by a live condition rather than started only by an enter event. If a panel closes while the cursor is already resting at an edge, scrolling should resume without requiring the pointer to leave and re-enter.

## HUD zones and pointer ownership

Two similar-sounding rules serve different purposes and should not be merged.

### `in_hud_zone()` blocks world hover

`edge_scroll_overlay.in_hud_zone()` decides where **world hover** stops. It reads `hud.top_panel_height` and `hud.bottom_panel_height`, using the exact snake-case property names.

Those full-width zones prevent world units and placement previews from responding to a pointer that is actually interacting with the HUD.

### `blocks_edge_scroll()` blocks camera scrolling

`HUD.blocks_edge_scroll()` is much narrower and covers only the minimap.

Camera edge scrolling intentionally continues behind the top and bottom HUD panels so every physical screen edge remains useful. Folding the world-pointer and camera-scroll zones together would make the top and bottom edges dead.

The overlay accepts `Qt.NoButton` and sits beneath the HUD, so it does not consume clicks intended for HUD controls.

## Manual regression checklist

Automated tests cover geometry, binding behavior, and legend contents, but several interactions depend on real windowing, focus, scale, and pointer behavior. Run this checklist when changing camera controls or edge scrolling.

### Layouts to cover

1. **Windowed, 1280×720, interface scale 100%**
2. **Fullscreen, native resolution, interface scale 100%**
3. **High resolution (2560×1440 or 3840×2160), interface scale 100%**
4. **Interface scale 150% and 200%** through **Settings → Accessibility → Interface size**
5. **Right-to-left language** through **Settings → Language → العربية**

### Edge-scroll checks for every layout

- [ ] Left, right, top, and bottom edges scroll in the expected direction. The bottom edge still works with the command panel under the cursor.
- [ ] Every corner scrolls on both axes at once.
- [ ] Scrolling stops immediately when the cursor leaves the trigger band.
- [ ] The band remains easy to reach, including on high-DPI displays and at large interface scales.
- [ ] Top and bottom edges feel as strong as the side edges, both at the physical edge and halfway through the band.
- [ ] Entering a corner does not increase total camera speed.
- [ ] **Settings → Accessibility** reports a band width in pixels that matches the actual target size.
- [ ] Disabling **Edge scrolling** stops edge movement everywhere while keyboard pan, right-drag, wheel zoom, and minimap navigation continue to work.
- [ ] The in-game legend reports edge scrolling as `off` when disabled.
- [ ] Raising and lowering **Edge scroll strength** visibly changes both band width and pan speed, and the pixel readout follows it.

### Interaction checks

- [ ] Right-drag pans without edge scrolling fighting the drag.
- [ ] At maximum edge-scroll strength, dragging the minimap moves only according to the minimap, even if the drag leaves the minimap near a screen edge.
- [ ] Commander first-person mode does not scroll the RTS camera from screen edges.
- [ ] Start a right-drag, open the menu with `Esc` mid-drag, return to the battle, and confirm edge scrolling still works.
- [ ] Hold an arrow key, alt-tab away, release the key outside the window, return, and confirm the camera is not still panning and edge scroll still works.
- [ ] Alt-tab away with the cursor resting on an edge and confirm the camera does not continue scrolling in the background.
- [ ] Open Settings, close it while the pointer is already on an edge, and confirm scrolling resumes without extra pointer movement.
- [ ] Hover HUD controls and confirm they highlight without world units highlighting underneath.
- [ ] Click all top-bar and command-grid buttons and confirm the edge overlay does not swallow input.
- [ ] Issue an attack order near a screen edge and confirm the order lands at the intended cursor location rather than after unwanted camera drift.
- [ ] Move building and formation placement previews over the HUD and confirm the previews stop following the pointer there.

### First-run legend and help

- [ ] On a fresh profile, the camera legend appears on the first battle.
- [ ] Dismissing the legend keeps it dismissed across restarts.
- [ ] The star button in the top bar can show it again.
- [ ] The legend's **Camera settings** button opens Settings.
- [ ] The field manual's **Camera** tab lists the same nine controls with the same key names and follows rebound pan or rotate keys.
- [ ] The pan row names both arrows and `WASD`, and rebinding either slot updates the legend.

### Bindings and framing

- [ ] **Settings → Controls** shows two key buttons per command, and assigning an alternate already used by another command warns before reassignment.
- [ ] Clearing only the alternate with `Backspace` leaves the primary binding intact; **Default** restores both.
- [ ] `WASD` pans; `A` and `S` no longer mean Stop or Attack, while `C` and `Z` do.
- [ ] `Ctrl+Up` and `Ctrl+Down` tilt; plain `Up` and `Down` continue to pan when Ctrl is not held.
- [ ] `R` / `F` tilt up and down. While a rally flag can be placed, `R` places it without moving the camera.
- [ ] `V` opens the formation planner.
- [ ] `Home` and the top-bar Reset button land on the same view.
- [ ] On Cannae, Reset frames the camp and surrounding battlefield rather than a single soldier. On the tutorial, Reset is closer but still wider than one formation.
- [ ] Load a save from a build predating the camera-action rename and confirm custom bindings survive with tilt mapped to the behavior formerly labeled orbit.

The camera system is easiest to maintain when every movement has one name, every help surface reads one source of truth, and edge scrolling is controlled by explicit geometry and state rather than incidental event delivery.
