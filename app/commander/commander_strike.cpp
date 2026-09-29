#include "app/commander/commander_strike.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>

#include "app/commander/commander_entity_access.h"
#include "app/commander/commander_heading.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/systems/combat_actions/combat_action_definition.h"
#include "game/systems/combat_actions/combat_action_service.h"
#include "game/systems/combat_actions/melee_intent_solver.h"
#include "game/systems/combat_system/damage_application.h"
#include "game/systems/combat_system/damage_processor.h"
#include "game/systems/duel_spacing.h"
#include "game/systems/rpg_combat_system/rpg_targeting.h"

namespace App::Core {

namespace {

constexpr float k_held_restart_interval = 0.12F;
constexpr float k_combo_reset_window = 1.0F;
constexpr float k_strike_acquisition_bonus = 0.55F;
constexpr float k_heavy_acquisition_bonus = 3.0F;
constexpr float k_special_acquisition_bonus = 1.2F;

auto pending_acquisition_bonus(const Engine::Core::CombatActionIntent* pending)
    -> float {
  if (pending != nullptr &&
      pending->type == Engine::Core::CommanderCombatIntentType::Heavy) {
    return k_heavy_acquisition_bonus;
  }
  if (pending != nullptr &&
      pending->type == Engine::Core::CommanderCombatIntentType::Special) {
    return k_special_acquisition_bonus;
  }
  return k_strike_acquisition_bonus;
}

auto pressed_intent_type(const CommanderInputSnapshot& tick, bool jump_followup)
    -> Engine::Core::CommanderCombatIntentType {
  if (jump_followup) {
    return Engine::Core::CommanderCombatIntentType::Jump;
  }
  if (tick.special_pressed) {
    return Engine::Core::CommanderCombatIntentType::Special;
  }
  return tick.heavy_pressed ? Engine::Core::CommanderCombatIntentType::Heavy
                            : Engine::Core::CommanderCombatIntentType::Light;
}

auto hit_role_boost(const Engine::Core::RpgCommanderActionComponent& struck) -> float {
  auto const* definition = Game::Systems::CombatActions::find_combat_action_definition(
      static_cast<Game::Systems::CombatActions::CombatActionId>(
          struck.combat_action_id));
  if (definition == nullptr) {
    return 1.0F;
  }
  using Game::Systems::CombatActions::CommanderActionRole;
  if (definition->role == CommanderActionRole::Dive ||
      definition->role == CommanderActionRole::Finisher) {
    return 1.55F;
  }
  if (definition->role == CommanderActionRole::Launcher ||
      definition->role == CommanderActionRole::Special) {
    return 1.3F;
  }
  return 1.0F;
}

} // namespace

void CommanderStrike::reset() {
  m_observed_action_hit_count = 0;
  m_combo_miss_timer = 0.0F;
  m_primary_held_duration = 0.0F;
}

void CommanderStrike::advance_hold_timers(
    Engine::Core::CommanderComponent* commander_data,
    bool primary_held,
    bool attack_animation_active,
    float dt) {
  if (primary_held) {
    m_combo_miss_timer = 0.0F;
    m_primary_held_duration += dt;
  } else if (attack_animation_active) {
    m_primary_held_duration = 0.0F;
  } else {
    m_primary_held_duration = 0.0F;
    m_combo_miss_timer += dt;
    if (m_combo_miss_timer >= k_combo_reset_window && commander_data != nullptr) {
      commander_data->combo_step = 0;
      m_combo_miss_timer = 0.0F;
    }
  }

  if (primary_held) {
    m_held_restart_delay = std::max(0.0F, m_held_restart_delay - dt);
  } else {
    m_held_restart_delay = 0.0F;
  }
}

auto CommanderStrike::open_queue(Engine::Core::Entity& commander, float dt)
    -> Engine::Core::CombatIntentQueueComponent* {
  auto* intents =
      Engine::Core::get_or_add_component<Engine::Core::CombatIntentQueueComponent>(
          &commander);
  if (intents != nullptr) {
    Game::Systems::CombatActions::expire_stale_intents(*intents, dt);
  }
  return intents;
}

void CommanderStrike::queue_held_continuation(
    Engine::Core::Entity& commander,
    Engine::Core::CombatIntentQueueComponent& intents,
    const CommanderInputSnapshot& tick,
    const IntentGate& gate) {
  auto const* held_action =
      commander.get_component<Engine::Core::RpgCommanderActionComponent>();
  auto const* held_definition =
      held_action != nullptr && held_action->action_running
          ? Game::Systems::CombatActions::find_combat_action_definition(
                static_cast<Game::Systems::CombatActions::CombatActionId>(
                    held_action->combat_action_id))
          : nullptr;
  bool const held_melee_combo =
      tick.primary_held && !tick.primary_pressed && intents.empty() &&
      held_definition != nullptr &&
      (held_definition->weapon_family ==
           Game::Systems::CombatActions::WeaponFamily::Sword ||
       held_definition->weapon_family ==
           Game::Systems::CombatActions::WeaponFamily::Spear) &&
      held_action->normalized_action_time >=
          Game::Systems::CombatActions::action_event_normalized_time(
              *held_definition,
              Game::Systems::CombatActions::CombatActionEventType::RecoveryStart,
              0.75F);

  bool const held_melee_restart = tick.primary_held && !tick.primary_pressed &&
                                  intents.empty() && held_definition == nullptr &&
                                  !gate.guarding && !gate.bow_stance &&
                                  gate.dodge_clear && m_held_restart_delay <= 0.0F;
  if (!held_melee_combo && !held_melee_restart) {
    return;
  }

  auto const* body =
      commander.get_component<Engine::Core::CommanderBodyControlComponent>();
  Engine::Core::CombatActionIntent continuation;
  continuation.type = Engine::Core::CommanderCombatIntentType::Light;
  continuation.has_swing = body != nullptr;
  if (body != nullptr) {
    continuation.swing = body->steered_intent;
    continuation.swing.charge = 0.0F;
  }
  continuation.pressed_at = intents.clock;
  continuation.held_duration = 0.0F;
  intents.push(continuation);
  m_held_restart_delay = k_held_restart_interval;
}

auto CommanderStrike::queue_intents(Engine::Core::Entity& commander,
                                    Engine::Core::CombatIntentQueueComponent& intents,
                                    const CommanderInputSnapshot& tick,
                                    const IntentGate& gate) -> bool {
  queue_held_continuation(commander, intents, tick, gate);

  if (!(tick.primary_pressed || tick.heavy_pressed || tick.special_pressed ||
        gate.jump_followup_pending)) {
    return false;
  }
  auto const* body =
      commander.get_component<Engine::Core::CommanderBodyControlComponent>();
  Engine::Core::CombatActionIntent intent;
  intent.type = pressed_intent_type(tick, gate.jump_followup_pending);
  intent.has_swing = body != nullptr;
  if (body != nullptr) {
    intent.swing = body->steered_intent;
  }
  intent.pressed_at = intents.clock;
  intents.push(intent);
  return true;
}

auto CommanderStrike::request_primary(const PrimaryRequestContext& context,
                                      CommanderTargeting& targeting) -> bool {
  auto* commander =
      controlled_commander(context.world, context.commander_id, context.local_owner_id);
  if (commander == nullptr) {
    return false;
  }

  auto const* aim = commander->get_component<Engine::Core::RpgCommanderAimComponent>();
  bool const shooting =
      aim != nullptr && aim->stance == Engine::Core::FpvWeaponStance::Bow;

  auto const* queued =
      commander->get_component<Engine::Core::CombatIntentQueueComponent>();
  auto const* pending =
      queued != nullptr && queued->count > 0U ? &queued->entries[0] : nullptr;

  Engine::Core::EntityID target_id = 0;
  if (shooting) {
    targeting.clear_primary_slot();
  } else {
    target_id = targeting.find_primary_target(context.world,
                                              context.commander_id,
                                              context.local_owner_id,
                                              context.view_yaw,
                                              pending_acquisition_bonus(pending));
  }
  auto const attack_result =
      Game::Systems::CombatActions::CombatActionService::request_attack(
          context.world,
          {.attacker_id = context.commander_id,
           .target_hint_id = target_id,
           .target_soldier_slot = targeting.primary_slot(),
           .move_right_axis = context.move_right_axis,
           .move_forward_axis = context.move_forward_axis,
           .primary_held_duration =
               pending != nullptr ? pending->held_duration : m_primary_held_duration,
           .intent_type = pending != nullptr
                              ? pending->type
                              : Engine::Core::CommanderCombatIntentType::Light,
           .has_swing = pending != nullptr && pending->has_swing,
           .swing = pending != nullptr ? pending->swing : Engine::Core::MeleeIntent{}});

  if (attack_result.outcome == Engine::Core::CombatIntentOutcome::Accepted) {
    m_combo_miss_timer = 0.0F;
  }
  return true;
}

auto CommanderStrike::dispatch_pending(
    const PrimaryRequestContext& context,
    CommanderTargeting& targeting,
    Engine::Core::Entity& commander,
    Engine::Core::CombatIntentQueueComponent& intents,
    const StrikeSinks& sinks) -> bool {
  if (auto* pending = intents.front(); pending != nullptr) {
    pending->held_duration =
        pending->type == Engine::Core::CommanderCombatIntentType::Heavy
            ? m_primary_held_duration
            : 0.0F;
  }
  if (!request_primary(context, targeting)) {
    return false;
  }
  auto const* queue =
      commander.get_component<Engine::Core::CombatIntentQueueComponent>();
  if (queue == nullptr ||
      queue->last_outcome != Engine::Core::CombatIntentOutcome::Accepted) {
    return true;
  }

  intents.pop_front();
  if (sinks.feedback != nullptr) {
    PlayerFeedbackEvent event;
    event.type = PlayerFeedbackType::AttackCommitted;
    event.entity = context.commander_id;
    event.has_world_position = true;
    event.world_position = context.commander_position;
    sinks.feedback->publish(std::move(event));
  }
  if (sinks.probe != nullptr) {
    sinks.probe->note_attack_start();
    sinks.probe->note_pose_response();
  }
  return true;
}

void CommanderStrike::advance_melee(Engine::Core::Entity& commander,
                                    const MeleeAdvanceInput& input) {
  Game::Systems::CombatActions::advance_melee_control(
      commander,
      {.aim_delta_x = input.aim_delta_x,
       .aim_delta_y = input.aim_delta_y,
       .view_pitch_degrees = input.view_pitch,
       .move_right_axis = input.move_right_axis,
       .move_forward_axis = input.move_forward_axis,
       .held_duration = m_primary_held_duration,
       .primary_held = input.primary_held,
       .guard_held = input.guard_held,
       .delta_time = input.dt});
}

auto CommanderStrike::observe_hits(Engine::Core::Entity& commander)
    -> std::optional<float> {
  auto const* struck =
      commander.get_component<Engine::Core::RpgCommanderActionComponent>();
  if (struck == nullptr) {
    m_observed_action_hit_count = 0;
    return std::nullopt;
  }
  if (!struck->action_running) {
    m_observed_action_hit_count = 0;
    return std::nullopt;
  }
  if (struck->hit_target_count < m_observed_action_hit_count) {
    m_observed_action_hit_count = struck->hit_target_count;
    return std::nullopt;
  }
  if (struck->hit_target_count == m_observed_action_hit_count) {
    return std::nullopt;
  }
  m_observed_action_hit_count = struck->hit_target_count;
  return hit_role_boost(*struck) *
         std::clamp(struck->last_contact_speed /
                        Game::Systems::Combat::k_reference_weapon_speed,
                    0.35F,
                    1.8F);
}

} // namespace App::Core
