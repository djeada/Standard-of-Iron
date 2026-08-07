#include "barracks_stockpile.h"

#include <QMatrix4x4>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "../../game/core/component.h"
#include "../../game/core/entity.h"
#include "../../game/systems/resource_stockpile.h"
#include "../geom/stone.h"
#include "../gl/primitives.h"
#include "../submitter.h"
#include "building_decay.h"
#include "building_render_common.h"
#include "building_state.h"

namespace Render::GL {
namespace {

using Game::Systems::k_stockpile_center_x;
using Game::Systems::k_stockpile_center_z;
using Game::Systems::k_stockpile_deposit_flash_seconds;
using Game::Systems::k_stockpile_half_depth;
using Game::Systems::k_stockpile_half_width;

constexpr float k_bed_height = 0.035F;
constexpr float k_ground = k_bed_height * 2.0F;
constexpr float k_detail_distance_sq = 5200.0F;

constexpr float k_yard_back_x = k_stockpile_center_x - k_stockpile_half_width + 0.09F;
constexpr float k_yard_near_z = k_stockpile_center_z - k_stockpile_half_depth + 0.09F;
constexpr float k_yard_far_z = k_stockpile_center_z + k_stockpile_half_depth - 0.09F;
constexpr float k_gate_x = k_stockpile_center_x + 0.55F;
constexpr float k_stall_edge_x = k_stockpile_center_x + 0.10F;

constexpr float k_pile_center_x = k_stockpile_center_x - 0.55F;
constexpr float k_wood_pile_z = k_stockpile_center_z - 1.35F;
constexpr float k_stone_pile_z = k_stockpile_center_z;
constexpr float k_iron_pile_z = k_stockpile_center_z + 1.35F;

constexpr float k_rail_low_y = 0.17F;
constexpr float k_rail_high_y = 0.34F;

auto rand01(std::uint32_t seed) -> float {
  std::uint32_t value = seed + 0x9E3779B9U;
  value ^= value >> 15U;
  value *= 0x85EBCA6BU;
  value ^= value >> 13U;
  value *= 0xC2B2AE35U;
  value ^= value >> 16U;
  return static_cast<float>(value & 0xFFFFFFU) / static_cast<float>(0xFFFFFF);
}

auto jitter(std::uint32_t seed, float amount) -> float {
  return ((rand01(seed) * 2.0F) - 1.0F) * amount;
}

auto tint(const QVector3D& color, float amount) -> QVector3D {
  return QVector3D(std::clamp(color.x() * amount, 0.0F, 1.0F),
                   std::clamp(color.y() * amount, 0.0F, 1.0F),
                   std::clamp(color.z() * amount, 0.0F, 1.0F));
}

auto brighten(const QVector3D& color, float mix) -> QVector3D {
  return QVector3D(std::clamp(color.x() + ((1.0F - color.x()) * mix), 0.0F, 1.0F),
                   std::clamp(color.y() + ((1.0F - color.y()) * mix), 0.0F, 1.0F),
                   std::clamp(color.z() + ((1.0F - color.z()) * mix), 0.0F, 1.0F));
}

struct Yard {
  QMatrix4x4 frame;
  Mesh* cube{nullptr};
  Mesh* stone{nullptr};
  Texture* white{nullptr};
  bool detailed{true};
};

void box(ISubmitter& out,
         const Yard& yard,
         const QVector3D& pos,
         const QVector3D& half_extent,
         float yaw_degrees,
         const QVector3D& color) {
  if (yard.cube == nullptr) {
    return;
  }
  QMatrix4x4 model = yard.frame;
  model.translate(pos);
  if (yaw_degrees != 0.0F) {
    model.rotate(yaw_degrees, 0.0F, 1.0F, 0.0F);
  }
  model.scale(half_extent);
  out.mesh(yard.cube, model, color, yard.white, 1.0F);
}

void rock(ISubmitter& out,
          const Yard& yard,
          const QVector3D& pos,
          const QVector3D& radii,
          float yaw_degrees,
          float tilt_degrees,
          const QVector3D& color) {
  Mesh* mesh = (yard.stone != nullptr) ? yard.stone : yard.cube;
  if (mesh == nullptr) {
    return;
  }
  QMatrix4x4 model = yard.frame;
  model.translate(pos);
  model.rotate(yaw_degrees, 0.0F, 1.0F, 0.0F);
  if (tilt_degrees != 0.0F) {
    model.rotate(tilt_degrees, 1.0F, 0.0F, 0.0F);
  }
  model.scale(radii);
  out.mesh(mesh, model, color, yard.white, 1.0F);
}

void rail_along_z(ISubmitter& out,
                  const Yard& yard,
                  float x,
                  float z_from,
                  float z_to,
                  float y,
                  const QVector3D& color) {
  box(out,
      yard,
      QVector3D(x, y, (z_from + z_to) * 0.5F),
      QVector3D(0.035F, 0.045F, std::abs(z_to - z_from) * 0.5F),
      0.0F,
      color);
}

void rail_along_x(ISubmitter& out,
                  const Yard& yard,
                  float z,
                  float x_from,
                  float x_to,
                  float y,
                  const QVector3D& color) {
  box(out,
      yard,
      QVector3D((x_from + x_to) * 0.5F, y, z),
      QVector3D(std::abs(x_to - x_from) * 0.5F, 0.045F, 0.035F),
      0.0F,
      color);
}

void fence_post(ISubmitter& out,
                const Yard& yard,
                float x,
                float z,
                float height,
                const StockpileYardStyle& style) {
  auto const seed = static_cast<std::uint32_t>((x * 97.0F) + (z * 31.0F) + 400.0F);
  float const lean = jitter(seed, 1.6F);
  box(out,
      yard,
      QVector3D(x, height * 0.5F, z),
      QVector3D(0.072F, height * 0.5F, 0.072F),
      lean,
      tint(style.timber_dark, 0.92F + (rand01(seed * 3U) * 0.22F)));
  box(out,
      yard,
      QVector3D(x, height + 0.025F, z),
      QVector3D(0.088F, 0.028F, 0.088F),
      lean,
      style.timber);
}

void draw_bed(ISubmitter& out, const Yard& yard, const StockpileYardStyle& style) {
  box(out,
      yard,
      QVector3D(k_stockpile_center_x, k_bed_height, k_stockpile_center_z),
      QVector3D(k_stockpile_half_width, k_bed_height, k_stockpile_half_depth),
      0.0F,
      style.gravel);

  box(out,
      yard,
      QVector3D(
          (k_yard_back_x + k_stall_edge_x) * 0.5F, k_ground, k_stockpile_center_z),
      QVector3D((k_stall_edge_x - k_yard_back_x) * 0.5F,
                0.004F,
                k_stockpile_half_depth - 0.06F),
      0.0F,
      tint(style.gravel, 0.86F));

  if (!yard.detailed) {
    return;
  }

  QVector3D const scuff = (style.gravel + style.earth_light) * 0.5F;
  constexpr std::array<std::array<float, 3>, 3> k_scuffs{
      {{0.78F, 0.0F, 0.68F}, {0.52F, -1.15F, 0.40F}, {0.95F, 1.05F, 0.34F}}};
  for (std::size_t i = 0; i < k_scuffs.size(); ++i) {
    auto const seed = static_cast<std::uint32_t>(131 + (i * 41));
    rock(out,
         yard,
         QVector3D(k_stockpile_center_x + k_scuffs.at(i).at(0),
                   k_ground,
                   k_stockpile_center_z + k_scuffs.at(i).at(1)),
         QVector3D(k_scuffs.at(i).at(2), 0.006F, k_scuffs.at(i).at(2) * 0.78F),
         rand01(seed) * 180.0F,
         0.0F,
         tint(scuff, 0.97F + (rand01(seed * 7U) * 0.07F)));
  }
}

void draw_yard_clutter(ISubmitter& out,
                       const Yard& yard,
                       const StockpileYardStyle& style) {
  constexpr float k_stack_x = k_stockpile_center_x + 0.98F;
  constexpr float k_stack_z = k_stockpile_center_z - 1.55F;
  constexpr float k_board_thickness = 0.021F;

  for (int i = 0; i < 4; ++i) {
    auto const seed = static_cast<std::uint32_t>(733 + (i * 47));
    box(out,
        yard,
        QVector3D(k_stack_x + jitter(seed, 0.025F),
                  k_ground + k_board_thickness +
                      (static_cast<float>(i) * k_board_thickness * 2.2F),
                  k_stack_z + jitter(seed * 3U, 0.05F)),
        QVector3D(0.125F, k_board_thickness, 0.40F),
        jitter(seed * 5U, 3.5F),
        tint(style.timber, 0.84F + (rand01(seed * 7U) * 0.30F)));
  }

  for (int i = 0; i < 2; ++i) {
    auto const seed = static_cast<std::uint32_t>(811 + (i * 53));
    float const radius = 0.145F + (rand01(seed) * 0.025F);
    rock(out,
         yard,
         QVector3D(k_stack_x + 0.31F + (static_cast<float>(i) * 0.27F),
                   k_ground + (radius * 0.78F),
                   k_stack_z + 0.30F + (static_cast<float>(i) * 0.22F)),
         QVector3D(radius, radius * 0.86F, radius * 0.82F),
         rand01(seed * 5U) * 360.0F,
         jitter(seed * 7U, 12.0F),
         tint(style.earth_light, 1.04F + (rand01(seed * 11U) * 0.12F)));
  }
}

void draw_fence(ISubmitter& out, const Yard& yard, const StockpileYardStyle& style) {
  constexpr float k_post_height = 0.44F;
  constexpr float k_gate_post_height = 0.62F;

  rail_along_z(out,
               yard,
               k_yard_back_x,
               k_yard_near_z,
               k_yard_far_z,
               k_rail_low_y,
               style.timber);
  rail_along_z(out,
               yard,
               k_yard_back_x,
               k_yard_near_z,
               k_yard_far_z,
               k_rail_high_y,
               style.timber);
  for (int i = 0; i < 4; ++i) {
    float const t = static_cast<float>(i) / 3.0F;
    fence_post(out,
               yard,
               k_yard_back_x,
               k_yard_near_z + (t * (k_yard_far_z - k_yard_near_z)),
               k_post_height,
               style);
  }

  for (int side = 0; side < 2; ++side) {
    float const z = (side == 0) ? k_yard_near_z : k_yard_far_z;
    rail_along_x(out, yard, z, k_yard_back_x, k_gate_x, k_rail_low_y, style.timber);
    rail_along_x(out, yard, z, k_yard_back_x, k_gate_x, k_rail_high_y, style.timber);
    fence_post(out, yard, (k_yard_back_x + k_gate_x) * 0.5F, z, k_post_height, style);
    fence_post(out, yard, k_gate_x, z, k_gate_post_height, style);
  }
}

void draw_timber_pile(ISubmitter& out,
                      const Yard& yard,
                      const StockpileYardStyle& style,
                      float fill,
                      float flash) {
  constexpr float k_log_radius = 0.115F;
  constexpr float k_log_half_length = 0.60F;
  constexpr float k_stake_half_z = k_log_half_length + 0.13F;

  for (int side = 0; side < 2; ++side) {
    float const z = k_wood_pile_z + ((side == 0) ? -k_stake_half_z : k_stake_half_z);
    box(out,
        yard,
        QVector3D(k_pile_center_x, 0.085F, z),
        QVector3D(0.50F, 0.075F, 0.045F),
        0.0F,
        style.timber_dark);
    for (int end = 0; end < 2; ++end) {
      float const x = k_pile_center_x + ((end == 0) ? -0.47F : 0.47F);
      box(out,
          yard,
          QVector3D(x, 0.31F, z),
          QVector3D(0.045F, 0.31F, 0.045F),
          0.0F,
          style.timber_dark);
    }
  }

  if (fill <= 0.01F) {
    return;
  }

  constexpr std::array<int, 4> k_row_counts{5, 4, 3, 2};
  constexpr std::array<float, 4> k_row_offsets{-0.42F, -0.315F, -0.21F, -0.105F};
  constexpr float k_log_pitch = 0.21F;
  constexpr float k_row_pitch = 0.155F;
  constexpr int k_max_logs = 14;
  int const logs =
      std::clamp(static_cast<int>(std::lround(fill * k_max_logs)), 1, k_max_logs);
  int drawn = 0;

  QVector3D const cut_face = brighten(style.timber, 0.42F);

  for (std::size_t row = 0; row < k_row_counts.size() && drawn < logs; ++row) {
    float const y = 0.135F + (static_cast<float>(row) * k_row_pitch);
    int const count = k_row_counts.at(row);
    for (int i = 0; i < count && drawn < logs; ++i, ++drawn) {
      auto const seed = static_cast<std::uint32_t>(211 + (drawn * 29));
      float const x = k_pile_center_x + k_row_offsets.at(row) +
                      (static_cast<float>(i) * k_log_pitch);
      float const half_length = k_log_half_length + jitter(seed * 3U, 0.045F);
      float const shift = jitter(seed * 5U, 0.04F);
      QVector3D const color =
          brighten(tint(style.timber, 0.80F + (rand01(seed) * 0.36F)), flash * 0.25F);

      submit_building_cylinder(out,
                               yard.frame,
                               QVector3D(x, y, k_wood_pile_z + shift - half_length),
                               QVector3D(x, y, k_wood_pile_z + shift + half_length),
                               k_log_radius,
                               color,
                               yard.white);
      if (yard.detailed) {
        for (int end = 0; end < 2; ++end) {
          float const z =
              k_wood_pile_z + shift + ((end == 0) ? -half_length : half_length);
          box(out,
              yard,
              QVector3D(x, y, z),
              QVector3D(k_log_radius * 0.78F, k_log_radius * 0.78F, 0.018F),
              0.0F,
              tint(cut_face, 0.94F + (rand01(seed * 7U) * 0.12F)));
        }
      }
    }
  }
}

void draw_stone_pile(ISubmitter& out,
                     const Yard& yard,
                     const StockpileYardStyle& style,
                     float fill,
                     float flash) {
  if (fill <= 0.01F) {
    return;
  }

  struct Course {
    float y;
    int count;
    float first_z;
  };
  constexpr std::array<Course, 4> k_courses{{{0.180F, 4, -0.45F},
                                             {0.368F, 3, -0.30F},
                                             {0.556F, 2, -0.15F},
                                             {0.744F, 1, 0.0F}}};
  constexpr float k_block_pitch = 0.30F;
  constexpr int k_max_blocks = 10;

  int const blocks =
      std::clamp(static_cast<int>(std::lround(fill * k_max_blocks)), 1, k_max_blocks);
  int drawn = 0;

  for (const auto& course : k_courses) {
    if (drawn >= blocks) {
      break;
    }
    for (int i = 0; i < course.count && drawn < blocks; ++i, ++drawn) {
      auto const seed = static_cast<std::uint32_t>(331 + (drawn * 43));

      QVector3D const base = ((drawn % 2) == 0) ? style.stone_light : style.stone_mid;
      QVector3D const color =
          brighten(tint(base, 0.92F + (rand01(seed) * 0.16F)), flash * 0.3F);
      box(out,
          yard,
          QVector3D(k_pile_center_x + jitter(seed * 3U, 0.016F),
                    course.y,
                    k_stone_pile_z + course.first_z +
                        (static_cast<float>(i) * k_block_pitch)),
          QVector3D(0.215F, 0.092F, 0.145F),
          jitter(seed * 7U, 2.0F),
          color);
    }
  }

  if (fill > 0.35F) {
    box(out,
        yard,
        QVector3D(k_pile_center_x + 0.40F, 0.100F, k_stone_pile_z - 0.52F),
        QVector3D(0.205F, 0.090F, 0.145F),
        -14.0F,
        brighten(tint(style.stone_dark, 1.28F), flash * 0.25F));
  }
  if (!yard.detailed) {
    return;
  }
  for (int i = 0; i < 4; ++i) {
    auto const seed = static_cast<std::uint32_t>(509 + (i * 37));
    rock(out,
         yard,
         QVector3D(k_pile_center_x + jitter(seed, 0.42F),
                   k_ground + 0.02F,
                   k_stone_pile_z + jitter(seed * 3U, 0.48F)),
         QVector3D(0.075F + (rand01(seed * 5U) * 0.045F),
                   0.035F,
                   0.065F + (rand01(seed * 7U) * 0.04F)),
         rand01(seed * 11U) * 360.0F,
         0.0F,
         tint(style.stone_dark, 0.95F + (rand01(seed * 13U) * 0.25F)));
  }
}

void draw_ore_pile(ISubmitter& out,
                   const Yard& yard,
                   const StockpileYardStyle& style,
                   float fill,
                   float flash) {
  constexpr float k_bin_half_x = 0.46F;
  constexpr float k_bin_half_z = 0.50F;
  constexpr float k_wall = 0.045F;
  constexpr float k_bin_height = 0.27F;

  for (int side = 0; side < 2; ++side) {
    float const sign = (side == 0) ? -1.0F : 1.0F;
    box(out,
        yard,
        QVector3D(k_pile_center_x + (sign * k_bin_half_x),
                  k_bin_height * 0.5F,
                  k_iron_pile_z),
        QVector3D(k_wall, k_bin_height * 0.5F, k_bin_half_z),
        0.0F,
        style.timber_dark);
    box(out,
        yard,
        QVector3D(k_pile_center_x,
                  k_bin_height * 0.5F,
                  k_iron_pile_z + (sign * k_bin_half_z)),
        QVector3D(k_bin_half_x, k_bin_height * 0.5F, k_wall),
        0.0F,
        style.timber_dark);
  }
  for (int corner = 0; corner < 4; ++corner) {
    float const sx = ((corner & 1) == 0) ? -1.0F : 1.0F;
    float const sz = ((corner & 2) == 0) ? -1.0F : 1.0F;
    box(out,
        yard,
        QVector3D(k_pile_center_x + (sx * k_bin_half_x),
                  k_bin_height * 0.56F,
                  k_iron_pile_z + (sz * k_bin_half_z)),
        QVector3D(0.062F, k_bin_height * 0.56F, 0.062F),
        0.0F,
        style.timber);
  }

  if (fill <= 0.01F) {
    return;
  }

  float const bed_top = 0.06F + (fill * (k_bin_height - 0.05F));
  box(out,
      yard,
      QVector3D(k_pile_center_x, bed_top * 0.5F, k_iron_pile_z),
      QVector3D(k_bin_half_x - k_wall, bed_top * 0.5F, k_bin_half_z - k_wall),
      0.0F,
      brighten(tint(style.ore, 0.72F), flash * 0.30F));

  constexpr std::array<std::array<float, 2>, 10> k_lump_spots{{{-0.24F, -0.30F},
                                                               {0.20F, -0.28F},
                                                               {-0.05F, -0.02F},
                                                               {0.26F, 0.22F},
                                                               {-0.27F, 0.26F},
                                                               {0.04F, 0.36F},
                                                               {-0.14F, 0.12F},
                                                               {0.16F, -0.05F},
                                                               {-0.22F, -0.10F},
                                                               {0.02F, -0.34F}}};
  int const lumps = std::clamp(static_cast<int>(std::lround(fill * 12.0F)),
                               1,
                               static_cast<int>(k_lump_spots.size()));
  for (int i = 0; i < lumps; ++i) {
    auto const seed = static_cast<std::uint32_t>(457 + (i * 53));
    float const radius = 0.10F + (rand01(seed) * 0.05F);
    QVector3D const base = (rand01(seed * 3U) > 0.62F) ? style.ore_rust : style.ore;
    rock(
        out,
        yard,
        QVector3D(k_pile_center_x + k_lump_spots.at(i).at(0) + jitter(seed * 5U, 0.05F),
                  bed_top + (radius * 0.5F) + (rand01(seed * 19U) * 0.04F),
                  k_iron_pile_z + k_lump_spots.at(i).at(1) + jitter(seed * 7U, 0.05F)),
        QVector3D(radius, radius * 0.76F, radius * 0.92F),
        rand01(seed * 11U) * 360.0F,
        jitter(seed * 13U, 22.0F),
        brighten(tint(base, 0.88F + (rand01(seed * 17U) * 0.28F)), flash * 0.35F));
  }

  if (!yard.detailed) {
    return;
  }
  for (int i = 0; i < 3; ++i) {
    auto const seed = static_cast<std::uint32_t>(661 + (i * 31));
    float const radius = 0.07F + (rand01(seed) * 0.035F);
    rock(out,
         yard,
         QVector3D(k_pile_center_x + 0.62F + jitter(seed * 3U, 0.16F),
                   k_ground + (radius * 0.5F),
                   k_iron_pile_z + jitter(seed * 5U, 0.44F)),
         QVector3D(radius, radius * 0.7F, radius * 0.9F),
         rand01(seed * 7U) * 360.0F,
         jitter(seed * 11U, 18.0F),
         tint(style.ore_rust, 0.9F + (rand01(seed * 13U) * 0.3F)));
  }
}

} // namespace

void draw_barracks_stockpile(const DrawContext& ctx,
                             ISubmitter& out,
                             Mesh* unit,
                             Texture* white,
                             const StockpileYardStyle& style) {
  if (ctx.entity == nullptr) {
    return;
  }
  const auto* transform = ctx.entity->get_component<Engine::Core::TransformComponent>();
  if (transform == nullptr) {
    return;
  }

  const BuildingState state = resolve_building_state(ctx);
  StockpileYardStyle decayed = style;
  if (state != BuildingState::Normal) {
    int seed = 0;
    for (QVector3D* slot : {&decayed.gravel,
                            &decayed.earth_light,
                            &decayed.stone_light,
                            &decayed.stone_mid,
                            &decayed.stone_dark,
                            &decayed.timber,
                            &decayed.timber_dark,
                            &decayed.ore,
                            &decayed.ore_rust}) {
      *slot = decayed_color(*slot, state, ++seed);
    }
  }

  Yard yard;
  yard.frame.translate(
      transform->position.x, transform->position.y, transform->position.z);
  yard.frame.rotate(transform->rotation.y, 0.0F, 1.0F, 0.0F);
  yard.cube = (unit != nullptr) ? unit : get_unit_cube();
  yard.stone = Render::Geom::Stone::get();
  yard.white = white;
  yard.detailed = ctx.distance_sq <= k_detail_distance_sq;

  draw_bed(out, yard, decayed);
  draw_fence(out, yard, decayed);
  if (yard.detailed) {
    draw_yard_clutter(out, yard, decayed);
  }

  if (state == BuildingState::Destroyed) {
    return;
  }

  const auto* stockpile = ctx.entity->get_component<Engine::Core::StockpileComponent>();
  if (stockpile == nullptr) {
    return;
  }

  float const flash = std::clamp(
      stockpile->deposit_flash / k_stockpile_deposit_flash_seconds, 0.0F, 1.0F);

  draw_timber_pile(out, yard, decayed, stockpile->wood_fill, flash);
  draw_stone_pile(out, yard, decayed, stockpile->stone_fill, flash);
  draw_ore_pile(out, yard, decayed, stockpile->iron_fill, flash);
}

} // namespace Render::GL
