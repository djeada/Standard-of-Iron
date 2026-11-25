#include "game/commander/commander_handoff.h"

#include <algorithm>

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/systems/movement/order_service.h"

namespace Game::Commander {
namespace {

constexpr int k_rpg_pool_base = 120;
constexpr int k_rpg_pool_min = 130;
constexpr int k_rpg_pool_max = 220;
constexpr int k_rpg_pool_health_divisor = 60;

auto rpg_pool_for_commander(const Engine::Core::UnitComponent* unit) -> int {
  if (unit == nullptr) {
    return 150;
  }
  const int scaled = k_rpg_pool_base + (unit->max_health / k_rpg_pool_health_divisor);
  return std::clamp(scaled, k_rpg_pool_min, k_rpg_pool_max);
}

void settle_body_on_handoff(Engine::Core::CommanderComponent& data) {
  data.power_strike_active = false;
  data.just_struck_enemy = false;
  data.last_strike_combo_step = 0U;
  data.jump_active = false;
  data.jump_phase = 0.0F;
  data.jump_height_offset = 0.0F;

  data.airborne_velocity = 0.0F;
  data.dive_attack_active = false;
  data.dodge_active = false;
  data.dodge_phase = 0.0F;
}

void clear_target_state(Engine::Core::RpgCommanderTargetComponent& targets) {
  constexpr auto k_no_slot =
      Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot;
  targets.explicit_lock_target_id = 0;
  targets.explicit_lock_soldier_slot = k_no_slot;
  targets.aim_candidate_id = 0;
  targets.aim_candidate_soldier_slot = k_no_slot;
  targets.aim_candidate_in_range = false;
  targets.recent_hit_target_id = 0;
  targets.recent_hit_soldier_slot = k_no_slot;
  targets.recent_hit_timer = 0.0F;
}

void clear_idle_action(Engine::Core::RpgCommanderActionComponent& action) {
  constexpr auto k_no_slot =
      Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot;
  action.phase = Engine::Core::RpgCommanderActionPhase::None;
  action.combat_action_id = 0U;
  action.melee_attack_sequence = 0;
  action.active_target_id = 0;
  action.active_target_soldier_slot = k_no_slot;
  action.last_hit_target_id = 0;
  action.last_hit_soldier_slot = k_no_slot;
  action.hit_target_ids.fill(0U);
  action.hit_target_soldier_slots.fill(k_no_slot);
  action.hit_target_count = 0U;
  action.last_damage = 0;
  action.normalized_action_time = 0.0F;
  action.next_event_index = 0U;
  action.last_event_type = 0U;
  action.last_event_valid = false;
  action.action_active = false;
  action.weapon_trace_active = false;
}

} // namespace

auto find_commander(Engine::Core::World& world, int owner) -> Engine::Core::Entity* {
  Engine::Core::EntityID found = 0;
  for (auto [id, unit, commander, transform] :
       world.view<const Engine::Core::UnitComponent,
                  const Engine::Core::CommanderComponent,
                  const Engine::Core::TransformComponent>()) {
    if (unit.owner_id == owner && unit.health > 0 && (found == 0 || id < found)) {
      found = id;
    }
  }
  return world.get_entity(found);
}

void begin_control(Engine::Core::Entity& commander) {
  auto const* existing_action =
      commander.get_component<Engine::Core::RpgCommanderActionComponent>();
  bool const preserve_advanced_action =
      existing_action != nullptr && existing_action->action_running;

  if (auto* commander_data =
          commander.get_component<Engine::Core::CommanderComponent>()) {
    commander_data->rally_requested = false;
    commander_data->rally_requires_manual_trigger = true;
    commander_data->fpv_controlled = true;
    if (!preserve_advanced_action) {
      commander_data->combo_step = 0;
      settle_body_on_handoff(*commander_data);
    }
  }

  if (auto* stamina =
          Engine::Core::get_or_add_component<Engine::Core::StaminaComponent>(
              &commander)) {
    constexpr float k_commander_stamina = 180.0F;
    constexpr float k_commander_stamina_regen = 30.0F;

    if (stamina->max_stamina < k_commander_stamina) {
      stamina->max_stamina = k_commander_stamina;
    }
    stamina->regen_rate = std::max(stamina->regen_rate, k_commander_stamina_regen);
  }

  if (auto* rpg = Engine::Core::get_or_add_component<Engine::Core::RpgHealthComponent>(
          &commander)) {
    auto* unit = commander.get_component<Engine::Core::UnitComponent>();
    if (!rpg->active) {

      int const pool = std::max(1, rpg_pool_for_commander(unit));
      int const own_health = unit != nullptr ? std::max(1, unit->max_health) : pool;
      rpg->incoming_damage_scale =
          std::min(1.0F, static_cast<float>(own_health) / static_cast<float>(pool));
    }
    rpg->active = true;
  }

  if (auto* rpg_targets =
          Engine::Core::get_or_add_component<Engine::Core::RpgCommanderTargetComponent>(
              &commander)) {
    clear_target_state(*rpg_targets);
  }
  if (auto* rpg_action =
          Engine::Core::get_or_add_component<Engine::Core::RpgCommanderActionComponent>(
              &commander);
      rpg_action != nullptr && !preserve_advanced_action) {
    clear_idle_action(*rpg_action);
  }
}

void resume_control(Engine::Core::Entity& commander) {
  if (auto* data = commander.get_component<Engine::Core::CommanderComponent>();
      data != nullptr && !data->flag_rally_in_progress) {
    data->fpv_controlled = true;
  }
}

void end_control(Engine::Core::World* world,
                 Engine::Core::EntityID controlled_commander_id) {
  if (world == nullptr || controlled_commander_id == 0) {
    return;
  }

  auto* commander = world->get_entity(controlled_commander_id);
  if (commander == nullptr) {
    return;
  }

  auto const* existing_action =
      commander->get_component<Engine::Core::RpgCommanderActionComponent>();
  bool const preserve_advanced_action =
      existing_action != nullptr && existing_action->action_running;

  if (auto* commander_data =
          commander->get_component<Engine::Core::CommanderComponent>()) {
    commander_data->rally_requested = false;
    commander_data->rally_requires_manual_trigger = true;
    commander_data->fpv_controlled = false;
    if (!preserve_advanced_action) {
      settle_body_on_handoff(*commander_data);
    }
  }
  if (auto* guard = commander->get_component<Engine::Core::CommanderGuardComponent>()) {

    guard->active = false;
    guard->perfect_guard_remaining = 0.0F;
    guard->rearm_requires_release = false;
  }
  if (auto* rpg = commander->get_component<Engine::Core::RpgHealthComponent>()) {
    rpg->active = false;
    rpg->dodge_grace_remaining = 0.0F;
  }
  if (auto* rpg_targets =
          commander->get_component<Engine::Core::RpgCommanderTargetComponent>()) {
    clear_target_state(*rpg_targets);
  }

  world->remove<Engine::Core::CommanderPresentationSampleComponent>(
      controlled_commander_id);
  if (auto* rpg_action =
          commander->get_component<Engine::Core::RpgCommanderActionComponent>();
      rpg_action != nullptr && !preserve_advanced_action) {
    clear_idle_action(*rpg_action);
  }
  Game::Systems::OrderService::reset_movement(commander);
}

} // namespace Game::Commander
