#include "mounted_charge_processor.h"

#include <QtMath>

#include <algorithm>
#include <cmath>

#include "../../core/component.h"
#include "../../core/entity.h"
#include "../../core/world.h"
#include "../../units/spawn_type.h"
#include "../combat_actions/body_impact.h"
#include "../combat_actions/combat_action_events.h"

namespace Game::Systems::Combat {

namespace {

using ActionId = Game::Systems::CombatActions::CombatActionId;
using EventType = Game::Systems::CombatActions::CombatActionEventType;

[[nodiscard]] auto
movement_speed(const Engine::Core::MovementComponent& movement) -> float {
  return std::hypot(movement.get_vx(), movement.get_vz());
}

[[nodiscard]] auto
is_charge_action(const Engine::Core::RpgCommanderActionComponent* action) -> bool {
  return action != nullptr &&
         action->combat_action_id ==
             static_cast<std::uint8_t>(ActionId::MountedChargeImpact);
}

[[nodiscard]] auto
is_knockdown_interrupted(const Engine::Core::Entity& entity) -> bool {
  auto const* stagger = entity.get_component<Engine::Core::StaggerComponent>();
  return stagger != nullptr && stagger->tier == Engine::Core::StaggerTier::Knockdown &&
         stagger->remaining > 0.0F;
}

[[nodiscard]] auto should_ai_request_charge(Engine::Core::World& world,
                                            Engine::Core::Entity& entity,
                                            float speed) -> bool {
  auto const* commander = entity.get_component<Engine::Core::CommanderComponent>();
  if (commander != nullptr && commander->fpv_controlled) {
    return false;
  }
  auto const* attack_target =
      entity.get_component<Engine::Core::AttackTargetComponent>();
  auto const* transform = entity.get_component<Engine::Core::TransformComponent>();
  auto* target =
      attack_target != nullptr ? world.get_entity(attack_target->target_id) : nullptr;
  auto const* target_transform =
      target != nullptr ? target->get_component<Engine::Core::TransformComponent>()
                        : nullptr;
  if (transform == nullptr || target_transform == nullptr || speed <= 0.0F) {
    return false;
  }
  float const dx = target_transform->position.x - transform->position.x;
  float const dz = target_transform->position.z - transform->position.z;
  float const distance_sq = dx * dx + dz * dz;
  if (distance_sq < 4.0F || distance_sq > 64.0F) {
    return false;
  }
  float const distance = std::sqrt(distance_sq);
  float const yaw = qDegreesToRadians(transform->rotation.y);
  float const facing = (std::sin(yaw) * dx + std::cos(yaw) * dz) / distance;
  return facing >= 0.70F;
}

void enter_cooldown(Engine::Core::MountedChargeComponent& charge) {
  charge.state = Engine::Core::MountedChargeState::Cooldown;
  charge.cooldown_remaining =
      std::max(charge.cooldown_remaining, charge.cooldown_duration);
  charge.intent_requested = false;
  charge.intent_source = Engine::Core::MountedChargeIntentSource::None;
  charge.below_cancel_speed_time = 0.0F;
  charge.active_target_id = 0;
}

void clear_charge_action(Engine::Core::Entity& entity) {
  if (auto* action = entity.get_component<Engine::Core::RpgCommanderActionComponent>();
      is_charge_action(action)) {
    Game::Systems::CombatActions::reset_combat_action_event_runtime(*action);
    action->combat_action_id = static_cast<std::uint8_t>(ActionId::None);
    action->phase = Engine::Core::RpgCommanderActionPhase::None;
    action->active_target_id = 0;
    action->action_running = false;
    action->action_completed = true;
  }
  if (auto* combat = entity.get_component<Engine::Core::CombatStateComponent>()) {
    combat->animation_state = Engine::Core::CombatAnimationState::Idle;
    combat->state_time = 0.0F;
    combat->state_duration = 0.0F;
    combat->damage_dealt_this_swing = false;
  }
}

[[nodiscard]] auto start_charge_action(
    Engine::Core::Entity& entity,
    Engine::Core::MountedChargeComponent& charge,
    const Game::Systems::CombatActions::CombatActionDefinition& definition,
    Engine::Core::EntityID target_hint,
    float impact_speed) -> bool {
  auto* combat =
      Engine::Core::get_or_add_component<Engine::Core::CombatStateComponent>(&entity);
  auto* action =
      Engine::Core::get_or_add_component<Engine::Core::RpgCommanderActionComponent>(
          &entity);
  if (combat == nullptr || action == nullptr) {
    return false;
  }

  combat->animation_state = Engine::Core::CombatAnimationState::Strike;
  combat->attack_family = Engine::Core::CombatAttackFamily::None;
  combat->attack_direction = Engine::Core::AttackDirection::Thrust;
  combat->state_time = 0.0F;
  combat->state_duration = Engine::Core::CombatStateComponent::k_strike_duration;
  combat->damage_dealt_this_swing = false;
  combat->input_buffered = false;

  action->phase = Engine::Core::RpgCommanderActionPhase::Strike;
  action->combat_action_id = static_cast<std::uint8_t>(definition.id);
  action->active_target_id = target_hint;
  action->action_duration = definition.duration_seconds;
  Game::Systems::CombatActions::reset_combat_action_event_runtime(*action);

  charge.state = Engine::Core::MountedChargeState::Charging;
  charge.intent_requested = false;
  charge.below_cancel_speed_time = 0.0F;
  charge.cooldown_remaining = 0.0F;
  charge.impact_speed = impact_speed;
  charge.active_target_id = target_hint;
  charge.last_cancel_reason = Engine::Core::MountedChargeCancelReason::None;
  return true;
}

} // namespace

auto request_mounted_charge(Engine::Core::Entity& entity,
                            Engine::Core::MountedChargeIntentSource source) -> bool {
  auto const* unit = entity.get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr || unit->health <= 0 ||
      !Game::Units::is_cavalry(unit->spawn_type) ||
      unit->spawn_type == Game::Units::SpawnType::HorseArcher ||
      source == Engine::Core::MountedChargeIntentSource::None) {
    return false;
  }
  auto* charge =
      Engine::Core::get_or_add_component<Engine::Core::MountedChargeComponent>(&entity);
  if (charge == nullptr || charge->state != Engine::Core::MountedChargeState::Ready ||
      charge->cooldown_remaining > 0.0F) {
    return false;
  }
  charge->intent_requested = true;
  charge->intent_source = source;
  charge->last_cancel_reason = Engine::Core::MountedChargeCancelReason::None;
  return true;
}

auto cancel_mounted_charge(Engine::Core::Entity& entity,
                           Engine::Core::MountedChargeCancelReason reason) -> bool {
  auto* charge = entity.get_component<Engine::Core::MountedChargeComponent>();
  if (charge == nullptr || (charge->state == Engine::Core::MountedChargeState::Ready &&
                            !charge->intent_requested)) {
    return false;
  }
  clear_charge_action(entity);
  charge->last_cancel_reason = reason;
  enter_cooldown(*charge);
  return true;
}

void process_mounted_charge_intents(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }
  auto const* definition = Game::Systems::CombatActions::find_combat_action_definition(
      ActionId::MountedChargeImpact);
  if (definition == nullptr) {
    return;
  }

  for (auto* entity : world->get_entities_with<Engine::Core::MovementComponent>()) {
    if (entity == nullptr ||
        entity->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }
    auto const* unit = entity->get_component<Engine::Core::UnitComponent>();
    auto* movement = entity->get_component<Engine::Core::MovementComponent>();
    if (unit == nullptr || movement == nullptr ||
        !Game::Units::is_cavalry(unit->spawn_type) ||
        unit->spawn_type == Game::Units::SpawnType::HorseArcher) {
      continue;
    }

    auto* charge = entity->get_component<Engine::Core::MountedChargeComponent>();
    if (charge == nullptr) {
      charge = entity->add_component<Engine::Core::MountedChargeComponent>();
    }
    if (charge == nullptr) {
      continue;
    }

    if (unit->health <= 0) {
      (void)cancel_mounted_charge(*entity,
                                  Engine::Core::MountedChargeCancelReason::InvalidUnit);
      continue;
    }
    if (charge->state == Engine::Core::MountedChargeState::Cooldown) {
      charge->cooldown_remaining =
          std::max(0.0F, charge->cooldown_remaining - delta_time);
      if (charge->cooldown_remaining <= 0.0F) {
        charge->state = Engine::Core::MountedChargeState::Ready;
        charge->last_cancel_reason = Engine::Core::MountedChargeCancelReason::None;
      }
      continue;
    }

    float const speed = movement_speed(*movement);
    if (charge->state == Engine::Core::MountedChargeState::Charging ||
        charge->state == Engine::Core::MountedChargeState::ImpactActive) {
      if (is_knockdown_interrupted(*entity)) {
        (void)cancel_mounted_charge(
            *entity, Engine::Core::MountedChargeCancelReason::Interrupted);
        continue;
      }
      auto const impact_contact =
          Game::Systems::CombatActions::find_body_impact_contact(
              *world, *entity, *definition, charge->active_target_id);
      bool const has_reached_impact_contact = impact_contact.target_id != 0U;
      charge->below_cancel_speed_time =
          speed < charge->cancel_speed && !has_reached_impact_contact
              ? charge->below_cancel_speed_time + std::max(0.0F, delta_time)
              : 0.0F;
      if (charge->below_cancel_speed_time >= charge->speed_loss_grace) {
        (void)cancel_mounted_charge(*entity,
                                    Engine::Core::MountedChargeCancelReason::SpeedLost);
        continue;
      }
      if (!is_charge_action(
              entity->get_component<Engine::Core::RpgCommanderActionComponent>())) {
        enter_cooldown(*charge);
      }
      continue;
    }

    auto contact = Game::Systems::CombatActions::find_body_impact_contact(
        *world, *entity, *definition);
    if (!charge->intent_requested && should_ai_request_charge(*world, *entity, speed)) {
      (void)request_mounted_charge(*entity,
                                   Engine::Core::MountedChargeIntentSource::AI);
    }
    if (!charge->intent_requested && charge->auto_contact_enabled &&
        contact.target_id != 0) {
      (void)request_mounted_charge(
          *entity, Engine::Core::MountedChargeIntentSource::ContactAuto);
    }
    if (charge->intent_requested && contact.target_id == 0U &&
        speed < charge->cancel_speed) {
      charge->below_cancel_speed_time += std::max(0.0F, delta_time);
      if (charge->below_cancel_speed_time >= charge->speed_loss_grace) {
        (void)cancel_mounted_charge(*entity,
                                    Engine::Core::MountedChargeCancelReason::SpeedLost);
      }
      continue;
    }
    if (speed >= charge->cancel_speed) {
      charge->below_cancel_speed_time = 0.0F;
    }

    if (!charge->intent_requested || speed < charge->min_start_speed ||
        contact.target_id == 0U) {
      continue;
    }
    (void)start_charge_action(*entity, *charge, *definition, contact.target_id, speed);
  }
}

void handle_mounted_charge_action_events(
    Engine::Core::Entity& entity,
    const Engine::Core::RpgCommanderActionComponent& action,
    const Game::Systems::CombatActions::CombatActionDefinition& definition,
    std::span<const Game::Systems::CombatActions::CombatActionEvent> events) {
  if (!is_charge_action(&action) || definition.id != ActionId::MountedChargeImpact) {
    return;
  }
  auto* charge = entity.get_component<Engine::Core::MountedChargeComponent>();
  if (charge == nullptr) {
    return;
  }
  for (auto const& event : events) {
    if (event.type == EventType::ActiveStart) {
      charge->state = Engine::Core::MountedChargeState::ImpactActive;
    } else if (event.type == EventType::RecoveryStart ||
               event.type == EventType::ExitSafe) {
      enter_cooldown(*charge);
      return;
    }
  }
}

} // namespace Game::Systems::Combat
