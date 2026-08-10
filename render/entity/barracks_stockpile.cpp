#include "barracks_stockpile.h"

#include <QMatrix4x4>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "building_decay.h"
#include "building_render_common.h"
#include "building_state.h"
#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/systems/resource_stockpile.h"
#include "render/geom/stone.h"
#include "render/gl/primitives.h"
#include "render/submitter.h"

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

constexpr float k_yard_inset = 0.10F;
constexpr float k_yard_west_x =
    k_stockpile_center_x - k_stockpile_half_width + k_yard_inset;
constexpr float k_yard_east_x =
    k_stockpile_center_x + k_stockpile_half_width - k_yard_inset;
constexpr float k_yard_near_z =
    k_stockpile_center_z - k_stockpile_half_depth + k_yard_inset;
constexpr float k_yard_far_z =
    k_stockpile_center_z + k_stockpile_half_depth - k_yard_inset;

constexpr float k_rail_low_y = 0.17F;
constexpr float k_rail_high_y = 0.34F;
constexpr float k_post_height = 0.46F;
constexpr float k_gate_post_height = 0.62F;

constexpr float k_crib_center_x = k_stockpile_center_x - 0.62F;
constexpr float k_crib_half_x = 0.56F;
constexpr float k_crib_half_z = 0.58F;
constexpr float k_crib_wall = 0.05F;
constexpr float k_crib_height = 0.30F;
constexpr float k_crib_front_height = 0.11F;
constexpr float k_deck_top = k_ground + 0.03F;
constexpr float k_crib_inner_x = k_crib_half_x - k_crib_wall;
constexpr float k_crib_inner_z = k_crib_half_z - k_crib_wall;

constexpr float k_wood_bay_z = k_stockpile_center_z - 1.32F;
constexpr float k_stone_bay_z = k_stockpile_center_z;
constexpr float k_iron_bay_z = k_stockpile_center_z + 1.32F;

constexpr float k_apron_west_x = k_crib_center_x + k_crib_half_x + 0.08F;

auto rand01(std::uint32_t seed) -> float {
  std::uint32_t value = seed + 0x9E3779B9U;
  value ^= value >> 15U;
  value *= 0x85EBCA6BU;
  value ^= value >> 13U;
  value *= 0xC2B2AE35U;
  value ^= value >> 16U;
  return static_cast<float>(value & 0xFFFFFFU) / static_cast<float>(0xFFFFFF);
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

auto blend(const QVector3D& from, const QVector3D& to, float amount) -> QVector3D {
  return from + ((to - from) * amount);
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
  box(out,
      yard,
      QVector3D(x, height * 0.5F, z),
      QVector3D(0.070F, height * 0.5F, 0.070F),
      0.0F,
      tint(style.timber_dark, 0.94F + (rand01(seed) * 0.16F)));
  box(out,
      yard,
      QVector3D(x, height + 0.024F, z),
      QVector3D(0.086F, 0.024F, 0.086F),
      0.0F,
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
          (k_apron_west_x + k_yard_east_x) * 0.5F, k_ground, k_stockpile_center_z),
      QVector3D((k_yard_east_x - k_apron_west_x) * 0.5F,
                0.005F,
                k_stockpile_half_depth - k_yard_inset),
      0.0F,
      blend(style.gravel, style.earth_light, 0.55F));

  box(out,
      yard,
      QVector3D(
          (k_yard_west_x + k_apron_west_x) * 0.5F, k_ground, k_stockpile_center_z),
      QVector3D((k_apron_west_x - k_yard_west_x) * 0.5F,
                0.004F,
                k_stockpile_half_depth - k_yard_inset),
      0.0F,
      tint(style.gravel, 0.88F));
}

void draw_fence(ISubmitter& out, const Yard& yard, const StockpileYardStyle& style) {
  rail_along_z(out,
               yard,
               k_yard_west_x,
               k_yard_near_z,
               k_yard_far_z,
               k_rail_low_y,
               style.timber);
  rail_along_z(out,
               yard,
               k_yard_west_x,
               k_yard_near_z,
               k_yard_far_z,
               k_rail_high_y,
               style.timber);

  constexpr int k_west_spans = 4;
  for (int i = 1; i < k_west_spans; ++i) {
    float const t = static_cast<float>(i) / static_cast<float>(k_west_spans);
    fence_post(out,
               yard,
               k_yard_west_x,
               k_yard_near_z + (t * (k_yard_far_z - k_yard_near_z)),
               k_post_height,
               style);
  }

  constexpr int k_side_spans = 3;
  for (int side = 0; side < 2; ++side) {
    float const z = (side == 0) ? k_yard_near_z : k_yard_far_z;
    rail_along_x(
        out, yard, z, k_yard_west_x, k_yard_east_x, k_rail_low_y, style.timber);
    rail_along_x(
        out, yard, z, k_yard_west_x, k_yard_east_x, k_rail_high_y, style.timber);
    for (int i = 0; i < k_side_spans; ++i) {
      float const t = static_cast<float>(i) / static_cast<float>(k_side_spans);
      fence_post(out,
                 yard,
                 k_yard_west_x + (t * (k_yard_east_x - k_yard_west_x)),
                 z,
                 k_post_height,
                 style);
    }
    fence_post(out, yard, k_yard_east_x, z, k_gate_post_height, style);
  }
}

void draw_crib(ISubmitter& out,
               const Yard& yard,
               const StockpileYardStyle& style,
               float bay_z) {
  QVector3D const plank = style.timber;
  QVector3D const frame = style.timber_dark;

  box(out,
      yard,
      QVector3D(k_crib_center_x, k_deck_top - 0.015F, bay_z),
      QVector3D(k_crib_half_x - 0.01F, 0.015F, k_crib_half_z - 0.01F),
      0.0F,
      tint(frame, 1.10F));

  box(out,
      yard,
      QVector3D(k_crib_center_x - k_crib_half_x, k_crib_height * 0.5F, bay_z),
      QVector3D(k_crib_wall, k_crib_height * 0.5F, k_crib_half_z),
      0.0F,
      plank);
  for (int side = 0; side < 2; ++side) {
    float const sign = (side == 0) ? -1.0F : 1.0F;
    box(out,
        yard,
        QVector3D(
            k_crib_center_x, k_crib_height * 0.5F, bay_z + (sign * k_crib_half_z)),
        QVector3D(k_crib_half_x, k_crib_height * 0.5F, k_crib_wall),
        0.0F,
        plank);
  }
  box(out,
      yard,
      QVector3D(k_crib_center_x + k_crib_half_x, k_crib_front_height * 0.5F, bay_z),
      QVector3D(k_crib_wall, k_crib_front_height * 0.5F, k_crib_half_z),
      0.0F,
      plank);

  for (int corner = 0; corner < 4; ++corner) {
    float const sx = ((corner & 1) == 0) ? -1.0F : 1.0F;
    float const sz = ((corner & 2) == 0) ? -1.0F : 1.0F;
    float const height = (sx < 0.0F) ? k_crib_height + 0.05F : k_crib_height;
    box(out,
        yard,
        QVector3D(k_crib_center_x + (sx * k_crib_half_x),
                  height * 0.5F,
                  bay_z + (sz * k_crib_half_z)),
        QVector3D(0.058F, height * 0.5F, 0.058F),
        0.0F,
        frame);
  }
}

void draw_timber_load(ISubmitter& out,
                      const Yard& yard,
                      const StockpileYardStyle& style,
                      float fill,
                      float flash) {
  if (fill <= 0.01F) {
    return;
  }

  constexpr float k_log_radius = 0.100F;
  constexpr float k_log_half_length = k_crib_inner_z - 0.01F;
  constexpr float k_log_pitch = 0.213F;
  constexpr float k_row_pitch = 0.176F;
  constexpr std::array<int, 3> k_row_counts{4, 3, 2};
  constexpr int k_max_logs = 9;

  int const logs =
      std::clamp(static_cast<int>(std::lround(fill * k_max_logs)), 1, k_max_logs);
  int drawn = 0;

  QVector3D const cut_face = brighten(style.timber, 0.42F);

  for (std::size_t row = 0; row < k_row_counts.size() && drawn < logs; ++row) {
    int const count = k_row_counts.at(row);
    float const y = k_deck_top + k_log_radius + (static_cast<float>(row) * k_row_pitch);
    float const first_x = -(static_cast<float>(count - 1) * 0.5F * k_log_pitch);
    for (int i = 0; i < count && drawn < logs; ++i, ++drawn) {
      auto const seed = static_cast<std::uint32_t>(211 + (drawn * 29));
      float const x = k_crib_center_x + first_x + (static_cast<float>(i) * k_log_pitch);
      QVector3D const color =
          brighten(tint(style.timber, 0.86F + (rand01(seed) * 0.26F)), flash * 0.25F);

      submit_building_cylinder(out,
                               yard.frame,
                               QVector3D(x, y, k_wood_bay_z - k_log_half_length),
                               QVector3D(x, y, k_wood_bay_z + k_log_half_length),
                               k_log_radius,
                               color,
                               yard.white);
      if (yard.detailed) {
        for (int end = 0; end < 2; ++end) {
          float const z =
              k_wood_bay_z + ((end == 0) ? -k_log_half_length : k_log_half_length);
          box(out,
              yard,
              QVector3D(x, y, z),
              QVector3D(k_log_radius * 0.80F, k_log_radius * 0.80F, 0.016F),
              0.0F,
              tint(cut_face, 0.96F + (rand01(seed * 7U) * 0.08F)));
        }
      }
    }
  }
}

void draw_stone_load(ISubmitter& out,
                     const Yard& yard,
                     const StockpileYardStyle& style,
                     float fill,
                     float flash) {
  if (fill <= 0.01F) {
    return;
  }

  constexpr float k_block_half_x = 0.245F;
  constexpr float k_block_half_y = 0.072F;
  constexpr float k_block_half_z = 0.104F;
  constexpr float k_block_pitch = 0.272F;
  constexpr float k_course_pitch = 0.152F;
  constexpr std::array<int, 4> k_course_counts{4, 3, 2, 1};
  constexpr int k_max_blocks = 10;

  int const blocks =
      std::clamp(static_cast<int>(std::lround(fill * k_max_blocks)), 1, k_max_blocks);
  int drawn = 0;

  QVector3D const face = blend(style.stone_mid, style.stone_light, 0.30F);
  QVector3D const shade = tint(style.stone_dark, 0.78F);

  for (std::size_t course = 0; course < k_course_counts.size() && drawn < blocks;
       ++course) {
    int const count = k_course_counts.at(course);
    float const y =
        k_deck_top + k_block_half_y + (static_cast<float>(course) * k_course_pitch);
    float const first_z = -(static_cast<float>(count - 1) * 0.5F * k_block_pitch);
    float const course_shift = ((course % 2) == 0) ? -0.022F : 0.022F;
    for (int i = 0; i < count && drawn < blocks; ++i, ++drawn) {
      QVector3D const base = ((i + static_cast<int>(course)) % 2 == 0) ? face : shade;
      box(out,
          yard,
          QVector3D(k_crib_center_x + course_shift,
                    y,
                    k_stone_bay_z + first_z + (static_cast<float>(i) * k_block_pitch)),
          QVector3D(k_block_half_x, k_block_half_y, k_block_half_z),
          0.0F,
          brighten(base, flash * 0.30F));
    }
  }
}

void draw_ore_load(ISubmitter& out,
                   const Yard& yard,
                   const StockpileYardStyle& style,
                   float fill,
                   float flash) {
  if (fill <= 0.01F) {
    return;
  }

  float const bed_top = k_deck_top + 0.03F + (fill * (k_crib_height - 0.10F));
  box(out,
      yard,
      QVector3D(k_crib_center_x, (k_deck_top + bed_top) * 0.5F, k_iron_bay_z),
      QVector3D(k_crib_inner_x, (bed_top - k_deck_top) * 0.5F, k_crib_inner_z),
      0.0F,
      brighten(blend(tint(style.ore, 1.02F), style.ore_rust, 0.22F), flash * 0.28F));

  constexpr std::array<std::array<float, 2>, 9> k_lump_spots{{{-0.30F, -0.34F},
                                                              {0.00F, -0.36F},
                                                              {0.30F, -0.30F},
                                                              {-0.32F, 0.00F},
                                                              {0.00F, -0.02F},
                                                              {0.31F, 0.03F},
                                                              {-0.28F, 0.33F},
                                                              {0.02F, 0.35F},
                                                              {0.30F, 0.31F}}};
  int const lumps = std::clamp(static_cast<int>(std::lround(fill * 11.0F)),
                               1,
                               static_cast<int>(k_lump_spots.size()));
  for (int i = 0; i < lumps; ++i) {
    auto const seed = static_cast<std::uint32_t>(457 + (i * 53));
    float const radius = 0.088F + (rand01(seed) * 0.038F);
    QVector3D const base = (rand01(seed * 3U) > 0.66F) ? style.ore_rust : style.ore;
    rock(out,
         yard,
         QVector3D(k_crib_center_x + k_lump_spots.at(i).at(0),
                   bed_top + (radius * 0.38F),
                   k_iron_bay_z + k_lump_spots.at(i).at(1)),
         QVector3D(radius, radius * 0.62F, radius * 0.90F),
         rand01(seed * 11U) * 360.0F,
         0.0F,
         brighten(tint(base, 1.08F + (rand01(seed * 17U) * 0.24F)), flash * 0.32F));
  }
}

void draw_barrel(ISubmitter& out,
                 const Yard& yard,
                 const StockpileYardStyle& style,
                 float x,
                 float z) {
  constexpr float k_radius = 0.130F;
  constexpr float k_height = 0.33F;

  auto ring = [&](float from_y, float to_y, float radius, const QVector3D& color) {
    submit_building_cylinder(out,
                             yard.frame,
                             QVector3D(x, from_y, z),
                             QVector3D(x, to_y, z),
                             radius,
                             color,
                             yard.white);
  };

  ring(k_ground, k_ground + k_height, k_radius, tint(style.timber, 1.02F));
  ring(k_ground + 0.055F, k_ground + 0.085F, k_radius * 1.05F, style.timber_dark);
  ring(k_ground + 0.235F, k_ground + 0.265F, k_radius * 1.05F, style.timber_dark);
  ring(k_ground + k_height,
       k_ground + k_height + 0.018F,
       k_radius * 0.94F,
       blend(style.timber, style.timber_dark, 0.55F));
}

void draw_yard_props(ISubmitter& out,
                     const Yard& yard,
                     const StockpileYardStyle& style) {
  constexpr float k_lane_z = k_stockpile_center_z;
  constexpr float k_lane_from_x = k_apron_west_x + 0.04F;
  constexpr float k_lane_to_x = k_yard_east_x - 0.06F;
  constexpr int k_lane_boards = 6;
  constexpr float k_board_pitch =
      (k_lane_to_x - k_lane_from_x) / static_cast<float>(k_lane_boards);

  for (int i = 0; i < k_lane_boards; ++i) {
    box(out,
        yard,
        QVector3D(k_lane_from_x + ((static_cast<float>(i) + 0.5F) * k_board_pitch),
                  k_ground + 0.012F,
                  k_lane_z),
        QVector3D(k_board_pitch * 0.42F, 0.012F, 0.34F),
        0.0F,
        tint(style.timber, 0.90F + (static_cast<float>(i % 2) * 0.14F)));
  }

  constexpr float k_plank_x = k_yard_east_x - 0.30F;
  constexpr float k_plank_z = k_yard_far_z - 0.70F;
  constexpr float k_plank_thickness = 0.024F;
  for (int i = 0; i < 4; ++i) {
    box(out,
        yard,
        QVector3D(k_plank_x,
                  k_ground + k_plank_thickness +
                      (static_cast<float>(i) * k_plank_thickness * 2.1F),
                  k_plank_z),
        QVector3D(0.17F, k_plank_thickness, 0.46F),
        0.0F,
        tint(style.timber, 0.88F + (static_cast<float>(i) * 0.07F)));
  }

  draw_barrel(out, yard, style, k_yard_east_x - 0.28F, k_yard_near_z + 0.44F);
  draw_barrel(out, yard, style, k_yard_east_x - 0.30F, k_yard_near_z + 0.78F);
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
  for (float const bay_z : {k_wood_bay_z, k_stone_bay_z, k_iron_bay_z}) {
    draw_crib(out, yard, decayed, bay_z);
  }
  if (yard.detailed) {
    draw_yard_props(out, yard, decayed);
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

  draw_timber_load(out, yard, decayed, stockpile->wood_fill, flash);
  draw_stone_load(out, yard, decayed, stockpile->stone_fill, flash);
  draw_ore_load(out, yard, decayed, stockpile->iron_fill, flash);
}

} // namespace Render::GL
