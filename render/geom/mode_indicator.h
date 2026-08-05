#pragma once

#include <QVector3D>

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>

#include "../../game/systems/unit_activity.h"
#include "../gl/mesh.h"
#include "icon_glyph.h"

namespace Render::Geom {

using IndicatorKind = Game::Systems::ActivityKind;
using IndicatorState = Game::Systems::ActivityState;

inline constexpr std::size_t k_indicator_kind_count =
    static_cast<std::size_t>(IndicatorKind::Blocked) + 1U;

constexpr float k_indicator_height_base = 2.05F;
constexpr float k_indicator_alpha = 0.95F;
constexpr float k_indicator_height_multiplier = 1.35F;
constexpr float k_indicator_render_scale_height_multiplier = 2.2F;
constexpr float k_indicator_head_gap = 0.45F;
constexpr float k_frustum_cull_margin = 1.5F;
constexpr float k_indicator_tilt_radians = 0.24F;

constexpr float k_indicator_world_size = 1.05F;

constexpr float k_indicator_fade_start_sq = 3600.0F;
constexpr float k_indicator_fade_end_sq = 8100.0F;

[[nodiscard]] constexpr auto indicator_world_size() noexcept -> float {
  return k_indicator_world_size;
}

[[nodiscard]] constexpr auto
indicator_height_for_unit(float selection_ring_size,
                          float render_scale) noexcept -> float {
  float const footprint_height =
      std::max(selection_ring_size, 0.0F) * k_indicator_height_multiplier;
  float const scaled_height =
      std::max(render_scale, 0.0F) * k_indicator_render_scale_height_multiplier;
  float const anchor_height =
      std::max(k_indicator_height_base, std::max(footprint_height, scaled_height));
  return anchor_height + k_indicator_head_gap;
}

[[nodiscard]] constexpr auto
indicator_distance_fade(float distance_sq) noexcept -> float {
  if (distance_sq <= k_indicator_fade_start_sq) {
    return 1.0F;
  }
  if (distance_sq >= k_indicator_fade_end_sq) {
    return 0.0F;
  }
  float const span = k_indicator_fade_end_sq - k_indicator_fade_start_sq;
  return 1.0F - (distance_sq - k_indicator_fade_start_sq) / span;
}

[[nodiscard]] auto indicator_has_glyph(IndicatorKind kind) noexcept -> bool;

[[nodiscard]] auto indicator_base_color(IndicatorKind kind) noexcept -> QVector3D;

[[nodiscard]] auto indicator_color(IndicatorKind kind,
                                   IndicatorState state) noexcept -> QVector3D;

[[nodiscard]] auto indicator_state_alpha(IndicatorState state) noexcept -> float;

[[nodiscard]] auto build_indicator_glyph(IndicatorKind kind) -> GlyphBuilder;

class ModeIndicator {
public:
  static auto mesh_for(IndicatorKind kind) -> Render::GL::Mesh*;
  static void release_meshes();

private:
  static std::array<std::unique_ptr<Render::GL::Mesh>, k_indicator_kind_count> s_meshes;
};

} // namespace Render::Geom
