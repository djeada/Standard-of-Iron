#pragma once

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Game::Formation::planning {

inline constexpr float k_pi = std::numbers::pi_v<float>;
inline constexpr float k_deg_to_rad = k_pi / 180.0F;

struct Bounds {
  bool valid{false};
  float min_x{0.0F};
  float max_x{0.0F};
  float min_z{0.0F};
  float max_z{0.0F};

  void expand(const QVector3D& point) {
    if (!valid) {
      valid = true;
      min_x = max_x = point.x();
      min_z = max_z = point.z();
      return;
    }
    min_x = std::min(min_x, point.x());
    max_x = std::max(max_x, point.x());
    min_z = std::min(min_z, point.z());
    max_z = std::max(max_z, point.z());
  }

  [[nodiscard]] auto width() const -> float { return valid ? max_x - min_x : 0.0F; }
  [[nodiscard]] auto depth() const -> float { return valid ? max_z - min_z : 0.0F; }
  [[nodiscard]] auto half_width() const -> float {
    return valid ? std::max(std::abs(min_x), std::abs(max_x)) : 0.0F;
  }
};

struct FrameAxes {
  QVector3D lateral{1.0F, 0.0F, 0.0F};
  QVector3D depth{0.0F, 0.0F, 1.0F};
};

[[nodiscard]] auto rotate_offset(const QVector3D& local,
                                 float yaw_degrees) -> QVector3D;

[[nodiscard]] auto frame_axes(float facing_degrees) -> FrameAxes;

[[nodiscard]] auto local_overlap(float dx,
                                 float dz,
                                 float half_width_a,
                                 float half_depth_a,
                                 float half_width_b,
                                 float half_depth_b,
                                 float gap) -> bool;

} // namespace Game::Formation::planning
