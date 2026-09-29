#include "formation_frame.h"

namespace Game::Formation::planning {

namespace {
constexpr float k_overlap_epsilon = 1.0e-3F;
}

auto rotate_offset(const QVector3D& local, float yaw_degrees) -> QVector3D {
  float const yaw = yaw_degrees * k_deg_to_rad;
  float const sin_yaw = std::sin(yaw);
  float const cos_yaw = std::cos(yaw);
  return {local.x() * cos_yaw + local.z() * sin_yaw,
          local.y(),
          -local.x() * sin_yaw + local.z() * cos_yaw};
}

auto frame_axes(float facing_degrees) -> FrameAxes {
  return {rotate_offset(QVector3D(1.0F, 0.0F, 0.0F), facing_degrees),
          rotate_offset(QVector3D(0.0F, 0.0F, 1.0F), facing_degrees)};
}

auto local_overlap(float dx,
                   float dz,
                   float half_width_a,
                   float half_depth_a,
                   float half_width_b,
                   float half_depth_b,
                   float gap) -> bool {
  return std::abs(dx) + k_overlap_epsilon < half_width_a + half_width_b + gap &&
         std::abs(dz) + k_overlap_epsilon < half_depth_a + half_depth_b + gap;
}

} // namespace Game::Formation::planning
