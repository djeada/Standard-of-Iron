#pragma once

#include <QVector3D>

namespace Render::GL {

struct SwordRenderConfig {
  QVector3D metal_color{0.72F, 0.73F, 0.78F};
  QVector3D grip_color{};
  QVector3D fuller_color{};
  float sword_length = 0.84F;
  float sword_width = 0.072F;
  float guard_half_width = 0.125F;
  float handle_radius = 0.016F;
  float pommel_radius = 0.045F;
  float pommel_length = 0.05F;
  float blade_ricasso = 0.12F;
  float blade_taper_bias = 0.62F;
  float blade_mid_width_scale = 1.12F;
  float blade_tip_width_scale = 0.16F;
  float blade_curve = 0.02F;
  float guard_curve = 0.03F;
  float guard_spike_length = 0.06F;
  int blade_back_spike_count = 0;
  float blade_back_spike_length = 0.06F;
  bool has_scabbard = true;
  int material_id = 3;
  bool has_hooked_pommel = false;
};

inline constexpr QVector3D k_sword_blade_axis_in_grip{0.02F, 0.97F, 0.0F};

} // namespace Render::GL
