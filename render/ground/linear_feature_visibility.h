#pragma once

#include <QVector3D>

#include <algorithm>
#include <cmath>

#include "../../game/map/render_visibility_rules.h"

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

[[nodiscard]] inline auto
recommended_linear_feature_visibility_sample_count(float segment_length,
                                                   float tile_size) -> int {
  const float safe_tile = std::max(tile_size, 0.001F);
  return std::max(2, static_cast<int>(std::ceil(segment_length / safe_tile)) + 1);
}

inline auto evaluate_linear_feature_visibility(
    const Game::Map::VisibilityService::Snapshot* snapshot,
    const QVector3D& start,
    const QVector3D& end,
    const LinearFeatureVisibilityOptions& options = {})
    -> LinearFeatureVisibilityResult {
  LinearFeatureVisibilityResult result;
  if (snapshot == nullptr || !snapshot->initialized) {
    return result;
  }

  const int sample_count = std::max(2, options.sample_count);
  auto max_visibility_state = Game::Map::RenderVisibilityState::Hidden;
  bool any_sample_in_bounds = false;

  for (int i = 0; i < sample_count; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(sample_count - 1);
    const QVector3D sample = start + (end - start) * t;
    any_sample_in_bounds =
        any_sample_in_bounds || Game::Map::is_world_position_in_visibility_bounds(
                                    *snapshot, sample.x(), sample.z());

    const auto sample_visibility =
        Game::Map::classify_world_visibility(*snapshot, sample.x(), sample.z());
    if (sample_visibility == Game::Map::RenderVisibilityState::Visible) {
      max_visibility_state = Game::Map::RenderVisibilityState::Visible;
      break;
    }
    if (sample_visibility == Game::Map::RenderVisibilityState::Explored) {
      max_visibility_state = Game::Map::RenderVisibilityState::Explored;
    }
  }

  if (!any_sample_in_bounds && options.treat_out_of_bounds_as_visible) {
    return result;
  }

  if (max_visibility_state == Game::Map::RenderVisibilityState::Hidden) {
    result.visible = false;
    return result;
  }

  if (max_visibility_state == Game::Map::RenderVisibilityState::Explored) {
    result.alpha = options.explored_alpha;
    result.color_multiplier = options.explored_tint;
  }

  return result;
}

} // namespace Render::Ground
