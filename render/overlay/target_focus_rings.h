#pragma once

#include <QVector3D>

#include <cstdint>
#include <span>

namespace Render::GL {
class FrameSubmitter;
class DrawResources;

enum class TargetFocusVisualRole : std::uint8_t {
  Inspected,
  LockedTarget,
  IncomingAttacker,
};

struct TargetFocusVisual {
  QVector3D position{0.0F, 0.0F, 0.0F};
  float radius{0.5F};
  TargetFocusVisualRole role{TargetFocusVisualRole::LockedTarget};
  bool hostile{true};
  bool is_building{false};
  int weight{1};
};

struct TargetFocusStyle {
  bool reduced_motion{false};
};

void render_target_focus_rings(FrameSubmitter* renderer,
                               const DrawResources* resources,
                               std::span<const TargetFocusVisual> visuals,
                               const TargetFocusStyle& style = {});

} // namespace Render::GL
