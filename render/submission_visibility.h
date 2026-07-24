#pragma once

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "../game/map/render_visibility_rules.h"
#include "gl/camera.h"

namespace Render::GL {

enum class SubmissionFogMode : std::uint8_t {
  Ignore,
  VisibleOnly,
  Revealed,
};

struct SubmissionVisibilityResult {
  bool in_frustum = true;
  bool fog_visible = true;

  [[nodiscard]] auto accepted() const noexcept -> bool {
    return in_frustum && fog_visible;
  }
};

class SubmissionVisibilityPolicy {
public:
  static constexpr float k_frustum_guard_band = 1.5F;

  void reset(const Camera* camera,
             const Game::Map::VisibilityService::Snapshot* snapshot) noexcept {
    m_camera = camera;
    m_snapshot = snapshot;
  }

  [[nodiscard]] auto
  accepts_sphere(const QVector3D& center,
                 float radius,
                 SubmissionFogMode fog_mode = SubmissionFogMode::Ignore) const -> bool {
    return evaluate_sphere(center, radius, fog_mode).accepted();
  }

  [[nodiscard]] auto
  evaluate_sphere(const QVector3D& center,
                  float radius,
                  SubmissionFogMode fog_mode = SubmissionFogMode::Ignore) const
      -> SubmissionVisibilityResult {
    SubmissionVisibilityResult result;
    result.in_frustum =
        m_camera == nullptr ||
        m_camera->is_in_frustum(center, std::max(radius, 0.1F) + k_frustum_guard_band);
    result.fog_visible = accepts_fog_sphere(center, radius, fog_mode);
    return result;
  }

  [[nodiscard]] auto accepts_segment(
      const QVector3D& start,
      const QVector3D& end,
      float radius,
      SubmissionFogMode fog_mode = SubmissionFogMode::Ignore) const -> bool {
    const QVector3D center = (start + end) * 0.5F;
    const float bounds_radius = (end - center).length() + std::max(radius, 0.1F);
    if (m_camera != nullptr &&
        !m_camera->is_in_frustum(center, bounds_radius + k_frustum_guard_band)) {
      return false;
    }
    return accepts_fog_sphere(center, bounds_radius, fog_mode);
  }

  [[nodiscard]] auto
  snapshot() const noexcept -> const Game::Map::VisibilityService::Snapshot* {
    return m_snapshot;
  }

private:
  [[nodiscard]] static auto state_matches(Game::Map::VisibilityState state,
                                          SubmissionFogMode mode) noexcept -> bool {
    return mode == SubmissionFogMode::VisibleOnly
               ? state == Game::Map::VisibilityState::Visible
               : state != Game::Map::VisibilityState::Unseen;
  }

  [[nodiscard]] auto accepts_fog_sphere(const QVector3D& center,
                                        float radius,
                                        SubmissionFogMode mode) const -> bool {
    if (mode == SubmissionFogMode::Ignore || m_snapshot == nullptr ||
        !m_snapshot->initialized) {
      return true;
    }

    const auto& snapshot = *m_snapshot;
    if (snapshot.width <= 0 || snapshot.height <= 0 || snapshot.tile_size <= 0.0F ||
        snapshot.cells.empty()) {
      return false;
    }

    const float safe_radius = std::max(radius, 0.1F);
    const auto world_to_grid = [&snapshot](float world, float half) {
      const float grid = world / snapshot.tile_size + half;
      return static_cast<int>(std::floor(grid + 0.5F));
    };
    const int unclamped_min_x =
        world_to_grid(center.x() - safe_radius, snapshot.half_width);
    const int unclamped_max_x =
        world_to_grid(center.x() + safe_radius, snapshot.half_width);
    const int unclamped_min_z =
        world_to_grid(center.z() - safe_radius, snapshot.half_height);
    const int unclamped_max_z =
        world_to_grid(center.z() + safe_radius, snapshot.half_height);
    if (unclamped_max_x < 0 || unclamped_min_x >= snapshot.width ||
        unclamped_max_z < 0 || unclamped_min_z >= snapshot.height) {
      return false;
    }

    const int min_x = std::clamp(unclamped_min_x, 0, snapshot.width - 1);
    const int max_x = std::clamp(unclamped_max_x, 0, snapshot.width - 1);
    const int min_z = std::clamp(unclamped_min_z, 0, snapshot.height - 1);
    const int max_z = std::clamp(unclamped_max_z, 0, snapshot.height - 1);
    for (int grid_z = min_z; grid_z <= max_z; ++grid_z) {
      for (int grid_x = min_x; grid_x <= max_x; ++grid_x) {
        if (state_matches(snapshot.state_at(grid_x, grid_z), mode)) {
          return true;
        }
      }
    }
    return false;
  }

  const Camera* m_camera = nullptr;
  const Game::Map::VisibilityService::Snapshot* m_snapshot = nullptr;
};

} // namespace Render::GL
