#include "wall_renderer_common.h"

#include <array>
#include <cstdint>
#include <string>

namespace Render::GL {
namespace {

constexpr std::uint8_t k_connection_north = 1U << 0U;
constexpr std::uint8_t k_connection_east = 1U << 1U;
constexpr std::uint8_t k_connection_south = 1U << 2U;
constexpr std::uint8_t k_connection_west = 1U << 3U;

auto variant_mask(WallVariant variant) -> std::uint8_t {
  switch (variant) {
  case WallVariant::End:
    return k_connection_east;
  case WallVariant::Straight:
    return k_connection_east | k_connection_west;
  case WallVariant::Corner:
    return k_connection_east | k_connection_north;
  case WallVariant::Tee:
    return k_connection_east | k_connection_north | k_connection_south;
  case WallVariant::Cross:
    return k_connection_north | k_connection_east | k_connection_south |
           k_connection_west;
  case WallVariant::Isolated:
  default:
    return 0;
  }
}

auto wall_archetype_index(WallVariant variant) -> std::size_t {
  return static_cast<std::size_t>(variant);
}

auto alternating_stake_color(const WallPalette& palette, int index) -> QVector3D {
  const bool even_index = (index % 2) == 0;
  const bool use_light = palette.alternate_starts_light ? even_index : !even_index;
  return use_light ? palette.wood_light : palette.wood_mid;
}

void add_x_span(BuildingArchetypeDesc& desc,
                const WallPalette& palette,
                const WallGeometry& geometry,
                float direction,
                float length,
                bool capped_outer_end) {
  constexpr int k_stakes = 5;
  const float center_x = direction * (length * 0.5F);

  desc.add_box(QVector3D(center_x, geometry.lower_lashing_y, 0.0F),
               QVector3D(length + 0.06F, 0.09F, geometry.x_lashing_half_z),
               palette.rope,
               BuildingStateMask::All,
               BuildingLODMask::Full);
  desc.add_box(QVector3D(center_x, geometry.upper_lashing_y, 0.0F),
               QVector3D(length + 0.06F, 0.09F, geometry.x_lashing_half_z),
               palette.rope,
               BuildingStateMask::All,
               BuildingLODMask::Full);
  desc.add_box(QVector3D(center_x, geometry.x_shadow_y, 0.0F),
               QVector3D(length + 0.08F, 0.07F, geometry.x_shadow_half_z),
               palette.shadow,
               BuildingStateMask::All,
               BuildingLODMask::Full);

  for (int i = 0; i < k_stakes; ++i) {
    const float t = (static_cast<float>(i) + 0.5F) / static_cast<float>(k_stakes);
    const float x = direction * (t * length);
    desc.add_box(QVector3D(x, geometry.stake_center_y, 0.0F),
                 QVector3D(geometry.x_stake_half_x,
                           geometry.stake_half_height * 2.0F,
                           geometry.x_stake_half_z),
                 alternating_stake_color(palette, i),
                 BuildingStateMask::All,
                 BuildingLODMask::Full);
    desc.add_box(QVector3D(x, geometry.tip_center_y, 0.0F),
                 QVector3D(geometry.x_tip_half_x,
                           geometry.tip_half_height * 2.0F,
                           geometry.x_tip_half_z),
                 palette.wood_dark,
                 BuildingStateMask::All,
                 BuildingLODMask::Full);
  }

  if (capped_outer_end) {
    desc.add_box(QVector3D(direction * (length - 0.02F), geometry.stake_center_y, 0.0F),
                 QVector3D(geometry.x_cap_half_x,
                           geometry.stake_half_height * 2.0F,
                           geometry.x_cap_half_z),
                 palette.shadow,
                 BuildingStateMask::All,
                 BuildingLODMask::Full);
  }
}

void add_z_span(BuildingArchetypeDesc& desc,
                const WallPalette& palette,
                const WallGeometry& geometry,
                float direction,
                float length,
                bool capped_outer_end) {
  constexpr int k_stakes = 5;
  const float center_z = direction * (length * 0.5F);

  desc.add_box(QVector3D(0.0F, geometry.lower_lashing_y, center_z),
               QVector3D(geometry.z_lashing_half_x, 0.09F, length + 0.06F),
               palette.rope,
               BuildingStateMask::All,
               BuildingLODMask::Full);
  desc.add_box(QVector3D(0.0F, geometry.upper_lashing_y, center_z),
               QVector3D(geometry.z_lashing_half_x, 0.09F, length + 0.06F),
               palette.rope,
               BuildingStateMask::All,
               BuildingLODMask::Full);
  desc.add_box(QVector3D(0.0F, geometry.z_shadow_y, center_z),
               QVector3D(geometry.z_shadow_half_x, 0.07F, length + 0.08F),
               palette.shadow,
               BuildingStateMask::All,
               BuildingLODMask::Full);

  for (int i = 0; i < k_stakes; ++i) {
    const float t = (static_cast<float>(i) + 0.5F) / static_cast<float>(k_stakes);
    const float z = direction * (t * length);
    desc.add_box(QVector3D(0.0F, geometry.stake_center_y, z),
                 QVector3D(geometry.z_stake_half_x,
                           geometry.stake_half_height * 2.0F,
                           geometry.z_stake_half_z),
                 alternating_stake_color(palette, i),
                 BuildingStateMask::All,
                 BuildingLODMask::Full);
    desc.add_box(QVector3D(0.0F, geometry.tip_center_y, z),
                 QVector3D(geometry.z_tip_half_x,
                           geometry.tip_half_height * 2.0F,
                           geometry.z_tip_half_z),
                 palette.wood_dark,
                 BuildingStateMask::All,
                 BuildingLODMask::Full);
  }

  if (capped_outer_end) {
    desc.add_box(QVector3D(0.0F, geometry.stake_center_y, direction * (length - 0.02F)),
                 QVector3D(geometry.z_cap_half_x,
                           geometry.stake_half_height * 2.0F,
                           geometry.z_cap_half_z),
                 palette.shadow,
                 BuildingStateMask::All,
                 BuildingLODMask::Full);
  }
}

void add_center_post(BuildingArchetypeDesc& desc,
                     const WallPalette& palette,
                     const WallGeometry& geometry) {
  desc.add_box(
      QVector3D(0.0F, geometry.center_base_y, 0.0F),
      QVector3D(geometry.center_base_half_x, 0.06F, geometry.center_base_half_z),
      palette.wood_dark,
      BuildingStateMask::All,
      BuildingLODMask::Full);
  desc.add_box(QVector3D(0.0F, geometry.stake_center_y, 0.0F),
               QVector3D(geometry.center_post_half_x,
                         geometry.stake_half_height * 2.0F,
                         geometry.center_post_half_z),
               palette.shadow,
               BuildingStateMask::All,
               BuildingLODMask::Full);
  desc.add_box(
      QVector3D(0.0F, geometry.tip_center_y + geometry.center_tip_y_offset, 0.0F),
      QVector3D(geometry.center_tip_half_x,
                (geometry.tip_half_height + geometry.center_tip_y_offset) * 2.0F,
                geometry.center_tip_half_z),
      palette.wood_dark,
      BuildingStateMask::All,
      BuildingLODMask::Full);
}

} // namespace

auto build_wall_archetype_set(std::string_view name_prefix,
                              const WallPalette& palette,
                              const WallGeometry& geometry)
    -> std::array<RenderArchetype, 6> {
  std::array<RenderArchetype, 6> out{};

  for (int i = 0; i < static_cast<int>(out.size()); ++i) {
    const auto variant = static_cast<WallVariant>(i);
    const auto mask = variant_mask(variant);
    BuildingArchetypeDesc desc(std::string(name_prefix) + "_" + std::to_string(i));

    add_center_post(desc, palette, geometry);

    if (variant == WallVariant::Isolated) {
      add_x_span(desc, palette, geometry, -1.0F, geometry.open_span_length, true);
      add_x_span(desc, palette, geometry, 1.0F, geometry.open_span_length, true);
    } else if (variant == WallVariant::End) {
      add_x_span(desc, palette, geometry, 1.0F, geometry.connected_span_length, false);
      add_x_span(desc, palette, geometry, -1.0F, geometry.open_span_length, true);
    } else {
      if ((mask & k_connection_east) != 0U) {
        add_x_span(
            desc, palette, geometry, 1.0F, geometry.connected_span_length, false);
      }
      if ((mask & k_connection_west) != 0U) {
        add_x_span(
            desc, palette, geometry, -1.0F, geometry.connected_span_length, false);
      }
      if ((mask & k_connection_north) != 0U) {
        add_z_span(
            desc, palette, geometry, -1.0F, geometry.connected_span_length, false);
      }
      if ((mask & k_connection_south) != 0U) {
        add_z_span(
            desc, palette, geometry, 1.0F, geometry.connected_span_length, false);
      }
    }

    out[static_cast<std::size_t>(i)] =
        build_building_archetype(desc, BuildingState::Normal);
  }

  return out;
}

auto wall_renderer_variants()
    -> const std::array<std::pair<std::string_view, WallVariant>, 7>& {
  static const std::array<std::pair<std::string_view, WallVariant>, 7> k_variants = {{
      {"wall_segment", WallVariant::Straight},
      {"wall_segment_isolated", WallVariant::Isolated},
      {"wall_segment_end", WallVariant::End},
      {"wall_segment_straight", WallVariant::Straight},
      {"wall_segment_corner", WallVariant::Corner},
      {"wall_segment_tee", WallVariant::Tee},
      {"wall_segment_cross", WallVariant::Cross},
  }};
  return k_variants;
}

void submit_wall_segment_variant(ISubmitter& out,
                                 const DrawContext& ctx,
                                 const std::array<RenderArchetype, 6>& archetypes,
                                 WallVariant variant) {
  submit_building_instance(out, ctx, archetypes[wall_archetype_index(variant)]);
  draw_building_selection_overlay(out, ctx, BuildingSelectionStyle{2.0F, 2.0F});
}

} // namespace Render::GL
