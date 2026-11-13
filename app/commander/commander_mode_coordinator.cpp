#include "app/commander/commander_mode_coordinator.h"

#include <QPointF>

#include <algorithm>

#include "app/commander/commander_control_controller.h"
#include "app/economy/production_manager.h"
#include "game/command/command_queue.h"
#include "game/core/component_gameplay.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "game/render_bridge/picking_service.h"
#include "game/session/selection_service.h"
#include "game/session/session_context.h"
#include "game/systems/movement/order_service.h"

namespace App::Core {
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

// The three helpers below are the one definition of what a control handoff
// clears. Entry and exit used to carry their own copies, and the copies had
// already drifted from the fields the controller writes.

// Strike, jump and dodge flags the direct-control tick owns. A handoff with no
// authored action running puts the body back on the ground, standing.
void settle_body_on_handoff(Engine::Core::CommanderComponent& data) {
  data.power_strike_active = false;
  data.just_struck_enemy = false;
  data.last_strike_combo_step = 0U;
  data.jump_active = false;
  data.jump_phase = 0.0F;
  data.jump_height_offset = 0.0F;
  // Left set, these cut the next jump short: the jump timer reads a stale dive
  // as one still in progress.
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

// Only for a body with no authored action running; a running one is preserved
// whole so it finishes under its new owner.
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

auto seed_barracks_rally_preview_impl(Engine::Core::World* world,
                                      int local_owner_id) -> std::optional<QVector3D> {
  if (world == nullptr) {
    return std::nullopt;
  }

  auto* selection_system = &Game::Session::session_for(*world).selection();
  if (selection_system == nullptr) {
    return std::nullopt;
  }

  auto& terrain = Game::Session::session_for(*world).terrain();
  for (auto const entity_id : selection_system->get_selected_units()) {
    auto* entity = world->get_entity(entity_id);
    if (entity == nullptr) {
      continue;
    }

    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if ((unit == nullptr) || unit->owner_id != local_owner_id ||
        unit->spawn_type != Game::Units::SpawnType::Barracks) {
      continue;
    }

    if (auto* production = entity->get_component<Engine::Core::ProductionComponent>();
        production != nullptr && production->rally_set) {
      return QVector3D(
          production->rally_x,
          terrain.resolve_surface_world_y(production->rally_x, production->rally_z),
          production->rally_z);
    }

    if (auto* transform = entity->get_component<Engine::Core::TransformComponent>()) {
      return QVector3D(
          transform->position.x,
          terrain.resolve_surface_world_y(transform->position.x, transform->position.z),
          transform->position.z);
    }
  }

  return std::nullopt;
}

} // namespace

auto to_string(CommanderModeState state) -> const char* {
  switch (state) {
  case CommanderModeState::Inactive:
    return "inactive";
  case CommanderModeState::Entering:
    return "entering";
  case CommanderModeState::Active:
    return "active";
  case CommanderModeState::Exiting:
    return "exiting";
  }
  return "inactive";
}

auto CommanderModeCoordinator::begin_enter() -> bool {
  if (m_state != CommanderModeState::Inactive) {
    return false;
  }
  m_state = CommanderModeState::Entering;
  return true;
}

auto CommanderModeCoordinator::begin_exit() -> bool {
  if (m_state != CommanderModeState::Active) {
    return false;
  }
  m_state = CommanderModeState::Exiting;
  return true;
}

void CommanderModeCoordinator::complete_transition() {
  switch (m_state) {
  case CommanderModeState::Entering:
    m_state = CommanderModeState::Active;
    break;
  case CommanderModeState::Exiting:
    m_state = CommanderModeState::Inactive;
    break;
  case CommanderModeState::Inactive:
  case CommanderModeState::Active:
    break;
  }
}

void CommanderModeCoordinator::abort_transition() {
  switch (m_state) {
  case CommanderModeState::Entering:
    m_state = CommanderModeState::Inactive;
    break;
  case CommanderModeState::Exiting:
    m_state = CommanderModeState::Active;
    break;
  case CommanderModeState::Inactive:
  case CommanderModeState::Active:
    break;
  }
}

void CommanderModeCoordinator::force_inactive() {
  m_state = CommanderModeState::Inactive;
}

auto CommanderModeCoordinator::store_rts_selection(
    const CommanderSelectionContext& context) const -> CommanderSelectionEffects {
  CommanderSelectionEffects effects;
  if (context.selection_controller != nullptr) {
    context.selection_controller->get_selected_unit_ids(
        effects.saved_rts_selection_ids);
  }
  return effects;
}

void CommanderModeCoordinator::select_controlled_commander(
    const CommanderSelectionContext& context) const {
  if (context.controlled_commander_id == 0 || context.selection_controller == nullptr) {
    return;
  }

  context.selection_controller->select_single_unit(context.controlled_commander_id,
                                                   context.local_owner_id);
}

auto CommanderModeCoordinator::restore_rts_selection(
    const CommanderSelectionContext& context) const -> CommanderSelectionEffects {
  CommanderSelectionEffects effects;
  effects.clear_saved_rts_selection_ids = true;

  if (context.world == nullptr || context.saved_rts_selection_ids == nullptr) {
    return effects;
  }

  auto* selection_system = &Game::Session::session_for(*context.world).selection();
  if (selection_system == nullptr) {
    return effects;
  }

  selection_system->clear_selection();
  for (const auto id : *context.saved_rts_selection_ids) {
    auto* entity = context.world->get_entity(id);
    auto* unit = entity != nullptr
                     ? entity->get_component<Engine::Core::UnitComponent>()
                     : nullptr;
    if (unit == nullptr || unit->health <= 0 ||
        unit->owner_id != context.local_owner_id) {
      continue;
    }
    selection_system->select_unit(id);
  }

  effects.sync_selection_flags = true;
  effects.emit_selected_units_changed = true;
  return effects;
}

auto CommanderModeCoordinator::enter_commander_control_mode(
    const CommanderModeContext& context) const -> CommanderModeEffects {
  CommanderModeEffects effects;
  if (context.is_spectator_mode || context.commander == nullptr ||
      context.commander_camera == nullptr || context.commander_control == nullptr) {
    return effects;
  }

  effects.entered = true;
  effects.controlled_commander_id = context.commander->get_id();
  effects.save_follow_selection_snapshot = true;
  effects.saved_follow_selection_enabled = context.follow_selection_enabled;

  auto const* existing_action =
      context.commander->get_component<Engine::Core::RpgCommanderActionComponent>();
  bool const preserve_advanced_action =
      existing_action != nullptr && existing_action->action_running;

  if (auto* commander_data =
          context.commander->get_component<Engine::Core::CommanderComponent>()) {
    commander_data->rally_requested = false;
    commander_data->rally_requires_manual_trigger = true;
    commander_data->fpv_controlled = true;
    if (!preserve_advanced_action) {
      commander_data->combo_step = 0;
      settle_body_on_handoff(*commander_data);
    }
    commander_data->punish_window_remaining = 0.0F;
  }

  if (auto* stamina =
          Engine::Core::get_or_add_component<Engine::Core::StaminaComponent>(
              context.commander)) {
    constexpr float k_commander_stamina = 180.0F;
    constexpr float k_commander_stamina_regen = 30.0F;
    // Changing control may raise capacity, but cannot refund spent stamina or
    // skip the recovery delay established by a combat action.
    if (stamina->max_stamina < k_commander_stamina) {
      stamina->max_stamina = k_commander_stamina;
    }
    stamina->regen_rate = std::max(stamina->regen_rate, k_commander_stamina_regen);
  }

  if (auto* transform =
          context.commander->get_component<Engine::Core::TransformComponent>()) {
    effects.commander_view_yaw = transform->rotation.y;
  }

  if (auto* rpg = Engine::Core::get_or_add_component<Engine::Core::RpgHealthComponent>(
          context.commander)) {
    auto* unit = context.commander->get_component<Engine::Core::UnitComponent>();
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
              context.commander)) {
    clear_target_state(*rpg_targets);
  }
  if (auto* rpg_action =
          Engine::Core::get_or_add_component<Engine::Core::RpgCommanderActionComponent>(
              context.commander);
      rpg_action != nullptr && !preserve_advanced_action) {
    clear_idle_action(*rpg_action);
  }

  return effects;
}

auto CommanderModeCoordinator::exit_commander_control_mode(
    const CommanderModeContext& context) const -> CommanderModeEffects {
  CommanderModeEffects effects;
  clear_controlled_commander_state_impl(context.world, context.controlled_commander_id);
  effects.controlled_commander_id = 0;
  if (context.rts_follow_selection_snapshot_valid) {
    effects.restored_follow_selection_enabled = context.rts_follow_selection_snapshot;
  }
  return effects;
}

void CommanderModeCoordinator::clear_controlled_commander_state(
    const CommanderModeContext& context) const {
  clear_controlled_commander_state_impl(context.world, context.controlled_commander_id);
}

auto CommanderModeCoordinator::begin_commander_flag_rally(
    const CommanderRallyContext& context) const -> CommanderRallyEffects {
  CommanderRallyEffects effects;
  if (context.world == nullptr) {
    return effects;
  }

  auto* commander = active_commander(context);
  if (commander == nullptr) {
    if (context.commander_mode_active) {
      effects.should_exit_commander_mode = true;
    }
    return effects;
  }

  auto* commander_data = commander->get_component<Engine::Core::CommanderComponent>();
  if (commander_data == nullptr || commander_data->flag_rally_in_progress) {
    return effects;
  }

  if (context.commander_mode_active) {
    effects.reset_commander_input = true;
  }
  effects.clear_rally_preview = true;
  effects.cursor_mode = CursorMode::PlaceCommanderRally;
  effects.seed_preview_from_view_center = true;
  return effects;
}

auto CommanderModeCoordinator::confirm_commander_flag_rally(
    const CommanderRallyContext& context) const -> CommanderRallyEffects {
  CommanderRallyEffects effects;
  if (context.cursor_mode != CursorMode::PlaceCommanderRally) {
    return effects;
  }

  if (context.world == nullptr || context.picking_service == nullptr ||
      context.camera == nullptr) {
    effects.cursor_mode = CursorMode::Normal;
    return effects;
  }

  QVector3D hit;
  if (!Game::Systems::PickingService::screen_to_ground(
          QPointF(context.screen_x, context.screen_y),
          *context.camera,
          context.viewport_width,
          context.viewport_height,
          hit)) {
    return effects;
  }

  auto* commander = active_commander(context);
  auto* unit = commander != nullptr
                   ? commander->get_component<Engine::Core::UnitComponent>()
                   : nullptr;
  auto* commander_data =
      commander != nullptr
          ? commander->get_component<Engine::Core::CommanderComponent>()
          : nullptr;
  if (commander == nullptr || unit == nullptr || commander_data == nullptr ||
      commander_data->flag_rally_in_progress ||
      unit->owner_id != context.local_owner_id || unit->health <= 0) {
    effects.clear_rally_preview = true;
    effects.cursor_mode = CursorMode::Normal;
    return effects;
  }

  if (context.commander_mode_active) {
    commander_data->fpv_controlled = false;
    effects.reset_commander_input = true;
  }
  Game::Command::submit(*context.world,
                        Game::Command::Source::LocalPlayer,
                        context.local_owner_id,
                        Game::Command::UseCommanderAbility{
                            .commander = commander->get_id(),
                            .ability = Game::Command::CommanderAbility::FlagRally,
                            .target = hit});

  effects.clear_rally_preview = true;
  effects.cursor_mode = CursorMode::Normal;
  return effects;
}

auto CommanderModeCoordinator::rally_where_the_commander_stands(
    const CommanderRallyContext& context) const -> CommanderRallyEffects {
  CommanderRallyEffects effects;
  auto* commander = active_commander(context);
  if (context.world == nullptr || commander == nullptr) {
    return effects;
  }
  auto const id = commander->get_id();
  auto* unit = context.world->try_get<Engine::Core::UnitComponent>(id);
  auto* commander_data = context.world->try_get<Engine::Core::CommanderComponent>(id);
  auto const* transform = context.world->try_get<Engine::Core::TransformComponent>(id);
  if (unit == nullptr || commander_data == nullptr || transform == nullptr ||
      commander_data->flag_rally_in_progress ||
      unit->owner_id != context.local_owner_id || unit->health <= 0) {
    return effects;
  }

  if (context.commander_mode_active) {
    commander_data->fpv_controlled = false;
    effects.reset_commander_input = true;
  }
  Game::Command::submit(
      *context.world,
      Game::Command::Source::LocalPlayer,
      context.local_owner_id,
      Game::Command::UseCommanderAbility{
          .commander = commander->get_id(),
          .ability = Game::Command::CommanderAbility::FlagRally,
          .target = QVector3D(transform->position.x, 0.0F, transform->position.z)});
  effects.clear_rally_preview = true;
  return effects;
}

auto CommanderModeCoordinator::cancel_commander_flag_rally(CursorMode cursor_mode) const
    -> CommanderRallyEffects {
  CommanderRallyEffects effects;
  effects.clear_rally_preview = true;
  if (cursor_mode == CursorMode::PlaceCommanderRally) {
    effects.cursor_mode = CursorMode::Normal;
  }
  return effects;
}

auto CommanderModeCoordinator::begin_barracks_rally_placement(
    const BarracksRallyContext& context) const -> BarracksRallyEffects {
  BarracksRallyEffects effects;
  if (!has_selected_local_barracks(context.world, context.local_owner_id)) {
    return effects;
  }

  effects.clear_rally_preview = true;
  effects.cursor_mode = CursorMode::PlaceBarracksRally;
  effects.rally_preview = seed_barracks_rally_preview_from_selection(context);
  return effects;
}

auto CommanderModeCoordinator::confirm_barracks_rally_placement(
    const BarracksRallyContext& context) const -> BarracksRallyEffects {
  BarracksRallyEffects effects;
  if (context.cursor_mode != CursorMode::PlaceBarracksRally) {
    return effects;
  }

  if (!has_selected_local_barracks(context.world, context.local_owner_id) ||
      context.production_manager == nullptr || context.viewport == nullptr) {
    effects.clear_rally_preview = true;
    effects.cursor_mode = CursorMode::Normal;
    return effects;
  }

  if (!context.production_manager->set_rally_at_screen(context.screen_x,
                                                       context.screen_y,
                                                       context.local_owner_id,
                                                       *context.viewport)) {
    return effects;
  }

  effects.clear_rally_preview = true;
  effects.cursor_mode = CursorMode::Normal;
  return effects;
}

auto CommanderModeCoordinator::cancel_barracks_rally_placement(
    CursorMode cursor_mode) const -> BarracksRallyEffects {
  BarracksRallyEffects effects;
  effects.clear_rally_preview = true;
  if (cursor_mode == CursorMode::PlaceBarracksRally) {
    effects.cursor_mode = CursorMode::Normal;
  }
  return effects;
}

auto CommanderModeCoordinator::seed_barracks_rally_preview_from_selection(
    const BarracksRallyContext& context) const -> std::optional<QVector3D> {
  return seed_barracks_rally_preview_impl(context.world, context.local_owner_id);
}

auto CommanderModeCoordinator::update_commander_control_mode(
    const CommanderUpdateContext& context) const -> CommanderUpdateEffects {
  CommanderUpdateEffects effects;
  if (context.world == nullptr || context.commander_camera == nullptr ||
      context.commander_control == nullptr) {
    effects.should_exit_commander_mode = true;
    return effects;
  }

  if (!context.commander_control->update_simulation(*context.world,
                                                    context.controlled_commander_id,
                                                    context.local_owner_id,
                                                    context.dt)) {
    effects.should_exit_commander_mode = true;
    return effects;
  }

  return effects;
}

auto CommanderModeCoordinator::restore_controlled_commander_direct_control_if_ready(
    const CommanderDirectControlContext& context) const
    -> CommanderDirectControlEffects {
  CommanderDirectControlEffects effects;
  if (!context.commander_mode_active || context.world == nullptr ||
      context.placing_commander_rally) {
    return effects;
  }

  auto* commander = context.world->get_entity(context.controlled_commander_id);
  auto* unit = commander != nullptr
                   ? commander->get_component<Engine::Core::UnitComponent>()
                   : nullptr;
  auto* commander_data =
      commander != nullptr
          ? commander->get_component<Engine::Core::CommanderComponent>()
          : nullptr;
  if (commander == nullptr || unit == nullptr || commander_data == nullptr ||
      unit->health <= 0 || commander_data->fpv_controlled ||
      commander_data->flag_rally_in_progress) {
    return effects;
  }

  commander_data->fpv_controlled = true;
  effects.reset_commander_input = true;
  return effects;
}

void CommanderModeCoordinator::clear_controlled_commander_state_impl(
    Engine::Core::World* world, Engine::Core::EntityID controlled_commander_id) const {
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
    commander_data->punish_window_remaining = 0.0F;
  }
  if (auto* guard = commander->get_component<Engine::Core::CommanderGuardComponent>()) {
    // Release input-owned protection; simulation-owned guard-break recovery
    // survives the handoff and continues to tick in CommanderSystem.
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

auto CommanderModeCoordinator::active_commander(const CommanderRallyContext& context)
    -> Engine::Core::Entity* {
  return context.commander_mode_active ? context.controlled_commander
                                       : context.local_commander;
}

auto CommanderModeCoordinator::has_selected_local_barracks(Engine::Core::World* world,
                                                           int local_owner_id) -> bool {
  if (world == nullptr) {
    return false;
  }

  auto* selection_system = &Game::Session::session_for(*world).selection();
  if (selection_system == nullptr) {
    return false;
  }

  for (auto const entity_id : selection_system->get_selected_units()) {
    auto* entity = world->get_entity(entity_id);
    if (entity == nullptr) {
      continue;
    }

    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if ((unit != nullptr) && unit->owner_id == local_owner_id &&
        unit->spawn_type == Game::Units::SpawnType::Barracks) {
      return true;
    }
  }

  return false;
}

} // namespace App::Core
