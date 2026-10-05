#include "movement_system_duel_footwork.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "core/component_commander.h"
#include "core/component_presentation.h"
#include "movement_system_collision.h"
#include "systems/duel_spacing.h"
#include "units/spawn_type.h"
#include "util/planar_math.h"

namespace Game::Systems {

namespace {

constexpr float k_duel_footwork_degrees_per_second = 18.0F;
constexpr float k_duel_footwork_period_seconds = 9.0F;
constexpr float k_duel_footwork_turn_degrees_per_second = 360.0F;
constexpr float k_duel_min_reach_sq = 0.04F;
constexpr float k_duel_measure_step_speed = 1.6F;
constexpr float k_duel_measure_gain_per_second = 4.5F;
constexpr float k_duel_measure_advance = 0.17F;
constexpr float k_duel_measure_retreat = -0.11F;
constexpr float k_duel_measure_breath = 0.05F;
constexpr float k_duel_measure_breath_period_seconds = 3.4F;

[[nodiscard]] auto duel_measure_target(const Engine::Core::Entity& entity,
                                       float clock,
                                       float breath_phase) -> float {
  auto const* action =
      entity.get_component<Engine::Core::RpgCommanderActionComponent>();
  if (action != nullptr && action->action_running && action->combat_action_id != 0U) {
    float const t = std::clamp(action->normalized_action_time, 0.0F, 1.0F);
    if (t < 0.12F) {
      return k_duel_measure_retreat * 0.4F;
    }
    if (t < 0.62F) {
      float const s = (t - 0.12F) / 0.50F;
      float const eased = s * s * (3.0F - 2.0F * s);
      return k_duel_measure_retreat * 0.4F +
             (k_duel_measure_advance - k_duel_measure_retreat * 0.4F) * eased;
    }
    float const s = std::clamp((t - 0.62F) / 0.33F, 0.0F, 1.0F);
    return k_duel_measure_advance +
           (k_duel_measure_retreat - k_duel_measure_advance) * s;
  }
  float const breath = std::sin(clock * 2.0F * std::numbers::pi_v<float> /
                                    k_duel_measure_breath_period_seconds +
                                breath_phase);
  return k_duel_measure_retreat * 0.5F + breath * k_duel_measure_breath;
}

[[nodiscard]] auto apply_commander_duel(
    Mover& mover,
    Engine::Core::AttackComponent& attack,
    Engine::Core::CommanderDuelComponent& duel,
    const Engine::Core::TransformComponent& opponent_transform) -> bool {
  Engine::Core::Entity* entity = &mover.entity;
  Engine::Core::TransformComponent& transform = mover.transform;
  float const delta_time = mover.delta_time;

  if (std::hypot(duel.slide_vx, duel.slide_vz) > 0.05F) {
    MovementCollision::slide_body_to(*entity,
                                     transform,
                                     transform.position.x + duel.slide_vx * delta_time,
                                     transform.position.z + duel.slide_vz * delta_time,
                                     true);
    float const drag =
        std::exp(-Engine::Core::CommanderDuelComponent::k_slide_drag * delta_time);
    duel.slide_vx *= drag;
    duel.slide_vz *= drag;
  } else {
    duel.slide_vx = 0.0F;
    duel.slide_vz = 0.0F;
  }

  auto const* routine = entity->get_component<Engine::Core::ShowcaseRoutineComponent>();
  bool const flipping =
      routine != nullptr && !routine->finished && !routine->steps.empty();
  bool const off_balance = flipping ||
                           entity->has_component<Engine::Core::StaggerComponent>() ||
                           entity->has_component<Engine::Core::CombatLaunchComponent>();
  attack.melee_footwork_offset = 0.0F;
  if (!off_balance) {
    float const rx = transform.position.x - opponent_transform.position.x;
    float const rz = transform.position.z - opponent_transform.position.z;
    if (std::abs(duel.orbit_degrees_per_second) > 0.01F) {
      float const angle = duel.orbit_degrees_per_second * delta_time *
                          std::numbers::pi_v<float> / 180.0F;
      float const cos_a = std::cos(angle);
      float const sin_a = std::sin(angle);
      MovementCollision::slide_body_to(
          *entity,
          transform,
          opponent_transform.position.x + (rx * cos_a) - (rz * sin_a),
          opponent_transform.position.z + (rx * sin_a) + (rz * cos_a),
          true);
    }
    float const to_x = opponent_transform.position.x - transform.position.x;
    float const to_z = opponent_transform.position.z - transform.position.z;
    float const separation = std::hypot(to_x, to_z);
    if (separation > 0.0001F && duel.approach_speed > 0.0F) {
      constexpr float k_duel_gain_per_second = 8.0F;
      float const error = separation - duel.desired_separation;
      float const max_step = duel.approach_speed * delta_time;
      float const step =
          std::clamp(error * k_duel_gain_per_second * delta_time, -max_step, max_step);
      auto const measure = MovementCollision::slide_body_to(
          *entity,
          transform,
          transform.position.x + (to_x / separation * step),
          transform.position.z + (to_z / separation * step),
          true);
      attack.melee_footwork_offset =
          std::copysign(std::hypot(measure.accepted_dx, measure.accepted_dz), step);
    }
  }

  if (!flipping) {
    float const face_x = opponent_transform.position.x - transform.position.x;
    float const face_z = opponent_transform.position.z - transform.position.z;
    if ((face_x * face_x) + (face_z * face_z) > k_duel_min_reach_sq) {
      transform.rotation.y = Game::Systems::turn_yaw_toward(
          transform.rotation.y,
          Game::Systems::yaw_degrees_from_direction(face_x, face_z),
          k_duel_footwork_turn_degrees_per_second * delta_time);
    }
  }
  transform.desired_yaw = transform.rotation.y;
  transform.has_desired_yaw = false;
  return true;
}

} // namespace

auto DuelFootwork::apply(Mover& mover,
                         Engine::Core::AttackComponent& attack) const -> bool {
  Engine::Core::World* world = &mover.world;
  Engine::Core::Entity* entity = &mover.entity;
  Engine::Core::TransformComponent& transform = mover.transform;
  float const delta_time = mover.delta_time;
  if (!DuelSpacing::is_duel_body(*entity)) {
    return false;
  }

  auto* opponent = world->get_entity(attack.melee_lock_target_id);
  if (opponent == nullptr || !DuelSpacing::is_duel_body(*opponent)) {
    return false;
  }

  auto const* opponent_attack =
      opponent->get_component<Engine::Core::AttackComponent>();
  if (opponent_attack == nullptr || !opponent_attack->in_melee_lock ||
      opponent_attack->melee_lock_target_id != entity->get_id()) {
    return false;
  }

  auto const* opponent_transform =
      opponent->get_component<Engine::Core::TransformComponent>();
  if (opponent_transform == nullptr) {
    return false;
  }

  float const rx = transform.position.x - opponent_transform->position.x;
  float const rz = transform.position.z - opponent_transform->position.z;
  if ((rx * rx) + (rz * rz) < k_duel_min_reach_sq) {
    return false;
  }

  auto* duel = entity->get_component<Engine::Core::CommanderDuelComponent>();
  if (duel != nullptr && duel->opponent_id != opponent->get_id()) {
    duel = nullptr;
  }
  if (duel != nullptr) {
    return apply_commander_duel(mover, attack, *duel, *opponent_transform);
  }

  auto const lhs = std::min(entity->get_id(), opponent->get_id());
  auto const rhs = std::max(entity->get_id(), opponent->get_id());
  float const direction = (((lhs + rhs) & 1U) == 0U) ? 1.0F : -1.0F;
  float const sway = std::sin(m_clock * 2.0F * std::numbers::pi_v<float> /
                              k_duel_footwork_period_seconds);
  float const angle = direction * sway * k_duel_footwork_degrees_per_second *
                      delta_time * std::numbers::pi_v<float> / 180.0F;

  float const cos_a = std::cos(angle);
  float const sin_a = std::sin(angle);

  MovementCollision::slide_body_to(
      *entity,
      transform,
      opponent_transform->position.x + (rx * cos_a) - (rz * sin_a),
      opponent_transform->position.z + (rx * sin_a) + (rz * cos_a),
      true);

  {
    float const breath_phase = static_cast<float>(entity->get_id() % 17U) / 17.0F *
                               2.0F * std::numbers::pi_v<float>;
    float const advance = duel_measure_target(*entity, m_clock, breath_phase);

    auto const standoff = DuelSpacing::standoff_between(*entity, *opponent);
    float const min_separation = standoff.minimum;
    float const desired_separation =
        std::max(min_separation, standoff.preferred - advance);
    float const to_x = opponent_transform->position.x - transform.position.x;
    float const to_z = opponent_transform->position.z - transform.position.z;
    float const separation = std::hypot(to_x, to_z);
    if (separation > 0.0001F) {
      float const error = separation - desired_separation;
      float const max_step = k_duel_measure_step_speed * delta_time;
      float step = std::clamp(
          error * k_duel_measure_gain_per_second * delta_time, -max_step, max_step);
      step = std::min(step, std::max(0.0F, separation - min_separation));
      auto const measure = MovementCollision::slide_body_to(
          *entity,
          transform,
          transform.position.x + (to_x / separation * step),
          transform.position.z + (to_z / separation * step),
          true);
      attack.melee_footwork_offset =
          std::copysign(std::hypot(measure.accepted_dx, measure.accepted_dz), step);
    }
  }

  float const face_x = opponent_transform->position.x - transform.position.x;
  float const face_z = opponent_transform->position.z - transform.position.z;
  float const target_yaw = Game::Systems::yaw_degrees_from_direction(face_x, face_z);
  auto const* footwork_unit = entity->get_component<Engine::Core::UnitComponent>();
  float const footwork_turn_speed =
      footwork_unit != nullptr
          ? std::min(k_duel_footwork_turn_degrees_per_second,
                     Game::Units::body_turn_speed_degrees(footwork_unit->spawn_type))
          : k_duel_footwork_turn_degrees_per_second;
  transform.rotation.y = Game::Systems::turn_yaw_toward(
      transform.rotation.y, target_yaw, footwork_turn_speed * delta_time);
  transform.desired_yaw = transform.rotation.y;
  transform.has_desired_yaw = false;
  return true;
}

} // namespace Game::Systems
