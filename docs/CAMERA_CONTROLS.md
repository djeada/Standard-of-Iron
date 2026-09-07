# Camera controls

Nine ways to move the view, one legend that lists all of them, and a manual
checklist for the layouts the automated tests cannot reach.

## The nine controls

| Control      | How                                               | Where it lives                                 |
| ------------ | ------------------------------------------------- | ---------------------------------------------- |
| Edge scroll  | Push the cursor into a screen edge                | `ui/qml/Main.qml`, `edge_scroll_overlay`       |
| Keyboard pan | Arrow keys **or** `WASD`, Shift for a double step | `rts.camera_pan_*` in `ui/input_bindings.cpp`  |
| Drag pan     | Hold the right button and drag                    | `ui/qml/GameView.qml`, `renderArea` mouse area |
| Zoom         | Mouse wheel, or `PgUp` / `PgDown`                 | `rts.camera_zoom_*`                            |
| Rotate       | `Q` / `E`, Shift to swing further                 | `rts.camera_rotate_*`                          |
| Tilt         | `Ctrl+Up` / `Ctrl+Down`, Shift to tilt further    | `rts.camera_tilt_*`                            |
| Minimap jump | Left-click or drag the minimap                    | `ui/qml/HUDTop.qml`, `minimapMouse`            |
| Follow       | Button in the top bar                             | `ui/qml/HUDTop.qml`                            |
| Reset        | `Home`, or the button in the top bar              | `rts.camera_reset`                             |

`ui/qml/CameraGuide.qml` is the single list every surface reads: the compact
in-battle legend (`CameraLegend.qml`), the Camera tab of the field manual
(`HelpPanel.qml`), and the live edge-scroll state shown in both. Adding a
control means adding one entry there; nothing else needs a second copy.

## What the words mean

The three rotations are separate commands and were, until recently, separately
misnamed. Keep them apart:

- **Pan** moves the point the camera looks at across the ground. It never
  changes the angle.
- **Rotate** swings the camera around that point on the horizontal circle —
  yaw. `Camera::yaw` and `CameraService::yaw`.
- **Tilt** raises the camera towards an overhead view or lowers it towards the
  horizon — pitch. `CameraService::tilt`, which used to be called
  `orbit_direction` and was bound as "orbit camera left/right" even though it
  never touched yaw. `Camera::orbit(yaw, pitch)` keeps its name: it is the
  two-axis primitive both of the above drive.

`R` and `T` used to carry tilt, which put tilt on the same key as the commander
rally flag and relied on a contextual-priority rule to sort out which one the
player meant. Tilt now lives on `Ctrl+Up` / `Ctrl+Down` and `R` belongs to the
rally flag alone.

### The pitch sign

`CameraService::tilt` takes a direction where **positive raises the camera**,
and it passes the opposite sign down to the orbit. That is not a typo. The
camera's own pitch is the elevation of the _view_ direction, so straight down is
`-85` and level is `-5`: raising the camera makes that number smaller. Read the
sign the other way round — as `orbit_direction` did — and `Ctrl+Up` dives at the
horizon while `Ctrl+Down` climbs. The unit tests assert the camera's height
above its target rather than its pitch, because the height cannot be read
backwards.

## Two keys per command

`InputBindings` stores a **primary** and an **alternate** chord per action
(`InputBindings::Slot`), so panning answers to the arrows and to `WASD` without
either being second-class. The alternate is stored under the action id with an
`|alt` suffix, remapped from its own button in Settings › Controls, and resolved
by `actions_for_key` alongside the primary.

`WASD` on the pan commands is why **Attack** moved to `C` and **Stop** to `Z`.
A key can only mean one thing in a context, and a pan key that is dead whenever
troops are selected — which is nearly always — is not a pan key.

Saved keymaps survive both changes: a renamed action carries its stored chord
across on load, and a player who rebound Attack or Stop keeps what they chose.
The rename pairs by behaviour, not by name — `rts.camera_orbit_left` lowered the
camera, so it becomes `rts.camera_tilt_down` — because a key that survives the
upgrade doing the opposite of what it used to do is worse than one that is
simply unbound.

## Where a reset lands

Every map authors the camera that frames its whole engagement — 18 on the
48-tile Sepulcher Watch, 273 on the 650-tile field at Cannae. Reset used to
ignore all of it and snap to a flat 12 units on every map alike, which on a
battle map meant going from the whole battlefield to one soldier's shield in a
single keystroke. The mission's opening shot went through the same code, so a
campaign began at that same 12 units.

`Game::reset_framing` in `game/camera_framing.h` derives the reset view from
what the map authored: a third of its distance, floored at 24 units, and never
further out than the map itself asked for. The authored tilt and swing carry
over unchanged. `GameConfig::camera_reset_framing` applies it, falling back to
the built-in default only when no map is loaded.

That one function is what the camp focus at load
(`app/session/level_orchestrator.cpp`), the Reset command
(`CameraService::snap_to_entity`) and the skirmish opening shot
(`SkirmishRuntimeCoordinator::center_camera_on_local_forces`) all read, so the
opening framing and the framing a reset returns to cannot drift apart.

## Speed settings

Keyboard pan, wheel zoom and `Q`/`E` rotation each carry a user speed scale —
a quarter to three times the designed pace, set in Settings › Controls. The
values persist through `App::Core::UserSettings` and are pushed into the
atomics in `game/render_bridge/camera_speeds.h`, which
`CameraService::move`, `CameraService::zoom` and `CameraService::yaw` read on
every call. Drag pan, minimap jumps and tilt deliberately stay unscaled: they
are already proportional to the gesture that drives them, so a multiplier would
just resell the same motion twice.

## Edge scroll geometry

The maths lives in `ui/edge_scroll.cpp` rather than inline in QML so it can be
tested (`tests/ui/edge_scroll_test.cpp`).

- The band is 26 logical px on **every** side at the default sensitivity,
  multiplied by both the edge-scroll sensitivity and the interface scale, and
  never narrower than 10 px.
- Push grows toward the edge on the same square curve on both axes, and starts
  at `k_entry_push` (35%) rather than at nothing: crossing into the band moves
  the camera at once, and pinning the cursor to the edge runs it.
- A corner is clamped to the magnitude of a single edge, so rounding one does
  not lurch the view by a factor of sqrt(2).
- An unknown cursor position (`-1`), a zero-sized surface or a cursor past the
  surface all yield no movement, so a stale pointer cannot scroll the map.

Scaling the band by the interface scale is what keeps it reachable on a 4K panel
at 200%; without it the band stays 26 physical px while every other target
doubles.

The two axes are deliberately symmetric. They were not: the vertical band was
10 px against 12 horizontally and ramped as a **cube** against a square, so the
middle of the top band moved the camera at an eighth of the pace the middle of a
side band did. Pushing the cursor up read as broken, because for all but the
last pixel or two it was.

### Where the cursor comes from

`EdgeScroll.cursorIn(item)` reads `QCursor::pos()` and maps it into the overlay,
and the overlay's 16 ms timer polls that. It deliberately does **not** use the
overlay's own hover events.

The overlay is a full-screen `hoverEnabled` MouseArea sitting _below_ the HUD
(`z: 0.5` against the HUD's `z: 1`), because Qt stops hover delivery at the
first item that accepts it and an overlay above the HUD swallowed every HUD
tooltip. But the same rule cuts the other way: while the overlay drove edge
scroll from its own `positionChanged`, the top bar, the command panel, the
minimap and the commander overlay each ate the cursor before it arrived. The
top edge, the bottom edge and all four corners simply did not scroll. Polling
the platform cursor gives both — HUD tooltips open, and every screen edge
scrolls.

### The minimap

The minimap is the one HUD control that is itself a camera move, so edge scroll
has to keep off it. It is refused by state rather than by geometry:
`HUD.blocks_edge_scroll()` returns true for the minimap's own rectangle, and
`mainWindow.edge_scroll_disabled` covers a drag that wanders out of it
(`hud.minimap_drag_active`).

This replaced a one-pixel geometric clearance — the band's widest setting was
`12 * 2.0 = 24` px against a minimap whose hit area ended 25 px from the right —
which is what pinned the base band at 12 px in the first place. A margin nobody
chose is not a margin; the band is now free to be as wide as it needs to be.

## What suppresses edge scroll

`mainWindow.edge_scroll_disabled` is **derived**, never assigned:

```qml
readonly property bool edge_scroll_disabled: gameViewItem.camera_pan_active
    || !mainWindow.active || mainWindow.overlay_active
    || hud.commander_rpg_mode || hud.minimap_drag_active
```

`camera_pan_active` is in turn derived from the live pan state
(`renderArea.key_pan_count > 0 || renderArea.mouse_pan_active`). This matters:
the flag used to be set on right-press and cleared on right-release, so a drag
interrupted by a lost grab — a modal opening, the window losing focus — left it
latched and killed edge scrolling for the rest of the session. Keep it derived.

The last three terms are what a hover-driven overlay used to get for free: an
open panel, the commander's own full-screen cursor overlay and the minimap each
took the cursor away from it. A cursor read straight from the platform sees none
of that, so every reason to _not_ scroll now has to be stated. Anything new that
covers the map and wants the pointer to itself belongs in this list.

The overlay's timer is likewise bound to a live condition rather than started
from `onEntered`, because an item that becomes visible under a stationary cursor
does not reliably get an enter event: closing the menu with the pointer already
at rest used to leave the timer stopped.

## HUD zones

`edge_scroll_overlay.in_hud_zone()` decides where **world hover** stops, not
where scrolling stops. Scrolling deliberately still works over the HUD so that
every screen edge scrolls, including the bottom edge behind the command panel.
The function reads `hud.top_panel_height` and `hud.bottom_panel_height` — note
the snake_case; the camelCase spellings do not exist and silently evaluate to
`undefined`, which is how world hover used to leak through the HUD.

`HUD.blocks_edge_scroll()` is the much smaller sibling that decides where
**scrolling** stops, and it covers the minimap alone. Do not fold the two
together: `blocks_world_pointer()` claims both full-width panels, and scrolling
has to reach through them or the top and bottom edges are dead.

The overlay accepts `Qt.NoButton`, so it never takes a click from a HUD button,
and it sits below the HUD so HUD hover states and tooltips work.

## Manual regression checklist

Automated coverage stops at the geometry and the legend contents. Run this pass
by hand when touching anything above. Every row should behave identically.

### Layouts to cover

1. **Windowed, 1280×720, interface scale 100%**
2. **Fullscreen, native resolution, interface scale 100%**
3. **High resolution (2560×1440 or 3840×2160), interface scale 100%**
4. **Interface scale 150% and 200%** (Settings › Accessibility › Interface size)
5. **Right-to-left language** (Settings › Language › العربية)

### Checks per layout

- [ ] Cursor to the **left** edge scrolls left; **right**, **top** and **bottom**
      each scroll their own way. The bottom edge scrolls even though the command
      panel is under the cursor.
- [ ] Each **corner** scrolls on both axes at once.
- [ ] Scrolling **stops** as soon as the cursor leaves the band.
- [ ] The band feels reachable — no hunting for a sliver of pixels. On a
      high-DPI panel confirm the band grew with the interface scale.
- [ ] The top and bottom edges feel exactly as strong as the sides, at the very
      edge and halfway into the band alike.
- [ ] Rounding a corner does not speed the camera up.
- [ ] Settings › Accessibility shows the band width in px and it **matches** what
      you have to reach for.
- [ ] Turning **edge scrolling off** stops it everywhere; the legend then reads
      `off`, and keyboard pan, right-drag, wheel zoom and the minimap still work.
- [ ] Raise and lower **edge scroll strength**: the band visibly widens and the
      camera visibly speeds up, and the px readout tracks it.

### Interaction checks (any one layout)

- [ ] Right-drag pans; edge scroll does **not** fight it mid-drag.
- [ ] With **edge scroll strength at maximum**, drag the camera around by the
      minimap: the camera goes where the minimap says and does **not** also
      creep sideways, including when the drag wanders off the minimap onto the
      screen edge beside it.
- [ ] In the commander's first-person mode the screen edges do **not** scroll
      the RTS camera.
- [ ] Right-drag, then press `Esc` / open the menu mid-drag, then return to the
      battle: edge scroll still works. (This is the regression that used to
      latch it off permanently.)
- [ ] Hold an arrow key, alt-tab away, release it outside the window, alt-tab
      back: edge scroll still works and the camera is not still panning.
- [ ] Alt-tab away with the cursor parked on an edge: the camera does **not**
      keep scrolling in the background.
- [ ] Open Settings, then close it with the cursor already resting on an edge:
      edge scroll resumes without moving the mouse out and back in.
- [ ] Hover a HUD button: it highlights, and no world unit highlights behind it.
- [ ] Click every top-bar button and every command-grid button: each one
      registers; none is swallowed by the edge overlay.
- [ ] Issue an attack order with the cursor near a screen edge: the order lands
      where the cursor is, not where the camera drifted to.
- [ ] Place a building and a formation with the cursor over the HUD: the preview
      does not follow the cursor into the panel.

### First-run and legend

- [ ] On a **fresh profile**, the camera legend appears on the first battle.
- [ ] Dismissing it (× or the top-bar star) keeps it dismissed across restarts.
- [ ] The star button in the top bar toggles it back on.
- [ ] The legend's **Camera settings** button opens Settings.
- [ ] The field manual's **Camera** tab lists the same nine controls with the
      same key names, and both follow a rebound pan or rotate key.
- [ ] The legend's pan row names **both** the arrows and `WASD`, and rebinding
      either slot in Settings updates it.

### Bindings and framing

- [ ] Settings › Controls shows two key buttons for every command; binding the
      second one to a key another command already holds warns before taking it.
- [ ] Clearing the alternate with Backspace leaves the primary alone, and
      **Default** on the row restores both.
- [ ] `WASD` pans; `A` and `S` no longer stop or attack, and `C` and `Z` do.
- [ ] `Ctrl+Up` / `Ctrl+Down` tilt; plain `Up` / `Down` still pan while Ctrl is
      not held.
- [ ] `R` places a rally flag and does nothing to the camera.
- [ ] `Home` and the top-bar Reset button land on the same view.
- [ ] Start Cannae and press `Home`: the camera frames the camp and the ground
      around it, not a single soldier. Start the tutorial and do the same: the
      view is closer, but still wider than one formation.
- [ ] Load a save from a build before this change: any camera key you had
      rebound is still yours, and tilt is where "orbit" used to be.
