# Iron and Ember design system

This directory is the shared visual and interaction foundation for Standard of
Iron game screens and tools.

- `Theme.qml` exposes semantic colors and a high-contrast variant. Its default
  values come from the C++ `Theme` singleton, which also generates the QWidget
  stylesheet used by Arena and the map/mission editor.
- `Metrics.qml`, `Typography.qml`, and `Motion.qml` centralize sizing, type, and
  timing. `Motion.reducedMotion` collapses nonessential transitions.
- `controls/`, `surfaces/`, `overlays/`, and `layouts/` contain reusable
  components. New screens should compose these components instead of declaring
  literal colors, radii, margins, or transition durations.
- `ComponentGallery.qml` displays representative normal, focused, destructive,
  disabled, progress, objective, resource, and notification states.

The game shell is atmospheric and spacious. The tool shell and generated
QWidget stylesheet use the same language at a denser, utilitarian scale.
