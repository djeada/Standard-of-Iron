#pragma once

#include "../../game/map/visibility_service.h"
#include <QVector3D>
#include <algorithm>
#include <cmath>

namespace Render::Ground {

struct LinearFeatureVisibilityOptions {
  int sample_count = 5;
  bool treat_out_of_bounds_as_visible = false;
  float explored_alpha = 0.5F;
  QVector3D explored_tint{0.4F, 0.4F, 0.45F};
};

struct LinearFeatureVisibilityResult {
  bool visible = true;
  float alpha = 1.0F;
  QVector3D color_multiplier{1.0F, 1.0F, 1.0F};
};

inline auto evaluate_linear_feature_visibility(
    const Game::Map::VisibilityService::Snapshot *snapshot,
    const QVector3D &start, const QVector3D &end,
    const LinearFeatureVisibilityOptions &options = {})
    -> LinearFeatureVisibilityResult {
  LinearFeatureVisibilityResult result;
  if (snapshot == nullptr || !snapshot->initialized) {
    return result;
  }

  auto is_in_bounds = [&](const QVector3D &position) -> bool {
    constexpr float k_half_cell_offset = 0.5F;
    const float grid_x =
        position.x() / snapshot->tile_size + snapshot->half_width;
    const float grid_z =
        position.z() / snapshot->tile_size + snapshot->half_height;
    const int ix = static_cast<int>(std::floor(grid_x + k_half_cell_offset));
    const int iz = static_cast<int>(std::floor(grid_z + k_half_cell_offset));
    return ix >= 0 && ix < snapshot->width && iz >= 0 && iz < snapshot->height;
  };

  const int sample_count = std::max(2, options.sample_count);
  int max_visibility_state = 0;
  bool any_sample_in_bounds = false;

  for (int i = 0; i < sample_count; ++i) {
    const float t =
        static_cast<float>(i) / static_cast<float>(sample_count - 1);
    const QVector3D sample = start + (end - start) * t;
    any_sample_in_bounds = any_sample_in_bounds || is_in_bounds(sample);

    if (snapshot->isVisibleWorld(sample.x(), sample.z())) {
      max_visibility_state = 2;
      break;
    }
    if (snapshot->isExploredWorld(sample.x(), sample.z())) {
      max_visibility_state = std::max(max_visibility_state, 1);
    }
  }

  if (!any_sample_in_bounds && options.treat_out_of_bounds_as_visible) {
    return result;
  }

  if (max_visibility_state == 0) {
    result.visible = false;
    return result;
  }

  if (max_visibility_state == 1) {
    result.alpha = options.explored_alpha;
    result.color_multiplier = options.explored_tint;
  }

  return result;
}

} // namespace Render::Ground
