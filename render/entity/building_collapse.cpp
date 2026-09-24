#include "building_collapse.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

#include "game/core/component_core.h"
#include "game/core/component_gameplay.h"
#include "game/core/component_structures.h"
#include "game/core/death_sequence.h"
#include "game/core/entity.h"
#include "game/core/world.h"
#include "render/gl/primitives.h"
#include "render/submitter.h"

namespace Render::GL {
namespace {

constexpr float k_pi = std::numbers::pi_v<float>;
constexpr float k_shudder_degrees = 1.4F;
constexpr float k_fall_tilt_degrees = 9.0F;
constexpr float k_fall_spread = 0.10F;
constexpr float k_fall_slump = 0.14F;
constexpr float k_chunk_drop_seconds = 0.38F;
constexpr float k_dust_settle_seconds = 5.0F;
constexpr float k_smoke_seconds = 9.0F;

auto hash01(std::uint32_t seed, std::uint32_t salt) noexcept -> float {
  std::uint32_t h = seed * 747796405U + salt * 2891336453U + 0x9E3779B9U;
  h ^= h >> 16U;
  h *= 0x7FEB352DU;
  h ^= h >> 15U;
  h *= 0x846CA68BU;
  h ^= h >> 16U;
  return static_cast<float>(h & 0xFFFFFFU) / static_cast<float>(0x1000000U);
}

auto signed_hash(std::uint32_t seed, std::uint32_t salt) noexcept -> float {
  return (hash01(seed, salt) * 2.0F) - 1.0F;
}

auto smooth(float t) noexcept -> float {
  t = std::clamp(t, 0.0F, 1.0F);
  return t * t * (3.0F - (2.0F * t));
}

// Where a point on the footprint sits in the world: `local` is in footprint
// units (-1..1 on each axis), turned by the building's yaw.
auto footprint_point(const BuildingCollapse& collapse,
                     float local_x,
                     float local_z) -> QVector3D {
  float const yaw = collapse.yaw_degrees * (k_pi / 180.0F);
  float const x = local_x * collapse.footprint.half_width;
  float const z = local_z * collapse.footprint.half_depth;
  float const c = std::cos(yaw);
  float const s = std::sin(yaw);
  return {collapse.base.x() + (x * c) + (z * s),
          collapse.base.y(),
          collapse.base.z() - (x * s) + (z * c)};
}

auto heading_direction(const BuildingCollapse& collapse) -> QVector3D {
  return {std::sin(collapse.heading), 0.0F, std::cos(collapse.heading)};
}

// Seconds after death at which the ruin has come `through` of the way down
// (0 = still standing, 1 = landed); inverts the fall curve below.
auto seconds_at_fall_through(const BuildingCollapse& collapse,
                             float through) noexcept -> float {
  float const progress =
      k_collapse_shudder_fraction + (through * (1.0F - k_collapse_shudder_fraction));
  return progress * collapse.duration;
}

auto footprint_radius(const BuildingCollapse& collapse) noexcept -> float {
  return std::sqrt(collapse.footprint.half_width * collapse.footprint.half_depth);
}

} // namespace

auto building_collapse_footprint(Game::Units::SpawnType type) noexcept
    -> BuildingCollapseFootprint {
  using Game::Units::SpawnType;
  switch (type) {
  case SpawnType::Barracks:
    return {.half_width = 2.6F, .half_depth = 2.6F, .height = 3.2F};
  case SpawnType::Home:
    return {.half_width = 2.15F, .half_depth = 2.2F, .height = 2.6F};
  case SpawnType::Marketplace:
    return {.half_width = 2.75F, .half_depth = 2.75F, .height = 2.2F};
  case SpawnType::Temple:
    return {.half_width = 6.2F, .half_depth = 4.6F, .height = 4.6F};
  case SpawnType::Farm:
    return {.half_width = 3.0F, .half_depth = 3.0F, .height = 2.0F};
  case SpawnType::DefenseTower:
    return {.half_width = 1.5F, .half_depth = 1.5F, .height = 4.8F};
  case SpawnType::WallSegment:
    return {.half_width = 1.1F, .half_depth = 1.0F, .height = 2.4F};
  case SpawnType::WallGate:
    return {.half_width = Engine::Core::GateComponent::k_structure_half_span,
            .half_depth = Engine::Core::GateComponent::k_cross_half_extent,
            .height = 3.2F};
  default:
    return {};
  }
}

auto resolve_building_collapse(const Engine::Core::World& world,
                               Engine::Core::EntityID entity_id) -> BuildingCollapse {
  BuildingCollapse collapse;
  if (!world.has<Engine::Core::BuildingComponent>(entity_id)) {
    return collapse;
  }
  auto const* death = world.try_get<Engine::Core::DeathAnimationComponent>(entity_id);
  auto const* transform = world.try_get<Engine::Core::TransformComponent>(entity_id);
  if (death == nullptr || transform == nullptr ||
      death->profile != Engine::Core::DeathSequenceProfile::Structure) {
    return collapse;
  }

  collapse.active = true;
  collapse.state = death->state;
  collapse.elapsed = Engine::Core::death_sequence_elapsed(*death);
  collapse.duration = std::max(death->state_duration, 0.001F);
  collapse.settled_for = std::max(0.0F, collapse.elapsed - death->state_duration);
  collapse.heading =
      static_cast<float>(death->sequence_variant) * (2.0F * k_pi / 256.0F);
  collapse.yaw_degrees = transform->rotation.y;
  collapse.seed = static_cast<std::uint32_t>(entity_id) * 2654435761U;
  collapse.base =
      QVector3D(transform->position.x, transform->position.y, transform->position.z);
  if (auto const* unit = world.try_get<Engine::Core::UnitComponent>(entity_id)) {
    collapse.footprint = building_collapse_footprint(unit->spawn_type);
  }

  float const progress =
      death->state == Engine::Core::DeathSequenceState::Dying
          ? std::clamp(
                death->state_time / std::max(death->state_duration, 0.001F), 0.0F, 1.0F)
          : 1.0F;
  // The structure first shudders on its failing supports, then gives way and
  // accelerates down like anything falling, landing at the end of Dying.
  float const shudder_end = k_collapse_shudder_fraction;
  collapse.shudder =
      progress < shudder_end
          ? smooth(progress / shudder_end)
          : std::clamp(1.0F - ((progress - shudder_end) / 0.30F), 0.0F, 1.0F);
  float const fall_t =
      std::clamp((progress - shudder_end) / (1.0F - shudder_end), 0.0F, 1.0F);
  collapse.fall = fall_t * fall_t;
  collapse.sink = smooth(Engine::Core::death_sink_progress(*death));
  return collapse;
}

auto building_collapse_sink_depth(const BuildingCollapse& collapse) noexcept -> float {
  return std::max(0.9F, collapse.footprint.height * k_collapse_remaining_height) +
         0.25F;
}

auto building_collapse_model(const QMatrix4x4& model,
                             const BuildingCollapse& collapse) -> QMatrix4x4 {
  if (!collapse.active) {
    return model;
  }
  QVector3D const heading = heading_direction(collapse);
  QVector3D const tilt_axis =
      QVector3D::crossProduct(QVector3D(0.0F, 1.0F, 0.0F), heading).normalized();

  float const fall = collapse.fall;
  float const shake_a =
      std::sin((collapse.elapsed * 47.0F) + (hash01(collapse.seed, 1U) * 6.0F));
  float const shake_b =
      std::sin((collapse.elapsed * 61.0F) + (hash01(collapse.seed, 2U) * 6.0F));
  float const shake = collapse.shudder * k_shudder_degrees;

  QVector3D const slump = heading * (k_fall_slump * footprint_radius(collapse) * fall);
  float const sink = collapse.sink * building_collapse_sink_depth(collapse);

  QMatrix4x4 world;
  world.translate(collapse.base + slump - QVector3D(0.0F, sink, 0.0F));
  world.rotate(fall * k_fall_tilt_degrees, tilt_axis);
  world.rotate(shake * shake_a, tilt_axis);
  world.rotate(shake * shake_b, heading);
  float const spread = 1.0F + (k_fall_spread * fall);
  world.scale(spread, 1.0F - ((1.0F - k_collapse_remaining_height) * fall), spread);
  world.translate(-collapse.base);
  return world * model;
}

void submit_building_collapse_rubble(ISubmitter& out,
                                     const BuildingCollapse& collapse) {
  if (!collapse.active) {
    return;
  }
  Mesh* cube = get_unit_cube();
  if (cube == nullptr) {
    return;
  }

  float const radius = footprint_radius(collapse);
  float const area =
      4.0F * collapse.footprint.half_width * collapse.footprint.half_depth;
  int const count = std::clamp(static_cast<int>(area * 0.9F), 10, 40);
  float const chunk_scale = std::clamp(radius / 2.0F, 0.6F, 1.5F);
  float const sink = collapse.sink * building_collapse_sink_depth(collapse);
  QVector3D const heading = heading_direction(collapse);

  float const fall_time = std::max(0.0F, collapse.elapsed);

  for (int i = 0; i < count; ++i) {
    auto const index = static_cast<std::uint32_t>(i);
    std::uint32_t const seed = collapse.seed + (index * 97U);

    float const release = 0.12F + (hash01(seed, 3U) * 0.70F);
    if (collapse.fall < release * release) {
      continue;
    }
    float const released_at =
        seconds_at_fall_through(collapse, release) + (hash01(seed, 4U) * 0.05F);
    float const drop_t =
        std::clamp((fall_time - released_at) / k_chunk_drop_seconds, 0.0F, 1.0F);

    float const angle = hash01(seed, 5U) * 2.0F * k_pi;
    float const reach = std::sqrt(hash01(seed, 6U)) * 1.05F;
    QVector3D rest =
        footprint_point(collapse, std::cos(angle) * reach, std::sin(angle) * reach);
    rest += heading * (0.25F * radius * hash01(seed, 7U));

    float const size = (0.20F + (hash01(seed, 8U) * 0.34F)) * chunk_scale;
    float const height = size * (0.45F + (hash01(seed, 9U) * 0.45F));
    float const drop_from =
        collapse.footprint.height * (0.35F + (hash01(seed, 10U) * 0.55F));
    float const lift = drop_from * (1.0F - (drop_t * drop_t));
    float const tumble = (1.0F - drop_t) * 160.0F * signed_hash(seed, 11U);

    bool const timber = hash01(seed, 12U) < 0.3F;
    QVector3D const stone(0.55F, 0.52F, 0.47F);
    QVector3D const stone_dark(0.34F, 0.32F, 0.29F);
    QVector3D const beam(0.27F, 0.20F, 0.14F);
    float const shade = 0.80F + (hash01(seed, 13U) * 0.30F);
    QVector3D const color =
        (timber ? beam : stone_dark + ((stone - stone_dark) * hash01(seed, 14U))) *
        shade;

    QMatrix4x4 model;
    model.translate(rest.x(), rest.y() + (height * 0.55F) + lift - sink, rest.z());
    model.rotate(hash01(seed, 15U) * 180.0F, 0.0F, 1.0F, 0.0F);
    model.rotate((signed_hash(seed, 16U) * 24.0F) + tumble, 1.0F, 0.0F, 0.0F);
    model.rotate(signed_hash(seed, 17U) * 18.0F, 0.0F, 0.0F, 1.0F);
    if (timber) {
      model.scale(size * 1.9F, height * 0.42F, height * 0.42F);
    } else {
      model.scale(size, height, size * (0.7F + (hash01(seed, 18U) * 0.3F)));
    }
    out.mesh(cube, model, color, nullptr, 1.0F, 10);
  }
}

void submit_building_collapse_effects(ISubmitter& out,
                                      const BuildingCollapse& collapse,
                                      float animation_time) {
  if (!collapse.active) {
    return;
  }
  float const radius = footprint_radius(collapse);
  QVector3D const dust_color(0.60F, 0.55F, 0.47F);

  // The shudder shakes loose a little grit before anything falls.
  if (collapse.state == Engine::Core::DeathSequenceState::Dying &&
      collapse.shudder > 0.0F) {
    out.combat_dust(collapse.base,
                    dust_color,
                    radius * 0.9F,
                    0.55F * collapse.shudder,
                    animation_time);
  }

  // Masonry hitting the ground throws out a ring of debris bursts. Each one is
  // an impact whose age runs from the moment the ruin lands.
  float const since_impact =
      collapse.elapsed - seconds_at_fall_through(collapse, 0.55F);
  if (since_impact >= 0.0F && since_impact < 6.0F) {
    int const bursts = std::clamp(static_cast<int>(radius * 2.0F), 3, 8);
    for (int i = 0; i < bursts; ++i) {
      auto const index = static_cast<std::uint32_t>(i);
      float const angle =
          ((static_cast<float>(i) + (hash01(collapse.seed, 30U + index) * 0.6F)) /
           static_cast<float>(bursts)) *
          2.0F * k_pi;
      float const reach = 0.45F + (hash01(collapse.seed, 40U + index) * 0.45F);
      QVector3D const at =
          footprint_point(collapse, std::cos(angle) * reach, std::sin(angle) * reach);
      float const delay = hash01(collapse.seed, 50U + index) * 0.35F;
      float const age = since_impact - delay;
      if (age < 0.0F) {
        continue;
      }
      out.stone_impact(at, dust_color, radius * 0.55F, 1.4F, age);
    }
  }

  // A dust cloud rolls out from the ruin and settles over a few seconds.
  float const cloud =
      collapse.fall > 0.2F
          ? std::clamp(
                1.0F - (collapse.settled_for / k_dust_settle_seconds), 0.0F, 1.0F)
          : 0.0F;
  if (cloud > 0.0F && collapse.state != Engine::Core::DeathSequenceState::Sinking) {
    float const swell = 0.8F + (0.6F * std::min(1.0F, collapse.settled_for / 1.5F));
    out.combat_dust(collapse.base,
                    dust_color,
                    radius * 1.35F * swell,
                    1.6F * cloud * std::min(1.0F, collapse.fall * 1.5F),
                    animation_time);
    for (int i = 0; i < 4; ++i) {
      auto const index = static_cast<std::uint32_t>(i);
      float const angle =
          (static_cast<float>(i) * 0.5F * k_pi) + hash01(collapse.seed, 60U + index);
      QVector3D const at =
          footprint_point(collapse, std::cos(angle) * 1.1F, std::sin(angle) * 1.1F);
      out.combat_dust(at,
                      dust_color,
                      radius * 0.8F * swell,
                      1.1F * cloud,
                      animation_time + static_cast<float>(i) * 1.7F);
    }
  }

  // Smoke keeps rising from the rubble after the dust has gone.
  if (collapse.state != Engine::Core::DeathSequenceState::Dying) {
    float const smoke =
        std::clamp(1.0F - (collapse.settled_for / k_smoke_seconds), 0.0F, 1.0F) *
        (1.0F - collapse.sink);
    int const columns = std::clamp(static_cast<int>(radius), 1, 3);
    for (int i = 0; i < columns && smoke > 0.0F; ++i) {
      auto const index = static_cast<std::uint32_t>(i);
      QVector3D const at =
          footprint_point(collapse,
                          signed_hash(collapse.seed, 70U + index) * 0.5F,
                          signed_hash(collapse.seed, 80U + index) * 0.5F) +
          QVector3D(0.0F, 0.3F, 0.0F);
      out.hearth_smoke(at,
                       QVector3D(0.30F, 0.29F, 0.28F),
                       0.55F + (radius * 0.12F),
                       0.55F * smoke,
                       (animation_time * 0.55F) +
                           (hash01(collapse.seed, 90U + index) * 50.0F));
    }
  }
}

} // namespace Render::GL
