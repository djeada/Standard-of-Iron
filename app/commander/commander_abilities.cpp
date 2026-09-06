#include "app/commander/commander_abilities.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>

#include "app/commander/commander_motor.h"
#include "game/audio/audio_cues.h"
#include "game/core/component_gameplay.h"
#include "game/core/world.h"
#include "game/session/session_context.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/building_line_of_sight.h"
#include "game/systems/combat_system/mounted_charge_processor.h"
#include "game/systems/combat_system/target_rules.h"
#include "game/systems/owner_registry.h"
#include "game/systems/rpg_combat_system/rpg_commander_damage.h"
#include "game/units/spawn_type.h"

namespace App::Core {
namespace {

constexpr float k_degrees_to_radians = 0.017453292519943295F;

constexpr float k_bash_range = 2.5F;
constexpr float k_bash_stagger_duration = 0.5F;
constexpr float k_bash_cooldown = 3.0F;
constexpr float k_bash_punish_window = 0.75F;

constexpr float k_rush_cooldown = 4.5F;
constexpr float k_rush_max_range = 8.0F;
constexpr float k_rush_stop_distance = 1.35F;
constexpr float k_rush_default_distance = 3.6F;
constexpr int k_rush_damage = 18;
constexpr float k_rush_stagger_duration = 0.35F;
constexpr float k_rush_contact_range = 2.35F;
constexpr float k_rush_punish_window = 0.85F;
constexpr float k_rush_launch_speed = 8.0F;

constexpr float k_second_wind_cooldown = 8.0F;
constexpr float k_second_wind_posture_restore = 55.0F;
constexpr float k_second_wind_stamina_restore = 35.0F;
constexpr float k_second_wind_guard_window = 0.35F;

auto buildings_of(const Engine::Core::World& world)
    -> const Game::Systems::BuildingCollisionRegistry& {
  return Game::Session::session_for(world).building_collision();
}

void apply_stagger(Engine::Core::Entity& entity, float duration) {
  if (auto* existing = entity.get_component<Engine::Core::StaggerComponent>()) {
    existing->remaining = std::max(existing->remaining, duration);
    return;
  }
  entity.add_component<Engine::Core::StaggerComponent>(duration);
}

void open_punish_window(Engine::Core::Entity& entity, float duration) {
  if (auto* commander = entity.get_component<Engine::Core::CommanderComponent>()) {
    commander->punish_window_remaining =
        std::max(commander->punish_window_remaining, duration);
  }
}

auto is_idle_for_ability(const Engine::Core::Entity& commander) -> bool {
  auto const* combat_state =
      commander.get_component<Engine::Core::CombatStateComponent>();
  return combat_state == nullptr ||
         combat_state->animation_state == Engine::Core::CombatAnimationState::Idle;
}

void refuse() {
  Game::Audio::play_cue(Game::Audio::Cue::k_combat_ability_refused);
}

} // namespace

void CommanderAbilities::reset() {
  m_shield_bash_cooldown = 0.0F;
  m_vanguard_rush_cooldown = 0.0F;
  m_second_wind_cooldown = 0.0F;
}

void CommanderAbilities::advance_cooldowns(Engine::Core::CommanderComponent* commander,
                                           float dt) {
  auto decay = [dt](float& cooldown) {
    if (cooldown > 0.0F) {
      cooldown = std::max(0.0F, cooldown - dt);
    }
  };
  decay(m_shield_bash_cooldown);
  decay(m_vanguard_rush_cooldown);
  decay(m_second_wind_cooldown);

  if (commander != nullptr) {
    commander->shield_bash_cooldown_remaining = m_shield_bash_cooldown;
    commander->vanguard_rush_cooldown_remaining = m_vanguard_rush_cooldown;
    commander->second_wind_cooldown_remaining = m_second_wind_cooldown;
  }
}

auto CommanderAbilities::activate(const CommanderAbilityRequest& request,
                                  const CommanderAbilityContext& context)
    -> CommanderAbilityOutcome {
  CommanderAbilityOutcome outcome;
  if (context.world == nullptr || context.commander == nullptr) {
    return outcome;
  }

  if (request.shield_bash) {
    static_cast<void>(try_shield_bash(context));
  }
  if (request.vanguard_rush) {
    outcome.rescan_primary_target = try_vanguard_rush(context);
  }
  if (request.second_wind) {
    static_cast<void>(try_second_wind(context));
  }
  return outcome;
}

auto CommanderAbilities::resolve_target(const CommanderAbilityContext& context,
                                        float max_range) const
    -> Engine::Core::EntityID {
  auto& world = *context.world;
  auto* transform =
      context.commander->get_component<Engine::Core::TransformComponent>();
  if (transform == nullptr) {
    return 0;
  }

  const QVector3D origin(transform->position.x, 0.0F, transform->position.z);
  const float max_range_sq = max_range * max_range;
  auto& owners = Game::Session::session_for(world).owners();

  auto qualifies = [&](Engine::Core::EntityID candidate_id) -> bool {
    auto* candidate = world.get_entity(candidate_id);
    auto* candidate_unit = (candidate != nullptr)
                               ? candidate->get_component<Engine::Core::UnitComponent>()
                               : nullptr;
    auto* candidate_transform =
        (candidate != nullptr)
            ? candidate->get_component<Engine::Core::TransformComponent>()
            : nullptr;
    if (candidate_unit == nullptr || candidate_transform == nullptr ||
        Game::Systems::Combat::evaluate_target(
            owners,
            context.local_owner_id,
            candidate,
            {.intent = Game::Systems::Combat::EngagementIntent::AutoAcquired,
             .allow_buildings = true}) != Game::Systems::Combat::TargetRefusal::None) {
      return false;
    }

    const QVector3D target(
        candidate_transform->position.x, 0.0F, candidate_transform->position.z);
    return (target - origin).lengthSquared() <= max_range_sq &&
           Game::Systems::has_clear_building_los(buildings_of(world), origin, target);
  };

  if (context.locked_target_id != 0 && qualifies(context.locked_target_id)) {
    return context.locked_target_id;
  }
  if (context.soft_target_id != 0 && qualifies(context.soft_target_id)) {
    return context.soft_target_id;
  }
  return 0;
}

auto CommanderAbilities::try_shield_bash(const CommanderAbilityContext& context)
    -> bool {
  auto& world = *context.world;
  auto& commander = *context.commander;

  auto* guard = commander.get_component<Engine::Core::CommanderGuardComponent>();
  auto* cmd_comp = commander.get_component<Engine::Core::CommanderComponent>();
  if (guard == nullptr || !guard->active || m_shield_bash_cooldown > 0.0F ||
      context.airborne) {
    refuse();
    return false;
  }

  auto* transform = commander.get_component<Engine::Core::TransformComponent>();
  if (transform == nullptr) {
    return false;
  }

  auto& owners = Game::Session::session_for(world).owners();
  const QVector3D cmd_pos(
      transform->position.x, transform->position.y, transform->position.z);
  for (auto* entity : world.collect_entities_with<Engine::Core::UnitComponent>()) {
    if (entity == nullptr || entity->get_id() == context.commander_id) {
      continue;
    }
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    auto* ent_tf = entity->get_component<Engine::Core::TransformComponent>();
    if (unit == nullptr || ent_tf == nullptr ||
        Game::Systems::Combat::evaluate_target(
            owners,
            context.local_owner_id,
            entity,
            {.intent = Game::Systems::Combat::EngagementIntent::AutoAcquired,
             .allow_buildings = true}) != Game::Systems::Combat::TargetRefusal::None) {
      continue;
    }
    const QVector3D epos(ent_tf->position.x, ent_tf->position.y, ent_tf->position.z);
    if ((epos - cmd_pos).length() > k_bash_range) {
      continue;
    }
    apply_stagger(*entity, k_bash_stagger_duration);
    open_punish_window(*entity, k_bash_punish_window);
  }

  m_shield_bash_cooldown = k_bash_cooldown;
  Game::Audio::play_cue(Game::Audio::Cue::k_combat_shield_bash);
  if (cmd_comp != nullptr) {
    cmd_comp->shield_bash_cooldown_remaining = m_shield_bash_cooldown;
  }
  return true;
}

auto CommanderAbilities::try_vanguard_rush(const CommanderAbilityContext& context)
    -> bool {
  auto& world = *context.world;
  auto& commander = *context.commander;

  if (m_vanguard_rush_cooldown > 0.0F || context.dodging || context.airborne) {
    refuse();
    return false;
  }

  auto* transform = commander.get_component<Engine::Core::TransformComponent>();
  auto* unit = commander.get_component<Engine::Core::UnitComponent>();
  auto* movement = commander.get_component<Engine::Core::MovementComponent>();
  auto* cmd_comp = commander.get_component<Engine::Core::CommanderComponent>();
  if (transform == nullptr || unit == nullptr || !is_idle_for_ability(commander)) {
    return false;
  }

  const QVector3D start(
      transform->position.x, transform->position.y, transform->position.z);
  const float yaw_rad = context.view_yaw * k_degrees_to_radians;
  QVector3D rush_direction(std::sin(yaw_rad), 0.0F, std::cos(yaw_rad));
  float rush_distance = k_rush_default_distance;

  if (Game::Units::is_cavalry(unit->spawn_type) && movement != nullptr) {
    movement->set_manual_velocity(rush_direction.x() * k_rush_launch_speed,
                                  rush_direction.z() * k_rush_launch_speed);
    if (!Game::Systems::Combat::request_mounted_charge(
            commander, Engine::Core::MountedChargeIntentSource::Player)) {
      return false;
    }
    m_vanguard_rush_cooldown = k_rush_cooldown;
    Game::Audio::play_cue(Game::Audio::Cue::k_combat_vanguard_rush);
    if (cmd_comp != nullptr) {
      cmd_comp->vanguard_rush_cooldown_remaining = m_vanguard_rush_cooldown;
    }
    return true;
  }

  Engine::Core::Entity* target = nullptr;
  const auto target_id = resolve_target(context, k_rush_max_range);
  if (target_id != 0) {
    target = world.get_entity(target_id);
    auto* target_transform =
        (target != nullptr) ? target->get_component<Engine::Core::TransformComponent>()
                            : nullptr;
    if (target_transform != nullptr) {
      QVector3D const to_target(target_transform->position.x - start.x(),
                                0.0F,
                                target_transform->position.z - start.z());
      if (to_target.lengthSquared() > 0.0001F) {
        const float target_distance = std::sqrt(to_target.lengthSquared());
        rush_direction = to_target / target_distance;
        rush_distance = std::clamp(target_distance - k_rush_stop_distance,
                                   1.4F,
                                   k_rush_default_distance + 0.4F);
      }
    }
  }

  const QVector3D desired = start + rush_direction * rush_distance;
  const QVector3D resolved = CommanderMotor::reachable_ground_position(
      Game::Session::session_for(world), start, desired, context.commander_id);
  if (context.motor != nullptr) {
    static_cast<void>(context.motor->teleport(
        *transform, resolved, CommanderDisplacementSource::StrikeLunge));
  }
  if (movement != nullptr) {
    movement->set_manual_velocity(rush_direction.x() * k_rush_launch_speed,
                                  rush_direction.z() * k_rush_launch_speed);
  }

  if (target != nullptr) {
    auto* target_unit = target->get_component<Engine::Core::UnitComponent>();
    auto* target_transform = target->get_component<Engine::Core::TransformComponent>();
    if (target_unit != nullptr && target_transform != nullptr &&
        target_unit->health > 0) {
      const QVector3D target_pos(target_transform->position.x,
                                 target_transform->position.y,
                                 target_transform->position.z);
      if ((target_pos - resolved).length() <= k_rush_contact_range &&
          Game::Systems::has_clear_building_los(
              buildings_of(world), resolved, target_pos)) {
        Game::Systems::RpgCombat::deal_commander_attack_damage(
            &world, target, k_rush_damage, context.commander_id);
        if (target_unit->health > 0) {
          apply_stagger(*target, k_rush_stagger_duration);
          open_punish_window(*target, k_rush_punish_window);
        }
      }
    }
  }

  m_vanguard_rush_cooldown = k_rush_cooldown;
  Game::Audio::play_cue(Game::Audio::Cue::k_combat_vanguard_rush);
  if (cmd_comp != nullptr) {
    cmd_comp->vanguard_rush_cooldown_remaining = m_vanguard_rush_cooldown;
  }
  return true;
}

auto CommanderAbilities::try_second_wind(const CommanderAbilityContext& context)
    -> bool {
  auto& commander = *context.commander;

  if (m_second_wind_cooldown > 0.0F || context.dodging || context.airborne) {
    refuse();
    return false;
  }

  auto* cmd_comp = commander.get_component<Engine::Core::CommanderComponent>();
  if (cmd_comp == nullptr || !is_idle_for_ability(commander)) {
    return false;
  }

  cmd_comp->posture = std::max(0.0F, cmd_comp->posture - k_second_wind_posture_restore);
  if (auto* stamina = commander.get_component<Engine::Core::StaminaComponent>()) {
    stamina->stamina = std::min(stamina->max_stamina,
                                stamina->stamina + k_second_wind_stamina_restore);
  }
  auto* guard = commander.get_component<Engine::Core::CommanderGuardComponent>();
  if (guard == nullptr) {
    guard = commander.add_component<Engine::Core::CommanderGuardComponent>();
  }
  if (guard != nullptr && guard->guard_break_remaining <= 0.0F) {
    guard->perfect_guard_remaining =
        std::max(guard->perfect_guard_remaining, k_second_wind_guard_window);
  }

  m_second_wind_cooldown = k_second_wind_cooldown;
  Game::Audio::play_cue(Game::Audio::Cue::k_combat_second_wind);
  cmd_comp->second_wind_cooldown_remaining = m_second_wind_cooldown;
  return true;
}

} // namespace App::Core
