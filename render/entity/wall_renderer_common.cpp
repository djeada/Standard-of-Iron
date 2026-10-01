#include "wall_renderer_common.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <string>

#include "building_archetype_library.h"
#include "building_decay.h"
#include "game/core/component_gameplay.h"
#include "game/core/wall_walk_geometry.h"
#include "game/core/world.h"

namespace Render::GL {
namespace {

constexpr auto k_mask_intact = k_building_state_mask_intact;
constexpr auto k_mask_broken = static_cast<BuildingStateMask>(
    static_cast<std::uint8_t>(BuildingStateMask::Damaged) |
    static_cast<std::uint8_t>(BuildingStateMask::Destroyed));

auto span_seed(std::size_t dir, int index) -> int {
  return (static_cast<int>(dir) * 101) + (index * 7) + 3;
}

constexpr std::uint8_t k_connection_north = 1U << 0U;
constexpr std::uint8_t k_connection_east = 1U << 1U;
constexpr std::uint8_t k_connection_south = 1U << 2U;
constexpr std::uint8_t k_connection_west = 1U << 3U;

constexpr std::size_t k_dir_north = 0U;
constexpr std::size_t k_dir_east = 1U;
constexpr std::size_t k_dir_south = 2U;
constexpr std::size_t k_dir_west = 3U;
constexpr std::size_t k_dir_count = 4U;

constexpr float k_connected_span_length = 1.0F;
constexpr float k_stake_spacing = 0.2F;

constexpr float k_span_end_berm = 0.000F;
constexpr float k_span_end_bank = -0.024F;
constexpr float k_span_end_plank = 0.008F;
constexpr float k_span_end_seam = -0.012F;

struct PlanarDir {
  float x{0.0F};
  float z{0.0F};
};

constexpr std::array<PlanarDir, k_dir_count> k_directions{PlanarDir{0.0F, -1.0F},
                                                          PlanarDir{1.0F, 0.0F},
                                                          PlanarDir{0.0F, 1.0F},
                                                          PlanarDir{-1.0F, 0.0F}};

constexpr std::array<std::uint8_t, k_dir_count> k_direction_bits{
    k_connection_north, k_connection_east, k_connection_south, k_connection_west};

enum class SpanKind : std::uint8_t {
  None,
  Connected,
  Open
};

using WallLayout = std::array<SpanKind, k_dir_count>;

struct SpanAxis {
  std::size_t positive{0U};
  std::size_t negative{0U};
};

constexpr std::array<SpanAxis, 2> k_span_axes{SpanAxis{k_dir_east, k_dir_west},
                                              SpanAxis{k_dir_south, k_dir_north}};

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

auto layout_for(WallVariant variant) -> WallLayout {
  WallLayout layout{};
  layout.fill(SpanKind::None);

  const auto mask = variant_mask(variant);
  for (std::size_t dir = 0; dir < k_dir_count; ++dir) {
    if ((mask & k_direction_bits[dir]) != 0U) {
      layout[dir] = SpanKind::Connected;
    }
  }

  if (variant == WallVariant::Isolated) {
    layout[k_dir_east] = SpanKind::Open;
    layout[k_dir_west] = SpanKind::Open;
  } else if (variant == WallVariant::End) {
    layout[k_dir_west] = SpanKind::Open;
  }
  return layout;
}

auto is_present(const WallLayout& layout, std::size_t dir) -> bool {
  return layout[dir] != SpanKind::None;
}

auto span_length(const WallLayout& layout,
                 std::size_t dir,
                 const WallGeometry& geometry) -> float {
  return layout[dir] == SpanKind::Open ? geometry.open_span_length
                                       : k_connected_span_length;
}

auto span_slot_count(float length) -> int {
  const int slot_total = static_cast<int>(std::lround(length / k_stake_spacing)) - 1;
  return slot_total < 1 ? 1 : slot_total;
}

auto slot_offset(int slot) -> float {
  return (static_cast<float>(slot) + 1.5F) * k_stake_spacing;
}

auto rail_offset(const WallGeometry& geometry) -> float {
  return geometry.stake_radius + (geometry.rail_radius * 0.85F);
}

auto terminal_post_radius(const WallGeometry& geometry) -> float {
  return geometry.post_radius * 0.85F;
}

auto terminal_post_offset(const WallGeometry& geometry, float length) -> float {
  return length - terminal_post_radius(geometry);
}

auto rail_reach(const WallLayout& layout,
                std::size_t dir,
                const WallGeometry& geometry) -> float {
  const float length = span_length(layout, dir, geometry);
  return layout[dir] == SpanKind::Open ? terminal_post_offset(geometry, length)
                                       : length;
}

auto point_at(std::size_t dir, float along, float lateral, float height) -> QVector3D {
  const PlanarDir forward = k_directions[dir];
  const PlanarDir side = k_directions[(dir + 1U) % k_dir_count];
  return {(forward.x * along) + (side.x * lateral),
          height,
          (forward.z * along) + (side.z * lateral)};
}

auto extents_at(std::size_t dir,
                float along_half,
                float height_half,
                float lateral_half) -> QVector3D {
  return k_directions[dir].x != 0.0F ? QVector3D(along_half, height_half, lateral_half)
                                     : QVector3D(lateral_half, height_half, along_half);
}

auto alternating_stake_color(const WallPalette& palette, int index) -> QVector3D {
  const bool even_index = (index % 2) == 0;
  const bool use_light = palette.alternate_starts_light ? even_index : !even_index;
  return use_light ? palette.wood_light : palette.wood_mid;
}

auto stake_height_variation(const WallGeometry& geometry, int index) -> float {
  if (!geometry.irregular_stakes) {
    return 0.0F;
  }
  constexpr std::array<float, 5> k_variation{-0.08F, 0.04F, -0.02F, 0.07F, -0.05F};
  return k_variation[static_cast<std::size_t>(index) % k_variation.size()];
}

auto stake_lean(const WallGeometry& geometry, int index) -> float {
  constexpr std::array<float, 5> k_lean{0.008F, -0.013F, 0.004F, -0.007F, 0.012F};
  const float lean = k_lean[static_cast<std::size_t>(index) % k_lean.size()];
  return geometry.irregular_stakes ? lean * 1.6F : lean;
}

auto binding_color(const WallPalette& palette,
                   const WallGeometry& geometry) -> QVector3D {
  return geometry.metal_bands ? palette.masonry_accent : palette.rope;
}

void add_stake_bindings(BuildingArchetypeDesc& desc,
                        const WallPalette& palette,
                        const WallGeometry& geometry,
                        std::size_t dir,
                        float along,
                        float radius,
                        BuildingStateMask states) {
  const float lateral_half = rail_offset(geometry) + (geometry.rail_radius * 0.9F);
  BuildingPartLabel const label(desc, "stake_binding");
  for (const float height : {geometry.lower_rail_y, geometry.upper_rail_y}) {
    desc.add_box(point_at(dir, along, 0.0F, height),
                 extents_at(dir, radius * 0.95F, 0.042F, lateral_half),
                 binding_color(palette, geometry),
                 states);
  }
}

void add_snapped_stake(BuildingArchetypeDesc& desc,
                       const WallPalette& palette,
                       std::size_t dir,
                       float along,
                       float lean,
                       float radius,
                       float top,
                       BuildingStateMask states) {
  desc.add_cylinder(point_at(dir, along, 0.0F, 0.02F),
                    point_at(dir, along, lean, top),
                    radius,
                    palette.wood_mid,
                    states);
  desc.add_cone(point_at(dir, along, lean, top + 0.07F),
                point_at(dir, along, lean, top - 0.02F),
                radius * 1.04F,
                palette.wood_dark * 0.75F,
                states);
}

void add_span_debris(BuildingArchetypeDesc& desc,
                     const WallPalette& palette,
                     std::size_t dir,
                     float length,
                     float lateral_half) {
  const float along_half = length * 0.5F;
  const QVector3D center = point_at(dir, along_half, 0.0F, 0.0F);
  const QVector3D extent = extents_at(dir, along_half * 0.86F, 0.0F, lateral_half);

  add_rubble_field(desc,
                   RubbleField{.center = center,
                               .extent = extent,
                               .stone = palette.rubble,
                               .stone_dark = palette.earth_dark,
                               .chunk_scale = 0.85F,
                               .count = 5,
                               .seed = static_cast<int>(dir) * 23,
                               .states = BuildingStateMask::Damaged});
  add_rubble_field(desc,
                   RubbleField{.center = center,
                               .extent = extent * 1.15F,
                               .stone = palette.rubble,
                               .stone_dark = palette.earth_dark,
                               .chunk_scale = 1.15F,
                               .count = 11,
                               .seed = (static_cast<int>(dir) * 23) + 500,
                               .states = BuildingStateMask::Destroyed});
  add_charred_beams(desc,
                    CharredBeams{.center = center,
                                 .extent = extent,
                                 .timber = palette.wood_dark * 0.45F,
                                 .length = 0.55F,
                                 .radius = 0.05F,
                                 .count = 3,
                                 .seed = (static_cast<int>(dir) * 29) + 90,
                                 .states = BuildingStateMask::Destroyed});
  add_scorch_patch(desc,
                   ScorchPatch{.center = center,
                               .radius = along_half * 0.7F,
                               .count = 4,
                               .seed = (static_cast<int>(dir) * 37) + 130,
                               .states = k_mask_broken});
}

void add_span_stakes(BuildingArchetypeDesc& desc,
                     const WallPalette& palette,
                     const WallGeometry& geometry,
                     std::size_t dir,
                     float length,
                     bool capped) {
  const int slot_total = span_slot_count(length);
  const int stakes = capped ? slot_total - 1 : slot_total;

  for (int i = 0; i < stakes; ++i) {
    const float along = slot_offset(i);
    const float radius =
        geometry.stake_radius * (0.97F + (0.035F * static_cast<float>(i % 3)));
    const float top = geometry.stake_height + stake_height_variation(geometry, i);
    const float lean = stake_lean(geometry, i);

    const float roll = decay_hash(span_seed(dir, i));
    const bool snaps_when_damaged = roll < 0.45F;
    const bool stands_when_destroyed = roll > 0.78F;
    const BuildingStateMask upright_states =
        snaps_when_damaged ? BuildingStateMask::Normal : k_mask_intact;

    desc.add_cylinder(point_at(dir, along, 0.0F, 0.02F),
                      point_at(dir, along, lean, top),
                      radius,
                      alternating_stake_color(palette, i),
                      upright_states);
    desc.add_cone(point_at(dir, along, lean, top - 0.01F),
                  point_at(dir, along, lean, top + geometry.tip_height),
                  radius * 1.05F,
                  palette.wood_dark,
                  upright_states);
    add_stake_bindings(desc, palette, geometry, dir, along, radius, upright_states);

    if (snaps_when_damaged) {
      add_snapped_stake(desc,
                        palette,
                        dir,
                        along,
                        lean,
                        radius,
                        top * (0.38F + (roll * 0.50F)),
                        BuildingStateMask::Damaged);
    }
    if (stands_when_destroyed) {
      add_snapped_stake(desc,
                        palette,
                        dir,
                        along,
                        lean * 2.4F,
                        radius,
                        top * (0.09F + (roll * 0.17F)),
                        BuildingStateMask::Destroyed);
    }
  }

  if (!capped) {
    return;
  }

  const float along = terminal_post_offset(geometry, length);
  const float radius = terminal_post_radius(geometry);
  const float top = geometry.stake_height + (geometry.post_extra_height * 0.5F);
  desc.add_cylinder(point_at(dir, along, 0.0F, 0.02F),
                    point_at(dir, along, 0.0F, top),
                    radius,
                    palette.wood_mid,
                    k_mask_intact);
  desc.add_cone(point_at(dir, along, 0.0F, top - 0.01F),
                point_at(dir, along, 0.0F, top + geometry.tip_height),
                radius * 1.02F,
                palette.wood_dark,
                BuildingStateMask::Normal);
  add_snapped_stake(desc,
                    palette,
                    dir,
                    along,
                    0.05F,
                    radius,
                    top * 0.20F,
                    BuildingStateMask::Destroyed);
}

void add_rails(BuildingArchetypeDesc& desc,
               const WallPalette& palette,
               const WallGeometry& geometry,
               const WallLayout& layout) {
  const float offset = rail_offset(geometry);

  for (const SpanAxis axis : k_span_axes) {
    const bool positive_present = is_present(layout, axis.positive);
    const bool negative_present = is_present(layout, axis.negative);
    if (!positive_present && !negative_present) {
      continue;
    }

    for (int side = -1; side <= 1; side += 2) {
      const std::size_t perpendicular =
          (axis.positive + (side > 0 ? 1U : 3U)) % k_dir_count;
      const float mitre = is_present(layout, perpendicular) ? offset : 0.0F;
      const float high =
          positive_present ? rail_reach(layout, axis.positive, geometry) : mitre;
      const float low =
          negative_present ? -rail_reach(layout, axis.negative, geometry) : -mitre;
      if (high - low < 1.0e-3F) {
        continue;
      }

      const float lateral = static_cast<float>(side) * offset;
      for (const float height : {geometry.lower_rail_y, geometry.upper_rail_y}) {
        const bool upper = height > geometry.lower_rail_y;
        desc.add_cylinder(point_at(axis.positive, low, lateral, height),
                          point_at(axis.positive, high, lateral, height),
                          geometry.rail_radius,
                          palette.wood_dark,
                          upper ? BuildingStateMask::Normal : k_mask_intact);
      }

      desc.add_cylinder(
          point_at(axis.positive, low * 0.72F, lateral * 1.7F, geometry.rail_radius),
          point_at(axis.positive, high * 0.64F, lateral * 2.3F, geometry.rail_radius),
          geometry.rail_radius * 0.92F,
          palette.wood_dark * 0.62F,
          BuildingStateMask::Destroyed);
    }
  }
}

void add_span_backing(BuildingArchetypeDesc& desc,
                      const WallPalette& palette,
                      const WallGeometry& geometry,
                      std::size_t dir,
                      float reach) {
  const float bottom = geometry.earthwork_base ? geometry.berm_height * 0.5F : 0.02F;
  const float top = geometry.upper_rail_y + 0.16F;
  const float half_height = (top - bottom) * 0.5F;
  const float lateral_half = geometry.stake_radius * 0.62F;
  const QVector3D plank = palette.wood_dark * 0.92F;

  BuildingPartLabel const label(desc, "span_backing");

  const float plank_start = lateral_half + 0.03F;
  const float plank_half = (reach + k_span_end_plank - plank_start) * 0.5F;
  desc.add_box(point_at(dir, plank_start + plank_half, 0.0F, bottom + half_height),
               extents_at(dir, plank_half, half_height, lateral_half),
               plank,
               k_mask_intact);

  const float seam_start = plank_start + 0.018F;
  const float seam_half = (reach + k_span_end_seam - seam_start) * 0.5F;
  for (const float seam_y :
       {bottom + (top - bottom) * 0.36F, bottom + (top - bottom) * 0.68F}) {
    desc.add_box(point_at(dir, seam_start + seam_half, 0.0F, seam_y),
                 extents_at(dir, seam_half, 0.012F, lateral_half + 0.006F),
                 palette.wood_dark * 0.62F,
                 k_mask_intact);
  }
}

void add_span_braces(BuildingArchetypeDesc& desc,
                     const WallPalette& palette,
                     const WallGeometry& geometry,
                     std::size_t dir,
                     float length) {
  const float radius = geometry.rail_radius * 0.90F;
  const float foot_lateral = std::min(geometry.berm_half_width * 1.30F - 0.06F, 0.33F);
  const float head_lateral = rail_offset(geometry) + (geometry.rail_radius * 0.6F);
  const float foot_y = geometry.earthwork_base ? geometry.berm_height * 0.55F : 0.02F;
  const float head_y = geometry.upper_rail_y - 0.04F;

  const std::array<float, 2> k_strut_t = geometry.cross_braced
                                             ? std::array<float, 2>{0.42F, 0.78F}
                                             : std::array<float, 2>{0.56F, -1.0F};
  BuildingPartLabel const label(desc, "span_brace");
  for (const float t : k_strut_t) {
    if (t < 0.0F) {
      continue;
    }
    const float along = length * t;
    for (int side = -1; side <= 1; side += 2) {
      const float s = static_cast<float>(side);
      desc.add_cylinder(point_at(dir, along, s * foot_lateral, foot_y),
                        point_at(dir, along, s * head_lateral, head_y),
                        radius,
                        palette.wood_dark,
                        k_mask_intact);
      desc.add_box(point_at(dir, along, s * foot_lateral, foot_y + 0.03F),
                   extents_at(dir, radius * 1.3F, 0.035F, 0.05F),
                   palette.wood_dark * 0.7F,
                   k_mask_intact);
    }
  }
}

void add_junction_post(BuildingArchetypeDesc& desc,
                       const WallPalette& palette,
                       const WallGeometry& geometry) {
  const float top = geometry.stake_height + geometry.post_extra_height;
  desc.add_cylinder(QVector3D(0.0F, 0.02F, 0.0F),
                    QVector3D(0.0F, top, 0.0F),
                    geometry.post_radius,
                    palette.wood_mid,
                    k_mask_intact);
  desc.add_cone(QVector3D(0.0F, top - 0.01F, 0.0F),
                QVector3D(0.0F, top + (geometry.tip_height * 1.1F), 0.0F),
                geometry.post_radius * 1.02F,
                palette.wood_dark,
                BuildingStateMask::Normal);

  desc.add_cylinder(QVector3D(0.0F, 0.02F, 0.0F),
                    QVector3D(0.04F, top * 0.24F, -0.03F),
                    geometry.post_radius * 0.94F,
                    palette.wood_dark,
                    BuildingStateMask::Destroyed);

  for (const float height : {geometry.lower_rail_y, geometry.upper_rail_y}) {
    desc.add_cylinder(QVector3D(0.0F, height - 0.05F, 0.0F),
                      QVector3D(0.0F, height + 0.05F, 0.0F),
                      geometry.post_radius * 1.08F,
                      binding_color(palette, geometry),
                      k_mask_intact);
  }
}

void add_earth_berm(BuildingArchetypeDesc& desc,
                    const WallPalette& palette,
                    const WallGeometry& geometry,
                    const WallLayout& layout) {
  constexpr float k_sink = 0.03F;

  constexpr float k_bank_spread = 1.30F;
  constexpr float k_bank_height_ratio = 0.55F;
  const float half_height = (geometry.berm_height + k_sink) * 0.5F;
  const float center_y = (geometry.berm_height - k_sink) * 0.5F;
  const float half_width = geometry.berm_half_width;
  const float bank_half_width = half_width * k_bank_spread;
  const float bank_half_height =
      ((geometry.berm_height * k_bank_height_ratio) + k_sink) * 0.5F;
  const float bank_center_y =
      ((geometry.berm_height * k_bank_height_ratio) - k_sink) * 0.5F;
  const QVector3D bank_color = palette.earth_light * 0.92F + palette.earth_dark * 0.08F;

  desc.add_box(QVector3D(0.0F, center_y, 0.0F),
               QVector3D(half_width, half_height, half_width),
               palette.earth_light);
  desc.add_box(QVector3D(0.0F, bank_center_y, 0.0F),
               QVector3D(bank_half_width, bank_half_height, bank_half_width),
               bank_color);

  constexpr std::array<float, 2> k_rubble_t{0.34F, 0.72F};
  for (std::size_t dir = 0; dir < k_dir_count; ++dir) {
    if (!is_present(layout, dir)) {
      continue;
    }

    const float length = span_length(layout, dir, geometry);
    if (length - half_width < 0.05F) {
      continue;
    }

    BuildingPartLabel const label(desc, "earthwork");
    const float along_half = (length + k_span_end_berm - half_width) * 0.5F;
    const float along_center = half_width + along_half;
    desc.add_box(point_at(dir, along_center, 0.0F, center_y),
                 extents_at(dir, along_half, half_height, half_width),
                 palette.earth_light);
    const float bank_along_half = (length + k_span_end_bank - bank_half_width) * 0.5F;
    if (bank_along_half > 0.02F) {
      desc.add_box(
          point_at(dir, bank_half_width + bank_along_half, 0.0F, bank_center_y),
          extents_at(dir, bank_along_half, bank_half_height, bank_half_width),
          bank_color);
    }

    const float bank_top = bank_center_y + bank_half_height;
    for (std::size_t i = 0; i < k_rubble_t.size(); ++i) {
      const float along = half_width + ((length - half_width) * k_rubble_t[i]);

      const float lateral =
          (i % 2 == 0) ? bank_half_width - 0.13F : -(bank_half_width - 0.13F);
      const float size = 0.055F + (0.015F * static_cast<float>((dir + i) % 3U));
      desc.add_box(point_at(dir, along, lateral, bank_top),
                   QVector3D(size, size * 0.7F, size),
                   (i % 2 == 0) ? palette.rubble : palette.earth_dark,
                   BuildingStateMask::All);
    }
  }
}

void add_masonry(BuildingArchetypeDesc& desc,
                 const WallPalette& palette,
                 const WallGeometry& geometry,
                 const WallLayout& layout) {
  const float half_width = geometry.masonry_half_width;
  const float half_height = geometry.masonry_height * 0.5F;
  const float center_y = half_height + 0.06F;
  const float coping_y = center_y + half_height + 0.07F;

  const float ruin_scale = 0.58F;
  desc.add_box(QVector3D(0.0F, center_y + 0.06F, 0.0F),
               QVector3D(half_width, half_height + 0.06F, half_width),
               palette.wood_dark,
               k_mask_intact);
  desc.add_box(QVector3D(0.0F, (center_y + 0.06F) * ruin_scale, 0.0F),
               QVector3D(half_width, (half_height + 0.06F) * ruin_scale, half_width),
               palette.wood_dark,
               BuildingStateMask::Destroyed);
  desc.add_box(QVector3D(0.0F, coping_y + 0.12F, 0.0F),
               QVector3D(half_width + 0.05F, 0.07F, half_width + 0.05F),
               palette.masonry_accent,
               BuildingStateMask::Normal);
  if (palette.horned_masonry) {
    desc.add_box(QVector3D(0.0F, coping_y + 0.36F, 0.0F),
                 QVector3D(0.11F, 0.17F, 0.11F),
                 palette.wood_dark,
                 BuildingStateMask::Normal);
  }

  constexpr int k_merlons = 3;
  for (std::size_t dir = 0; dir < k_dir_count; ++dir) {
    if (!is_present(layout, dir)) {
      continue;
    }

    const float length = span_length(layout, dir, geometry);
    if (length - half_width < 0.05F) {
      continue;
    }

    const float along_half = (length - half_width) * 0.5F;
    const float along_center = half_width + along_half;
    desc.add_box(point_at(dir, along_center, 0.0F, center_y),
                 extents_at(dir, along_half, half_height, half_width * 0.82F),
                 palette.wood_mid,
                 k_mask_intact);
    desc.add_box(
        point_at(dir, along_center, 0.0F, center_y * ruin_scale),
        extents_at(dir, along_half, half_height * ruin_scale, half_width * 0.82F),
        palette.wood_mid,
        BuildingStateMask::Destroyed);
    desc.add_box(point_at(dir, along_center, 0.0F, coping_y),
                 extents_at(dir, along_half, 0.07F, half_width * 0.94F),
                 palette.masonry_accent,
                 k_mask_intact);

    for (int i = 0; i < k_merlons; ++i) {
      const float t = (static_cast<float>(i) + 0.5F) / static_cast<float>(k_merlons);
      const float along = half_width + ((length - half_width) * t);
      const float roll = decay_hash(span_seed(dir, i) + 41);
      const bool survives_damage = roll > 0.45F;
      desc.add_box(point_at(dir, along, 0.0F, coping_y + 0.19F),
                   extents_at(dir, 0.09F, 0.13F, half_width * 0.86F),
                   (i % 2 == 0) ? palette.masonry_accent : palette.wood_light,
                   survives_damage ? k_mask_intact : BuildingStateMask::Normal);
      if (!survives_damage) {
        desc.add_box(point_at(dir, along, 0.0F, coping_y + 0.10F),
                     extents_at(dir, 0.085F, 0.04F, half_width * 0.80F),
                     palette.rubble,
                     BuildingStateMask::Damaged);
      }
    }

    add_span_debris(desc, palette, dir, length, half_width);
  }
}

void add_palisade(BuildingArchetypeDesc& desc,
                  const WallPalette& palette,
                  const WallGeometry& geometry,
                  const WallLayout& layout) {
  if (geometry.earthwork_base) {
    add_earth_berm(desc, palette, geometry, layout);
  }

  add_junction_post(desc, palette, geometry);
  add_rails(desc, palette, geometry, layout);

  for (std::size_t dir = 0; dir < k_dir_count; ++dir) {
    if (!is_present(layout, dir)) {
      continue;
    }
    const float length = span_length(layout, dir, geometry);
    add_span_stakes(
        desc, palette, geometry, dir, length, layout[dir] == SpanKind::Open);
    add_span_backing(desc, palette, geometry, dir, rail_reach(layout, dir, geometry));
    add_span_braces(desc, palette, geometry, dir, rail_reach(layout, dir, geometry));
    add_span_debris(desc, palette, dir, length, rail_offset(geometry) * 1.6F);
  }
}

} // namespace

namespace WW = Game::Systems::WallWalk;

auto build_wall_walk_span_desc(std::string_view name_prefix,
                               const WallPalette& palette) -> BuildingArchetypeDesc {
  BuildingArchetypeDesc desc(std::string(name_prefix) + "_walk_span");
  desc.set_material(k_building_material_wood);
  constexpr auto k_standing = static_cast<BuildingStateMask>(
      static_cast<std::uint8_t>(BuildingStateMask::Normal) |
      static_cast<std::uint8_t>(BuildingStateMask::Damaged));
  const float top = WW::k_deck_height;
  const float inner = WW::k_deck_inner_edge;
  const float outer = WW::k_deck_outer_edge;
  constexpr int k_boards = 4;
  const float board = (outer - inner) / static_cast<float>(k_boards);
  for (int i = 0; i < k_boards; ++i) {
    const float z = inner + board * (static_cast<float>(i) + 0.5F);
    const QVector3D tone = (i % 2 == 0) ? palette.wood_light : palette.wood_mid * 1.08F;
    desc.add_box(QVector3D(0.0F, top - WW::k_deck_thickness * 0.5F, z),
                 QVector3D(0.5F, WW::k_deck_thickness * 0.5F, board * 0.47F),
                 tone,
                 k_standing);
  }
  desc.add_box(QVector3D(0.0F, top - 0.11F, outer - 0.03F),
               QVector3D(0.5F, 0.05F, 0.035F),
               palette.wood_dark,
               k_standing);
  desc.add_box(QVector3D(0.0F, top - 0.10F, inner + 0.02F),
               QVector3D(0.5F, 0.045F, 0.03F),
               palette.wood_dark,
               k_standing);
  desc.add_box(QVector3D(0.0F, top - 0.10F, (inner + outer) * 0.5F),
               QVector3D(0.04F, 0.045F, (outer - inner) * 0.5F + 0.02F),
               palette.wood_mid,
               k_standing);
  desc.add_cylinder(QVector3D(0.0F, 0.0F, outer - 0.05F),
                    QVector3D(0.0F, top - 0.10F, outer - 0.05F),
                    0.055F,
                    palette.wood_dark,
                    k_standing);
  desc.add_cylinder(QVector3D(0.0F, top * 0.52F, outer - 0.05F),
                    QVector3D(0.0F, top - 0.14F, inner + 0.10F),
                    0.026F,
                    palette.wood_mid,
                    k_standing);
  desc.add_box(QVector3D(0.0F, top * 0.30F, outer - 0.05F),
               QVector3D(0.07F, 0.018F, 0.07F),
               palette.rope,
               k_standing);
  desc.add_rotated_box(QVector3D(0.05F, 0.04F, 0.55F),
                       QVector3D(0.42F, 0.025F, 0.06F),
                       QVector3D(0.0F, 17.0F, 6.0F),
                       palette.wood_dark,
                       BuildingStateMask::Destroyed);
  return desc;
}

auto build_wall_walk_landing_desc(std::string_view name_prefix,
                                  const WallPalette& palette) -> BuildingArchetypeDesc {
  BuildingArchetypeDesc desc(std::string(name_prefix) + "_walk_landing");
  desc.set_material(k_building_material_wood);
  constexpr auto k_standing = static_cast<BuildingStateMask>(
      static_cast<std::uint8_t>(BuildingStateMask::Normal) |
      static_cast<std::uint8_t>(BuildingStateMask::Damaged));
  const float top = WW::k_deck_height;
  const float half = (WW::k_deck_outer_edge - WW::k_deck_inner_edge) * 0.5F;
  desc.add_box(QVector3D(0.0F, top - WW::k_deck_thickness * 0.5F, 0.0F),
               QVector3D(half, WW::k_deck_thickness * 0.5F, half),
               palette.wood_light * 0.96F,
               k_standing);
  desc.add_cylinder(QVector3D(half - 0.05F, 0.0F, half - 0.05F),
                    QVector3D(half - 0.05F, top - 0.08F, half - 0.05F),
                    0.06F,
                    palette.wood_dark,
                    k_standing);
  desc.add_rotated_box(QVector3D(0.05F, 0.04F, 0.55F),
                       QVector3D(0.42F, 0.025F, 0.06F),
                       QVector3D(0.0F, 17.0F, 6.0F),
                       palette.wood_dark,
                       BuildingStateMask::Destroyed);
  return desc;
}

auto build_wall_walk_stair_desc(std::string_view name_prefix,
                                const WallPalette& palette) -> BuildingArchetypeDesc {
  BuildingArchetypeDesc desc(std::string(name_prefix) + "_walk_stair");
  desc.set_material(k_building_material_wood);
  constexpr auto k_standing = static_cast<BuildingStateMask>(
      static_cast<std::uint8_t>(BuildingStateMask::Normal) |
      static_cast<std::uint8_t>(BuildingStateMask::Damaged));
  const float top = WW::k_deck_height;
  const float head = WW::k_deck_outer_edge;
  const float foot = head + WW::k_stair_run;
  const float half_width = WW::k_stair_half_width;
  const int steps = WW::k_stair_steps;
  const float rise = top / static_cast<float>(steps);
  const float run = WW::k_stair_run / static_cast<float>(steps);

  for (float side : {-1.0F, 1.0F}) {
    const float x = side * (half_width + 0.01F);
    desc.add_cylinder(QVector3D(x, 0.02F, foot + 0.04F),
                      QVector3D(x, top - 0.04F, head - 0.02F),
                      0.045F,
                      palette.wood_dark,
                      k_standing);
    desc.add_cylinder(QVector3D(x * 1.06F, 0.0F, foot),
                      QVector3D(x * 1.06F, 0.95F, foot),
                      0.045F,
                      palette.wood_dark,
                      k_standing);
    desc.add_cylinder(QVector3D(x * 1.06F, 0.93F, foot),
                      QVector3D(x * 1.06F, top + 0.62F, head + 0.20F),
                      0.022F,
                      palette.wood_mid,
                      k_standing);
    desc.add_cylinder(QVector3D(x * 1.06F, top - 0.10F, head + 0.20F),
                      QVector3D(x * 1.06F, top + 0.64F, head + 0.20F),
                      0.035F,
                      palette.wood_dark,
                      k_standing);
  }
  for (int i = 0; i < steps; ++i) {
    const float y = rise * static_cast<float>(i + 1) - 0.025F;
    const float z = foot - run * (static_cast<float>(i) + 0.5F);
    const QVector3D tone = (i % 2 == 0) ? palette.wood_light : palette.wood_mid * 1.06F;
    desc.add_box(QVector3D(0.0F, y, z),
                 QVector3D(half_width, 0.025F, run * 0.62F),
                 tone,
                 k_standing);
  }
  desc.add_box(QVector3D(0.0F, 0.02F, foot + 0.12F),
               QVector3D(half_width + 0.06F, 0.02F, 0.16F),
               palette.earth_dark,
               k_standing);
  desc.add_rotated_box(QVector3D(0.05F, 0.04F, 0.55F),
                       QVector3D(0.42F, 0.025F, 0.06F),
                       QVector3D(0.0F, 17.0F, 6.0F),
                       palette.wood_dark,
                       BuildingStateMask::Destroyed);
  return desc;
}

auto build_wall_variant_desc(std::string_view name_prefix,
                             const WallPalette& palette,
                             const WallGeometry& geometry,
                             WallVariant variant) -> BuildingArchetypeDesc {
  const auto layout = layout_for(variant);
  BuildingArchetypeDesc desc(std::string(name_prefix) + "_" +
                             std::to_string(static_cast<int>(variant)));

  if (geometry.solid_masonry) {
    add_masonry(desc, palette, geometry, layout);
  } else {
    add_palisade(desc, palette, geometry, layout);
  }

  return desc;
}

auto wall_archetype_set(std::string_view name_prefix) -> WallArchetypeSet {
  WallArchetypeSet out{};

  for (int i = 0; i < static_cast<int>(out.variants.size()); ++i) {
    out.variants[static_cast<std::size_t>(i)] =
        &building_archetype_set(std::string(name_prefix) + "_" + std::to_string(i));
  }
  out.walk_span = &building_archetype_set(std::string(name_prefix) + "_walk_span");
  out.walk_landing =
      &building_archetype_set(std::string(name_prefix) + "_walk_landing");
  out.walk_stair = &building_archetype_set(std::string(name_prefix) + "_walk_stair");

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

namespace {

void submit_walk_piece(ISubmitter& out,
                       const DrawContext& ctx,
                       const BuildingArchetypeSet* piece,
                       const QVector3D& origin,
                       float nx,
                       float nz,
                       const QVector3D& offset) {
  if (piece == nullptr) {
    return;
  }
  DrawContext placed = ctx;
  QMatrix4x4 model;
  model.translate(origin + offset);
  model.rotate(
      std::atan2(nx, nz) * 180.0F / std::numbers::pi_v<float>, 0.0F, 1.0F, 0.0F);
  placed.model = model;
  submit_building_instance(out, placed, piece->for_state(resolve_building_state(ctx)));
}

// Hangs the wall walk on the town face of a segment: a balcony span towards
// each wall it runs into, a landing where the run turns, and a stair where the
// network put one.
void submit_wall_walk(ISubmitter& out,
                      const DrawContext& ctx,
                      const WallArchetypeSet& archetypes) {
  if (ctx.entity == nullptr || ctx.world == nullptr ||
      resolve_building_state(ctx) == BuildingState::Destroyed) {
    return;
  }
  const auto id = ctx.entity->get_id();
  const auto* wall = ctx.world->try_get<Engine::Core::WallSegmentComponent>(id);
  if (wall == nullptr || (wall->inner_x == 0 && wall->inner_z == 0) ||
      ctx.world->has<Engine::Core::WallConstructionSiteComponent>(id)) {
    return;
  }
  const QVector3D origin = ctx.model.column(3).toVector3D();
  const auto ix = static_cast<float>(wall->inner_x);
  const auto iz = static_cast<float>(wall->inner_z);
  const std::uint8_t mask = wall->connection_mask;
  const int connections = ((mask & k_connection_north) != 0U ? 1 : 0) +
                          ((mask & k_connection_east) != 0U ? 1 : 0) +
                          ((mask & k_connection_south) != 0U ? 1 : 0) +
                          ((mask & k_connection_west) != 0U ? 1 : 0);
  for (std::size_t dir = 0; dir < k_dir_count; ++dir) {
    const auto& d = k_directions[dir];
    const std::size_t opposite = (dir + 2U) % k_dir_count;
    const bool connected = (mask & k_direction_bits[dir]) != 0U;
    const bool open_end = connections == 1 && (mask & k_direction_bits[opposite]) != 0U;
    const bool isolated_run =
        connections == 0 && (d.x != 0.0F ? wall->inner_z != 0 : wall->inner_x != 0);
    if (!connected && !open_end && !isolated_run) {
      continue;
    }
    const float nx = d.x != 0.0F ? 0.0F : ix;
    const float nz = d.x != 0.0F ? iz : 0.0F;
    if (nx == 0.0F && nz == 0.0F) {
      continue;
    }
    submit_walk_piece(out,
                      ctx,
                      archetypes.walk_span,
                      origin,
                      nx,
                      nz,
                      QVector3D(d.x * 0.5F, 0.0F, d.z * 0.5F));
  }
  if (wall->inner_x != 0 && wall->inner_z != 0) {
    const float lane = (WW::k_deck_inner_edge + WW::k_deck_outer_edge) * 0.5F;
    submit_walk_piece(out,
                      ctx,
                      archetypes.walk_landing,
                      origin,
                      ix,
                      iz,
                      QVector3D(ix * lane, 0.0F, iz * lane));
  }
  if (wall->has_stair) {
    submit_walk_piece(
        out, ctx, archetypes.walk_stair, origin, ix, iz, QVector3D(0.0F, 0.0F, 0.0F));
  }
}

} // namespace

void submit_wall_segment_variant(ISubmitter& out,
                                 const DrawContext& ctx,
                                 const WallArchetypeSet& archetypes,
                                 WallVariant variant) {
  submit_building_instance(
      out,
      ctx,
      archetypes.variants[wall_archetype_index(variant)]->for_state(
          resolve_building_state(ctx)));
  submit_wall_walk(out, ctx, archetypes);
  draw_building_selection_overlay(out, ctx, BuildingSelectionStyle{2.0F, 2.0F});
}

} // namespace Render::GL
