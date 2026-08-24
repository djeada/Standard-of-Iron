#include "app/orders/command_controller.h"

#include <QCoreApplication>
#include <QDebug>
#include <QPointF>
#include <qglobal.h>
#include <qobject.h>
#include <qtmetamacros.h>
#include <qvectornd.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include "app/orders/army_formation_controller.h"
#include "app/orders/movement_utils.h"
#include "app/orders/order_issuer.h"
#include "app/orders/order_submission.h"
#include "app/orders/rts_action_model.h"
#include "game/audio/audio_cues.h"
#include "game/command/command.h"
#include "game/command/command_queue.h"
#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/core/world.h"
#include "game/formation/army_formation_registry.h"
#include "game/formation/army_formation_service.h"
#include "game/formation/formation_doctrine.h"
#include "game/game_config.h"
#include "game/render_bridge/picking_service.h"
#include "game/session/session_context.h"
#include "game/systems/combat_rules.h"
#include "game/systems/command_service.h"
#include "game/systems/food_targets.h"
#include "game/systems/owner_registry.h"
#include "game/systems/production_service.h"
#include "game/systems/selection_system.h"
#include "game/systems/troop_profile_service.h"
#include "game/units/spawn_type.h"
#include "game/util/asset_text.h"
#include "scene/camera.h"

namespace App::Controllers {

namespace {

void submit(Engine::Core::World* world, Game::Command::Payload payload) {
  Game::Command::submit(*world,
                        Game::Command::Source::LocalPlayer,
                        App::Orders::local_owner(world),
                        std::move(payload));
}

} // namespace

CommandController::CommandController(Engine::Core::World* world,
                                     Game::Systems::SelectionSystem* selection_system,
                                     Game::Systems::PickingService* picking_service,
                                     QObject* parent)
    : QObject(parent)
    , m_world(world)
    , m_selection_system(selection_system)
    , m_picking_service(picking_service)
    , m_orders(world,
               [this](const App::Core::OrderOutcome& outcome) {
                 emit order_feedback(outcome);
               })
    , m_formation(
          world, selection_system, [this](const App::Core::OrderOutcome& outcome) {
            emit order_feedback(outcome);
          }) {
  connect(&m_formation,
          &ArmyFormationController::formation_mode_changed,
          this,
          &CommandController::formation_mode_changed);
  connect(&m_formation,
          &ArmyFormationController::formation_placement_started,
          this,
          &CommandController::formation_placement_started);
  connect(&m_formation,
          &ArmyFormationController::formation_placement_updated,
          this,
          &CommandController::formation_placement_updated);
  connect(&m_formation,
          &ArmyFormationController::formation_placement_ended,
          this,
          &CommandController::formation_placement_ended);
  connect(&m_formation,
          &ArmyFormationController::formation_placement_rejected,
          this,
          &CommandController::formation_placement_rejected);
  connect(&m_formation,
          &ArmyFormationController::formation_preview_changed,
          this,
          &CommandController::formation_preview_changed);
}

auto CommandController::on_attack_click(qreal sx,
                                        qreal sy,
                                        int viewport_width,
                                        int viewport_height,
                                        void* camera) -> CommandResult {
  using App::Core::OrderKind;
  CommandResult result;
  result.reset_cursor_to_normal = true;
  if ((m_selection_system == nullptr) || (m_picking_service == nullptr) ||
      (camera == nullptr) || (m_world == nullptr)) {
    return result;
  }

  const auto& selected = m_selection_system->get_selected_units();
  if (selected.empty()) {
    result.order = m_orders.reject(OrderKind::Attack, App::Core::no_selection_reason());
    return result;
  }

  auto* cam = static_cast<Render::GL::Camera*>(camera);
  Engine::Core::EntityID const target_id =
      Game::Systems::PickingService::pick_unit_first(
          float(sx), float(sy), *m_world, *cam, viewport_width, viewport_height, 0);

  auto* target_entity = target_id != 0 ? m_world->get_entity(target_id) : nullptr;
  auto* target_unit = target_entity != nullptr
                          ? target_entity->get_component<Engine::Core::UnitComponent>()
                          : nullptr;
  if (target_unit == nullptr) {
    QVector3D hit;
    if (Game::Systems::PickingService::screen_to_ground(
            QPointF(sx, sy), *cam, viewport_width, viewport_height, hit)) {
      result.order = m_orders.reject_at(
          OrderKind::Attack,
          App::Core::no_target_under_cursor_reason(OrderKind::Attack),
          hit);
    } else {
      result.order =
          m_orders.reject(OrderKind::Attack,
                          App::Core::no_target_under_cursor_reason(OrderKind::Attack));
    }
    return result;
  }

  auto const attackers = App::Core::filter_selected_units_for_action(
      m_world, selected, QStringLiteral("attack"));
  if (attackers.empty()) {
    result.order =
        m_orders.reject_on(OrderKind::Attack,
                           App::Core::no_eligible_units_reason(OrderKind::Attack),
                           target_id);
    return result;
  }

  result.order = m_orders.issue(
      OrderKind::Attack,
      Game::Command::AttackTarget{.units = attackers, .target = target_id},
      target_id);
  result.input_consumed = result.order.accepted();
  return result;
}

auto CommandController::on_attack_press(qreal sx,
                                        qreal sy,
                                        int viewport_width,
                                        int viewport_height,
                                        void* camera,
                                        int local_owner_id) -> CommandResult {
  CommandResult result;
  if ((m_selection_system == nullptr) || (m_picking_service == nullptr) ||
      (camera == nullptr) || (m_world == nullptr)) {
    return result;
  }

  const auto& selected = m_selection_system->get_selected_units();
  if (selected.empty()) {
    return result;
  }

  auto* cam = static_cast<Render::GL::Camera*>(camera);
  Engine::Core::EntityID const target_id = App::Utils::pick_enemy_unit_at_screen(
      m_world, cam, sx, sy, viewport_width, viewport_height, local_owner_id);
  if (target_id == 0U) {
    return result;
  }

  disable_run_mode_for_selected();
  result.order = m_orders.publish(
      App::Utils::issue_attack_command(m_world, selected, target_id, local_owner_id));
  result.input_consumed = true;
  return result;
}

auto CommandController::on_move_or_attack_click(qreal sx,
                                                qreal sy,
                                                int viewport_width,
                                                int viewport_height,
                                                void* camera,
                                                int local_owner_id) -> CommandResult {
  CommandResult result;
  if ((m_selection_system == nullptr) || (m_picking_service == nullptr) ||
      (camera == nullptr) || (m_world == nullptr)) {
    return result;
  }

  const auto& selected = m_selection_system->get_selected_units();
  if (selected.empty()) {
    return result;
  }

  auto* cam = static_cast<Render::GL::Camera*>(camera);
  result.order =
      m_orders.publish(App::Utils::issue_move_or_attack_command(m_world,
                                                                selected,
                                                                m_picking_service,
                                                                cam,
                                                                sx,
                                                                sy,
                                                                viewport_width,
                                                                viewport_height,
                                                                local_owner_id));
  result.input_consumed = result.order.issued();
  return result;
}

auto CommandController::on_minimap_move(const QVector3D& world_target,
                                        int local_owner_id) -> CommandResult {
  CommandResult result;
  if ((m_selection_system == nullptr) || (m_world == nullptr)) {
    return result;
  }
  const auto& selected = m_selection_system->get_selected_units();
  if (selected.empty()) {
    return result;
  }

  result.order = m_orders.publish(
      App::Utils::submit_ground_move(*m_world, selected, world_target, local_owner_id));
  result.input_consumed = result.order.accepted();
  return result;
}

auto CommandController::on_stop_command() -> CommandResult {
  CommandResult result;
  if ((m_selection_system == nullptr) || (m_world == nullptr)) {
    return result;
  }

  const auto& selected = m_selection_system->get_selected_units();
  if (selected.empty()) {
    result.order =
        m_orders.reject(App::Core::OrderKind::Stop, App::Core::no_selection_reason());
    return result;
  }

  bool had_hold_mode = false;
  bool had_active_formation = false;
  for (auto id : selected) {
    auto* entity = m_world->get_entity(id);
    if (entity == nullptr) {
      continue;
    }
    had_hold_mode = had_hold_mode ||
                    entity->get_component<Engine::Core::HoldModeComponent>() != nullptr;
    const auto* formation_mode =
        entity->get_component<Engine::Core::FormationModeComponent>();
    had_active_formation =
        had_active_formation || (formation_mode != nullptr && formation_mode->active);
  }

  result.order =
      m_orders.issue(App::Core::OrderKind::Stop,
                     Game::Command::Stop{.units = {selected.begin(), selected.end()}});
  if (!result.order.accepted()) {
    return result;
  }

  if (had_hold_mode) {
    emit hold_mode_changed(false);
  }
  if (had_active_formation) {
    emit formation_mode_changed(false);
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

  const auto& selected = m_selection_system->get_selected_units();
  if (selected.empty()) {
    result.order =
        m_orders.reject(App::Core::OrderKind::Hold, App::Core::no_selection_reason());
    return result;
  }

  int eligible_count = 0;
  int hold_active_count = 0;

  for (auto id : selected) {
    auto* entity = m_world->get_entity(id);
    if (entity == nullptr) {
      continue;
    }

    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr) {
      continue;
    }

    if (!Game::Units::can_use_hold_mode(unit->spawn_type)) {
      continue;
    }

    eligible_count++;

    auto* hold_mode = entity->get_component<Engine::Core::HoldModeComponent>();
    if ((hold_mode != nullptr) && hold_mode->active) {
      hold_active_count++;
    }
  }

  if (eligible_count == 0) {
    result.order = m_orders.reject(
        App::Core::OrderKind::Hold,
        App::Core::no_eligible_units_reason(App::Core::OrderKind::Hold));
    return result;
  }

  const bool should_enable_hold = (hold_active_count < eligible_count);

  result.order =
      m_orders.issue(App::Core::OrderKind::Hold,
                     Game::Command::SetHold{.units = {selected.begin(), selected.end()},
                                            .active = should_enable_hold});
  if (!result.order.accepted()) {
    return result;
  }

  emit hold_mode_changed(should_enable_hold);

  result.input_consumed = true;
  result.reset_cursor_to_normal = true;
  return result;
}

auto CommandController::on_auto_gather_command(const QString& priority_product_type)
    -> CommandResult {
  CommandResult result;
  if ((m_selection_system == nullptr) || (m_world == nullptr)) {
    return result;
  }

  auto const builders = App::Core::filter_selected_units_for_action(
      m_world, m_selection_system->get_selected_units(), QStringLiteral("auto_gather"));
  if (builders.empty()) {
    result.order = m_orders.reject(
        App::Core::OrderKind::Gather,
        m_selection_system->get_selected_units().empty()
            ? App::Core::no_selection_reason()
            : App::Core::no_eligible_units_reason(App::Core::OrderKind::Gather));
    return result;
  }

  int already_gathering = 0;
  for (auto const id : builders) {
    auto* entity = m_world->get_entity(id);
    const auto* builder =
        entity != nullptr
            ? entity->get_component<Engine::Core::BuilderProductionComponent>()
            : nullptr;
    already_gathering += (builder != nullptr && builder->auto_gather) ? 1 : 0;
  }

  const bool should_enable = already_gathering < static_cast<int>(builders.size());

  return issue_auto_gather(builders, should_enable, priority_product_type);
}

auto CommandController::set_auto_gather(
    bool active, const QString& priority_product_type) -> CommandResult {
  CommandResult result;
  if ((m_selection_system == nullptr) || (m_world == nullptr)) {
    return result;
  }

  auto const builders = App::Core::filter_selected_units_for_action(
      m_world, m_selection_system->get_selected_units(), QStringLiteral("auto_gather"));
  if (builders.empty()) {
    result.order = m_orders.reject(
        App::Core::OrderKind::Gather,
        m_selection_system->get_selected_units().empty()
            ? App::Core::no_selection_reason()
            : App::Core::no_eligible_units_reason(App::Core::OrderKind::Gather));
    return result;
  }

  return issue_auto_gather(builders, active, priority_product_type);
}

auto CommandController::issue_auto_gather(
    const std::vector<Engine::Core::EntityID>& builders,
    bool active,
    const QString& priority_product_type) -> CommandResult {
  CommandResult result;

  result.order =
      m_orders.issue(App::Core::OrderKind::Gather,
                     Game::Command::SetAutoGather{
                         .units = builders,
                         .active = active,
                         .priority_product_type = priority_product_type.toStdString()});
  if (!result.order.accepted()) {
    return result;
  }

  emit auto_gather_changed(active);

  result.input_consumed = true;
  result.reset_cursor_to_normal = true;
  return result;
}

namespace {

auto gate_mode_name(Engine::Core::GateComponent::ManualMode mode) -> QString {
  switch (mode) {
  case Engine::Core::GateComponent::ManualMode::ForcedOpen:
    return QStringLiteral("open");
  case Engine::Core::GateComponent::ManualMode::ForcedClosed:
    return QStringLiteral("closed");
  case Engine::Core::GateComponent::ManualMode::Automatic:
    break;
  }
  return QStringLiteral("auto");
}

} // namespace

auto CommandController::on_gate_command() -> CommandResult {
  CommandResult result;
  if ((m_selection_system == nullptr) || (m_world == nullptr)) {
    return result;
  }

  auto const gates = App::Core::filter_selected_units_for_action(
      m_world, m_selection_system->get_selected_units(), QStringLiteral("gate"));
  if (gates.empty()) {
    return result;
  }

  auto next_mode = Engine::Core::GateComponent::ManualMode::ForcedOpen;
  if (auto* first = m_world->get_entity(gates.front())) {
    if (const auto* gate = first->get_component<Engine::Core::GateComponent>()) {
      switch (gate->manual_mode) {
      case Engine::Core::GateComponent::ManualMode::Automatic:
        next_mode = Engine::Core::GateComponent::ManualMode::ForcedOpen;
        break;
      case Engine::Core::GateComponent::ManualMode::ForcedOpen:
        next_mode = Engine::Core::GateComponent::ManualMode::ForcedClosed;
        break;
      case Engine::Core::GateComponent::ManualMode::ForcedClosed:
        next_mode = Engine::Core::GateComponent::ManualMode::Automatic;
        break;
      }
    }
  }

  submit(m_world, Game::Command::SetGateMode{.units = gates, .mode = next_mode});

  emit gate_mode_changed(gate_mode_name(next_mode));

  result.input_consumed = true;
  result.reset_cursor_to_normal = true;
  return result;
}

auto CommandController::on_patrol_click(qreal sx,
                                        qreal sy,
                                        int viewport_width,
                                        int viewport_height,
                                        void* camera) -> CommandResult {
  CommandResult result;
  if ((m_selection_system == nullptr) || (m_world == nullptr) ||
      (m_picking_service == nullptr) || (camera == nullptr)) {
    if (m_has_patrol_first_waypoint) {
      clear_patrol_first_waypoint();
      result.reset_cursor_to_normal = true;
    }
    return result;
  }

  const auto& selected = m_selection_system->get_selected_units();
  if (selected.empty()) {
    if (m_has_patrol_first_waypoint) {
      clear_patrol_first_waypoint();
      result.reset_cursor_to_normal = true;
    }
    result.order =
        m_orders.reject(App::Core::OrderKind::Patrol, App::Core::no_selection_reason());
    return result;
  }

  auto const patrol_units = App::Core::filter_selected_units_for_action(
      m_world, selected, QStringLiteral("patrol"));
  if (patrol_units.empty()) {
    if (m_has_patrol_first_waypoint) {
      clear_patrol_first_waypoint();
    }
    result.order = m_orders.reject(
        App::Core::OrderKind::Patrol,
        App::Core::no_eligible_units_reason(App::Core::OrderKind::Patrol));
    result.reset_cursor_to_normal = true;
    return result;
  }

  auto* cam = static_cast<Render::GL::Camera*>(camera);
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

  result.order = m_orders.issue(
      App::Core::OrderKind::Patrol,
      Game::Command::Patrol{.units = {patrol_units.begin(), patrol_units.end()},
                            .first_waypoint = m_patrol_first_waypoint,
                            .second_waypoint = second_waypoint},
      0,
      &second_waypoint);

  clear_patrol_first_waypoint();
  result.input_consumed = result.order.accepted();
  result.reset_cursor_to_normal = true;
  return result;
}

auto CommandController::set_rally_at_screen(qreal sx,
                                            qreal sy,
                                            int viewport_width,
                                            int viewport_height,
                                            void* camera,
                                            int local_owner_id) -> CommandResult {
  CommandResult result;
  if ((m_world == nullptr) || (m_selection_system == nullptr) ||
      (m_picking_service == nullptr) || (camera == nullptr)) {
    return result;
  }

  auto* cam = static_cast<Render::GL::Camera*>(camera);
  QVector3D hit;
  if (!Game::Systems::PickingService::screen_to_ground(
          QPointF(sx, sy), *cam, viewport_width, viewport_height, hit)) {
    return result;
  }

  const auto& selected = m_selection_system->get_selected_units();
  auto barracks = Game::Systems::ProductionService::find_selected_barracks(
      *m_world, selected, local_owner_id);
  if (barracks == Engine::Core::NULL_ENTITY) {
    barracks = Game::Systems::ProductionService::find_selected_temple(
        *m_world, selected, local_owner_id);
  }
  if (barracks != Engine::Core::NULL_ENTITY) {
    App::Core::OrderRequest request;
    request.kind = App::Core::OrderKind::Rally;
    request.payload =
        Game::Command::SetRallyPoint{.building = barracks, .position = hit};
    request.target = barracks;
    request.has_destination = true;
    request.destination = hit;
    result.order = m_orders.publish(
        App::Core::submit_player_order(*m_world, local_owner_id, std::move(request)));
  }

  result.input_consumed = true;
  return result;
}

void CommandController::recruit_near_selected(const QString& unit_type,
                                              int local_owner_id) {
  if ((m_world == nullptr) || (m_selection_system == nullptr)) {
    return;
  }

  const auto& sel = m_selection_system->get_selected_units();
  if (sel.empty()) {
    return;
  }

  const auto product = Game::Units::troop_typeFromString(unit_type.toStdString());

  Engine::Core::EntityID building = Engine::Core::NULL_ENTITY;
  switch (Game::Systems::recruiting_building_for(product)) {
  case Game::Units::SpawnType::Home:
    building = Game::Systems::ProductionService::find_selected_home(
        *m_world, sel, local_owner_id);
    break;
  case Game::Units::SpawnType::Temple:
    building = Game::Systems::ProductionService::find_selected_temple(
        *m_world, sel, local_owner_id);
    break;
  default:
    building = Game::Systems::ProductionService::find_selected_barracks(
        *m_world, sel, local_owner_id);
    break;
  }
  if (building == Engine::Core::NULL_ENTITY) {
    return;
  }

  const auto ruling = Game::Systems::ProductionService::can_start_production(
      *m_world, building, product);
  if (ruling == Game::Systems::ProductionResult::GlobalTroopLimitReached) {
    emit troop_limit_reached();
    return;
  }
  if (ruling == Game::Systems::ProductionResult::InsufficientManpower) {
    emit insufficient_manpower();
    return;
  }
  if (ruling == Game::Systems::ProductionResult::InsufficientResources) {
    emit insufficient_resources(
        tr("Not enough wood, stone, or iron to recruit this unit."));
    return;
  }
  if (ruling != Game::Systems::ProductionResult::Success) {
    return;
  }
  submit(m_world, Game::Command::Produce{.building = building, .product = product});
}

void CommandController::reset_transient_state() {
  m_has_patrol_first_waypoint = false;
  m_patrol_first_waypoint = QVector3D();
  m_formation.reset_transient_state();
}

auto CommandController::any_selected_in_hold_mode() const -> bool {
  if ((m_selection_system == nullptr) || (m_world == nullptr)) {
    return false;
  }

  const auto& selected = m_selection_system->get_selected_units();
  for (Engine::Core::EntityID const entity_id : selected) {
    Engine::Core::Entity* entity = m_world->get_entity(entity_id);
    if (entity == nullptr) {
      continue;
    }

    auto* hold_mode = entity->get_component<Engine::Core::HoldModeComponent>();
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

  const auto& selected = m_selection_system->get_selected_units();
  for (Engine::Core::EntityID const entity_id : selected) {
    Engine::Core::Entity* entity = m_world->get_entity(entity_id);
    if (entity == nullptr) {
      continue;
    }

    auto* guard_mode = entity->get_component<Engine::Core::GuardModeComponent>();
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

  const auto& selected = m_selection_system->get_selected_units();
  if (selected.empty()) {
    result.order =
        m_orders.reject(App::Core::OrderKind::Guard, App::Core::no_selection_reason());
    return result;
  }

  int eligible_count = 0;
  int guard_active_count = 0;

  for (auto id : selected) {
    auto* entity = m_world->get_entity(id);
    if (entity == nullptr) {
      continue;
    }

    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr) {
      continue;
    }

    if (!Game::Units::can_use_guard_mode(unit->spawn_type)) {
      continue;
    }

    eligible_count++;

    auto* guard_mode = entity->get_component<Engine::Core::GuardModeComponent>();
    if ((guard_mode != nullptr) && guard_mode->active) {
      guard_active_count++;
    }
  }

  if (eligible_count == 0) {
    result.order = m_orders.reject(
        App::Core::OrderKind::Guard,
        App::Core::no_eligible_units_reason(App::Core::OrderKind::Guard));
    return result;
  }

  const bool should_enable_guard = (guard_active_count < eligible_count);

  result.order = m_orders.issue(
      App::Core::OrderKind::Guard,
      Game::Command::SetGuard{.units = {selected.begin(), selected.end()},
                              .active = should_enable_guard});
  if (!result.order.accepted()) {
    return result;
  }

  emit guard_mode_changed(should_enable_guard);

  result.input_consumed = true;
  result.reset_cursor_to_normal = true;
  return result;
}

auto CommandController::on_guard_click(qreal sx,
                                       qreal sy,
                                       int viewport_width,
                                       int viewport_height,
                                       void* camera) -> CommandResult {
  CommandResult result;
  if ((m_selection_system == nullptr) || (m_picking_service == nullptr) ||
      (camera == nullptr) || (m_world == nullptr)) {
    result.reset_cursor_to_normal = true;
    return result;
  }

  const auto& selected = m_selection_system->get_selected_units();
  if (selected.empty()) {
    result.order =
        m_orders.reject(App::Core::OrderKind::Guard, App::Core::no_selection_reason());
    result.reset_cursor_to_normal = true;
    return result;
  }

  auto const guard_units = App::Core::filter_selected_units_for_action(
      m_world, selected, QStringLiteral("guard"));
  if (guard_units.empty()) {
    result.order = m_orders.reject(
        App::Core::OrderKind::Guard,
        App::Core::no_eligible_units_reason(App::Core::OrderKind::Guard));
    result.reset_cursor_to_normal = true;
    return result;
  }

  auto* cam = static_cast<Render::GL::Camera*>(camera);
  QVector3D hit;
  if (!Game::Systems::PickingService::screen_to_ground(
          QPointF(sx, sy), *cam, viewport_width, viewport_height, hit)) {
    result.order = m_orders.reject(App::Core::OrderKind::Guard,
                                   App::Core::no_ground_under_cursor_reason());
    result.reset_cursor_to_normal = true;
    return result;
  }

  result.order = m_orders.issue(
      App::Core::OrderKind::Guard,
      Game::Command::SetGuard{
          .units = guard_units, .active = true, .anchor = hit, .has_anchor = true},
      0,
      &hit);
  result.reset_cursor_to_normal = true;
  if (!result.order.accepted()) {
    return result;
  }

  emit guard_mode_changed(true);

  result.input_consumed = true;
  return result;
}

auto CommandController::on_civilian_delivery_click(qreal sx,
                                                   qreal sy,
                                                   int viewport_width,
                                                   int viewport_height,
                                                   void* camera,
                                                   int local_owner_id)
    -> CommandResult {
  CommandResult result;
  if ((m_selection_system == nullptr) || (m_picking_service == nullptr) ||
      (camera == nullptr) || (m_world == nullptr)) {
    result.reset_cursor_to_normal = true;
    return result;
  }

  const auto& selected = m_selection_system->get_selected_units();
  if (selected.empty()) {
    result.order = m_orders.reject(App::Core::OrderKind::Deliver,
                                   App::Core::no_selection_reason());
    result.reset_cursor_to_normal = true;
    return result;
  }

  auto* cam = static_cast<Render::GL::Camera*>(camera);
  result.order =
      m_orders.publish(App::Utils::issue_civilian_delivery_command(m_world,
                                                                   selected,
                                                                   m_picking_service,
                                                                   cam,
                                                                   sx,
                                                                   sy,
                                                                   viewport_width,
                                                                   viewport_height,
                                                                   local_owner_id));
  result.input_consumed = result.order.accepted();
  result.reset_cursor_to_normal = true;
  return result;
}

auto CommandController::start_food_harvest(Engine::Core::EntityID target,
                                           const QString& product_type,
                                           int local_owner_id) -> CommandResult {
  CommandResult result;
  if (m_selection_system == nullptr || m_world == nullptr || target == 0 ||
      product_type.isEmpty()) {
    return result;
  }

  const auto& selected = m_selection_system->get_selected_units();
  if (selected.empty()) {
    result.order =
        m_orders.reject(App::Core::OrderKind::Gather, App::Core::no_selection_reason());
    return result;
  }

  std::vector<Engine::Core::EntityID> crew;
  crew.reserve(selected.size());
  for (auto const id : selected) {
    if (m_world->try_get<Engine::Core::BuilderProductionComponent>(id) != nullptr) {
      crew.push_back(id);
    }
  }
  if (crew.empty()) {
    result.order = m_orders.reject(
        App::Core::OrderKind::Gather,
        App::Core::no_eligible_units_reason(App::Core::OrderKind::Gather));
    return result;
  }

  auto const food_target =
      Game::Systems::resolve_food_target(*m_world, target, local_owner_id);
  if (!food_target.has_value() ||
      QString::fromLatin1(food_target->product_type.data(),
                          static_cast<qsizetype>(food_target->product_type.size())) !=
          product_type) {
    result.order = m_orders.reject(
        App::Core::OrderKind::Gather,
        App::Core::no_target_under_cursor_reason(App::Core::OrderKind::Gather));
    return result;
  }
  if (Game::Systems::food_target_claimed(*m_world, target)) {
    result.order =
        m_orders.reject(App::Core::OrderKind::Gather, App::Core::unit_busy_reason());
    return result;
  }

  QVector3D const site(food_target->x, 0.0F, food_target->z);
  App::Core::OrderRequest request;
  request.kind = App::Core::OrderKind::Gather;
  request.payload =
      Game::Command::StartHarvest{.units = std::move(crew),
                                  .construction_type = product_type.toStdString(),
                                  .resource_target = target,
                                  .site = site};
  request.has_destination = true;
  request.destination = site;
  result.order = m_orders.publish(
      App::Core::submit_player_order(*m_world, local_owner_id, std::move(request)));
  result.input_consumed = result.order.accepted();
  return result;
}

auto CommandController::on_builder_repair_click(qreal sx,
                                                qreal sy,
                                                int viewport_width,
                                                int viewport_height,
                                                void* camera,
                                                int local_owner_id) -> CommandResult {
  CommandResult result;
  if ((m_selection_system == nullptr) || (m_picking_service == nullptr) ||
      (camera == nullptr) || (m_world == nullptr)) {
    result.reset_cursor_to_normal = true;
    return result;
  }

  const auto& selected = m_selection_system->get_selected_units();
  if (selected.empty()) {
    result.order =
        m_orders.reject(App::Core::OrderKind::Repair, App::Core::no_selection_reason());
    result.reset_cursor_to_normal = true;
    return result;
  }

  auto* cam = static_cast<Render::GL::Camera*>(camera);
  result.order =
      m_orders.publish(App::Utils::issue_builder_repair_command(m_world,
                                                                selected,
                                                                m_picking_service,
                                                                cam,
                                                                sx,
                                                                sy,
                                                                viewport_width,
                                                                viewport_height,
                                                                local_owner_id));
  result.input_consumed = result.order.accepted();
  result.reset_cursor_to_normal = result.input_consumed;
  return result;
}

auto CommandController::on_builder_dismantle_click(qreal sx,
                                                   qreal sy,
                                                   int viewport_width,
                                                   int viewport_height,
                                                   void* camera,
                                                   int local_owner_id)
    -> CommandResult {
  CommandResult result;
  if ((m_selection_system == nullptr) || (m_picking_service == nullptr) ||
      (camera == nullptr) || (m_world == nullptr)) {
    result.reset_cursor_to_normal = true;
    return result;
  }

  const auto& selected = m_selection_system->get_selected_units();
  if (selected.empty()) {
    result.order =
        m_orders.reject(App::Core::OrderKind::Build, App::Core::no_selection_reason());
    result.reset_cursor_to_normal = true;
    return result;
  }

  auto* cam = static_cast<Render::GL::Camera*>(camera);
  result.order =
      m_orders.publish(App::Utils::issue_builder_dismantle_command(m_world,
                                                                   selected,
                                                                   m_picking_service,
                                                                   cam,
                                                                   sx,
                                                                   sy,
                                                                   viewport_width,
                                                                   viewport_height,
                                                                   local_owner_id));
  result.input_consumed = result.order.accepted();
  result.reset_cursor_to_normal = result.input_consumed;
  return result;
}

auto CommandController::on_run_command() -> CommandResult {
  CommandResult result;
  if (m_selection_system == nullptr || m_world == nullptr) {
    return result;
  }

  const auto& selected = m_selection_system->get_selected_units();
  if (selected.empty()) {
    return result;
  }

  struct UnitRunState {
    Engine::Core::Entity* entity;
    Engine::Core::StaminaComponent* stamina;
    Game::Systems::NationID nation_id;
    Game::Units::SpawnType spawn_type;
  };
  std::vector<UnitRunState> eligible_units;
  eligible_units.reserve(selected.size());

  int run_active_count = 0;

  for (const auto id : selected) {
    auto* entity = m_world->get_entity(id);
    if (entity == nullptr) {
      continue;
    }

    const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr || !Game::Units::can_use_run_mode(unit->spawn_type)) {
      continue;
    }

    auto* stamina = entity->get_component<Engine::Core::StaminaComponent>();
    const bool is_active = stamina != nullptr && stamina->run_requested;
    run_active_count += is_active ? 1 : 0;

    eligible_units.push_back({entity, stamina, unit->nation_id, unit->spawn_type});
  }

  if (eligible_units.empty()) {
    return result;
  }

  const bool should_enable_run =
      run_active_count < static_cast<int>(eligible_units.size());

  for (auto& [entity, stamina, nation_id, spawn_type] : eligible_units) {
    if (should_enable_run) {
      if (stamina == nullptr) {
        stamina = entity->add_component<Engine::Core::StaminaComponent>();

        const auto troop_type = Game::Units::spawn_typeToTroopType(spawn_type);
        if (troop_type.has_value()) {
          const auto profile =
              Game::Systems::TroopProfileService::instance().get_profile(nation_id,
                                                                         *troop_type);
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
    const auto* entity = m_world->get_entity(id);
    if (entity == nullptr) {
      continue;
    }

    const auto* stamina = entity->get_component<Engine::Core::StaminaComponent>();
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

  const auto& selected = m_selection_system->get_selected_units();
  if (selected.empty()) {
    return;
  }

  submit(m_world,
         Game::Command::SetRunMode{.units = {selected.begin(), selected.end()},
                                   .active = true});

  Game::Audio::play_cue(Game::Audio::Cue::k_combat_charge);
  emit run_mode_changed(true);
}

void CommandController::disable_run_mode_for_selected() {
  if (m_selection_system == nullptr || m_world == nullptr) {
    return;
  }

  const auto& selected = m_selection_system->get_selected_units();
  if (selected.empty()) {
    return;
  }

  submit(m_world,
         Game::Command::SetRunMode{.units = {selected.begin(), selected.end()},
                                   .active = false});

  emit run_mode_changed(false);
}

} // namespace App::Controllers
