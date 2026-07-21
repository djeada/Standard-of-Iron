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

auto span_stake_position(int index, int stake_count, bool connected_span) -> float {
  if (!connected_span || stake_count <= 1) {
    return (static_cast<float>(index) + 0.5F) / static_cast<float>(stake_count);
  }

  static constexpr float k_connected_start_t = 0.06F;
  static constexpr float k_connected_end_t = 0.98F;
  const auto denom = static_cast<float>(stake_count - 1);
  return k_connected_start_t + ((k_connected_end_t - k_connected_start_t) *
                                (static_cast<float>(index) / denom));
}

auto stake_height_variation(const WallGeometry& geometry, int index) -> float {
  if (!geometry.irregular_stakes) {
    return 0.0F;
  }
  constexpr std::array<float, 5> k_variation{-0.08F, 0.04F, -0.02F, 0.07F, -0.05F};
  return k_variation[static_cast<std::size_t>(index) % k_variation.size()];
}

void add_x_braces(BuildingArchetypeDesc& desc,
                  const WallPalette& palette,
                  const WallGeometry& geometry,
                  float direction,
                  float length,
                  float depth) {
  const QVector3D low(direction * length * 0.08F, 0.34F, -depth);
  const QVector3D high(direction * length * 0.92F, geometry.upper_lashing_y, -depth);
  desc.add_cylinder(low,
                    high,
                    0.055F,
                    palette.wood_dark,
                    BuildingStateMask::All,
                    BuildingLODMask::Full);
  if (geometry.cross_braced) {
    desc.add_cylinder(QVector3D(high.x(), low.y(), -depth),
                      QVector3D(low.x(), high.y(), -depth),
                      0.052F,
                      palette.shadow,
                      BuildingStateMask::All,
                      BuildingLODMask::Full);
  }
}

void add_z_braces(BuildingArchetypeDesc& desc,
                  const WallPalette& palette,
                  const WallGeometry& geometry,
                  float direction,
                  float length,
                  float depth) {
  const QVector3D low(-depth, 0.34F, direction * length * 0.08F);
  const QVector3D high(-depth, geometry.upper_lashing_y, direction * length * 0.92F);
  desc.add_cylinder(low,
                    high,
                    0.055F,
                    palette.wood_dark,
                    BuildingStateMask::All,
                    BuildingLODMask::Full);
  if (geometry.cross_braced) {
    desc.add_cylinder(QVector3D(-depth, low.y(), high.z()),
                      QVector3D(-depth, high.y(), low.z()),
                      0.052F,
                      palette.shadow,
                      BuildingStateMask::All,
                      BuildingLODMask::Full);
  }
}

void add_x_span(BuildingArchetypeDesc& desc,
                const WallPalette& palette,
                const WallGeometry& geometry,
                float direction,
                float length,
                bool capped_outer_end) {
  constexpr int k_stakes = 5;
  const bool connected_span = !capped_outer_end;
  const float center_x = direction * (length * 0.5F);
  const float stake_radius =
      (geometry.x_stake_half_x + geometry.x_stake_half_z) * 0.29F;

  desc.add_cylinder(
      QVector3D(0.0F, geometry.lower_lashing_y, -stake_radius * 0.55F),
      QVector3D(direction * length, geometry.lower_lashing_y, -stake_radius * 0.55F),
      0.070F,
      palette.wood_dark);
  desc.add_cylinder(
      QVector3D(0.0F, geometry.upper_lashing_y, -stake_radius * 0.55F),
      QVector3D(direction * length, geometry.upper_lashing_y, -stake_radius * 0.55F),
      0.064F,
      palette.wood_dark);
  desc.add_box(QVector3D(center_x, geometry.x_shadow_y, 0.0F),
               QVector3D(length + 0.08F, 0.06F, geometry.x_shadow_half_z),
               palette.shadow);
  add_x_braces(desc, palette, geometry, direction, length, stake_radius * 0.72F);

  for (int i = 0; i < k_stakes; ++i) {
    const float t = span_stake_position(i, k_stakes, connected_span);
    const float x = direction * (t * length);
    const float variation = stake_height_variation(geometry, i);
    const float top_y = geometry.stake_half_height * 2.0F + variation;
    const float radius = stake_radius * (0.94F + 0.035F * static_cast<float>(i % 3));
    desc.add_cylinder(QVector3D(x, 0.04F, 0.0F),
                      QVector3D(x, top_y, 0.0F),
                      radius,
                      alternating_stake_color(palette, i));
    desc.add_cone(QVector3D(x, top_y - 0.01F, 0.0F),
                  QVector3D(x, top_y + geometry.tip_half_height * 2.0F, 0.0F),
                  radius * 1.04F,
                  palette.wood_dark);
    const QVector3D binding_color =
        geometry.metal_bands ? palette.masonry_accent : palette.rope;
    for (float y : {geometry.lower_lashing_y, geometry.upper_lashing_y}) {
      desc.add_box(QVector3D(x, y, 0.0F),
                   QVector3D(radius * 2.25F, 0.055F, radius * 2.25F),
                   binding_color,
                   BuildingStateMask::All,
                   BuildingLODMask::Full);
    }
  }

  if (capped_outer_end) {
    const float x = direction * (length - 0.02F);
    const float radius = (geometry.x_cap_half_x + geometry.x_cap_half_z) * 0.29F;
    const float top_y = geometry.stake_half_height * 2.0F + 0.10F;
    desc.add_cylinder(
        QVector3D(x, 0.03F, 0.0F), QVector3D(x, top_y, 0.0F), radius, palette.shadow);
    desc.add_cone(QVector3D(x, top_y, 0.0F),
                  QVector3D(x, top_y + geometry.tip_half_height * 2.0F, 0.0F),
                  radius,
                  palette.wood_dark);
  }
}

void add_z_span(BuildingArchetypeDesc& desc,
                const WallPalette& palette,
                const WallGeometry& geometry,
                float direction,
                float length,
                bool capped_outer_end) {
  constexpr int k_stakes = 5;
  const bool connected_span = !capped_outer_end;
  const float center_z = direction * (length * 0.5F);
  const float stake_radius =
      (geometry.z_stake_half_x + geometry.z_stake_half_z) * 0.29F;

  desc.add_cylinder(
      QVector3D(-stake_radius * 0.55F, geometry.lower_lashing_y, 0.0F),
      QVector3D(-stake_radius * 0.55F, geometry.lower_lashing_y, direction * length),
      0.070F,
      palette.wood_dark);
  desc.add_cylinder(
      QVector3D(-stake_radius * 0.55F, geometry.upper_lashing_y, 0.0F),
      QVector3D(-stake_radius * 0.55F, geometry.upper_lashing_y, direction * length),
      0.064F,
      palette.wood_dark);
  desc.add_box(QVector3D(0.0F, geometry.z_shadow_y, center_z),
               QVector3D(geometry.z_shadow_half_x, 0.06F, length + 0.08F),
               palette.shadow);
  add_z_braces(desc, palette, geometry, direction, length, stake_radius * 0.72F);

  for (int i = 0; i < k_stakes; ++i) {
    const float t = span_stake_position(i, k_stakes, connected_span);
    const float z = direction * (t * length);
    const float variation = stake_height_variation(geometry, i);
    const float top_y = geometry.stake_half_height * 2.0F + variation;
    const float radius = stake_radius * (0.94F + 0.035F * static_cast<float>(i % 3));
    desc.add_cylinder(QVector3D(0.0F, 0.04F, z),
                      QVector3D(0.0F, top_y, z),
                      radius,
                      alternating_stake_color(palette, i));
    desc.add_cone(QVector3D(0.0F, top_y - 0.01F, z),
                  QVector3D(0.0F, top_y + geometry.tip_half_height * 2.0F, z),
                  radius * 1.04F,
                  palette.wood_dark);
    const QVector3D binding_color =
        geometry.metal_bands ? palette.masonry_accent : palette.rope;
    for (float y : {geometry.lower_lashing_y, geometry.upper_lashing_y}) {
      desc.add_box(QVector3D(0.0F, y, z),
                   QVector3D(radius * 2.25F, 0.055F, radius * 2.25F),
                   binding_color,
                   BuildingStateMask::All,
                   BuildingLODMask::Full);
    }
  }

  if (capped_outer_end) {
    const float z = direction * (length - 0.02F);
    const float radius = (geometry.z_cap_half_x + geometry.z_cap_half_z) * 0.29F;
    const float top_y = geometry.stake_half_height * 2.0F + 0.10F;
    desc.add_cylinder(
        QVector3D(0.0F, 0.03F, z), QVector3D(0.0F, top_y, z), radius, palette.shadow);
    desc.add_cone(QVector3D(0.0F, top_y, z),
                  QVector3D(0.0F, top_y + geometry.tip_half_height * 2.0F, z),
                  radius,
                  palette.wood_dark);
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
  const float radius =
      (geometry.center_post_half_x + geometry.center_post_half_z) * 0.29F;
  const float top_y = geometry.stake_half_height * 2.0F + 0.12F;
  desc.add_cylinder(QVector3D(0.0F, 0.03F, 0.0F),
                    QVector3D(0.0F, top_y, 0.0F),
                    radius,
                    palette.shadow);
  desc.add_cone(
      QVector3D(0.0F, top_y, 0.0F),
      QVector3D(0.0F,
                top_y + geometry.tip_half_height * 2.0F + geometry.center_tip_y_offset,
                0.0F),
      radius,
      palette.wood_dark);
}

void add_masonry_span(BuildingArchetypeDesc& desc,
                      const WallPalette& palette,
                      float direction_x,
                      float direction_z,
                      float length) {
  const bool along_x = direction_x != 0.0F;
  const QVector3D center(
      direction_x * length * 0.5F, 0.78F, direction_z * length * 0.5F);
  const QVector3D size = along_x ? QVector3D(length + 0.08F, 1.34F, 0.38F)
                                 : QVector3D(0.38F, 1.34F, length + 0.08F);
  desc.add_box(center, size, palette.wood_mid);
  desc.add_box(QVector3D(center.x(), 0.18F, center.z()),
               along_x ? QVector3D(length + 0.14F, 0.16F, 0.48F)
                       : QVector3D(0.48F, 0.16F, length + 0.14F),
               palette.shadow);
  desc.add_box(QVector3D(center.x(), 1.48F, center.z()),
               along_x ? QVector3D(length + 0.12F, 0.12F, 0.46F)
                       : QVector3D(0.46F, 0.12F, length + 0.12F),
               palette.masonry_accent);

  constexpr int k_merlons = 4;
  for (int i = 0; i < k_merlons; ++i) {
    const float t = (static_cast<float>(i) + 0.5F) / k_merlons;
    const float offset =
        direction_x != 0.0F ? direction_x * t * length : direction_z * t * length;
    const QVector3D merlon_center =
        along_x ? QVector3D(offset, 1.68F, 0.0F) : QVector3D(0.0F, 1.68F, offset);
    desc.add_box(merlon_center,
                 along_x ? QVector3D(0.18F, 0.26F, 0.44F)
                         : QVector3D(0.44F, 0.26F, 0.18F),
                 (i % 2 == 0) ? palette.masonry_accent : palette.wood_light,
                 BuildingStateMask::All,
                 BuildingLODMask::Full);
    if (palette.horned_masonry && i % 2 == 0) {
      desc.add_box(merlon_center + QVector3D(0.0F, 0.40F, 0.0F),
                   along_x ? QVector3D(0.09F, 0.20F, 0.24F)
                           : QVector3D(0.24F, 0.20F, 0.09F),
                   palette.wood_dark,
                   BuildingStateMask::All,
                   BuildingLODMask::Full);
      desc.add_box(merlon_center + QVector3D(0.0F, 0.61F, 0.0F),
                   along_x ? QVector3D(0.055F, 0.09F, 0.16F)
                           : QVector3D(0.16F, 0.09F, 0.055F),
                   palette.masonry_accent,
                   BuildingStateMask::All,
                   BuildingLODMask::Full);
    }
  }
}

void add_masonry_center(BuildingArchetypeDesc& desc, const WallPalette& palette) {
  desc.add_box(
      QVector3D(0.0F, 0.78F, 0.0F), QVector3D(0.52F, 1.40F, 0.52F), palette.wood_dark);
  desc.add_box(QVector3D(0.0F, 1.52F, 0.0F),
               QVector3D(0.58F, 0.14F, 0.58F),
               palette.masonry_accent);
  if (palette.horned_masonry) {
    desc.add_box(QVector3D(0.0F, 1.83F, 0.0F),
                 QVector3D(0.24F, 0.20F, 0.24F),
                 palette.wood_dark,
                 BuildingStateMask::All,
                 BuildingLODMask::Full);
    desc.add_box(QVector3D(0.0F, 2.08F, 0.0F),
                 QVector3D(0.10F, 0.12F, 0.10F),
                 palette.masonry_accent,
                 BuildingStateMask::All,
                 BuildingLODMask::Full);
  }
}

void add_earthwork_span(BuildingArchetypeDesc& desc,
                        const WallPalette& palette,
                        float direction_x,
                        float direction_z,
                        float length) {
  const bool along_x = direction_x != 0.0F;
  const QVector3D center(
      direction_x * length * 0.5F, 0.12F, direction_z * length * 0.5F);
  desc.add_box(center,
               along_x ? QVector3D(length + 0.12F, 0.20F, 0.68F)
                       : QVector3D(0.68F, 0.20F, length + 0.12F),
               palette.shadow);
  desc.add_box(QVector3D(center.x(), 0.24F, center.z()),
               along_x ? QVector3D(length + 0.08F, 0.10F, 0.52F)
                       : QVector3D(0.52F, 0.10F, length + 0.08F),
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

    if (geometry.solid_masonry) {
      add_masonry_center(desc, palette);
      if (variant == WallVariant::Isolated || variant == WallVariant::End ||
          (mask & k_connection_east) != 0U) {
        add_masonry_span(desc, palette, 1.0F, 0.0F, geometry.connected_span_length);
      }
      if (variant == WallVariant::Isolated || variant == WallVariant::End ||
          (mask & k_connection_west) != 0U) {
        add_masonry_span(desc, palette, -1.0F, 0.0F, geometry.connected_span_length);
      }
      if ((mask & k_connection_north) != 0U) {
        add_masonry_span(desc, palette, 0.0F, -1.0F, geometry.connected_span_length);
      }
      if ((mask & k_connection_south) != 0U) {
        add_masonry_span(desc, palette, 0.0F, 1.0F, geometry.connected_span_length);
      }
      out[static_cast<std::size_t>(i)] =
          build_building_archetype(desc, BuildingState::Normal);
      continue;
    }

    add_center_post(desc, palette, geometry);

    if (geometry.earthwork_base) {
      if (variant == WallVariant::Isolated || variant == WallVariant::End ||
          (mask & k_connection_east) != 0U) {
        add_earthwork_span(desc, palette, 1.0F, 0.0F, geometry.connected_span_length);
      }
      if (variant == WallVariant::Isolated || variant == WallVariant::End ||
          (mask & k_connection_west) != 0U) {
        add_earthwork_span(desc, palette, -1.0F, 0.0F, geometry.connected_span_length);
      }
      if ((mask & k_connection_north) != 0U) {
        add_earthwork_span(desc, palette, 0.0F, -1.0F, geometry.connected_span_length);
      }
      if ((mask & k_connection_south) != 0U) {
        add_earthwork_span(desc, palette, 0.0F, 1.0F, geometry.connected_span_length);
      }
    }

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
