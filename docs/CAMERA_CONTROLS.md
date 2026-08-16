# Camera controls

Seven ways to move the view, one legend that lists all of them, and a manual
checklist for the layouts the automated tests cannot reach.

## The seven controls

| Control        | How                                        | Where it lives                                 |
| -------------- | ------------------------------------------ | ---------------------------------------------- |
| Edge scroll    | Push the cursor into a screen edge         | `ui/qml/Main.qml`, `edge_scroll_overlay`       |
| Keyboard pan   | Arrow keys / WASD, Shift for a double step | `rts.camera_pan_*` in `ui/input_bindings.cpp`  |
| Drag pan       | Hold the right button and drag             | `ui/qml/GameView.qml`, `renderArea` mouse area |
| Zoom           | Mouse wheel                                | `ui/qml/GameView.qml`                          |
| Rotate         | `Q` / `E`, Shift to swing further          | `rts.camera_yaw_*`                             |
| Minimap jump   | Left-click or drag the minimap             | `ui/qml/HUDTop.qml`, `minimapMouse`            |
| Follow / Reset | Buttons in the top bar                     | `ui/qml/HUDTop.qml`                            |

`ui/qml/CameraGuide.qml` is the single list every surface reads: the compact
in-battle legend (`CameraLegend.qml`), the Camera tab of the field manual
(`HelpPanel.qml`), and the live edge-scroll state shown in both. Adding a
control means adding one entry there; nothing else needs a second copy.

## Edge scroll geometry

The maths lives in `ui/edge_scroll.cpp` rather than inline in QML so it can be
tested (`tests/ui/edge_scroll_test.cpp`).

- The band is 12 logical px along the sides and 10 along the top and bottom at
  the default sensitivity, multiplied by **both** the edge-scroll sensitivity
  and the interface scale, and never narrower than 4 px.
- Push grows toward the edge — squared horizontally, cubed vertically — so a
  cursor resting just inside the band creeps and a cursor pinned to the edge
  runs.
- An unknown cursor position (`-1`), a zero-sized surface or a cursor past the
  surface all yield no movement, so a stale pointer cannot scroll the map.

Scaling the band by the interface scale is what keeps a 12 px band reachable on
a 4K panel at 200%; without it the band stays 12 physical px while every other
target doubles.

### The minimap clearance

The minimap is the one HUD control that is itself a camera move, so a band
reaching under it would leave a minimap drag and edge scroll pushing the same
camera at once. It does not, but only just. Clicking along the minimap's right
edge in a running battle puts the last responsive pixel 25 logical px from the
right of the surface — `hudZoneMargin` plus the panel's own padding — while the
widest band the settings allow is `12 * 2.0 = 24`. Both sides scale with the
interface, so the clearance stays proportional: measured at interface scale 1.0
the minimap stops at 1255 and the band starts at 1256, and at 1.5 it stops at
1243 and the band starts at 1244.

One logical pixel of headroom is not a margin anyone chose. Widening the base
band, raising `kMaxEdgeScrollSensitivity` or trimming the minimap's padding
closes it, and then a minimap drag and edge scroll fight over the camera.
`EdgeScrollTest.TheStrongestBandStaysClearOfTheMinimap` guards the C++ half;
the QML half is a hand measurement, so re-measure it if you touch either.

## What suppresses edge scroll

`mainWindow.edge_scroll_disabled` is **derived**, never assigned:

```qml
readonly property bool edge_scroll_disabled: gameViewItem.camera_pan_active || !mainWindow.active
```

`camera_pan_active` is in turn derived from the live pan state
(`renderArea.key_pan_count > 0 || renderArea.mouse_pan_active`). This matters:
the flag used to be set on right-press and cleared on right-release, so a drag
interrupted by a lost grab — a modal opening, the window losing focus — left it
latched and killed edge scrolling for the rest of the session. Keep it derived.

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

The overlay sits above the HUD (`z: 2` against `z: 1`) but accepts
`Qt.NoButton`, so it never takes a click from a HUD button, and `MouseArea` does
not consume hover events, so HUD hover states still work underneath it.

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
      creep sideways. See "The minimap clearance" above — this passes on a
      single pixel.
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
- [ ] The field manual's **Camera** tab lists the same seven controls with the
      same key names, and both follow a rebound pan or rotate key.
