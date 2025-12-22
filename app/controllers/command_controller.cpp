#include "command_controller.h"
#include "../../game/core/component.h"
#include "../../game/core/entity.h"
#include "../../game/core/world.h"
#include "../../game/systems/command_service.h"
#include "../../game/systems/formation_planner.h"
#include "../../game/systems/picking_service.h"
#include "../../game/systems/production_service.h"
#include "../../game/systems/selection_system.h"
#include "../../game/systems/troop_profile_service.h"
#include "../../render/gl/camera.h"
#include "../utils/movement_utils.h"
#include "game/game_config.h"
#include "units/spawn_type.h"
#include <QPointF>
#include <cmath>
#include <numbers>
#include <qglobal.h>
#include <qobject.h>
#include <qtmetamacros.h>
#include <qvectornd.h>
#include <vector>

namespace App::Controllers {

CommandController::CommandController(
    Engine::Core::World *world,
    Game::Systems::SelectionSystem *selection_system,
    Game::Systems::PickingService *picking_service, QObject *parent)
    : QObject(parent), m_world(world), m_selection_system(selection_system),
      m_picking_service(picking_service) {}

auto CommandController::on_attack_click(qreal sx, qreal sy, int viewport_width,
                                        int viewport_height,
                                        void *camera) -> CommandResult {
  CommandResult result;
  if ((m_selection_system == nullptr) || (m_picking_service == nullptr) ||
      (camera == nullptr) || (m_world == nullptr)) {
    result.reset_cursor_to_normal = true;
    return result;
  }

  const auto &selected = m_selection_system->get_selected_units();
  if (selected.empty()) {
    result.reset_cursor_to_normal = true;
    return result;
  }

  auto *cam = static_cast<Render::GL::Camera *>(camera);
  Engine::Core::EntityID const target_id =
      Game::Systems::PickingService::pick_unit_first(
          float(sx), float(sy), *m_world, *cam, viewport_width, viewport_height,
          0);

  if (target_id == 0) {
    result.reset_cursor_to_normal = true;
    return result;
  }

  auto *target_entity = m_world->get_entity(target_id);
  if (target_entity == nullptr) {
    return result;
  }

  auto *target_unit =
      target_entity->get_component<Engine::Core::UnitComponent>();
  if (target_unit == nullptr) {
    return result;
  }

  Game::Systems::CommandService::attack_target(*m_world, selected, target_id,
                                               true);

  emit attack_target_selected();

  result.input_consumed = true;
  result.reset_cursor_to_normal = true;
  return result;
}

auto CommandController::on_stop_command() -> CommandResult {
  CommandResult result;
  if ((m_selection_system == nullptr) || (m_world == nullptr)) {
    return result;
  }

  const auto &selected = m_selection_system->get_selected_units();
  if (selected.empty()) {
    return result;
  }

  for (auto id : selected) {
    auto *entity = m_world->get_entity(id);
    if (entity == nullptr) {
      continue;
    }

    reset_movement(entity);
    entity->remove_component<Engine::Core::AttackTargetComponent>();

    if (auto *patrol = entity->get_component<Engine::Core::PatrolComponent>()) {
      patrol->patrolling = false;
      patrol->waypoints.clear();
    }

    auto *hold_mode = entity->get_component<Engine::Core::HoldModeComponent>();
    if ((hold_mode != nullptr) && hold_mode->active) {
      hold_mode->active = false;
      hold_mode->exit_cooldown = hold_mode->stand_up_duration;
      emit hold_mode_changed(false);
    }

    auto *formation_mode =
        entity->get_component<Engine::Core::FormationModeComponent>();
    if ((formation_mode != nullptr) && formation_mode->active) {
      formation_mode->active = false;
      emit formation_mode_changed(false);
    }
  }

  result.input_consumed = true;
  result.reset_cursor_to_normal = true;
  return result;
}

auto CommandController::on_hold_command() -> CommandResult {
  CommandResult result;
  if ((m_selection_system == nullptr) || (m_world == nullptr)) {
    return result;
  }

  const auto &selected = m_selection_system->get_selected_units();
  if (selected.empty()) {
    return result;
  }

  int eligible_count = 0;
  int hold_active_count = 0;

  for (auto id : selected) {
    auto *entity = m_world->get_entity(id);
    if (entity == nullptr) {
      continue;
    }

    auto *unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr) {
      continue;
    }

    if (!Game::Units::can_use_hold_mode(unit->spawn_type)) {
      continue;
    }

    eligible_count++;

    auto *hold_mode = entity->get_component<Engine::Core::HoldModeComponent>();
    if ((hold_mode != nullptr) && hold_mode->active) {
      hold_active_count++;
    }
  }

  if (eligible_count == 0) {
    return result;
  }

  const bool should_enable_hold = (hold_active_count < eligible_count);

  for (auto id : selected) {
    auto *entity = m_world->get_entity(id);
    if (entity == nullptr) {
      continue;
    }

    auto *unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr) {
      continue;
    }

    if (!Game::Units::can_use_hold_mode(unit->spawn_type)) {
      continue;
    }

    auto *hold_mode = entity->get_component<Engine::Core::HoldModeComponent>();

    if (should_enable_hold) {

      reset_movement(entity);
      entity->remove_component<Engine::Core::AttackTargetComponent>();

      // Clear melee lock to prevent auto-attacking while in hold mode
      auto *attack_comp = entity->get_component<Engine::Core::AttackComponent>();
      if (attack_comp != nullptr) {
        attack_comp->in_melee_lock = false;
        attack_comp->melee_lock_target_id = 0;
      }

      if (auto *patrol =
              entity->get_component<Engine::Core::PatrolComponent>()) {
        patrol->patrolling = false;
        patrol->waypoints.clear();
      }

      if (hold_mode == nullptr) {
        hold_mode = entity->add_component<Engine::Core::HoldModeComponent>();
      }
      hold_mode->active = true;
      hold_mode->exit_cooldown = 0.0F;

      auto *movement = entity->get_component<Engine::Core::MovementComponent>();
      if (movement != nullptr) {
        movement->has_target = false;
        movement->path.clear();
        movement->path_pending = false;
        movement->vx = 0.0F;
        movement->vz = 0.0F;
      }
    } else {

      if ((hold_mode != nullptr) && hold_mode->active) {
        hold_mode->active = false;
        hold_mode->exit_cooldown = hold_mode->stand_up_duration;
      }
    }
  }

  emit hold_mode_changed(should_enable_hold);

  result.input_consumed = true;
  result.reset_cursor_to_normal = true;
  return result;
}

auto CommandController::on_patrol_click(qreal sx, qreal sy, int viewport_width,
                                        int viewport_height,
                                        void *camera) -> CommandResult {
  CommandResult result;
  if ((m_selection_system == nullptr) || (m_world == nullptr) ||
      (m_picking_service == nullptr) || (camera == nullptr)) {
    if (m_has_patrol_first_waypoint) {
      clear_patrol_first_waypoint();
      result.reset_cursor_to_normal = true;
    }
    return result;
  }

  const auto &selected = m_selection_system->get_selected_units();
  if (selected.empty()) {
    if (m_has_patrol_first_waypoint) {
      clear_patrol_first_waypoint();
      result.reset_cursor_to_normal = true;
    }
    return result;
  }

  auto *cam = static_cast<Render::GL::Camera *>(camera);
  QVector3D hit;
  if (!Game::Systems::PickingService::screen_to_ground(
          QPointF(sx, sy), *cam, viewport_width, viewport_height, hit)) {
    if (m_has_patrol_first_waypoint) {
      clear_patrol_first_waypoint();
      result.reset_cursor_to_normal = true;
    }
    return result;
  }

  if (!m_has_patrol_first_waypoint) {
    m_has_patrol_first_waypoint = true;
    m_patrol_first_waypoint = hit;
    result.input_consumed = true;
    return result;
  }

  QVector3D const second_waypoint = hit;

  for (auto id : selected) {
    auto *entity = m_world->get_entity(id);
    if (entity == nullptr) {
      continue;
    }

    auto *building = entity->get_component<Engine::Core::BuildingComponent>();
    if (building != nullptr) {
      continue;
    }

    auto *patrol = entity->get_component<Engine::Core::PatrolComponent>();
    if (patrol == nullptr) {
      patrol = entity->add_component<Engine::Core::PatrolComponent>();
    }

    if (patrol != nullptr) {
      patrol->waypoints.clear();
      patrol->waypoints.emplace_back(m_patrol_first_waypoint.x(),
                                     m_patrol_first_waypoint.z());
      patrol->waypoints.emplace_back(second_waypoint.x(), second_waypoint.z());
      patrol->current_waypoint = 0;
      patrol->patrolling = true;
    }

    reset_movement(entity);
    entity->remove_component<Engine::Core::AttackTargetComponent>();
  }

  clear_patrol_first_waypoint();
  result.input_consumed = true;
  result.reset_cursor_to_normal = true;
  return result;
}

auto CommandController::set_rally_at_screen(
    qreal sx, qreal sy, int viewport_width, int viewport_height, void *camera,
    int local_owner_id) -> CommandResult {
  CommandResult result;
  if ((m_world == nullptr) || (m_selection_system == nullptr) ||
      (m_picking_service == nullptr) || (camera == nullptr)) {
    return result;
  }

  auto *cam = static_cast<Render::GL::Camera *>(camera);
  QVector3D hit;
  if (!Game::Systems::PickingService::screen_to_ground(
          QPointF(sx, sy), *cam, viewport_width, viewport_height, hit)) {
    return result;
  }

  Game::Systems::ProductionService::set_rally_for_first_selected_barracks(
      *m_world, m_selection_system->get_selected_units(), local_owner_id,
      hit.x(), hit.z());

  result.input_consumed = true;
  return result;
}

void CommandController::recruit_near_selected(const QString &unit_type,
                                              int local_owner_id) {
  if ((m_world == nullptr) || (m_selection_system == nullptr)) {
    return;
  }

  const auto &sel = m_selection_system->get_selected_units();
  if (sel.empty()) {
    return;
  }

  auto result = Game::Systems::ProductionService::
      start_production_for_first_selected_barracks(
          *m_world, sel, local_owner_id, unit_type.toStdString());

  if (result == Game::Systems::ProductionResult::GlobalTroopLimitReached) {
    emit troop_limit_reached();
  }
}

void CommandController::reset_movement(Engine::Core::Entity *entity) {
  App::Utils::reset_movement(entity);
}

auto CommandController::any_selected_in_hold_mode() const -> bool {
  if ((m_selection_system == nullptr) || (m_world == nullptr)) {
    return false;
  }

  const auto &selected = m_selection_system->get_selected_units();
  for (Engine::Core::EntityID const entity_id : selected) {
    Engine::Core::Entity *entity = m_world->get_entity(entity_id);
    if (entity == nullptr) {
      continue;
    }

    auto *hold_mode = entity->get_component<Engine::Core::HoldModeComponent>();
    if ((hold_mode != nullptr) && hold_mode->active) {
      return true;
    }
  }

  return false;
}

auto CommandController::any_selected_in_guard_mode() const -> bool {
  if ((m_selection_system == nullptr) || (m_world == nullptr)) {
    return false;
  }

  const auto &selected = m_selection_system->get_selected_units();
  for (Engine::Core::EntityID const entity_id : selected) {
    Engine::Core::Entity *entity = m_world->get_entity(entity_id);
    if (entity == nullptr) {
      continue;
    }

    auto *guard_mode =
        entity->get_component<Engine::Core::GuardModeComponent>();
    if ((guard_mode != nullptr) && guard_mode->active) {
      return true;
    }
  }

  return false;
}

auto CommandController::on_guard_command() -> CommandResult {
  CommandResult result;
  if ((m_selection_system == nullptr) || (m_world == nullptr)) {
    return result;
  }

  const auto &selected = m_selection_system->get_selected_units();
  if (selected.empty()) {
    return result;
  }

  int eligible_count = 0;
  int guard_active_count = 0;

  for (auto id : selected) {
    auto *entity = m_world->get_entity(id);
    if (entity == nullptr) {
      continue;
    }

    auto *unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr) {
      continue;
    }

    if (unit->spawn_type == Game::Units::SpawnType::Barracks) {
      continue;
    }

    eligible_count++;

    auto *guard_mode =
        entity->get_component<Engine::Core::GuardModeComponent>();
    if ((guard_mode != nullptr) && guard_mode->active) {
      guard_active_count++;
    }
  }

  if (eligible_count == 0) {
    return result;
  }

  const bool should_enable_guard = (guard_active_count < eligible_count);

  for (auto id : selected) {
    auto *entity = m_world->get_entity(id);
    if (entity == nullptr) {
      continue;
    }

    auto *unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr) {
      continue;
    }

    if (unit->spawn_type == Game::Units::SpawnType::Barracks) {
      continue;
    }

    auto *guard_mode =
        entity->get_component<Engine::Core::GuardModeComponent>();

    if (should_enable_guard) {

      if (guard_mode == nullptr) {
        guard_mode = entity->add_component<Engine::Core::GuardModeComponent>();
      }
      guard_mode->active = true;
      guard_mode->returning_to_guard_position = false;

      auto *transform =
          entity->get_component<Engine::Core::TransformComponent>();
      if (transform != nullptr) {
        guard_mode->guard_position_x = transform->position.x;
        guard_mode->guard_position_z = transform->position.z;
        guard_mode->has_guard_target = true;
        guard_mode->guarded_entity_id = 0;
      }

      auto *hold_mode =
          entity->get_component<Engine::Core::HoldModeComponent>();
      if ((hold_mode != nullptr) && hold_mode->active) {
        hold_mode->active = false;
      }

      if (auto *patrol =
              entity->get_component<Engine::Core::PatrolComponent>()) {
        patrol->patrolling = false;
        patrol->waypoints.clear();
      }
    } else {

      if ((guard_mode != nullptr) && guard_mode->active) {
        guard_mode->active = false;
        guard_mode->guarded_entity_id = 0;
        guard_mode->guard_position_x = 0.0F;
        guard_mode->guard_position_z = 0.0F;
        guard_mode->returning_to_guard_position = false;
        guard_mode->has_guard_target = false;
      }
    }
  }

  emit guard_mode_changed(should_enable_guard);

  result.input_consumed = true;
  result.reset_cursor_to_normal = true;
  return result;
}

auto CommandController::on_guard_click(qreal sx, qreal sy, int viewport_width,
                                       int viewport_height,
                                       void *camera) -> CommandResult {
  CommandResult result;
  if ((m_selection_system == nullptr) || (m_picking_service == nullptr) ||
      (camera == nullptr) || (m_world == nullptr)) {
    result.reset_cursor_to_normal = true;
    return result;
  }

  const auto &selected = m_selection_system->get_selected_units();
  if (selected.empty()) {
    result.reset_cursor_to_normal = true;
    return result;
  }

  auto *cam = static_cast<Render::GL::Camera *>(camera);
  QVector3D hit;
  if (!Game::Systems::PickingService::screen_to_ground(
          QPointF(sx, sy), *cam, viewport_width, viewport_height, hit)) {
    result.reset_cursor_to_normal = true;
    return result;
  }

  for (auto id : selected) {
    auto *entity = m_world->get_entity(id);
    if (entity == nullptr) {
      continue;
    }

    auto *building = entity->get_component<Engine::Core::BuildingComponent>();
    if (building != nullptr) {
      continue;
    }

    auto *guard_mode =
        entity->get_component<Engine::Core::GuardModeComponent>();
    if (guard_mode == nullptr) {
      guard_mode = entity->add_component<Engine::Core::GuardModeComponent>();
    }

    guard_mode->active = true;
    guard_mode->guarded_entity_id = 0;
    guard_mode->guard_position_x = hit.x();
    guard_mode->guard_position_z = hit.z();
    guard_mode->returning_to_guard_position = false;
    guard_mode->has_guard_target = true;

    auto *hold_mode = entity->get_component<Engine::Core::HoldModeComponent>();
    if ((hold_mode != nullptr) && hold_mode->active) {
      hold_mode->active = false;
    }

    if (auto *patrol = entity->get_component<Engine::Core::PatrolComponent>()) {
      patrol->patrolling = false;
      patrol->waypoints.clear();
    }

    reset_movement(entity);
    entity->remove_component<Engine::Core::AttackTargetComponent>();
  }

  emit guard_mode_changed(true);

  result.input_consumed = true;
  result.reset_cursor_to_normal = true;
  return result;
}

auto CommandController::on_formation_command() -> CommandResult {
  CommandResult result;
  if ((m_selection_system == nullptr) || (m_world == nullptr)) {
    return result;
  }

  const auto &selected = m_selection_system->get_selected_units();
  if (selected.size() <= 1) {
    return result;
  }

  int eligible_count = 0;
  int formation_active_count = 0;

  for (auto id : selected) {
    auto *entity = m_world->get_entity(id);
    if (entity == nullptr) {
      continue;
    }

    auto *unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr) {
      continue;
    }

    if (unit->spawn_type == Game::Units::SpawnType::Barracks) {
      continue;
    }

    eligible_count++;

    auto *formation_mode =
        entity->get_component<Engine::Core::FormationModeComponent>();
    if ((formation_mode != nullptr) && formation_mode->active) {
      formation_active_count++;
    }
  }

  if (eligible_count <= 1) {
    return result;
  }

  const bool should_enable_formation =
      (formation_active_count < eligible_count);

  for (auto id : selected) {
    auto *entity = m_world->get_entity(id);
    if (entity == nullptr) {
      continue;
    }

    auto *unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr) {
      continue;
    }

    if (unit->spawn_type == Game::Units::SpawnType::Barracks) {
      continue;
    }

    auto *formation_mode =
        entity->get_component<Engine::Core::FormationModeComponent>();

    if (should_enable_formation) {

      if (formation_mode == nullptr) {
        formation_mode =
            entity->add_component<Engine::Core::FormationModeComponent>();
      }
      formation_mode->active = true;

      auto *hold_mode =
          entity->get_component<Engine::Core::HoldModeComponent>();
      if ((hold_mode != nullptr) && hold_mode->active) {
        hold_mode->active = false;
      }

      auto *guard_mode =
          entity->get_component<Engine::Core::GuardModeComponent>();
      if ((guard_mode != nullptr) && guard_mode->active) {
        guard_mode->active = false;
      }

      if (auto *patrol =
              entity->get_component<Engine::Core::PatrolComponent>()) {
        patrol->patrolling = false;
        patrol->waypoints.clear();
      }
    } else {

      if ((formation_mode != nullptr) && formation_mode->active) {
        formation_mode->active = false;
      }
    }
  }

  if (should_enable_formation) {
    QVector3D center(0.0F, 0.0F, 0.0F);
    int valid_count = 0;

    m_formation_units.clear();

    for (auto id : selected) {
      auto *entity = m_world->get_entity(id);
      if (entity == nullptr) {
        continue;
      }

      auto *unit = entity->get_component<Engine::Core::UnitComponent>();
      if (unit == nullptr ||
          unit->spawn_type == Game::Units::SpawnType::Barracks) {
        continue;
      }

      m_formation_units.push_back(id);

      auto *transform =
          entity->get_component<Engine::Core::TransformComponent>();
      if (transform != nullptr) {
        center.setX(center.x() + transform->position.x);
        center.setY(center.y() + transform->position.y);
        center.setZ(center.z() + transform->position.z);
        valid_count++;
      }
    }

    if (valid_count > 0) {
      center.setX(center.x() / static_cast<float>(valid_count));
      center.setY(center.y() / static_cast<float>(valid_count));
      center.setZ(center.z() / static_cast<float>(valid_count));

      m_is_placing_formation = true;
      m_formation_placement_position = center;
      m_formation_placement_angle = 0.0F;

      emit formation_placement_started();
      emit formation_placement_updated(m_formation_placement_position,
                                       m_formation_placement_angle);
    }
  }

  result.input_consumed = true;
  result.reset_cursor_to_normal = false;
  return result;
}

bool CommandController::any_selected_in_formation_mode() const {
  if ((m_selection_system == nullptr) || (m_world == nullptr)) {
    return false;
  }

  const auto &selected = m_selection_system->get_selected_units();
  for (auto id : selected) {
    auto *entity = m_world->get_entity(id);
    if (entity == nullptr) {
      continue;
    }

    auto *formation_mode =
        entity->get_component<Engine::Core::FormationModeComponent>();
    if ((formation_mode != nullptr) && formation_mode->active) {
      return true;
    }
  }

  return false;
}

void CommandController::update_formation_placement(const QVector3D &position) {
  if (!m_is_placing_formation) {
    return;
  }
  m_formation_placement_position = position;
  emit formation_placement_updated(m_formation_placement_position,
                                   m_formation_placement_angle);
}

void CommandController::update_formation_rotation(float angle_degrees) {
  if (!m_is_placing_formation) {
    return;
  }
  m_formation_placement_angle = angle_degrees;
  emit formation_placement_updated(m_formation_placement_position,
                                   m_formation_placement_angle);
}

void CommandController::confirm_formation_placement() {
  if (!m_is_placing_formation || m_formation_units.empty()) {
    cancel_formation_placement();
    return;
  }

  auto formation_result =
      Game::Systems::FormationPlanner::get_formation_with_facing(
          *m_world, m_formation_units, m_formation_placement_position,
          Game::GameConfig::instance().gameplay().formation_spacing_default);

  float const angle_rad =
      m_formation_placement_angle * std::numbers::pi_v<float> / 180.0F;
  float const cos_a = std::cos(angle_rad);
  float const sin_a = std::sin(angle_rad);

  for (size_t i = 0; i < m_formation_units.size(); ++i) {

    if (i < formation_result.positions.size()) {
      QVector3D &pos = formation_result.positions[i];
      float const dx = pos.x() - m_formation_placement_position.x();
      float const dz = pos.z() - m_formation_placement_position.z();

      float const rotated_x = dx * cos_a - dz * sin_a;
      float const rotated_z = dx * sin_a + dz * cos_a;

      pos.setX(m_formation_placement_position.x() + rotated_x);
      pos.setZ(m_formation_placement_position.z() + rotated_z);
    }

    auto *entity = m_world->get_entity(m_formation_units[i]);
    if (entity == nullptr) {
      continue;
    }
    auto *transform = entity->get_component<Engine::Core::TransformComponent>();
    if (transform != nullptr) {

      float unit_facing = (i < formation_result.facing_angles.size())
                              ? formation_result.facing_angles[i]
                              : 0.0F;
      transform->desired_yaw = unit_facing + m_formation_placement_angle;
      transform->has_desired_yaw = true;
    }
  }

  Game::Systems::CommandService::MoveOptions opts;
  opts.group_move = m_formation_units.size() > 1;
  opts.clear_attack_intent = true;
  Game::Systems::CommandService::move_units(*m_world, m_formation_units,
                                            formation_result.positions, opts);

  m_is_placing_formation = false;
  m_formation_units.clear();
  emit formation_placement_ended();
  emit formation_mode_changed(true);
}

void CommandController::cancel_formation_placement() {
  if (!m_is_placing_formation) {
    return;
  }

  for (auto id : m_formation_units) {
    auto *entity = m_world->get_entity(id);
    if (entity == nullptr) {
      continue;
    }
    auto *formation_mode =
        entity->get_component<Engine::Core::FormationModeComponent>();
    if (formation_mode != nullptr) {
      formation_mode->active = false;
    }
  }

  m_is_placing_formation = false;
  m_formation_units.clear();
  emit formation_placement_ended();
  emit formation_mode_changed(false);
}

auto CommandController::on_run_command() -> CommandResult {
  CommandResult result;
  if (m_selection_system == nullptr || m_world == nullptr) {
    return result;
  }

  const auto &selected = m_selection_system->get_selected_units();
  if (selected.empty()) {
    return result;
  }

  struct UnitRunState {
    Engine::Core::Entity *entity;
    Engine::Core::StaminaComponent *stamina;
    Game::Systems::NationID nation_id;
    Game::Units::SpawnType spawn_type;
  };
  std::vector<UnitRunState> eligible_units;
  eligible_units.reserve(selected.size());

  int run_active_count = 0;

  for (const auto id : selected) {
    auto *entity = m_world->get_entity(id);
    if (entity == nullptr) {
      continue;
    }

    const auto *unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr || !Game::Units::can_use_run_mode(unit->spawn_type)) {
      continue;
    }

    auto *stamina = entity->get_component<Engine::Core::StaminaComponent>();
    const bool is_active = stamina != nullptr && stamina->run_requested;
    run_active_count += is_active ? 1 : 0;

    eligible_units.push_back(
        {entity, stamina, unit->nation_id, unit->spawn_type});
  }

  if (eligible_units.empty()) {
    return result;
  }

  const bool should_enable_run =
      run_active_count < static_cast<int>(eligible_units.size());

  for (auto &[entity, stamina, nation_id, spawn_type] : eligible_units) {
    if (should_enable_run) {
      if (stamina == nullptr) {
        stamina = entity->add_component<Engine::Core::StaminaComponent>();

        const auto troop_type = Game::Units::spawn_typeToTroopType(spawn_type);
        if (troop_type.has_value()) {
          const auto profile =
              Game::Systems::TroopProfileService::instance().get_profile(
                  nation_id, *troop_type);
          stamina->initialize_from_stats(profile.combat.max_stamina,
                                         profile.combat.stamina_regen_rate,
                                         profile.combat.stamina_depletion_rate);
        }
      }
      stamina->run_requested = true;
    } else if (stamina != nullptr) {
      stamina->run_requested = false;
      stamina->is_running = false;
    }
  }

  emit run_mode_changed(should_enable_run);

  result.input_consumed = true;
  result.reset_cursor_to_normal = true;
  return result;
}

auto CommandController::any_selected_in_run_mode() const -> bool {
  if (m_selection_system == nullptr || m_world == nullptr) {
    return false;
  }

  for (const auto id : m_selection_system->get_selected_units()) {
    const auto *entity = m_world->get_entity(id);
    if (entity == nullptr) {
      continue;
    }

    const auto *stamina =
        entity->get_component<Engine::Core::StaminaComponent>();
    if (stamina != nullptr && stamina->run_requested) {
      return true;
    }
  }

  return false;
}

void CommandController::enable_run_mode_for_selected() {
  if (m_selection_system == nullptr || m_world == nullptr) {
    return;
  }

  const auto &selected = m_selection_system->get_selected_units();
  if (selected.empty()) {
    return;
  }

  for (const auto id : selected) {
    auto *entity = m_world->get_entity(id);
    if (entity == nullptr) {
      continue;
    }

    const auto *unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr || !Game::Units::can_use_run_mode(unit->spawn_type)) {
      continue;
    }

    auto *stamina = entity->get_component<Engine::Core::StaminaComponent>();
    if (stamina == nullptr) {
      stamina = entity->add_component<Engine::Core::StaminaComponent>();

      const auto troop_type =
          Game::Units::spawn_typeToTroopType(unit->spawn_type);
      if (troop_type.has_value()) {
        const auto profile =
            Game::Systems::TroopProfileService::instance().get_profile(
                unit->nation_id, *troop_type);
        stamina->initialize_from_stats(profile.combat.max_stamina,
                                       profile.combat.stamina_regen_rate,
                                       profile.combat.stamina_depletion_rate);
      }
    }
    stamina->run_requested = true;
  }

  emit run_mode_changed(true);
}

void CommandController::disable_run_mode_for_selected() {
  if (m_selection_system == nullptr || m_world == nullptr) {
    return;
  }

  const auto &selected = m_selection_system->get_selected_units();
  if (selected.empty()) {
    return;
  }

  for (const auto id : selected) {
    auto *entity = m_world->get_entity(id);
    if (entity == nullptr) {
      continue;
    }

    auto *stamina = entity->get_component<Engine::Core::StaminaComponent>();
    if (stamina != nullptr) {
      stamina->run_requested = false;
      stamina->is_running = false;
    }
  }

  emit run_mode_changed(false);
}

} // namespace App::Controllers
