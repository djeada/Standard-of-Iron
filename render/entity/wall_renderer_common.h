#pragma once

#include <QVector3D>

#include <array>
#include <string_view>
#include <utility>

#include "../render_archetype.h"
#include "../submitter.h"
#include "building_archetype_desc.h"
#include "building_render_common.h"

namespace Render::GL {

enum class WallVariant {
  Isolated,
  End,
  Straight,
  Corner,
  Tee,
  Cross,
};

struct WallPalette {
  QVector3D wood_light{0.67F, 0.50F, 0.31F};
  QVector3D wood_mid{0.54F, 0.39F, 0.23F};
  QVector3D wood_dark{0.36F, 0.25F, 0.14F};
  QVector3D rope{0.46F, 0.38F, 0.24F};
  QVector3D shadow{0.24F, 0.17F, 0.09F};
  QVector3D masonry_accent{0.67F, 0.50F, 0.31F};
  bool alternate_starts_light{true};
  bool horned_masonry{false};
};

struct WallGeometry {
  bool solid_masonry{false};
  bool earthwork_base{false};
  bool cross_braced{false};
  bool metal_bands{false};
  bool irregular_stakes{false};
  float connected_span_length{1.04F};
  float open_span_length{0.96F};
  float stake_center_y{1.18F};
  float stake_half_height{1.18F};
  float tip_center_y{2.48F};
  float tip_half_height{0.18F};
  float lower_lashing_y{0.74F};
  float upper_lashing_y{1.52F};
  float x_lashing_half_z{0.20F};
  float x_shadow_y{0.30F};
  float x_shadow_half_z{0.24F};
  float x_stake_half_x{0.15F};
  float x_stake_half_z{0.19F};
  float x_tip_half_x{0.08F};
  float x_tip_half_z{0.11F};
  float x_cap_half_x{0.20F};
  float x_cap_half_z{0.24F};
  float z_lashing_half_x{0.20F};
  float z_shadow_y{0.30F};
  float z_shadow_half_x{0.24F};
  float z_stake_half_x{0.19F};
  float z_stake_half_z{0.15F};
  float z_tip_half_x{0.11F};
  float z_tip_half_z{0.08F};
  float z_cap_half_x{0.24F};
  float z_cap_half_z{0.20F};
  float center_base_y{0.18F};
  float center_base_half_x{0.52F};
  float center_base_half_z{0.52F};
  float center_post_half_x{0.26F};
  float center_post_half_z{0.26F};
  float center_tip_half_x{0.11F};
  float center_tip_half_z{0.11F};
  float center_tip_y_offset{0.02F};
};

auto build_wall_archetype_set(std::string_view name_prefix,
                              const WallPalette& palette,
                              const WallGeometry& geometry)
    -> std::array<RenderArchetype, 6>;
auto wall_renderer_variants()
    -> const std::array<std::pair<std::string_view, WallVariant>, 7>&;
void submit_wall_segment_variant(ISubmitter& out,
                                 const DrawContext& ctx,
                                 const std::array<RenderArchetype, 6>& archetypes,
                                 WallVariant variant);

} // namespace Render::GL
