#include "commander_mode_coordinator.h"

#include <QPointF>

#include "commander_control_controller.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "game/systems/command_service.h"
#include "game/systems/picking_service.h"
#include "game/systems/rpg_combat_system/rpg_combat_processor.h"
#include "game/systems/selection_system.h"
#include "input_command_handler.h"
#include "production_manager.h"

namespace App::Core {
namespace {

auto seed_barracks_rally_preview_impl(Engine::Core::World* world,
                                      int local_owner_id) -> std::optional<QVector3D> {
  if (world == nullptr) {
    return std::nullopt;
  }

  auto* selection_system = world->get_system<Game::Systems::SelectionSystem>();
  if (selection_system == nullptr) {
    return std::nullopt;
  }

  auto& terrain = Game::Map::TerrainService::instance();
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

  auto* selection_system = context.world->get_system<Game::Systems::SelectionSystem>();
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

  if (auto* commander_data =
          context.commander->get_component<Engine::Core::CommanderComponent>()) {
    commander_data->rally_requested = false;
    commander_data->rally_requires_manual_trigger = true;
    commander_data->fpv_controlled = true;
    commander_data->combo_step = 0;
    commander_data->power_strike_active = false;
    commander_data->just_struck_enemy = false;
    commander_data->last_strike_combo_step = 0U;
    commander_data->jump_active = false;
    commander_data->jump_phase = 0.0F;
    commander_data->jump_height_offset = 0.0F;
    commander_data->posture = 0.0F;
    commander_data->punish_window_remaining = 0.0F;
    commander_data->shield_bash_cooldown_remaining = 0.0F;
    commander_data->vanguard_rush_cooldown_remaining = 0.0F;
    commander_data->second_wind_cooldown_remaining = 0.0F;
  }

  if (auto* transform =
          context.commander->get_component<Engine::Core::TransformComponent>()) {
    effects.commander_view_yaw = transform->rotation.y;
  }

  if (auto* rpg = Engine::Core::get_or_add_component<Engine::Core::RpgHealthComponent>(
          context.commander)) {
    auto* unit = context.commander->get_component<Engine::Core::UnitComponent>();
    if (!rpg->active) {
      rpg->rpg_max_hp = (unit != nullptr) ? unit->max_health * 3 : 150;
      rpg->rpg_hp = rpg->rpg_max_hp;
    }
    rpg->active = true;
  }

  (void)Engine::Core::get_or_add_component<Engine::Core::RpgCommanderTargetComponent>(
      context.commander);
  if (auto* rpg_action =
          Engine::Core::get_or_add_component<Engine::Core::RpgCommanderActionComponent>(
              context.commander)) {
    rpg_action->phase = Engine::Core::RpgCommanderActionPhase::None;
    rpg_action->combat_action_id = 0U;
    rpg_action->melee_attack_sequence = 0;
    rpg_action->active_target_id = 0;
    rpg_action->last_hit_target_id = 0;
    rpg_action->hit_target_ids.fill(0U);
    rpg_action->hit_target_count = 0U;
    rpg_action->last_damage = 0;
    rpg_action->normalized_action_time = 0.0F;
    rpg_action->next_event_index = 0U;
    rpg_action->last_event_type = 0U;
    rpg_action->last_event_valid = false;
    rpg_action->action_active = false;
    rpg_action->weapon_trace_active = false;
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

  std::vector<Engine::Core::EntityID> const commander_selection{commander->get_id()};
  auto const move_plan = Game::Systems::CommandService::plan_ground_move(
      *context.world, commander_selection, hit);
  if (move_plan.positions.empty()) {
    effects.clear_rally_preview = true;
    effects.cursor_mode = CursorMode::Normal;
    return effects;
  }

  commander_data->begin_flag_rally(
      move_plan.resolved_target.x(), move_plan.resolved_target.z(), false);
  if (context.commander_mode_active) {
    commander_data->fpv_controlled = false;
    effects.reset_commander_input = true;
  }
  if (auto* stamina = commander->get_component<Engine::Core::StaminaComponent>()) {
    stamina->run_requested = false;
    stamina->is_running = false;
  }
  Game::Systems::CommandService::issue_ground_move(
      *context.world, commander_selection, move_plan);

  effects.clear_rally_preview = true;
  effects.cursor_mode = CursorMode::Normal;
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

  if (!context.commander_control->update(*context.world,
                                         context.controlled_commander_id,
                                         context.local_owner_id,
                                         *context.commander_camera,
                                         context.dt)) {
    effects.should_exit_commander_mode = true;
    return effects;
  }

  auto* commander = context.world->get_entity(context.controlled_commander_id);
  if (commander != nullptr) {
    auto* commander_data = commander->get_component<Engine::Core::CommanderComponent>();
    if (commander_data != nullptr && commander_data->just_struck_enemy) {
      commander_data->just_struck_enemy = false;
      float hit_stop_duration = 0.10F;
      if (commander_data->last_strike_combo_step >= 3) {
        hit_stop_duration = 0.18F;
      } else if (commander_data->power_strike_active) {
        hit_stop_duration = 0.14F;
      }
      effects.hit_stop_duration = hit_stop_duration;
    }
  }

  Game::Systems::RpgCombat::tick_rpg_combat(
      context.world, context.controlled_commander_id, context.dt);
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

  if (auto* commander_data =
          commander->get_component<Engine::Core::CommanderComponent>()) {
    commander_data->rally_requested = false;
    commander_data->rally_requires_manual_trigger = false;
    commander_data->fpv_controlled = false;
    commander_data->power_strike_active = false;
    commander_data->just_struck_enemy = false;
    commander_data->last_strike_combo_step = 0U;
    commander_data->jump_active = false;
    commander_data->jump_phase = 0.0F;
    commander_data->jump_height_offset = 0.0F;
    commander_data->posture = 0.0F;
    commander_data->punish_window_remaining = 0.0F;
    commander_data->shield_bash_cooldown_remaining = 0.0F;
    commander_data->vanguard_rush_cooldown_remaining = 0.0F;
    commander_data->second_wind_cooldown_remaining = 0.0F;
  }
  if (auto* guard = commander->get_component<Engine::Core::CommanderGuardComponent>()) {
    guard->active = false;
    guard->perfect_guard_remaining = 0.0F;
    guard->guard_break_remaining = 0.0F;
    guard->rearm_requires_release = false;
  }
  if (auto* rpg = commander->get_component<Engine::Core::RpgHealthComponent>()) {
    rpg->active = false;
    rpg->dodge_invincible = false;
  }
  if (auto* rpg_targets =
          commander->get_component<Engine::Core::RpgCommanderTargetComponent>()) {
    rpg_targets->explicit_lock_target_id = 0;
    rpg_targets->aim_candidate_id = 0;
    rpg_targets->recent_hit_target_id = 0;
    rpg_targets->recent_hit_timer = 0.0F;
  }
  if (auto* rpg_action =
          commander->get_component<Engine::Core::RpgCommanderActionComponent>()) {
    rpg_action->phase = Engine::Core::RpgCommanderActionPhase::None;
    rpg_action->combat_action_id = 0U;
    rpg_action->melee_attack_sequence = 0;
    rpg_action->active_target_id = 0;
    rpg_action->last_hit_target_id = 0;
    rpg_action->hit_target_ids.fill(0U);
    rpg_action->hit_target_count = 0U;
    rpg_action->last_damage = 0;
    rpg_action->normalized_action_time = 0.0F;
    rpg_action->next_event_index = 0U;
    rpg_action->last_event_type = 0U;
    rpg_action->last_event_valid = false;
    rpg_action->action_active = false;
    rpg_action->weapon_trace_active = false;
  }
  InputCommandHandler::reset_movement(commander);
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

  auto* selection_system = world->get_system<Game::Systems::SelectionSystem>();
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
