#pragma once

#include <QVector3D>

#include <span>

namespace Render::GL {
class Renderer;
class ResourceManager;

struct AttackTargetMarkerVisual {
  QVector3D position{0.0F, 0.0F, 0.0F};
  float radius{0.5F};
  bool hovered{false};
  bool attackable{true};
};

void render_attack_target_markers(Renderer* renderer,
                                  ResourceManager* resources,
                                  std::span<const AttackTargetMarkerVisual> markers);

} // namespace Render::GL
