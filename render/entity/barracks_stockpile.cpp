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
#include "building_render_common.h"
#include "building_state.h"

namespace Render::GL {
namespace {

using Game::Systems::k_stockpile_center_x;
using Game::Systems::k_stockpile_center_z;
using Game::Systems::k_stockpile_deposit_flash_seconds;
using Game::Systems::k_stockpile_half_depth;
using Game::Systems::k_stockpile_half_width;

constexpr float k_pile_center_x = k_stockpile_center_x - 0.55F;
constexpr float k_bed_height = 0.035F;
constexpr int k_kerb_long_steps = 11;
constexpr int k_kerb_short_steps = 6;
constexpr float k_detail_distance_sq = 5200.0F;

constexpr float k_wood_pile_z = k_stockpile_center_z - 1.20F;
constexpr float k_stone_pile_z = k_stockpile_center_z;
constexpr float k_iron_pile_z = k_stockpile_center_z + 1.25F;

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

void draw_bed(ISubmitter& out, const Yard& yard, const StockpileYardStyle& style) {
  box(out,
      yard,
      QVector3D(k_stockpile_center_x, k_bed_height, k_stockpile_center_z),
      QVector3D(k_stockpile_half_width, k_bed_height, k_stockpile_half_depth),
      0.0F,
      style.gravel);
}

void draw_kerb(ISubmitter& out, const Yard& yard, const StockpileYardStyle& style) {
  std::uint32_t seed = 17U;

  for (int side = 0; side < 2; ++side) {
    float const edge_x = k_stockpile_center_x + ((side == 0) ? -k_stockpile_half_width
                                                             : k_stockpile_half_width);
    for (int i = 0; i < k_kerb_long_steps; ++i) {
      float const t = static_cast<float>(i) / static_cast<float>(k_kerb_long_steps - 1);
      float const z = k_stockpile_center_z +
                      (((t * 2.0F) - 1.0F) * (k_stockpile_half_depth - 0.16F));
      ++seed;
      float const radius = 0.24F + (rand01(seed) * 0.13F);
      rock(out,
           yard,
           QVector3D(edge_x + jitter(seed * 3U, 0.09F),
                     0.05F + (rand01(seed * 5U) * 0.02F),
                     z + jitter(seed * 7U, 0.10F)),
           QVector3D(radius, 0.055F + (rand01(seed * 11U) * 0.03F), radius * 0.82F),
           rand01(seed * 13U) * 360.0F,
           jitter(seed * 17U, 5.0F),
           brighten(tint(style.stone_mid, 0.9F + (rand01(seed * 19U) * 0.26F)), 0.1F));
    }
  }

  for (int side = 0; side < 2; ++side) {
    float const edge_z = k_stockpile_center_z + ((side == 0) ? -k_stockpile_half_depth
                                                             : k_stockpile_half_depth);
    for (int i = 0; i < k_kerb_short_steps; ++i) {
      float const t =
          static_cast<float>(i) / static_cast<float>(k_kerb_short_steps - 1);
      float const x = k_stockpile_center_x +
                      (((t * 2.0F) - 1.0F) * (k_stockpile_half_width - 0.16F));
      ++seed;
      float const radius = 0.23F + (rand01(seed) * 0.14F);
      rock(out,
           yard,
           QVector3D(x + jitter(seed * 3U, 0.10F),
                     0.05F + (rand01(seed * 5U) * 0.02F),
                     edge_z + jitter(seed * 7U, 0.09F)),
           QVector3D(radius, 0.05F + (rand01(seed * 11U) * 0.03F), radius * 0.86F),
           rand01(seed * 13U) * 360.0F,
           jitter(seed * 17U, 5.0F),
           brighten(tint(style.stone_light, 0.88F + (rand01(seed * 19U) * 0.28F)),
                    0.12F));
    }
  }
}

void draw_flagstones(ISubmitter& out,
                     const Yard& yard,
                     const StockpileYardStyle& style) {
  constexpr int k_flagstone_count = 7;
  for (int i = 0; i < k_flagstone_count; ++i) {
    auto const seed = static_cast<std::uint32_t>(101 + (i * 37));
    float const x = k_stockpile_center_x + jitter(seed, k_stockpile_half_width - 0.35F);
    float const z =
        k_stockpile_center_z + jitter(seed * 3U, k_stockpile_half_depth - 0.40F);
    float const radius = 0.26F + (rand01(seed * 5U) * 0.16F);
    rock(out,
         yard,
         QVector3D(x, 0.045F, z),
         QVector3D(radius, 0.03F, radius * 0.74F),
         rand01(seed * 7U) * 360.0F,
         0.0F,
         brighten(tint(style.stone_dark, 0.92F + (rand01(seed * 11U) * 0.26F)), 0.08F));
  }
}

void draw_timber_pile(ISubmitter& out,
                      const Yard& yard,
                      const StockpileYardStyle& style,
                      float fill,
                      float flash) {
  if (fill <= 0.01F) {
    return;
  }

  constexpr std::array<int, 3> k_row_counts{4, 3, 2};
  constexpr float k_log_half_length = 0.62F;
  constexpr float k_log_radius = 0.115F;
  int const logs = std::max(1, static_cast<int>(std::lround(fill * 9.0F)));
  int drawn = 0;

  QVector3D const cut_face = brighten(style.timber, 0.35F);

  for (std::size_t row = 0; row < k_row_counts.size() && drawn < logs; ++row) {
    float const y = 0.15F + (static_cast<float>(row) * 0.19F);
    int const count = k_row_counts.at(row);
    for (int i = 0; i < count && drawn < logs; ++i, ++drawn) {
      auto const seed = static_cast<std::uint32_t>(211 + (drawn * 29));
      float const offset =
          (static_cast<float>(i) - ((static_cast<float>(count) - 1.0F) * 0.5F)) * 0.27F;
      float const x = k_pile_center_x + offset;
      float const half_length = k_log_half_length + jitter(seed * 3U, 0.05F);
      QVector3D const color =
          brighten(tint(style.timber, 0.82F + (rand01(seed) * 0.34F)), flash * 0.25F);

      submit_building_cylinder(out,
                               yard.frame,
                               QVector3D(x, y, k_wood_pile_z - half_length),
                               QVector3D(x, y, k_wood_pile_z + half_length),
                               k_log_radius,
                               color,
                               yard.white);
      if (yard.detailed) {
        box(out,
            yard,
            QVector3D(x, y, k_wood_pile_z - half_length),
            QVector3D(k_log_radius * 0.8F, k_log_radius * 0.8F, 0.02F),
            0.0F,
            cut_face);
        box(out,
            yard,
            QVector3D(x, y, k_wood_pile_z + half_length),
            QVector3D(k_log_radius * 0.8F, k_log_radius * 0.8F, 0.02F),
            0.0F,
            cut_face);
      }
    }
  }

  for (int side = 0; side < 2; ++side) {
    float const z = k_wood_pile_z + ((side == 0) ? -(k_log_half_length + 0.12F)
                                                 : (k_log_half_length + 0.12F));
    box(out,
        yard,
        QVector3D(k_pile_center_x, 0.10F, z),
        QVector3D(0.46F, 0.09F, 0.05F),
        0.0F,
        style.timber_dark);
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

  int const blocks = std::max(1, static_cast<int>(std::lround(fill * 8.0F)));
  int drawn = 0;

  for (int layer = 0; layer < 3 && drawn < blocks; ++layer) {
    int const per_layer = (layer == 0) ? 4 : ((layer == 1) ? 3 : 1);
    float const y = 0.15F + (static_cast<float>(layer) * 0.21F);
    for (int i = 0; i < per_layer && drawn < blocks; ++i, ++drawn) {
      auto const seed = static_cast<std::uint32_t>(331 + (drawn * 43));
      float const spread =
          (static_cast<float>(i) - ((static_cast<float>(per_layer) - 1.0F) * 0.5F));
      QVector3D const color = brighten(
          tint(style.stone_light, 0.86F + (rand01(seed) * 0.3F)), flash * 0.3F);
      box(out,
          yard,
          QVector3D(k_pile_center_x + jitter(seed * 3U, 0.05F),
                    y,
                    k_stone_pile_z + (spread * 0.34F) + jitter(seed * 5U, 0.03F)),
          QVector3D(0.22F, 0.11F, 0.155F),
          jitter(seed * 7U, 7.0F),
          color);
    }
  }
}

void draw_ore_pile(ISubmitter& out,
                   const Yard& yard,
                   const StockpileYardStyle& style,
                   float fill,
                   float flash) {
  if (fill <= 0.01F) {
    return;
  }

  int const lumps = std::max(1, static_cast<int>(std::lround(fill * 8.0F)));
  for (int i = 0; i < lumps; ++i) {
    auto const seed = static_cast<std::uint32_t>(457 + (i * 53));
    float const ring = (i < 5) ? 0.0F : 1.0F;
    float const y = 0.12F + (ring * 0.16F);
    float const radius = 0.18F + (rand01(seed) * 0.08F);
    QVector3D const base = (rand01(seed * 3U) > 0.45F) ? style.ore_rust : style.ore;
    rock(out,
         yard,
         QVector3D(k_pile_center_x + jitter(seed * 5U, 0.34F - (ring * 0.18F)),
                   y,
                   k_iron_pile_z + jitter(seed * 7U, 0.38F - (ring * 0.20F))),
         QVector3D(radius, radius * 0.78F, radius * 0.92F),
         rand01(seed * 11U) * 360.0F,
         jitter(seed * 13U, 20.0F),
         brighten(tint(base, 0.95F + (rand01(seed * 17U) * 0.35F)),
                  0.10F + (flash * 0.35F)));
  }

  if (fill > 0.55F) {
    for (int i = 0; i < 3; ++i) {
      auto const seed = static_cast<std::uint32_t>(613 + (i * 31));
      box(out,
          yard,
          QVector3D(k_pile_center_x + 0.46F,
                    0.09F + (static_cast<float>(i) * 0.07F),
                    k_iron_pile_z - 0.30F + (static_cast<float>(i) * 0.05F)),
          QVector3D(0.17F, 0.035F, 0.09F),
          jitter(seed, 12.0F),
          brighten(tint(style.ore, 1.15F), flash * 0.35F));
    }
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

  Yard yard;
  yard.frame.translate(
      transform->position.x, transform->position.y, transform->position.z);
  yard.frame.rotate(transform->rotation.y, 0.0F, 1.0F, 0.0F);
  yard.cube = (unit != nullptr) ? unit : get_unit_cube();
  yard.stone = Render::Geom::Stone::get();
  yard.white = white;
  yard.detailed = ctx.distance_sq <= k_detail_distance_sq;

  draw_bed(out, yard, style);
  draw_kerb(out, yard, style);
  if (yard.detailed) {
    draw_flagstones(out, yard, style);
  }

  if (resolve_building_state(ctx) == BuildingState::Destroyed) {
    return;
  }

  const auto* stockpile = ctx.entity->get_component<Engine::Core::StockpileComponent>();
  if (stockpile == nullptr) {
    return;
  }

  float const flash = std::clamp(
      stockpile->deposit_flash / k_stockpile_deposit_flash_seconds, 0.0F, 1.0F);

  draw_timber_pile(out, yard, style, stockpile->wood_fill, flash);
  draw_stone_pile(out, yard, style, stockpile->stone_fill, flash);
  draw_ore_pile(out, yard, style, stockpile->iron_fill, flash);
}

} // namespace Render::GL
