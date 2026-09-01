#include "barracks_renderer.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>

#include "game/core/component.h"
#include "game/visuals/team_colors.h"
#include "math/math_utils.h"
#include "render/entity/barracks_flag_renderer.h"
#include "render/entity/barracks_renderer_common.h"
#include "render/entity/barracks_stockpile.h"
#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_decay.h"
#include "render/entity/building_ornaments.h"
#include "render/entity/building_render_common.h"
#include "render/entity/building_state.h"
#include "render/entity/registry.h"
#include "render/gl/backend.h"
#include "render/gl/primitives.h"
#include "render/gl/resources.h"
#include "render/render_archetype.h"
#include "render/submitter.h"

namespace Render::GL::Roman {
namespace {

using Render::Geom::clamp_vec_01;

constexpr std::uint8_t k_team_slot = 0;

constexpr auto k_mask_intact = k_building_state_mask_intact;

struct RomanPalette {
  QVector3D plaster{0.90F, 0.86F, 0.76F};
  QVector3D plaster_shade{0.82F, 0.77F, 0.66F};
  QVector3D limestone{0.96F, 0.94F, 0.88F};
  QVector3D limestone_shade{0.88F, 0.85F, 0.78F};
  QVector3D limestone_dark{0.80F, 0.76F, 0.70F};
  QVector3D marble{0.98F, 0.97F, 0.95F};
  QVector3D cedar{0.52F, 0.38F, 0.26F};
  QVector3D cedar_dark{0.38F, 0.26F, 0.16F};
  QVector3D terracotta{0.76F, 0.32F, 0.18F};
  QVector3D terracotta_dark{0.46F, 0.12F, 0.07F};
  QVector3D dado{0.52F, 0.18F, 0.12F};
  QVector3D iron{0.30F, 0.30F, 0.32F};
  QVector3D gold{0.85F, 0.72F, 0.35F};
  QVector3D team{0.8F, 0.9F, 1.0F};
  QVector3D team_trim{0.48F, 0.54F, 0.60F};
};

inline auto make_palette(const QVector3D& team) -> RomanPalette {
  RomanPalette p;
  p.team = clamp_vec_01(team);
  p.team_trim =
      clamp_vec_01(QVector3D(team.x() * 0.6F, team.y() * 0.6F, team.z() * 0.6F));
  return p;
}

constexpr float k_platform_top = 0.16F;
constexpr float k_block_half_x = 1.60F;
constexpr float k_block_front_z = 0.50F;
constexpr float k_block_back_z = -1.62F;
constexpr float k_wall_top = 1.80F;
constexpr float k_veranda_post_z = 1.30F;
constexpr float k_roof_rise = 0.70F;
constexpr float k_roof_front_z = 1.52F;
constexpr float k_roof_back_z = -1.74F;

void add_platform(BuildingArchetypeDesc& desc, const RomanPalette& c) {
  desc.add_box(
      QVector3D(0.0F, 0.04F, 0.0F), QVector3D(1.72F, 0.04F, 1.52F), c.limestone_dark);
  desc.add_box(
      QVector3D(0.0F, 0.10F, 0.0F), QVector3D(1.62F, 0.02F, 1.42F), c.limestone_shade);
  desc.add_box(
      QVector3D(0.0F, 0.14F, 0.0F), QVector3D(1.55F, 0.02F, 1.42F), c.limestone);

  desc.add_box(
      QVector3D(0.0F, 0.04F, 1.68F), QVector3D(1.02F, 0.04F, 0.20F), c.limestone_dark);
  desc.add_box(QVector3D(0.0F, 0.10F, 1.54F),
               QVector3D(0.88F, 0.04F, 0.14F),
               c.limestone_shade,
               k_mask_intact);

  desc.add_box(
      QVector3D(0.0F, k_platform_top + 0.004F, (k_block_front_z + 1.42F) * 0.5F),
      QVector3D(k_block_half_x, 0.004F, (1.42F - k_block_front_z) * 0.5F),
      c.limestone_shade,
      k_mask_intact);
}

void add_block(BuildingArchetypeDesc& desc,
               const RomanPalette& c,
               BuildingState state) {
  const bool destroyed = state == BuildingState::Destroyed;
  const float block_cz = (k_block_front_z + k_block_back_z) * 0.5F;
  const float block_hz = (k_block_front_z - k_block_back_z) * 0.5F;
  const float wall_top = destroyed ? 0.72F : k_wall_top;
  const float wall_cy = (k_platform_top + wall_top) * 0.5F;
  const float wall_hy = (wall_top - k_platform_top) * 0.5F;

  desc.add_box(QVector3D(0.0F, wall_cy, block_cz),
               QVector3D(k_block_half_x, wall_hy, block_hz),
               c.plaster);

  desc.add_box(QVector3D(0.0F, k_platform_top + 0.15F, block_cz),
               QVector3D(k_block_half_x + 0.008F, 0.15F, block_hz + 0.008F),
               c.dado);

  for (const float qx : {-k_block_half_x, k_block_half_x}) {
    for (const float qz : {k_block_back_z, k_block_front_z}) {
      desc.add_box(QVector3D(qx, wall_cy, qz),
                   QVector3D(0.07F, wall_hy, 0.07F),
                   c.limestone_shade);
    }
  }

  if (destroyed) {
    return;
  }

  desc.add_box(QVector3D(0.0F, k_wall_top - 0.04F, block_cz),
               QVector3D(k_block_half_x + 0.05F, 0.04F, block_hz + 0.05F),
               c.limestone_shade,
               k_mask_intact);

  for (const float wx : {-1.20F, -0.40F, 0.40F, 1.20F}) {
    desc.add_box(QVector3D(wx, 1.36F, k_block_back_z - 0.012F),
                 QVector3D(0.11F, 0.13F, 0.015F),
                 c.cedar_dark * 0.55F,
                 k_mask_intact);
    desc.add_box(QVector3D(wx, 1.51F, k_block_back_z - 0.02F),
                 QVector3D(0.14F, 0.02F, 0.02F),
                 c.limestone_shade,
                 k_mask_intact);
  }
  for (const float sx : {-k_block_half_x - 0.012F, k_block_half_x + 0.012F}) {
    for (const float wz : {-1.10F, -0.30F}) {
      desc.add_box(QVector3D(sx, 1.36F, wz),
                   QVector3D(0.015F, 0.13F, 0.11F),
                   c.cedar_dark * 0.55F,
                   k_mask_intact);
    }
  }

  for (const float dx : {-1.00F, 0.0F, 1.00F}) {
    desc.add_box(QVector3D(dx, k_platform_top + 0.56F, k_block_front_z + 0.012F),
                 QVector3D(0.20F, 0.56F, 0.015F),
                 c.cedar_dark * 0.45F,
                 k_mask_intact);
    for (const float fx : {-0.22F, 0.22F}) {
      desc.add_box(QVector3D(dx + fx, k_platform_top + 0.58F, k_block_front_z + 0.02F),
                   QVector3D(0.03F, 0.58F, 0.025F),
                   c.cedar,
                   k_mask_intact);
    }
    desc.add_box(QVector3D(dx, k_platform_top + 1.18F, k_block_front_z + 0.02F),
                 QVector3D(0.27F, 0.035F, 0.03F),
                 c.cedar,
                 k_mask_intact);
  }

  for (const float sx : {-0.50F, 0.50F}) {
    desc.add_box(QVector3D(sx, 1.02F, k_block_front_z + 0.04F),
                 QVector3D(0.15F, 0.23F, 0.02F),
                 c.cedar_dark,
                 BuildingStateMask::Normal);
    desc.add_palette_box(QVector3D(sx, 1.02F, k_block_front_z + 0.055F),
                         QVector3D(0.13F, 0.21F, 0.012F),
                         k_team_slot,
                         BuildingStateMask::Normal);
    desc.add_box(QVector3D(sx, 1.02F, k_block_front_z + 0.075F),
                 QVector3D(0.035F, 0.035F, 0.012F),
                 c.gold,
                 BuildingStateMask::Normal);
  }
}

void add_veranda(BuildingArchetypeDesc& desc,
                 const RomanPalette& c,
                 BuildingState state) {
  if (state == BuildingState::Destroyed) {
    return;
  }
  constexpr std::array<float, 5> k_post_x{-1.50F, -0.75F, 0.0F, 0.75F, 1.50F};
  const float beam_y = k_wall_top - 0.16F;
  for (std::size_t i = 0; i < k_post_x.size(); ++i) {
    const float px = k_post_x[i];

    const BuildingStateMask states =
        (i == 1 || i == 3) ? BuildingStateMask::Normal : k_mask_intact;
    desc.add_box(QVector3D(px, k_platform_top + 0.03F, k_veranda_post_z),
                 QVector3D(0.09F, 0.03F, 0.09F),
                 c.limestone_shade,
                 states);
    desc.add_cylinder(QVector3D(px, k_platform_top, k_veranda_post_z),
                      QVector3D(px, beam_y, k_veranda_post_z),
                      0.062F,
                      c.cedar,
                      states);
    desc.add_box(QVector3D(px, beam_y - 0.02F, k_veranda_post_z),
                 QVector3D(0.09F, 0.03F, 0.09F),
                 c.cedar_dark,
                 states);
  }
  desc.add_box(QVector3D(0.0F, beam_y + 0.04F, k_veranda_post_z),
               QVector3D(k_block_half_x + 0.06F, 0.045F, 0.06F),
               c.cedar_dark,
               k_mask_intact);

  for (const float rx : {-1.30F, -0.90F, -0.45F, 0.0F, 0.45F, 0.90F, 1.30F}) {
    desc.add_box(
        QVector3D(rx, beam_y + 0.10F, (k_block_front_z + k_veranda_post_z) * 0.5F),
        QVector3D(0.03F, 0.03F, (k_veranda_post_z - k_block_front_z) * 0.5F),
        c.cedar_dark,
        k_mask_intact);
  }
}

void add_roof(BuildingArchetypeDesc& desc, const RomanPalette& c, BuildingState state) {
  if (state == BuildingState::Destroyed) {
    return;
  }
  const float centre_z = (k_roof_front_z + k_roof_back_z) * 0.5F;
  const float half_depth = (k_roof_front_z - k_roof_back_z) * 0.5F;
  const float eave_y = k_wall_top;
  const float theta = std::atan2(k_roof_rise, half_depth);
  const float theta_deg = theta * 180.0F / 3.14159265F;
  const float slope_len =
      std::sqrt(half_depth * half_depth + k_roof_rise * k_roof_rise);
  const float overhang = 0.10F;
  const QVector3D slab_scale(
      k_block_half_x + overhang, 0.045F, slope_len * 0.5F + overhang);
  const float cy = eave_y + k_roof_rise * 0.5F;

  desc.add_rotated_box(QVector3D(0.0F, cy, centre_z + half_depth * 0.5F),
                       slab_scale,
                       QVector3D(theta_deg, 0.0F, 0.0F),
                       c.terracotta,
                       BuildingStateMask::Normal);
  desc.add_rotated_box(QVector3D(0.0F, cy, centre_z - half_depth * 0.5F),
                       slab_scale,
                       QVector3D(-theta_deg, 0.0F, 0.0F),
                       c.terracotta * 0.96F,
                       k_mask_intact);

  for (const float rx : {-1.40F, -0.70F, 0.0F, 0.70F, 1.40F}) {
    desc.add_rotated_box(QVector3D(rx, cy + 0.02F, centre_z + half_depth * 0.5F),
                         QVector3D(0.035F, 0.035F, slope_len * 0.5F),
                         QVector3D(theta_deg, 0.0F, 0.0F),
                         c.cedar_dark,
                         BuildingStateMask::Damaged);
  }

  const QVector3D normal(0.0F, std::cos(theta), std::sin(theta));
  for (const float f : {0.22F, 0.46F, 0.70F, 0.92F}) {
    const float y = eave_y + k_roof_rise * f;
    const float dz = half_depth * (1.0F - f);
    const QVector3D lift = normal * 0.05F;
    desc.add_rotated_box(QVector3D(0.0F, y, centre_z + dz) +
                             QVector3D(0.0F, lift.y(), lift.z()),
                         QVector3D(k_block_half_x + overhang - 0.02F, 0.012F, 0.035F),
                         QVector3D(theta_deg, 0.0F, 0.0F),
                         c.terracotta_dark,
                         BuildingStateMask::Normal);
    desc.add_rotated_box(QVector3D(0.0F, y, centre_z - dz) +
                             QVector3D(0.0F, lift.y(), -lift.z()),
                         QVector3D(k_block_half_x + overhang - 0.02F, 0.012F, 0.035F),
                         QVector3D(-theta_deg, 0.0F, 0.0F),
                         c.terracotta_dark,
                         k_mask_intact);
  }

  desc.add_box(QVector3D(0.0F, eave_y + k_roof_rise + 0.02F, centre_z),
               QVector3D(k_block_half_x + overhang + 0.02F, 0.035F, 0.07F),
               c.terracotta_dark,
               k_mask_intact);

  for (const float gx : {-k_block_half_x, k_block_half_x}) {
    struct Step {
      float y;
      float half_z;
      float half_y;
    };
    const std::array<Step, 4> steps{Step{1.94F, 1.30F, 0.14F},
                                    Step{2.20F, 0.90F, 0.12F},
                                    Step{2.40F, 0.44F, 0.10F},
                                    Step{2.50F, 0.14F, 0.04F}};
    for (const Step& s : steps) {
      desc.add_box(QVector3D(gx, s.y, centre_z),
                   QVector3D(0.05F, s.half_y, s.half_z),
                   c.plaster_shade,
                   k_mask_intact);
    }
  }

  add_roman_roof_standard(desc,
                          QVector3D(0.0F, eave_y + k_roof_rise + 0.05F, centre_z),
                          0.86F,
                          c.gold,
                          c.terracotta_dark,
                          BuildingStateMask::Normal);
}

void add_yard_props(BuildingArchetypeDesc& desc, const RomanPalette& c) {

  const float rx = -1.45F;
  const float rz = 0.95F;
  for (const float dz : {-0.22F, 0.22F}) {
    desc.add_box(QVector3D(rx, k_platform_top + 0.36F, rz + dz),
                 QVector3D(0.03F, 0.36F, 0.03F),
                 c.cedar_dark,
                 k_mask_intact);
  }
  desc.add_box(QVector3D(rx, k_platform_top + 0.70F, rz),
               QVector3D(0.03F, 0.02F, 0.26F),
               c.cedar_dark,
               k_mask_intact);
  for (const float dz : {-0.15F, -0.05F, 0.05F, 0.15F}) {
    desc.add_cylinder(QVector3D(rx + 0.10F, k_platform_top, rz + dz),
                      QVector3D(rx - 0.02F, k_platform_top + 1.55F, rz + dz),
                      0.012F,
                      c.cedar,
                      BuildingStateMask::Normal);
    desc.add_cone(QVector3D(rx - 0.02F, k_platform_top + 1.55F, rz + dz),
                  QVector3D(rx - 0.04F, k_platform_top + 1.78F, rz + dz),
                  0.018F,
                  c.iron,
                  BuildingStateMask::Normal);
  }

  desc.add_box(QVector3D(1.40F, k_platform_top + 0.12F, 0.95F),
               QVector3D(0.14F, 0.12F, 0.30F),
               c.limestone_dark,
               k_mask_intact);
  desc.add_box(QVector3D(1.40F, k_platform_top + 0.22F, 0.95F),
               QVector3D(0.11F, 0.01F, 0.27F),
               QVector3D(0.30F, 0.42F, 0.50F),
               k_mask_intact);
}

auto build_barracks_archetype(BuildingState state) -> RenderArchetype {
  RomanPalette const c = make_palette(QVector3D(1.0F, 1.0F, 1.0F));
  BuildingArchetypeDesc desc("roman_barracks");

  add_platform(desc, c);
  add_block(desc, c, state);
  add_veranda(desc, c, state);
  add_roof(desc, c, state);
  add_yard_props(desc, c);

  if (state == BuildingState::Damaged) {
    add_rubble_field(desc,
                     RubbleField{.center = QVector3D(0.6F, k_platform_top, 1.0F),
                                 .extent = QVector3D(0.9F, 0.0F, 0.35F),
                                 .stone = c.terracotta_dark,
                                 .stone_dark = c.limestone_dark,
                                 .chunk_scale = 0.8F,
                                 .count = 6,
                                 .seed = 449,
                                 .states = BuildingStateMask::Damaged});
  }

  add_ruin_dressing(desc,
                    RuinDressing{.extent = QVector3D(1.46F, 0.0F, 1.28F),
                                 .stone = c.limestone_shade,
                                 .stone_dark = c.limestone_dark,
                                 .timber = c.cedar_dark * 0.6F,
                                 .ground_y = 0.16F,
                                 .scale = 1.25F,
                                 .seed = 443});

  return build_building_archetype(desc, state);
}

auto barracks_archetype(BuildingState state,
                        Mesh*,
                        Texture*) -> const RenderArchetype& {
  static const BuildingArchetypeSet k_set =
      build_stateful_building_archetype_set(build_barracks_archetype);
  return k_set.for_state(state);
}

void draw_phoenician_banner(const DrawContext& p,
                            ISubmitter& out,
                            Mesh* unit,
                            Texture* white,
                            const RomanPalette& c,
                            const BarracksFlagRenderer::ClothBannerResources* cloth) {
  BarracksFlagRenderer::draw_hanging_banner(
      p,
      out,
      unit,
      white,
      c.team,
      c.team_trim,
      {.pole_base = QVector3D(0.0F, 0.0F, -2.0F),
       .pole_height = 3.0F,
       .pole_radius = 0.045F,
       .banner_width = 0.9F,
       .banner_height = 0.6F,
       .pole_color = c.cedar,
       .beam_color = c.cedar,
       .connector_color = c.limestone,
       .ornament_offset = QVector3D(0.25F, 3.15F, 0.03F),
       .ornament_size = QVector3D(0.35F, 0.03F, 0.015F),
       .ornament_color = c.gold,
       .ring_count = 4,
       .ring_y_start = 0.4F,
       .ring_spacing = 0.5F,
       .ring_height = 0.025F,
       .ring_radius_scale = 2.0F,
       .ring_color = c.gold},
      cloth);
}

void draw_rally_flag(const DrawContext& p,
                     ISubmitter& out,
                     Texture* white,
                     const RomanPalette& c,
                     const BarracksFlagRenderer::ClothBannerResources* cloth) {
  BarracksFlagRenderer::FlagColors const colors{.team = c.team,
                                                .team_trim = c.team_trim,
                                                .timber = c.cedar,
                                                .timber_light = c.limestone,
                                                .wood_dark = c.cedar_dark};
  BarracksFlagRenderer::draw_rally_flag_if_any(p, out, white, colors, cloth);
}

void draw_stockpile_yard(const DrawContext& p,
                         ISubmitter& out,
                         Mesh* unit,
                         Texture* white,
                         const RomanPalette& c) {
  draw_barracks_stockpile(
      p,
      out,
      unit,
      white,
      StockpileYardStyle{.gravel = QVector3D(0.62F, 0.55F, 0.44F),
                         .earth_light = QVector3D(0.72F, 0.65F, 0.53F),
                         .stone_light = c.limestone,
                         .stone_mid = c.limestone_shade,
                         .stone_dark = c.limestone_dark,
                         .timber = c.cedar,
                         .timber_dark = c.cedar_dark,
                         .ore = QVector3D(0.35F, 0.34F, 0.36F),
                         .ore_rust = QVector3D(0.46F, 0.28F, 0.16F)});
}

void draw_barracks_ornaments(const DrawContext& p,
                             ISubmitter& out,
                             Mesh* unit,
                             Texture* white,
                             const QVector3D& team,
                             const BarracksFlagRenderer::ClothBannerResources* cloth) {
  RomanPalette const c = make_palette(team);
  draw_stockpile_yard(p, out, unit, white, c);
  draw_phoenician_banner(p, out, unit, white, c, cloth);
  draw_rally_flag(p, out, white, c, cloth);
}

} // namespace

void register_barracks_renderer(Render::GL::EntityRendererRegistry& registry) {
  register_barracks_renderer_variant(
      registry,
      BarracksRendererConfig{.nation_slug = "roman",
                             .archetype = &barracks_archetype,
                             .draw_ornaments = &draw_barracks_ornaments,
                             .selection = BuildingSelectionStyle{2.6F, 2.2F}});
}

} // namespace Render::GL::Roman
