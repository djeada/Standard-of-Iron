#include "home_renderer_common.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "../entity_appearance.h"
#include "building_archetype_desc.h"
#include "building_decay.h"
#include "game/core/component_core.h"
#include "home_activity.h"
#include "home_props.h"

namespace Render::GL {
namespace {

constexpr std::array<QVector3D, 4> k_leaf_greens{QVector3D(0.24F, 0.40F, 0.15F),
                                                 QVector3D(0.31F, 0.48F, 0.19F),
                                                 QVector3D(0.20F, 0.34F, 0.13F),
                                                 QVector3D(0.37F, 0.52F, 0.22F)};
constexpr std::array<QVector3D, 3> k_blossoms{QVector3D(0.80F, 0.26F, 0.22F),
                                              QVector3D(0.90F, 0.74F, 0.28F),
                                              QVector3D(0.86F, 0.84F, 0.78F)};
const QVector3D k_earth_dark{0.30F, 0.23F, 0.16F};
const QVector3D k_log_bark{0.40F, 0.27F, 0.16F};
const QVector3D k_log_end{0.74F, 0.58F, 0.38F};
const QVector3D k_plank{0.52F, 0.36F, 0.22F};
const QVector3D k_sack{0.74F, 0.64F, 0.46F};
const QVector3D k_iron{0.30F, 0.30F, 0.31F};

auto jitter(int seed, int salt) -> float {
  return decay_hash(seed * 131 + salt * 17) - 0.5F;
}

auto leaf(int seed, int salt) -> QVector3D {
  auto const index = static_cast<std::size_t>(decay_hash(seed * 7 + salt) * 3.99F);
  return k_leaf_greens[index % k_leaf_greens.size()] *
         (0.92F + decay_hash(seed * 11 + salt * 3) * 0.16F);
}

void add_jar(BuildingArchetypeDesc& desc,
             const QVector3D& foot,
             float scale,
             const QVector3D& clay,
             const QVector3D& band) {
  auto const at = [&](float y) {
    return foot + QVector3D(0.0F, y * scale, 0.0F);
  };
  float const r = 0.070F * scale;
  desc.add_cone(at(0.050F), at(-0.004F), r * 0.95F, clay, k_building_state_mask_intact);
  desc.add_cylinder(at(0.048F), at(0.180F), r, clay, k_building_state_mask_intact);
  desc.add_cylinder(
      at(0.118F), at(0.132F), r * 1.02F, band, k_building_state_mask_intact);
  desc.add_cone(at(0.178F), at(0.262F), r, clay, k_building_state_mask_intact);
  desc.add_cylinder(
      at(0.226F), at(0.288F), r * 0.36F, clay, k_building_state_mask_intact);
  desc.add_cylinder(
      at(0.280F), at(0.296F), r * 0.52F, band, k_building_state_mask_intact);
}

const QVector3D k_madder{0.62F, 0.27F, 0.21F};
const QVector3D k_straw{0.72F, 0.62F, 0.38F};

void add_path_stones(BuildingArchetypeDesc& desc, const HomeYardStyle& style) {
  float const p = style.plinth_half;
  int const seed = style.seed;
  for (int stone = 0; stone < 2; ++stone) {
    int const stone_seed = seed * 17 + stone;
    float const z = p + 0.46F + 0.20F * static_cast<float>(stone);
    desc.add_rotated_box(QVector3D(jitter(stone_seed, 3) * 0.10F, 0.0105F, z),
                         QVector3D(0.085F, 0.0045F, 0.055F),
                         QVector3D(0.0F, jitter(stone_seed, 4) * 30.0F, 0.0F),
                         style.stone * (0.90F + decay_hash(stone_seed) * 0.12F),
                         k_building_state_mask_intact);
  }
}

void add_wall_foot_wear(BuildingArchetypeDesc& desc, const HomeYardStyle& style) {
  float const face = style.wall_face + 0.006F;
  float const depth = 0.045F;
  float const half_y = 0.0375F;
  float const y = style.wall_foot_y + half_y + 0.002F;
  float const gap = std::max(style.wall_foot_door_gap, 0.05F);
  float const front_half = face;
  float const segment = (front_half - gap) * 0.5F;
  for (float const side : {-1.0F, 1.0F}) {
    desc.add_box(QVector3D(side * (gap + segment), y, face - depth),
                 QVector3D(segment, half_y, depth),
                 style.wall_foot,
                 k_building_state_mask_intact);
  }
  desc.add_box(QVector3D(0.0F, y - 0.002F, -(face - depth)),
               QVector3D(front_half, half_y - 0.002F, depth),
               style.wall_foot * 0.96F,
               k_building_state_mask_intact);
  for (float const side : {-1.0F, 1.0F}) {
    desc.add_box(QVector3D(side * (face - depth), y + 0.002F, 0.0F),
                 QVector3D(depth, half_y - 0.004F, face - 2.0F * depth - 0.006F),
                 style.wall_foot * 1.03F,
                 k_building_state_mask_intact);
  }
}

void add_rain_sump(BuildingArchetypeDesc& desc, const HomeYardStyle& style) {
  float const p = style.plinth_half;
  float const z = -(p + 0.21F);
  float const x0 = -p + 0.02F;
  QVector3D const sump(x0 - 0.10F, 0.0F, z);
  desc.add_cylinder(sump,
                    sump + QVector3D(0.0F, 0.07F, 0.0F),
                    0.085F,
                    style.stone * 0.86F,
                    k_building_state_mask_intact);
  desc.add_cylinder(sump + QVector3D(0.0F, 0.07F, 0.0F),
                    sump + QVector3D(0.0F, 0.074F, 0.0F),
                    0.062F,
                    k_earth_dark * 0.6F,
                    k_building_state_mask_intact);
}

void add_work_yard(BuildingArchetypeDesc& desc, const HomeYardStyle& style) {
  float const p = style.plinth_half;
  int const seed = style.seed;
  switch (style.variant % 3) {
  case 0: {
    QVector3D const hub(-(p + 0.36F), 0.016F, 0.86F);
    desc.add_cylinder(hub - QVector3D(0.0F, 0.012F, 0.0F),
                      hub + QVector3D(0.0F, 0.012F, 0.0F),
                      0.17F,
                      k_plank * 0.92F,
                      k_building_state_mask_intact);
    desc.add_cylinder(hub + QVector3D(0.0F, 0.012F, 0.0F),
                      hub + QVector3D(0.0F, 0.030F, 0.0F),
                      0.040F,
                      k_log_bark,
                      k_building_state_mask_intact);
    QVector3D const hay(-(p + 0.24F), 0.0F, -0.86F);
    desc.add_cone(hay,
                  hay + QVector3D(0.0F, 0.24F, 0.0F),
                  0.21F,
                  k_straw * (0.94F + decay_hash(seed + 31) * 0.10F),
                  k_building_state_mask_intact);
    desc.add_cone(hay + QVector3D(0.17F, 0.0F, 0.12F),
                  hay + QVector3D(0.17F, 0.15F, 0.12F),
                  0.13F,
                  k_straw * 0.88F,
                  k_building_state_mask_intact);
    break;
  }
  case 1: {
    float const x = p + 0.40F;
    for (float const z : {-0.42F, 0.34F}) {
      desc.add_cylinder(QVector3D(x, 0.0F, z),
                        QVector3D(x, 0.29F, z),
                        0.016F,
                        k_log_bark,
                        k_building_state_mask_intact);
    }
    desc.add_cylinder(QVector3D(x, 0.275F, -0.44F),
                      QVector3D(x, 0.275F, 0.36F),
                      0.010F,
                      k_plank,
                      k_building_state_mask_intact);
    for (int cloth = 0; cloth < 3; ++cloth) {
      float const z = -0.28F + 0.25F * static_cast<float>(cloth);
      desc.add_box(QVector3D(x, 0.205F, z),
                   QVector3D(0.006F, 0.065F, 0.085F),
                   cloth == 1 ? k_madder : k_sack * (0.92F + 0.05F * cloth),
                   k_building_state_mask_intact);
    }
    break;
  }
  default: {
    QVector3D const quern(p + 0.30F, 0.0F, -0.74F);
    desc.add_cylinder(quern,
                      quern + QVector3D(0.0F, 0.055F, 0.0F),
                      0.12F,
                      style.stone * 0.84F,
                      k_building_state_mask_intact);
    desc.add_cylinder(quern + QVector3D(0.0F, 0.055F, 0.0F),
                      quern + QVector3D(0.0F, 0.105F, 0.0F),
                      0.10F,
                      style.stone * 0.95F,
                      k_building_state_mask_intact);
    desc.add_cylinder(quern + QVector3D(0.05F, 0.105F, 0.0F),
                      quern + QVector3D(0.05F, 0.17F, 0.0F),
                      0.010F,
                      k_plank,
                      k_building_state_mask_intact);
    QVector3D const basket(p + 0.30F, 0.0F, 0.66F);
    desc.add_cylinder(basket,
                      basket + QVector3D(0.0F, 0.10F, 0.0F),
                      0.095F,
                      k_straw * 0.82F,
                      k_building_state_mask_intact);
    desc.add_cylinder(basket + QVector3D(0.0F, 0.10F, 0.0F),
                      basket + QVector3D(0.0F, 0.112F, 0.0F),
                      0.104F,
                      k_plank,
                      k_building_state_mask_intact);
    add_jar(desc,
            QVector3D(p + 0.48F, 0.0F, 0.40F),
            0.80F,
            style.clay * 0.97F,
            style.clay_dark);
    break;
  }
  }
  if (style.variant >= 3) {
    QVector3D const trough(p + 0.26F, 0.045F, -0.30F);
    desc.add_box(trough,
                 QVector3D(0.075F, 0.045F, 0.22F),
                 style.stone * 0.88F,
                 k_building_state_mask_intact);
    desc.add_box(trough + QVector3D(0.0F, 0.046F, 0.0F),
                 QVector3D(0.052F, 0.0015F, 0.196F),
                 QVector3D(0.24F, 0.34F, 0.40F),
                 k_building_state_mask_intact);
  }
}

} // namespace

auto home_visual_variant(std::uint32_t seed) -> int {
  std::uint32_t const parity = seed & 1U;
  std::uint32_t mixed = seed >> 1U;
  mixed ^= mixed >> 16U;
  mixed *= 0x7FEB352DU;
  mixed ^= mixed >> 15U;
  constexpr std::uint32_t k_per_parity = k_home_visual_variant_count / 2;
  return static_cast<int>(((mixed % k_per_parity) * 2U) + parity);
}

auto home_placement_seed(float world_x,
                         float world_z,
                         std::uint32_t fallback) -> std::uint32_t {
  if (!std::isfinite(world_x) || !std::isfinite(world_z)) {
    return fallback;
  }
  auto const qx = static_cast<std::uint32_t>(
      static_cast<std::int32_t>(std::lround(world_x * 2.0F)));
  auto const qz = static_cast<std::uint32_t>(
      static_cast<std::int32_t>(std::lround(world_z * 2.0F)));
  std::uint32_t seed = (qx * 0x9E3779B1U) ^ (qz * 0x85EBCA77U) ^ 0x27D4EB2FU;
  seed ^= seed >> 13U;
  seed *= 0xC2B2AE3DU;
  seed ^= seed >> 16U;

  constexpr float k_lot_pitch = 5.0F;
  auto const lot_x = static_cast<std::int32_t>(std::floor(world_x / k_lot_pitch));
  auto const lot_z = static_cast<std::int32_t>(std::floor(world_z / k_lot_pitch));
  auto const parity = static_cast<std::uint32_t>(lot_x + lot_z) & 1U;
  return (seed & ~1U) | parity;
}

auto home_tint_pick(std::uint32_t seed, std::size_t count) -> std::size_t {
  if (count == 0U) {
    return 0U;
  }
  std::uint32_t mixed = seed * 0x2C1B3C6DU;
  mixed ^= mixed >> 12U;
  mixed *= 0x297A2D39U;
  mixed ^= mixed >> 15U;
  return static_cast<std::size_t>(mixed % static_cast<std::uint32_t>(count));
}

auto home_tint_jitter(std::uint32_t seed) -> float {
  std::uint32_t mixed = (seed ^ 0x5BD1E995U) * 0x1B873593U;
  mixed ^= mixed >> 14U;
  return (static_cast<float>(mixed & 0xFFFFU) / 65535.0F) - 0.5F;
}

void add_home_yard(BuildingArchetypeDesc& desc, const HomeYardStyle& style) {
  float const p = style.plinth_half;
  float const w = style.wall_half;
  int const seed = style.seed;

  for (int stone = 0; stone < 3; ++stone) {
    float const z = p + 0.07F + 0.13F * static_cast<float>(stone);
    desc.add_box(QVector3D(jitter(seed, stone) * 0.05F, 0.016F + 0.001F * stone, z),
                 QVector3D(0.11F - 0.012F * stone, 0.004F, 0.050F),
                 style.stone * (0.92F + decay_hash(seed + stone) * 0.12F),
                 k_building_state_mask_intact);
  }

  float const pile_x = -(p + 0.05F);
  constexpr std::array<int, 3> k_logs_per_row{3, 2, 1};
  constexpr float k_log_r = 0.042F;
  for (std::size_t row = 0; row < k_logs_per_row.size(); ++row) {
    for (int log = 0; log < k_logs_per_row[row]; ++log) {
      int const log_seed = seed * 29 + static_cast<int>(row) * 5 + log;
      float const x =
          pile_x - (static_cast<float>(log) + 0.5F * static_cast<float>(row)) *
                       (k_log_r * 2.05F);
      float const y = k_log_r + static_cast<float>(row) * k_log_r * 1.78F;
      float const z0 = -0.58F + jitter(log_seed, 1) * 0.05F;
      float const z1 = 0.12F + jitter(log_seed, 2) * 0.05F;
      float const r = k_log_r * (0.88F + decay_hash(log_seed) * 0.22F);
      desc.add_cylinder(QVector3D(x, y, z0),
                        QVector3D(x, y, z1),
                        r,
                        k_log_bark * (0.9F + decay_hash(log_seed + 3) * 0.2F),
                        k_building_state_mask_intact);
      for (float const end : {z0 - 0.004F, z1 + 0.004F}) {
        desc.add_cylinder(QVector3D(x, y, end - 0.003F),
                          QVector3D(x, y, end + 0.003F),
                          r * 0.86F,
                          k_log_end,
                          k_building_state_mask_intact);
      }
    }
  }
  QVector3D const block(-(p + 0.16F), 0.0F, 0.40F);
  desc.add_cylinder(block,
                    block + QVector3D(0.0F, 0.085F, 0.0F),
                    0.062F,
                    k_log_bark,
                    k_building_state_mask_intact);
  desc.add_cylinder(block + QVector3D(0.0F, 0.083F, 0.0F),
                    block + QVector3D(0.0F, 0.089F, 0.0F),
                    0.056F,
                    k_log_end,
                    k_building_state_mask_intact);
  desc.add_cylinder(block + QVector3D(0.02F, 0.089F, 0.0F),
                    block + QVector3D(0.09F, 0.20F, 0.03F),
                    0.008F,
                    k_plank,
                    k_building_state_mask_intact);
  desc.add_box(block + QVector3D(0.02F, 0.100F, -0.004F),
               QVector3D(0.030F, 0.014F, 0.006F),
               k_iron,
               k_building_state_mask_intact);

  float const bed_x = p + 0.10F;
  desc.add_box(QVector3D(bed_x, 0.050F, 0.0F),
               QVector3D(0.075F, 0.050F, 0.52F),
               k_plank,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(bed_x, 0.1015F, 0.0F),
               QVector3D(0.062F, 0.0025F, 0.50F),
               k_earth_dark,
               k_building_state_mask_intact);
  for (int plant = 0; plant < 8; ++plant) {
    int const plant_seed = seed * 41 + plant;
    float const z =
        -0.44F + 0.125F * static_cast<float>(plant) + jitter(plant_seed, 1) * 0.03F;
    float const height = 0.07F + decay_hash(plant_seed) * 0.08F;
    QVector3D const base(bed_x + jitter(plant_seed, 2) * 0.03F, 0.102F, z);
    desc.add_cone(base,
                  base + QVector3D(0.0F, height, 0.0F),
                  0.050F + decay_hash(plant_seed + 5) * 0.022F,
                  leaf(seed, plant),
                  k_building_state_mask_intact);
    if (decay_hash(plant_seed + 9) > 0.55F) {
      auto const bloom =
          k_blossoms[static_cast<std::size_t>(plant) % k_blossoms.size()];
      desc.add_cylinder(base + QVector3D(0.0F, height * 0.82F, 0.0F),
                        base + QVector3D(0.0F, height * 0.82F + 0.012F, 0.0F),
                        0.018F,
                        bloom,
                        k_building_state_mask_intact);
    }
  }

  QVector3D const root(-(w + 0.04F), 0.0F, w + 0.04F);
  QVector3D previous = root;
  for (int step = 1; step <= 6; ++step) {
    QVector3D const next = root + QVector3D(jitter(seed, step) * 0.03F,
                                            0.15F * static_cast<float>(step),
                                            jitter(seed, step + 20) * 0.03F);
    desc.add_cylinder(previous, next, 0.012F, k_log_bark, k_building_state_mask_intact);
    previous = next;
  }
  for (int clump = 0; clump < 11; ++clump) {
    int const clump_seed = seed * 53 + clump;
    float const along = decay_hash(clump_seed) * 0.34F;
    float const y = 0.18F + decay_hash(clump_seed + 1) * 0.74F;
    bool const front = clump % 2 == 0;
    QVector3D const on_wall = front ? QVector3D(-(w - along), y, w + 0.012F)
                                    : QVector3D(-(w + 0.012F), y, w - along);
    QVector3D const outward =
        front ? QVector3D(0.0F, 0.0F, 1.0F) : QVector3D(-1.0F, 0.0F, 0.0F);
    desc.add_cylinder(on_wall,
                      on_wall + outward * (0.030F + decay_hash(clump_seed + 2) * 0.03F),
                      0.055F + decay_hash(clump_seed + 3) * 0.045F,
                      leaf(seed, clump + 30),
                      k_building_state_mask_intact);
  }

  float const jar_x = style.door_half_width + 0.17F;
  add_jar(desc, QVector3D(-jar_x, 0.0F, p + 0.07F), 1.0F, style.clay, style.clay_dark);
  add_jar(desc, QVector3D(jar_x, 0.0F, p + 0.09F), 0.86F, style.clay, style.clay_dark);
  add_jar(desc,
          QVector3D(-jar_x - 0.15F, 0.0F, p + 0.02F),
          0.72F,
          style.clay * 0.94F,
          style.clay_dark);
  QVector3D const pithos(w * 0.55F, 0.0F, -(p + 0.12F));
  desc.add_cylinder(pithos,
                    pithos + QVector3D(0.0F, 0.15F, 0.0F),
                    0.105F,
                    style.clay,
                    k_building_state_mask_intact);
  desc.add_cone(pithos + QVector3D(0.0F, 0.148F, 0.0F),
                pithos + QVector3D(0.0F, 0.215F, 0.0F),
                0.105F,
                style.clay,
                k_building_state_mask_intact);
  desc.add_cylinder(pithos + QVector3D(0.0F, 0.196F, 0.0F),
                    pithos + QVector3D(0.0F, 0.208F, 0.0F),
                    0.060F,
                    k_plank,
                    k_building_state_mask_intact);
  for (int sack = 0; sack < 3; ++sack) {
    QVector3D const at(w * 0.55F - 0.20F - 0.11F * static_cast<float>(sack % 2),
                       0.0F,
                       -(p + 0.08F + 0.05F * static_cast<float>(sack / 2)));
    float const lift = sack == 2 ? 0.085F : 0.0F;
    desc.add_cylinder(at + QVector3D(0.0F, lift, 0.0F),
                      at + QVector3D(0.0F, lift + 0.09F, 0.0F),
                      0.055F,
                      k_sack * (0.94F + decay_hash(seed + sack * 3) * 0.10F),
                      k_building_state_mask_intact);
    desc.add_cone(at + QVector3D(0.0F, lift + 0.088F, 0.0F),
                  at + QVector3D(0.0F, lift + 0.118F, 0.0F),
                  0.050F,
                  k_sack * 0.92F,
                  k_building_state_mask_intact);
  }

  add_path_stones(desc, style);
  add_wall_foot_wear(desc, style);
  add_rain_sump(desc, style);
  add_work_yard(desc, style);

  QVector3D const flue(style.vent.x(), style.chimney_base_y, style.vent.z());

  float const top =
      style.clay_oven_chimney ? style.vent.y() - 0.10F : style.vent.y() + 0.02F;
  if (style.clay_oven_chimney) {
    desc.add_cylinder(flue,
                      QVector3D(flue.x(), top, flue.z()),
                      0.085F,
                      style.clay,
                      k_building_state_mask_intact);
    desc.add_cylinder(QVector3D(flue.x(), top - 0.012F, flue.z()),
                      QVector3D(flue.x(), top + 0.012F, flue.z()),
                      0.100F,
                      style.clay_dark,
                      k_building_state_mask_intact);
  } else {
    float const half_h = (top - flue.y()) * 0.5F;
    desc.add_box(QVector3D(flue.x(), flue.y() + half_h, flue.z()),
                 QVector3D(0.085F, half_h, 0.085F),
                 style.chimney,
                 k_building_state_mask_intact);
    desc.add_box(QVector3D(flue.x(), top + 0.014F, flue.z()),
                 QVector3D(0.092F, 0.014F, 0.092F),
                 style.chimney * 0.82F,
                 k_building_state_mask_intact);
  }
}

void register_home_renderer_variant(EntityRendererRegistry& registry,
                                    const HomeRendererConfig& config) {

  static const bool props_registered = [] {
    register_home_prop_archetypes();
    return true;
  }();
  (void)props_registered;
  register_building_renderer(
      registry,
      config.nation_slug,
      "home",
      [config](const DrawContext& ctx, ISubmitter& out) {
        if (ctx.entity == nullptr) {
          return;
        }

        auto* r = ctx.entity->get_component<Engine::Core::RenderableComponent>();
        if (r == nullptr) {
          return;
        }

        const QVector3D team = Render::entity_color(*ctx.entity);
        const std::uint32_t seed =
            home_placement_seed(ctx.model(0, 3),
                                ctx.model(2, 3),
                                static_cast<std::uint32_t>(ctx.entity->get_id()));
        const BuildingState state = resolve_building_state(ctx);
        const auto palette_slots = config.palette_slots(team, seed, state);
        submit_building_instance(out,
                                 ctx,
                                 config.archetype(state, home_visual_variant(seed)),
                                 palette_slots);
        submit_home_activity(ctx, out, config.nation_slug == "carthage");
        submit_building_torches(ctx, out, config.torches);
        draw_building_selection_overlay(out, ctx, config.selection);
      });
}

} // namespace Render::GL
