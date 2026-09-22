#pragma once
#include <QVector3D>

#include "arrow_visual_style.h"

namespace Game::Systems {

struct ArrowInstance {
  QVector3D start;
  QVector3D end;
  QVector3D color;
  float t{};
  float speed{};
  bool active{};
  float arc_height{};
  float inv_dist{};
  float scale{1.0F};
  float length_scale{1.0F};
  float roll_deg{};
  float spin_rate_deg{};
  float trail_alpha{};
  float trail_length{};
  float brightness{1.0F};
  ArrowVisualStyle style{ArrowVisualStyle::Focused};
};

} // namespace Game::Systems
