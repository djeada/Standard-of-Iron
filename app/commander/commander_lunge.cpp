#include "app/commander/commander_lunge.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>

#include "app/commander/commander_heading.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/systems/combat_actions/combat_action_definition.h"
#include "game/systems/duel_spacing.h"
#include "game/systems/rpg_combat_system/rpg_targeting.h"

namespace App::Core {

namespace {

constexpr float k_strike_body_overlap_radius = 0.16F;
constexpr float k_strike_recoil_share = 0.45F;
constexpr float k_strike_circle_share = 0.85F;

struct StrikeTarget {
  bool valid{false};
  float dir_x{0.0F};
  float dir_z{0.0F};
  float distance{0.0F};

  float stop_distance{0.0F};
};

auto resolve_strike_target(Engine::Core::World& world,
                           const Engine::Core::Entity* commander,
                           const Engine::Core::RpgCommanderActionComponent& action,
                           const Engine::Core::TransformComponent& transform)
    -> StrikeTarget {
  auto* target = world.get_entity(action.active_target_id);
  auto const* unit = target != nullptr
                         ? target->get_component<Engine::Core::UnitComponent>()
                         : nullptr;
  if (unit == nullptr || unit->health <= 0) {
    return {};
  }
  auto const sample = Game::Systems::RpgCombat::resolve_soldier_target(
      *target, action.active_target_soldier_slot);
  auto const* target_transform =
      target->get_component<Engine::Core::TransformComponent>();
  float target_x = 0.0F;
  float target_z = 0.0F;
  if (sample.has_value()) {
    target_x = sample->position.x();
    target_z = sample->position.z();
  } else if (target_transform != nullptr) {
    target_x = target_transform->position.x;
    target_z = target_transform->position.z;
  } else {
    return {};
  }
  float const to_x = target_x - transform.position.x;
  float const to_z = target_z - transform.position.z;
  float const distance = std::hypot(to_x, to_z);
  if (distance <= 1.0e-4F) {
    return {};
  }
  float stop_distance = CommanderMotor::body_radius() + k_strike_body_overlap_radius;
  if (commander != nullptr && Game::Systems::DuelSpacing::is_duel_body(*target)) {
    stop_distance = std::max(
        stop_distance,
        Game::Systems::DuelSpacing::standoff_between(*commander, *target).preferred);
  }
  return {.valid = true,
          .dir_x = to_x / distance,
          .dir_z = to_z / distance,
          .distance = distance,
          .stop_distance = stop_distance};
}

void steer_strike_toward(
    const Game::Systems::CombatActions::CombatActionDefinition& definition,
    const Engine::Core::RpgCommanderActionComponent& action,
    const StrikeTarget& target,
    Engine::Core::TransformComponent& transform,
    float dt) {
  if (!target.valid || definition.target_assist.maximum_turn_degrees <= 0.0F) {
    return;
  }
  float const target_yaw =
      std::atan2(target.dir_x, target.dir_z) * k_radians_to_degrees;
  float const yaw_delta = signed_angle_delta(target_yaw, transform.rotation.y);
  float const turn_rate = definition.target_assist.maximum_turn_degrees /
                          std::max(0.08F, action.action_duration * 0.45F);
  float const correction = std::clamp(yaw_delta, -turn_rate * dt, turn_rate * dt);
  transform.rotation.y = wrap_angle_degrees(transform.rotation.y + correction);
  transform.desired_yaw = transform.rotation.y;
  transform.has_desired_yaw = true;
}

struct StrikeCarryWindow {
  float start{0.0F};
  float end{0.0F};
  float recovery_start{0.0F};
  float exit_safe{0.0F};
};

auto strike_carry_window(const Game::Systems::CombatActions::CombatActionDefinition&
                             definition) -> StrikeCarryWindow {
  using Game::Systems::CombatActions::CombatActionEventType;
  auto const at = [&](CombatActionEventType type, float fallback) {
    return Game::Systems::CombatActions::action_event_normalized_time(
        definition, type, fallback);
  };
  bool const authored =
      definition.movement.end_normalized > definition.movement.start_normalized;
  float const start = authored ? definition.movement.start_normalized
                               : at(CombatActionEventType::WindupStart, 0.08F);
  float const end = authored ? definition.movement.end_normalized
                             : at(CombatActionEventType::WeaponTraceEnd, 0.60F);
  return {.start = start,
          .end = end,
          .recovery_start = at(CombatActionEventType::RecoveryStart, 0.75F),
          .exit_safe = at(CombatActionEventType::ExitSafe, 0.92F)};
}

auto span_fraction(float normalized, float start, float end) -> float {
  return std::clamp((normalized - start) / std::max(1.0e-4F, end - start), 0.0F, 1.0F);
}

auto strike_drive(float u) -> float {
  return u * (2.0F - u);
}

struct LungeProgress {
  float before{0.0F};
  float advanced{0.0F};
  float recoil_advanced{0.0F};
};

auto lunge_progress(const Engine::Core::RpgCommanderActionComponent& action,
                    const StrikeCarryWindow& window) -> LungeProgress {
  float const previous_time =
      std::min(action.previous_normalized_action_time, action.normalized_action_time);
  float const before =
      strike_drive(span_fraction(previous_time, window.start, window.end));
  float const advanced = strike_drive(span_fraction(
                             action.normalized_action_time, window.start, window.end)) -
                         before;
  float const recoil_advanced =
      window.exit_safe > window.recovery_start
          ? span_fraction(action.normalized_action_time,
                          window.recovery_start,
                          window.exit_safe) -
                span_fraction(previous_time, window.recovery_start, window.exit_safe)
          : 0.0F;
  return {.before = before, .advanced = advanced, .recoil_advanced = recoil_advanced};
}

struct LungeMover {
  CommanderMotor& motor;
  Engine::Core::Entity& commander;
  Engine::Core::TransformComponent& transform;
  bool airborne;
  float dt;

  auto operator()(float move_x, float move_z) const -> float {
    QVector3D const from(
        transform.position.x, transform.position.y, transform.position.z);
    static_cast<void>(motor.advance(commander,
                                    transform,
                                    {.from = from,
                                     .to = from + QVector3D(move_x, 0.0F, move_z),
                                     .source = CommanderDisplacementSource::StrikeLunge,
                                     .airborne = airborne,
                                     .dt = dt}));
    return std::hypot(transform.position.x - from.x(), transform.position.z - from.z());
  }
};

struct LungeStep {
  float authored_distance{0.0F};
  float forward_sign{1.0F};
  float dir_x{0.0F};
  float dir_z{0.0F};
  LungeProgress progress;
};

void recoil_lunge(const StrikeCarry& carry,
                  const LungeMover& move_body,
                  const LungeStep& step) {
  float const blocked = std::max(0.0F, carry.requested - carry.delivered);
  float const recoil_step =
      std::min(blocked, std::abs(step.authored_distance) * k_strike_recoil_share) *
      step.progress.recoil_advanced;
  if (recoil_step > 1.0e-5F) {
    static_cast<void>(move_body(-step.dir_x * recoil_step, -step.dir_z * recoil_step));
  }
}

void drive_lunge(StrikeCarry& carry,
                 const LungeMover& move_body,
                 const Engine::Core::Entity& commander,
                 const StrikeTarget& target,
                 const LungeStep& lunge_step) {
  float dir_x = lunge_step.dir_x;
  float dir_z = lunge_step.dir_z;
  float const advanced = lunge_step.progress.advanced;
  float step = std::abs(lunge_step.authored_distance) * advanced;
  float lateral_step = 0.0F;
  if (target.valid && lunge_step.forward_sign > 0.0F) {
    dir_x = target.dir_x;
    dir_z = target.dir_z;
    float const remaining_authored =
        std::abs(lunge_step.authored_distance) * (1.0F - lunge_step.progress.before);
    float const gap = target.distance - target.stop_distance;
    if (gap > remaining_authored) {
      step += (gap - remaining_authored) * advanced;
    }
    carry.requested += step;
    float const allowed = std::min(step, std::max(0.0F, gap));
    lateral_step = (step - allowed) * k_strike_circle_share;
    step = allowed;
  } else {
    carry.requested += step;
  }
  if (step <= 1.0e-5F && lateral_step <= 1.0e-5F) {
    return;
  }

  float lateral_sign = 1.0F;
  if (auto const* combat =
          commander.get_component<Engine::Core::CombatStateComponent>()) {
    lateral_sign = combat->intent.strike_dir_x >= 0.0F ? 1.0F : -1.0F;
  }
  carry.delivered += move_body(dir_x * step + dir_z * lateral_step * lateral_sign,
                               dir_z * step - dir_x * lateral_step * lateral_sign);
}

} // namespace

auto CommanderLunge::apply(Engine::Core::World& world,
                           Engine::Core::Entity& commander,
                           Engine::Core::TransformComponent& transform,
                           CommanderMotor& motor,
                           bool dodging,
                           bool airborne,
                           float dt) -> float {
  if (dt <= 0.0F || dodging) {
    return 0.0F;
  }
  float const before_x = transform.position.x;
  float const before_z = transform.position.z;
  advance(world, commander, transform, motor, airborne, dt);
  return std::hypot(transform.position.x - before_x, transform.position.z - before_z);
}

void CommanderLunge::advance(Engine::Core::World& world,
                             Engine::Core::Entity& commander,
                             Engine::Core::TransformComponent& transform,
                             CommanderMotor& motor,
                             bool airborne,
                             float dt) {
  auto const* action =
      commander.get_component<Engine::Core::RpgCommanderActionComponent>();
  if (action == nullptr || !action->action_running) {
    return;
  }
  auto const* definition = Game::Systems::CombatActions::find_combat_action_definition(
      static_cast<Game::Systems::CombatActions::CombatActionId>(
          action->combat_action_id));
  if (definition == nullptr) {
    return;
  }

  auto const target = resolve_strike_target(world, &commander, *action, transform);
  steer_strike_toward(*definition, *action, target, transform, dt);

  float const authored_distance = definition->movement.distance;
  if (std::abs(authored_distance) <= 0.01F) {
    return;
  }
  auto const window = strike_carry_window(*definition);
  if (window.end <= window.start) {
    return;
  }

  auto const progress = lunge_progress(*action, window);
  if (progress.advanced <= 0.0F && progress.recoil_advanced <= 0.0F) {
    return;
  }

  if (m_carry.sequence != action->melee_attack_sequence) {
    m_carry = {.sequence = action->melee_attack_sequence};
  }

  LungeMover const move_body{motor, commander, transform, airborne, dt};
  float const forward_sign = authored_distance >= 0.0F ? 1.0F : -1.0F;
  float const yaw = transform.rotation.y * k_degrees_to_radians;
  LungeStep const step{.authored_distance = authored_distance,
                       .forward_sign = forward_sign,
                       .dir_x = std::sin(yaw) * forward_sign,
                       .dir_z = std::cos(yaw) * forward_sign,
                       .progress = progress};

  if (progress.advanced <= 0.0F) {
    recoil_lunge(m_carry, move_body, step);
    return;
  }
  drive_lunge(m_carry, move_body, commander, target, step);
}

} // namespace App::Core
