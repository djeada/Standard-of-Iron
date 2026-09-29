#include "undead_guardians.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "core/component_combat.h"
#include "core/component_core.h"
#include "core/component_gameplay.h"
#include "core/entity.h"
#include "core/world.h"
#include "game/map/terrain_service.h"
#include "game/systems/combat_rules.h"
#include "game/systems/combat_system/combat_utils.h"
#include "game/systems/movement/order_service.h"
#include "game/util/planar_math.h"

namespace Game::Systems {

namespace {

constexpr float k_golden_angle_radians = 2.3999632F;
constexpr float k_min_spawn_ring_radius = 2.0F;
constexpr float k_spawn_ring_fraction = 0.8F;
constexpr int k_spawn_placement_attempts = 12;

constexpr float k_leash_poll_seconds = 0.5F;
constexpr float k_leash_slack = 1.5F;
constexpr float k_min_guard_radius = 2.0F;
constexpr float k_post_ring_fraction = 0.5F;
constexpr float k_min_post_ring_radius = 1.5F;
constexpr float k_post_ring_margin = 2.0F;
constexpr float k_post_ring_drift_degrees_per_second = 4.0F;
constexpr float k_post_arrival_distance = 1.25F;
constexpr int k_post_placement_attempts = 6;

auto post_ring_radius(const Game::Map::UndeadZone& definition) -> float {
  float const inside_leash =
      std::max(k_min_post_ring_radius, definition.leash_radius - k_post_ring_margin);
  return std::clamp(
      definition.radius * k_post_ring_fraction, k_min_post_ring_radius, inside_leash);
}

auto yaw_degrees_toward(float dx, float dz) -> float {
  return std::atan2(dx, dz) * 180.0F / std::numbers::pi_v<float>;
}
} // namespace

UndeadGuardians::UndeadGuardians(const Game::Map::TerrainService& terrain)
    : m_terrain(terrain) {
}

auto UndeadGuardians::guard_post_for_index(const UndeadRuntimeZone& zone,
                                           int post_index,
                                           int post_count) const -> QVector3D {
  auto const& terrain_service = m_terrain;
  QVector3D const origin = undead_zone_origin(zone);
  int const total = std::max(1, post_count);
  float const ring = post_ring_radius(zone.definition);
  float const base_angle =
      zone.post_ring_phase_degrees * std::numbers::pi_v<float> / 180.0F +
      2.0F * std::numbers::pi_v<float> * static_cast<float>(post_index) /
          static_cast<float>(total);

  for (int attempt = 0; attempt < k_post_placement_attempts; ++attempt) {
    float const angle =
        base_angle + static_cast<float>(attempt) * (k_golden_angle_radians * 0.5F);
    float const sample_radius =
        std::max(k_min_post_ring_radius, ring - static_cast<float>(attempt) * 0.5F);
    float const world_x = origin.x() + std::cos(angle) * sample_radius;
    float const world_z = origin.z() + std::sin(angle) * sample_radius;
    if (terrain_service.is_initialized() &&
        terrain_service.is_forbidden_world(world_x, world_z)) {
      continue;
    }
    return terrain_service.resolve_surface_world_position(
        world_x, world_z, k_undead_spawn_y_offset, origin.y());
  }

  return origin;
}

void UndeadGuardians::station_guardian(Engine::Core::World& world,
                                       const UndeadRuntimeZone& zone,
                                       Engine::Core::EntityID guardian_id,
                                       int post_index,
                                       int post_count,
                                       bool recall) const {
  auto* guardian = world.get_entity(guardian_id);
  auto* transform = world.try_get<Engine::Core::TransformComponent>(guardian_id);
  if (guardian == nullptr || transform == nullptr) {
    return;
  }

  QVector3D const post = guard_post_for_index(zone, post_index, post_count);
  QVector3D const origin = undead_zone_origin(zone);

  auto* guard =
      Engine::Core::get_or_add_component<Engine::Core::GuardModeComponent>(*guardian);
  if (guard == nullptr) {
    return;
  }
  guard->active = true;
  guard->has_guard_target = true;
  guard->guarded_entity_id = 0;
  guard->guard_position_x = post.x();
  guard->guard_position_z = post.z();
  guard->has_reach_center = true;
  guard->reach_center_x = origin.x();
  guard->reach_center_z = origin.z();
  guard->guard_radius = std::max(k_min_guard_radius, zone.definition.leash_radius);

  auto const* movement = world.try_get<Engine::Core::MovementComponent>(guardian_id);
  if (recall) {
    OrderService::clear_attack_target(guardian);
    CombatRules::clear_rts_melee_lock(guardian);
    float const homeward_reach = post_ring_radius(zone.definition) + k_post_ring_margin;
    bool const already_homeward =
        movement != nullptr && movement->get_has_target() &&
        std::hypot(movement->get_goal_x() - origin.x(),
                   movement->get_goal_y() - origin.z()) <= homeward_reach;
    if (!already_homeward) {
      OrderService::reset_movement(guardian);
      guard->returning_to_guard_position = false;
    }
    return;
  }

  auto const* attack_target =
      world.try_get<Engine::Core::AttackTargetComponent>(guardian_id);
  bool const busy = (attack_target != nullptr && attack_target->target_id != 0) ||
                    (movement != nullptr && movement->get_has_target());
  if (busy || transform->has_desired_yaw) {
    return;
  }

  float const dx = transform->position.x - post.x();
  float const dz = transform->position.z - post.z();
  if (dx * dx + dz * dz > k_post_arrival_distance * k_post_arrival_distance) {
    return;
  }
  float const out_x = post.x() - origin.x();
  float const out_z = post.z() - origin.z();
  if (out_x * out_x + out_z * out_z < 0.0001F) {
    return;
  }
  float const outward = yaw_degrees_toward(out_x, out_z);
  if (std::fabs(Game::Systems::signed_yaw_delta(transform->rotation.y, outward)) <
      1.0F) {
    return;
  }
  transform->desired_yaw = outward;
  transform->has_desired_yaw = true;
}

void UndeadGuardians::enforce_leash(Engine::Core::World& world,
                                    const UndeadRuntimeZone& zone) const {
  if (zone.active_spawn_ids.empty()) {
    return;
  }

  QVector3D const origin = undead_zone_origin(zone);
  float const leash = zone.definition.leash_radius + k_leash_slack;
  int const post_count = static_cast<int>(zone.active_spawn_ids.size());

  for (int index = 0; index < post_count; ++index) {
    Engine::Core::EntityID const guardian_id = zone.active_spawn_ids[index];
    auto const* transform =
        world.try_get<Engine::Core::TransformComponent>(guardian_id);
    if (transform == nullptr) {
      continue;
    }
    auto const* attack_target =
        world.try_get<Engine::Core::AttackTargetComponent>(guardian_id);
    auto* prey = attack_target != nullptr && attack_target->target_id != 0
                     ? world.get_entity(attack_target->target_id)
                     : nullptr;
    float allowed = leash;
    bool strayed = false;
    if (prey != nullptr) {
      auto* guardian = world.get_entity(guardian_id);
      strayed = !Combat::within_guard_reach(
          guardian, prey, Combat::GuardReachRule::AnswersFire);
      allowed += Combat::guard_answer_fire_margin(prey);
    }
    float const dx = transform->position.x - origin.x();
    float const dz = transform->position.z - origin.z();
    strayed = strayed || dx * dx + dz * dz > allowed * allowed;
    station_guardian(world, zone, guardian_id, index, post_count, strayed);
  }
}

auto UndeadGuardians::spawn_position_for_index(const UndeadRuntimeZone& zone,
                                               int spawn_index,
                                               int spawn_count) const -> QVector3D {
  auto const& terrain_service = m_terrain;
  QVector3D const origin = undead_zone_origin(zone);

  float const outer_radius =
      std::max(k_min_spawn_ring_radius, zone.definition.radius * k_spawn_ring_fraction);
  int const total = std::max(1, spawn_count);

  float const normalized =
      std::sqrt((static_cast<float>(spawn_index) + 0.5F) / static_cast<float>(total));
  float const base_radius =
      std::max(k_min_spawn_ring_radius * 0.5F, normalized * outer_radius);
  float const base_angle = static_cast<float>(spawn_index) * k_golden_angle_radians;

  for (int attempt = 0; attempt < k_spawn_placement_attempts; ++attempt) {
    float const angle =
        base_angle + static_cast<float>(attempt) * (k_golden_angle_radians * 0.25F);
    float const sample_radius =
        std::min(outer_radius, base_radius + static_cast<float>(attempt) * 0.6F);
    float const world_x = origin.x() + std::cos(angle) * sample_radius;
    float const world_z = origin.z() + std::sin(angle) * sample_radius;
    if (terrain_service.is_initialized() &&
        terrain_service.is_forbidden_world(world_x, world_z)) {
      continue;
    }
    return terrain_service.resolve_surface_world_position(
        world_x, world_z, k_undead_spawn_y_offset, origin.y());
  }

  return terrain_service.resolve_surface_world_position(
      origin.x(), origin.z(), k_undead_spawn_y_offset, origin.y());
}

void UndeadGuardians::enforce_leashes(Engine::Core::World& world,
                                      std::vector<UndeadRuntimeZone>& zones,
                                      float delta_time) {
  m_leash_poll += delta_time;
  if (m_leash_poll < k_leash_poll_seconds) {
    return;
  }
  for (auto& zone : zones) {
    zone.post_ring_phase_degrees =
        std::fmod(zone.post_ring_phase_degrees +
                      k_post_ring_drift_degrees_per_second * m_leash_poll,
                  360.0F);
    enforce_leash(world, zone);
  }
  m_leash_poll = 0.0F;
}

void UndeadGuardians::post_new_wave(Engine::Core::World& world,
                                    const UndeadRuntimeZone& zone) const {
  int const post_count = static_cast<int>(zone.active_spawn_ids.size());
  for (int index = 0; index < post_count; ++index) {
    station_guardian(
        world, zone, zone.active_spawn_ids[index], index, post_count, true);
  }
}

} // namespace Game::Systems
