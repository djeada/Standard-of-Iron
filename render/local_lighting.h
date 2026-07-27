#pragma once

#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace Render {

inline constexpr std::size_t k_max_local_lights = 8;

struct LocalLight {
  QVector3D position;
  QVector3D color{1.0F, 0.72F, 0.42F};
  float radius = 6.0F;
  float intensity = 1.0F;
  bool casts_shadow = false;
};

[[nodiscard]] inline auto select_local_lights(const std::vector<LocalLight>& lights,
                                              const QVector3D& camera_position)
    -> std::array<LocalLight, k_max_local_lights> {

  struct Ranked {
    LocalLight light;
    float score = -1.0F;
  };

  std::array<Ranked, k_max_local_lights> ranked{};
  std::size_t filled = 0;

  for (const auto& source : lights) {
    LocalLight light = source;
    light.radius = std::max(light.radius, 0.01F);
    light.intensity = std::max(light.intensity, 0.0F);

    const float distance_squared =
        std::max((light.position - camera_position).lengthSquared(), 1.0F);
    const float score =
        light.intensity * light.radius * light.radius / distance_squared;

    if (filled == ranked.size() && score <= ranked[filled - 1].score) {
      continue;
    }

    std::size_t slot = filled;
    while (slot > 0 && score > ranked[slot - 1].score) {
      --slot;
    }
    if (slot >= ranked.size()) {
      continue;
    }

    const std::size_t last = std::min(filled, ranked.size() - 1);
    for (std::size_t i = last; i > slot; --i) {
      ranked[i] = ranked[i - 1];
    }
    ranked[slot] = Ranked{light, score};
    filled = std::min(filled + 1, ranked.size());
  }

  std::array<LocalLight, k_max_local_lights> result{};
  for (std::size_t i = 0; i < ranked.size(); ++i) {
    if (i < filled) {
      result[i] = ranked[i].light;
      continue;
    }
    result[i].radius = 0.0F;
    result[i].intensity = 0.0F;
  }
  return result;
}

} // namespace Render
