#include "app/commander/commander_targeting.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <optional>
#include <vector>

#include "app/commander/commander_entity_access.h"
#include "app/commander/commander_heading.h"
#include "app/commander/commander_primary_scan.h"
#include "game/audio/audio_cues.h"
#include "game/core/component.h"
#include "game/core/simulation_timing.h"
#include "game/core/world.h"
#include "game/session/session_context.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/building_line_of_sight.h"
#include "game/systems/combat_system/target_rules.h"
#include "game/systems/rpg_combat_system/rpg_bow_aim.h"
#include "game/systems/rpg_combat_system/rpg_bow_draw.h"
#include "game/systems/rpg_combat_system/rpg_commander_damage.h"
#include "game/systems/rpg_combat_system/rpg_targeting.h"

namespace App::Core {

namespace {

constexpr float k_lock_range = 12.0F;
constexpr float k_lock_range_sq = k_lock_range * k_lock_range;
constexpr float k_lock_max_angle_degrees = 70.0F;
constexpr float k_lock_drop_sq = 18.0F * 18.0F;
constexpr float k_lock_lost_seconds = 0.35F;
constexpr float k_lock_minimum_distance = 0.75F;
constexpr float k_lock_full_authority_distance = 2.0F;
constexpr float k_lock_max_turn_degrees_per_second = 220.0F;
constexpr float k_manual_override_seconds = 0.35F;

auto buildings_of(const Engine::Core::World& world)
    -> const Game::Systems::BuildingCollisionRegistry& {
  return Game::Session::session_for(world).building_collision();
}

struct LockCandidate {
  Engine::Core::EntityID id = 0;
  std::uint16_t soldier_slot{CommanderTargeting::k_no_slot};
  float angle_diff = 0.0F;
  float distance_sq = 0.0F;
  bool visible = false;
};

auto better_soldier(const LockCandidate& resolved, const LockCandidate& best) -> bool {
  return std::abs(resolved.angle_diff) < std::abs(best.angle_diff) ||
         (std::abs(resolved.angle_diff) == std::abs(best.angle_diff) &&
          resolved.distance_sq < best.distance_sq);
}

auto best_soldier_in_reach(Engine::Core::World& world,
                           Engine::Core::Entity& candidate,
                           const QVector3D& origin,
                           float view_yaw) -> std::optional<LockCandidate> {
  std::optional<LockCandidate> best_soldier;
  for (auto const& soldier :
       Game::Systems::RpgCombat::live_soldier_targets(candidate)) {
    const float dx = soldier.position.x() - origin.x();
    const float dz = soldier.position.z() - origin.z();
    const float distance_sq = dx * dx + dz * dz;
    if (distance_sq > k_lock_range_sq) {
      continue;
    }
    const float angle_diff =
        signed_angle_delta(std::atan2(dx, dz) * k_radians_to_degrees, view_yaw);
    if (std::abs(angle_diff) > k_lock_max_angle_degrees ||
        !Game::Systems::has_clear_building_los(
            buildings_of(world), origin, soldier.position)) {
      continue;
    }
    LockCandidate const resolved{
        candidate.get_id(), soldier.soldier_slot, angle_diff, distance_sq, true};
    if (!best_soldier.has_value() || better_soldier(resolved, *best_soldier)) {
      best_soldier = resolved;
    }
  }
  return best_soldier;
}

auto collect_lock_candidates(Engine::Core::World& world,
                             Engine::Core::EntityID commander_id,
                             int local_owner_id,
                             const QVector3D& origin,
                             float view_yaw) -> std::vector<LockCandidate> {
  auto& owners = Game::Session::session_for(world).owners();
  std::vector<LockCandidate> candidates;

  for (auto* candidate : world.collect_entities_with<Engine::Core::UnitComponent>()) {
    if (candidate == nullptr || candidate->get_id() == commander_id) {
      continue;
    }
    auto* u = candidate->get_component<Engine::Core::UnitComponent>();
    auto* t = candidate->get_component<Engine::Core::TransformComponent>();
    if (u == nullptr || t == nullptr) {
      continue;
    }
    if (Game::Systems::Combat::evaluate_target(
            owners,
            local_owner_id,
            candidate,
            {.intent = Game::Systems::Combat::EngagementIntent::AutoAcquired,
             .allow_buildings = true}) != Game::Systems::Combat::TargetRefusal::None) {
      continue;
    }
    if (auto best = best_soldier_in_reach(world, *candidate, origin, view_yaw);
        best.has_value()) {
      candidates.push_back(*best);
    }
  }
  return candidates;
}

auto nearest_to_centre(const std::vector<LockCandidate>& candidates)
    -> const LockCandidate& {
  return *std::min_element(candidates.begin(),
                           candidates.end(),
                           [](const LockCandidate& a, const LockCandidate& b) {
                             if (a.visible != b.visible) {
                               return a.visible && !b.visible;
                             }
                             if (std::abs(a.angle_diff) != std::abs(b.angle_diff)) {
                               return std::abs(a.angle_diff) < std::abs(b.angle_diff);
                             }
                             return a.distance_sq < b.distance_sq;
                           });
}

auto next_in_sweep(std::vector<LockCandidate>& candidates,
                   Engine::Core::EntityID current) -> const LockCandidate& {
  std::stable_sort(candidates.begin(),
                   candidates.end(),
                   [](const LockCandidate& a, const LockCandidate& b) {
                     if (a.angle_diff != b.angle_diff) {
                       return a.angle_diff < b.angle_diff;
                     }
                     return a.distance_sq < b.distance_sq;
                   });
  auto it = std::find_if(candidates.begin(),
                         candidates.end(),
                         [current](const LockCandidate& c) { return c.id == current; });
  if (it == candidates.end() || std::next(it) == candidates.end()) {
    return candidates.front();
  }
  return *std::next(it);
}

void clear_published_lock(Engine::Core::Entity& commander) {
  if (auto* targets =
          commander.get_component<Engine::Core::RpgCommanderTargetComponent>()) {
    targets->explicit_lock_target_id = 0;
    targets->explicit_lock_soldier_slot = CommanderTargeting::k_no_slot;
    targets->aim_candidate_id = 0;
    targets->aim_candidate_soldier_slot = CommanderTargeting::k_no_slot;
    targets->aim_candidate_in_range = false;
  }
}

} // namespace

void CommanderTargeting::reset() {
  m_locked_target_id = 0;
  m_locked_target_slot = k_no_slot;
  m_soft_target_id = 0;
  m_soft_target_slot = k_no_slot;
  m_primary_target_slot = k_no_slot;
  m_lock_lost_timer = 0.0F;
}

auto CommanderTargeting::locked_position(Engine::Core::World& world) const
    -> std::optional<QVector3D> {
  if (m_locked_target_id == 0) {
    return std::nullopt;
  }
  auto* target = world.get_entity(m_locked_target_id);
  auto* target_unit = (target != nullptr)
                          ? target->get_component<Engine::Core::UnitComponent>()
                          : nullptr;
  auto const target_sample = target != nullptr
                                 ? Game::Systems::RpgCombat::resolve_soldier_target(
                                       *target, m_locked_target_slot)
                                 : std::nullopt;
  if (target_sample.has_value() && target_unit != nullptr && target_unit->health > 0) {
    return target_sample->position;
  }
  return std::nullopt;
}

void CommanderTargeting::cycle_lock(Engine::Core::World& world,
                                    Engine::Core::EntityID commander_id,
                                    int local_owner_id,
                                    float view_yaw) {
  Engine::Core::Timing::ScopedAccumulator const lock_scope(
      Engine::Core::Timing::commander_targeting());
  auto* commander = controlled_commander(world, commander_id, local_owner_id);
  if (commander == nullptr) {
    return;
  }
  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  if (transform == nullptr) {
    return;
  }

  const QVector3D origin(transform->position.x, 0.0F, transform->position.z);
  auto candidates =
      collect_lock_candidates(world, commander_id, local_owner_id, origin, view_yaw);

  if (candidates.empty()) {
    m_locked_target_id = 0;
    m_locked_target_slot = k_no_slot;
    m_soft_target_id = 0;
    m_soft_target_slot = k_no_slot;
    m_lock_lost_timer = 0.0F;
    clear_published_lock(*commander);
    Game::Audio::play_cue(Game::Audio::Cue::k_combat_ability_refused);
    return;
  }

  LockCandidate const chosen = m_locked_target_id == 0
                                   ? nearest_to_centre(candidates)
                                   : next_in_sweep(candidates, m_locked_target_id);
  m_locked_target_id = chosen.id;
  m_locked_target_slot = chosen.soldier_slot;
  m_soft_target_id = m_locked_target_id;
  m_soft_target_slot = m_locked_target_slot;
  m_lock_lost_timer = 0.0F;
  Game::Audio::play_cue(Game::Audio::Cue::k_combat_lock_on);

  if (auto* rpg_targets =
          Engine::Core::get_or_add_component<Engine::Core::RpgCommanderTargetComponent>(
              commander)) {
    rpg_targets->explicit_lock_target_id = m_locked_target_id;
    rpg_targets->explicit_lock_soldier_slot = m_locked_target_slot;
    rpg_targets->aim_candidate_id = m_soft_target_id;
    rpg_targets->aim_candidate_soldier_slot = m_soft_target_slot;
  }
}

auto CommanderTargeting::measure_lock(Engine::Core::World& world,
                                      const Engine::Core::TransformComponent& body,
                                      float view_yaw) -> std::optional<LockGeometry> {
  auto* target = world.get_entity(m_locked_target_id);
  auto* target_unit = (target != nullptr)
                          ? target->get_component<Engine::Core::UnitComponent>()
                          : nullptr;
  auto target_sample = target != nullptr
                           ? Game::Systems::RpgCombat::resolve_soldier_target(
                                 *target, m_locked_target_slot)
                           : std::nullopt;
  if (target_unit == nullptr || target_unit->health <= 0 ||
      !target_sample.has_value()) {
    m_locked_target_id = 0;
    m_locked_target_slot = k_no_slot;
    m_lock_lost_timer = 0.0F;
    return std::nullopt;
  }
  m_locked_target_slot = target_sample->soldier_slot;

  LockGeometry geometry;
  geometry.dx = target_sample->position.x() - body.position.x;
  geometry.dz = target_sample->position.z() - body.position.z;
  const QVector3D origin(body.position.x, 0.0F, body.position.z);
  geometry.visible = Game::Systems::has_clear_building_los(
      buildings_of(world), origin, target_sample->position);
  geometry.yaw_error = signed_angle_delta(
      std::atan2(geometry.dx, geometry.dz) * k_radians_to_degrees, view_yaw);
  return geometry;
}

void CommanderTargeting::release_lock() {
  m_locked_target_id = 0;
  m_locked_target_slot = k_no_slot;
  m_soft_target_id = 0;
  m_soft_target_slot = k_no_slot;
  m_lock_lost_timer = 0.0F;
}

auto CommanderTargeting::lock_expired(const LockGeometry& geometry, float dt) -> bool {
  if (geometry.dx * geometry.dx + geometry.dz * geometry.dz > k_lock_drop_sq) {
    m_lock_lost_timer += dt * 2.0F;
  } else if (!geometry.visible) {
    m_lock_lost_timer += dt;
  } else {
    m_lock_lost_timer = 0.0F;
  }
  if (m_lock_lost_timer <= k_lock_lost_seconds) {
    return false;
  }
  m_locked_target_id = 0;
  m_locked_target_slot = k_no_slot;
  m_lock_lost_timer = 0.0F;
  return true;
}

auto CommanderTargeting::spring_view(const LockGeometry& geometry,
                                     float view_yaw,
                                     float dt) -> float {
  float const target_distance =
      std::sqrt((geometry.dx * geometry.dx) + (geometry.dz * geometry.dz));
  if (target_distance < k_lock_minimum_distance) {
    return view_yaw;
  }
  float const close_range_authority =
      std::clamp((target_distance - k_lock_minimum_distance) /
                     (k_lock_full_authority_distance - k_lock_minimum_distance),
                 0.0F,
                 1.0F);

  float const manual_look = std::abs(signed_angle_delta(view_yaw, m_lock_spring_yaw));
  if (m_lock_spring_yaw_valid && manual_look > 0.05F) {
    m_lock_manual_override_timer = k_manual_override_seconds;
  }
  if (m_lock_manual_override_timer > 0.0F) {
    m_lock_manual_override_timer = std::max(0.0F, m_lock_manual_override_timer - dt);
    m_lock_spring_yaw = view_yaw;
    m_lock_spring_yaw_valid = true;
    return view_yaw;
  }

  const float k_lock_spring = geometry.visible ? 8.5F : 3.5F;
  float step = geometry.yaw_error * (1.0F - std::exp(-k_lock_spring * dt)) *
               close_range_authority;
  float const step_limit = k_lock_max_turn_degrees_per_second * dt;
  step = std::clamp(step, -step_limit, step_limit);
  view_yaw = wrap_angle_degrees(view_yaw + step);
  m_lock_spring_yaw = view_yaw;
  m_lock_spring_yaw_valid = true;
  return view_yaw;
}

auto CommanderTargeting::steer_view_toward_lock(Engine::Core::World& world,
                                                Engine::Core::Entity& commander,
                                                const LockSteerInput& input) -> float {
  if (m_locked_target_id == 0) {
    m_lock_spring_yaw_valid = false;
    m_lock_manual_override_timer = 0.0F;
    return input.view_yaw;
  }

  auto* body = commander.get_component<Engine::Core::TransformComponent>();
  if (body == nullptr) {
    return input.view_yaw;
  }
  auto const geometry = measure_lock(world, *body, input.view_yaw);
  if (!geometry.has_value()) {
    return input.view_yaw;
  }

  const bool escape_input = (input.run && input.backward) ||
                            (input.dodge_pressed && (input.backward || input.run));
  if (escape_input || (input.run && std::abs(geometry->yaw_error) > 95.0F)) {
    release_lock();
    return input.view_yaw;
  }
  if (lock_expired(*geometry, input.dt)) {
    return input.view_yaw;
  }
  return spring_view(*geometry, input.view_yaw, input.dt);
}

auto CommanderTargeting::find_primary_target(Engine::Core::World& world,
                                             Engine::Core::EntityID commander_id,
                                             int local_owner_id,
                                             float view_yaw,
                                             float extra_reach)
    -> Engine::Core::EntityID {
  Engine::Core::Timing::ScopedAccumulator const scope(
      Engine::Core::Timing::commander_targeting());
  m_primary_target_slot = k_no_slot;

  auto* commander = controlled_commander(world, commander_id, local_owner_id);
  if (commander == nullptr) {
    return 0;
  }

  auto* commander_transform =
      commander->get_component<Engine::Core::TransformComponent>();
  auto* commander_attack = commander->get_component<Engine::Core::AttackComponent>();
  if (commander_transform == nullptr) {
    return 0;
  }

  const float max_range =
      commander_reach(*commander, commander_attack) + std::max(0.0F, extra_reach);
  const float yaw_rad = view_yaw * k_degrees_to_radians;
  const PrimaryScan scan{
      world,
      commander,
      local_owner_id,
      QVector3D(commander_transform->position.x, 0.0F, commander_transform->position.z),
      QVector3D(std::sin(yaw_rad), 0.0F, std::cos(yaw_rad)),
      max_range};

  auto const found = pick_primary(scan,
                                  *commander,
                                  m_locked_target_id,
                                  m_locked_target_slot,
                                  m_soft_target_id,
                                  m_soft_target_slot);
  if (!found.has_value()) {
    m_soft_target_id = 0;
    m_soft_target_slot = k_no_slot;
    return 0;
  }
  m_primary_target_slot = found->soldier_slot;
  m_soft_target_id = found->entity_id;
  m_soft_target_slot = found->soldier_slot;
  return found->entity_id;
}

auto CommanderTargeting::resolve_aim_candidate(
    Engine::Core::World& world,
    Engine::Core::Entity& commander,
    Engine::Core::EntityID commander_id,
    int local_owner_id,
    Engine::Core::RpgCommanderAimComponent* aim,
    float view_yaw) -> Engine::Core::EntityID {
  if (aim == nullptr || aim->stance != Engine::Core::FpvWeaponStance::Bow) {
    return find_primary_target(world, commander_id, local_owner_id, view_yaw, 0.0F);
  }

  if (!aim->is_drawing()) {
    auto const* stamina = commander.get_component<Engine::Core::StaminaComponent>();
    aim->spread_degrees = Game::Systems::RpgCombat::aim_spread_degrees(
        *aim, stamina != nullptr ? stamina->get_stamina_ratio() : 1.0F);
  }

  constexpr float k_crosshair_forgiveness = 0.22F;
  constexpr float k_aim_overshoot_range = 12.0F;
  auto const* commander_attack =
      commander.get_component<Engine::Core::AttackComponent>();
  float const range = (commander_attack != nullptr ? commander_attack->range : 12.0F) +
                      k_aim_overshoot_range;
  auto const hit = Game::Systems::RpgCombat::raycast_enemy_bodies(
      world,
      commander,
      Game::Systems::RpgCombat::commander_aim_ray(commander, *aim),
      range,
      k_crosshair_forgiveness);
  if (hit.has_value()) {
    m_primary_target_slot = hit->soldier_slot;
    return hit->entity_id;
  }
  m_primary_target_slot = k_no_slot;
  return 0;
}

auto CommanderTargeting::publish_targets(Engine::Core::Entity& commander,
                                         Engine::Core::EntityID aim_candidate_id,
                                         float dt) -> std::optional<float> {
  std::optional<float> impact_kick;
  auto* rpg_targets =
      Engine::Core::get_or_add_component<Engine::Core::RpgCommanderTargetComponent>(
          &commander);
  if (rpg_targets == nullptr) {
    return impact_kick;
  }
  rpg_targets->explicit_lock_target_id = m_locked_target_id;
  rpg_targets->explicit_lock_soldier_slot = m_locked_target_slot;
  rpg_targets->aim_candidate_id = aim_candidate_id;
  rpg_targets->aim_candidate_soldier_slot =
      aim_candidate_id != 0 ? m_primary_target_slot : k_no_slot;
  rpg_targets->aim_candidate_in_range = aim_candidate_id != 0;

  if (rpg_targets->hit_confirm_sequence != m_observed_hit_confirm_sequence) {
    m_observed_hit_confirm_sequence = rpg_targets->hit_confirm_sequence;
    impact_kick = rpg_targets->recent_hit_killed ? 1.0F : 0.7F;
  }
  rpg_targets->recent_hit_timer = std::max(0.0F, rpg_targets->recent_hit_timer - dt);
  if (rpg_targets->recent_hit_timer <= 0.0F) {
    rpg_targets->recent_hit_target_id = 0;
    rpg_targets->recent_hit_soldier_slot = k_no_slot;
  }
  return impact_kick;
}

} // namespace App::Core
