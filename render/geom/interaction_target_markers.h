#pragma once

#include <QVector3D>

#include <span>

#include "mode_indicator.h"

namespace Render::GL {
class Renderer;
class ResourceManager;

struct InteractionTargetMarkerVisual {
  QVector3D position{0.0F, 0.0F, 0.0F};
  float radius{0.6F};
  bool hovered{false};
  Render::Geom::IndicatorKind action{Render::Geom::IndicatorKind::ChopWood};
};

void render_interaction_target_markers(
    Renderer* renderer,
    ResourceManager* resources,
    std::span<const InteractionTargetMarkerVisual> markers);

} // namespace Render::GL
