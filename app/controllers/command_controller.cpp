#include "command_controller.h"

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

#include "../../game/command/command.h"
#include "../../game/command/command_queue.h"
#include "../../game/core/component.h"
#include "../../game/core/entity.h"
#include "../../game/core/world.h"
#include "../../game/formation/army_formation_registry.h"
#include "../../game/formation/army_formation_service.h"
#include "../../game/formation/formation_doctrine.h"
#include "../../game/session/session_context.h"
#include "../../game/systems/combat_rules.h"
#include "../../game/systems/command_service.h"
#include "../../game/systems/owner_registry.h"
#include "../../game/systems/picking_service.h"
#include "../../game/systems/production_service.h"
#include "../../game/systems/selection_system.h"
#include "../../game/systems/troop_profile_service.h"
#include "../../game/util/asset_text.h"
#include "../core/order_service.h"
#include "../core/rts_action_model.h"
#include "../utils/movement_utils.h"
#include "game/audio/audio_cues.h"
#include "game/game_config.h"
#include "scene/camera.h"
#include "units/spawn_type.h"

namespace App::Controllers {

namespace {

auto local_owner(Engine::Core::World* world) -> int {
  if (auto* session = Game::Session::SessionContext::for_world(*world)) {
    return session->owners().get_local_player_id();
  }
  return Game::Systems::OwnerRegistry::instance().get_local_player_id();
}

void submit(Engine::Core::World* world, Game::Command::Payload payload) {
  Game::Command::submit(*world,
                        Game::Command::Source::LocalPlayer,
                        local_owner(world),
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
    , m_picking_service(picking_service) {
}

auto CommandController::publish_order(App::Core::OrderOutcome outcome)
    -> App::Core::OrderOutcome {
  if (outcome.issued()) {
    emit order_feedback(outcome);
  }
  return outcome;
}

auto CommandController::issue_order(App::Core::OrderKind kind,
                                    Game::Command::Payload payload,
                                    Engine::Core::EntityID target,
                                    const QVector3D* destination)
    -> App::Core::OrderOutcome {
  if (m_world == nullptr) {
    return publish_order(App::Core::rejected_order(
        kind,
        App::Core::rejection_reason_text(Game::Command::Rejection::MalformedPayload,
                                         kind)));
  }
  App::Core::OrderRequest request;
  request.kind = kind;
  request.payload = std::move(payload);
  request.target = target;
  if (destination != nullptr) {
    request.has_destination = true;
    request.destination = *destination;
  }
  return publish_order(App::Core::submit_player_order(
      *m_world, local_owner(m_world), std::move(request)));
}

auto CommandController::reject_order(App::Core::OrderKind kind,
                                     const QString& reason) -> App::Core::OrderOutcome {
  return publish_order(App::Core::rejected_order(kind, reason));
}

auto CommandController::reject_order_at(App::Core::OrderKind kind,
                                        const QString& reason,
                                        const QVector3D& destination)
    -> App::Core::OrderOutcome {
  return publish_order(App::Core::rejected_order_at(kind, reason, destination));
}

auto CommandController::reject_order_on(App::Core::OrderKind kind,
                                        const QString& reason,
                                        Engine::Core::EntityID target)
    -> App::Core::OrderOutcome {
  return publish_order(App::Core::rejected_order_on(kind, reason, target));
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
    result.order = reject_order(OrderKind::Attack, App::Core::no_selection_reason());
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
      result.order =
          reject_order_at(OrderKind::Attack,
                          App::Core::no_target_under_cursor_reason(OrderKind::Attack),
                          hit);
    } else {
      result.order =
          reject_order(OrderKind::Attack,
                       App::Core::no_target_under_cursor_reason(OrderKind::Attack));
    }
    return result;
  }

  auto const attackers = App::Core::filter_selected_units_for_action(
      m_world, selected, QStringLiteral("attack"));
  if (attackers.empty()) {
    result.order =
        reject_order_on(OrderKind::Attack,
                        App::Core::no_eligible_units_reason(OrderKind::Attack),
                        target_id);
    return result;
  }

  result.order =
      issue_order(OrderKind::Attack,
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
  result.order = publish_order(
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
      publish_order(App::Utils::issue_move_or_attack_command(m_world,
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

  result.order = publish_order(
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
        reject_order(App::Core::OrderKind::Stop, App::Core::no_selection_reason());
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
      issue_order(App::Core::OrderKind::Stop,
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
        reject_order(App::Core::OrderKind::Hold, App::Core::no_selection_reason());
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
    result.order =
        reject_order(App::Core::OrderKind::Hold,
                     App::Core::no_eligible_units_reason(App::Core::OrderKind::Hold));
    return result;
  }

  const bool should_enable_hold = (hold_active_count < eligible_count);

  result.order =
      issue_order(App::Core::OrderKind::Hold,
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
    result.order = reject_order(
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

  result.order =
      issue_order(App::Core::OrderKind::Gather,
                  Game::Command::SetAutoGather{
                      .units = builders,
                      .active = should_enable,
                      .priority_product_type = priority_product_type.toStdString()});
  if (!result.order.accepted()) {
    return result;
  }

  emit auto_gather_changed(should_enable);

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
        reject_order(App::Core::OrderKind::Patrol, App::Core::no_selection_reason());
    return result;
  }

  auto const patrol_units = App::Core::filter_selected_units_for_action(
      m_world, selected, QStringLiteral("patrol"));
  if (patrol_units.empty()) {
    if (m_has_patrol_first_waypoint) {
      clear_patrol_first_waypoint();
    }
    result.order =
        reject_order(App::Core::OrderKind::Patrol,
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

  result.order = issue_order(
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

  const auto barracks = Game::Systems::ProductionService::find_selected_barracks(
      *m_world, m_selection_system->get_selected_units(), local_owner_id);
  if (barracks != Engine::Core::NULL_ENTITY) {
    App::Core::OrderRequest request;
    request.kind = App::Core::OrderKind::Rally;
    request.payload =
        Game::Command::SetRallyPoint{.building = barracks, .position = hit};
    request.target = barracks;
    request.has_destination = true;
    request.destination = hit;
    result.order = publish_order(
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

  const bool wants_civilian =
      unit_type.compare(QStringLiteral("civilian"), Qt::CaseInsensitive) == 0;
  const auto building = wants_civilian
                            ? Game::Systems::ProductionService::find_selected_home(
                                  *m_world, sel, local_owner_id)
                            : Game::Systems::ProductionService::find_selected_barracks(
                                  *m_world, sel, local_owner_id);
  if (building == Engine::Core::NULL_ENTITY) {
    return;
  }
  const auto product = Game::Units::troop_typeFromString(unit_type.toStdString());

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

  if (!m_is_placing_formation) {
    m_formation_placement_position = QVector3D();
    m_formation_facing_degrees = 0.0F;
    m_formation_facing_explicit = false;
    m_formation_units.clear();
    return;
  }

  if (m_world != nullptr && !m_formation_units.empty()) {
    submit(
        m_world,
        Game::Command::SetFormationMode{.units = m_formation_units, .active = false});
  }

  m_is_placing_formation = false;
  m_formation_drag_active = false;
  m_formation_placement_position = QVector3D();
  m_formation_facing_degrees = 0.0F;
  m_formation_facing_explicit = false;
  m_formation_frontage = 0.0F;
  m_formation_units.clear();
  m_formation_preview = Game::Formation::ArmyFormationPlan{};
  emit formation_preview_changed();
  emit formation_placement_ended();
  emit formation_mode_changed(false);
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
        reject_order(App::Core::OrderKind::Guard, App::Core::no_selection_reason());
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
    result.order =
        reject_order(App::Core::OrderKind::Guard,
                     App::Core::no_eligible_units_reason(App::Core::OrderKind::Guard));
    return result;
  }

  const bool should_enable_guard = (guard_active_count < eligible_count);

  result.order =
      issue_order(App::Core::OrderKind::Guard,
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
        reject_order(App::Core::OrderKind::Guard, App::Core::no_selection_reason());
    result.reset_cursor_to_normal = true;
    return result;
  }

  auto const guard_units = App::Core::filter_selected_units_for_action(
      m_world, selected, QStringLiteral("guard"));
  if (guard_units.empty()) {
    result.order =
        reject_order(App::Core::OrderKind::Guard,
                     App::Core::no_eligible_units_reason(App::Core::OrderKind::Guard));
    result.reset_cursor_to_normal = true;
    return result;
  }

  auto* cam = static_cast<Render::GL::Camera*>(camera);
  QVector3D hit;
  if (!Game::Systems::PickingService::screen_to_ground(
          QPointF(sx, sy), *cam, viewport_width, viewport_height, hit)) {
    result.order = reject_order(App::Core::OrderKind::Guard,
                                App::Core::no_ground_under_cursor_reason());
    result.reset_cursor_to_normal = true;
    return result;
  }

  result.order = issue_order(
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
    result.order =
        reject_order(App::Core::OrderKind::Deliver, App::Core::no_selection_reason());
    result.reset_cursor_to_normal = true;
    return result;
  }

  auto* cam = static_cast<Render::GL::Camera*>(camera);
  result.order =
      publish_order(App::Utils::issue_civilian_delivery_command(m_world,
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
        reject_order(App::Core::OrderKind::Repair, App::Core::no_selection_reason());
    result.reset_cursor_to_normal = true;
    return result;
  }

  auto* cam = static_cast<Render::GL::Camera*>(camera);
  result.order =
      publish_order(App::Utils::issue_builder_repair_command(m_world,
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

auto CommandController::on_formation_command() -> CommandResult {
  CommandResult result;
  if ((m_selection_system == nullptr) || (m_world == nullptr)) {
    return result;
  }

  const auto& selected = m_selection_system->get_selected_units();
  if (selected.size() <= 1) {
    return result;
  }

  int eligible_count = 0;
  int formation_active_count = 0;

  for (auto id : selected) {
    auto* entity = m_world->get_entity(id);
    if (entity == nullptr) {
      continue;
    }

    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr) {
      continue;
    }

    if (!Game::Units::is_troop_spawn(unit->spawn_type)) {
      continue;
    }

    eligible_count++;

    auto* formation_mode =
        entity->get_component<Engine::Core::FormationModeComponent>();
    if ((formation_mode != nullptr) && formation_mode->active) {
      formation_active_count++;
    }
  }

  if (eligible_count <= 1) {
    return result;
  }

  const bool should_enable_formation = (formation_active_count < eligible_count);

  submit(m_world,
         Game::Command::SetFormationMode{.units = {selected.begin(), selected.end()},
                                         .active = should_enable_formation});

  if (should_enable_formation) {
    QVector3D center(0.0F, 0.0F, 0.0F);
    int valid_count = 0;

    m_formation_units.clear();

    for (auto id : selected) {
      auto* entity = m_world->get_entity(id);
      if (entity == nullptr) {
        continue;
      }

      auto* unit = entity->get_component<Engine::Core::UnitComponent>();
      if (unit == nullptr || !Game::Units::is_troop_spawn(unit->spawn_type)) {
        continue;
      }

      m_formation_units.push_back(id);

      auto* transform = entity->get_component<Engine::Core::TransformComponent>();
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
      m_is_right_drag_formation = false;
      m_formation_placement_position = center;
      reset_formation_facing();
      m_formation_frontage = 0.0F;
      m_formation_drag_active = false;
      m_formation_members.clear();
      invalidate_formation_layout();
      refresh_formation_preview();

      emit formation_placement_started();
      emit formation_placement_updated(m_formation_placement_position,
                                       m_formation_facing_degrees);
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

  const auto& selected = m_selection_system->get_selected_units();
  for (auto id : selected) {
    auto* entity = m_world->get_entity(id);
    if (entity == nullptr) {
      continue;
    }

    auto* formation_mode =
        entity->get_component<Engine::Core::FormationModeComponent>();
    if ((formation_mode != nullptr) && formation_mode->active) {
      return true;
    }
  }

  return false;
}

auto CommandController::auto_formation_facing() const -> float {
  if ((m_world == nullptr) || m_formation_units.empty()) {
    return 0.0F;
  }
  return Game::Formation::ArmyFormationService::auto_facing(
      *m_world, m_formation_units, m_formation_placement_position);
}

void CommandController::set_formation_facing(float degrees, bool explicit_choice) {
  float normalized = std::fmod(degrees + 180.0F, 360.0F);
  if (normalized < 0.0F) {
    normalized += 360.0F;
  }
  m_formation_facing_degrees = normalized - 180.0F;
  if (explicit_choice) {
    m_formation_facing_explicit = true;
  }
}

void CommandController::follow_auto_formation_facing() {
  if (m_formation_facing_explicit) {
    return;
  }
  set_formation_facing(auto_formation_facing(), false);
}

void CommandController::reset_formation_facing() {
  m_formation_facing_explicit = false;
  follow_auto_formation_facing();
}

void CommandController::update_formation_placement(const QVector3D& position) {
  if (!m_is_placing_formation) {
    return;
  }
  m_formation_placement_position = position;
  follow_auto_formation_facing();
  refresh_formation_preview();
  emit formation_placement_updated(m_formation_placement_position,
                                   m_formation_facing_degrees);
}

void CommandController::update_formation_rotation(float angle_degrees) {
  if (!m_is_placing_formation) {
    return;
  }
  set_formation_facing(angle_degrees, true);
  refresh_formation_preview();
  emit formation_placement_updated(m_formation_placement_position,
                                   m_formation_facing_degrees);
}

auto CommandController::begin_move_placement_at_position(const QVector3D& position)
    -> bool {
  if ((m_selection_system == nullptr) || (m_world == nullptr)) {
    return false;
  }

  const auto& selected = m_selection_system->get_selected_units();
  if (selected.empty()) {
    return false;
  }

  std::vector<Engine::Core::EntityID> troops;
  for (auto id : selected) {
    auto* entity = m_world->get_entity(id);
    if (entity == nullptr) {
      continue;
    }
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr || !Game::Units::is_troop_spawn(unit->spawn_type)) {
      continue;
    }
    troops.push_back(id);
  }

  if (troops.size() < 2) {
    return false;
  }

  m_formation_units = std::move(troops);
  m_is_placing_formation = true;
  m_is_right_drag_formation = true;
  m_formation_placement_position = position;
  reset_formation_facing();
  m_formation_members.clear();
  invalidate_formation_layout();

  emit formation_placement_started();
  emit formation_placement_updated(m_formation_placement_position,
                                   m_formation_facing_degrees);
  return true;
}

void CommandController::confirm_formation_placement() {
  if (!m_is_placing_formation || m_formation_units.empty()) {
    cancel_formation_placement();
    return;
  }

  Game::Command::DeployFormation deploy;
  deploy.units = m_formation_units;
  deploy.anchor = m_formation_placement_position;
  deploy.facing = m_formation_facing_degrees;
  deploy.frontage = m_formation_frontage;
  deploy.intent = m_formation_intent;
  deploy.doctrine = m_formation_doctrine_override;
  deploy.options = m_formation_options;
  deploy.spacing = Game::GameConfig::instance().gameplay().formation_spacing_default;

  {

    Game::Formation::ArmyFormationRequest request;
    request.members = deploy.units;
    request.anchor = deploy.anchor;
    request.facing = deploy.facing;
    request.frontage = deploy.frontage;
    request.intent = deploy.intent;
    request.doctrine = deploy.doctrine;
    request.options = deploy.options;
    request.spacing = deploy.spacing;
    auto const preview =
        Game::Formation::ArmyFormationService::preview(*m_world, request);
    if (!preview.valid) {
      emit formation_placement_rejected(
          QString::fromStdString(preview.rejection_reason));
    }
  }

  {
    const QVector3D anchor = deploy.anchor;
    (void)issue_order(App::Core::OrderKind::Formation, std::move(deploy), 0, &anchor);
  }

  m_is_placing_formation = false;
  m_formation_drag_active = false;
  m_formation_facing_explicit = false;
  m_formation_units.clear();
  m_formation_members.clear();
  invalidate_formation_layout();
  m_formation_preview = Game::Formation::ArmyFormationPlan{};
  emit formation_preview_changed();
  emit formation_placement_ended();
  if (!m_is_right_drag_formation) {
    emit formation_mode_changed(true);
  }
  m_is_right_drag_formation = false;
}

void CommandController::cancel_formation_placement() {
  if (!m_is_placing_formation) {
    return;
  }

  submit(m_world, Game::Command::ReleaseFormation{.units = m_formation_units});

  m_is_placing_formation = false;
  m_formation_drag_active = false;
  m_formation_facing_explicit = false;
  m_formation_frontage = 0.0F;
  m_formation_units.clear();
  m_formation_members.clear();
  invalidate_formation_layout();
  m_formation_preview = Game::Formation::ArmyFormationPlan{};
  emit formation_preview_changed();
  emit formation_placement_ended();
  if (!m_is_right_drag_formation) {
    emit formation_mode_changed(false);
  }
  m_is_right_drag_formation = false;
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

namespace {

auto scale_for_preset(const QString& preset,
                      float low,
                      float mid,
                      float high) -> float {
  const QString lowered = preset.trimmed().toLower();
  if (lowered == QStringLiteral("narrow") || lowered == QStringLiteral("shallow") ||
      lowered == QStringLiteral("tight")) {
    return low;
  }
  if (lowered == QStringLiteral("wide") || lowered == QStringLiteral("deep") ||
      lowered == QStringLiteral("loose")) {
    return high;
  }
  return mid;
}

auto preset_index_for_scale(float scale, float low, float mid, float high) -> int {
  float const to_low = std::abs(scale - low);
  float const to_mid = std::abs(scale - mid);
  float const to_high = std::abs(scale - high);
  if (to_low <= to_mid && to_low <= to_high) {
    return 0;
  }
  return to_mid <= to_high ? 1 : 2;
}

} // namespace

void CommandController::set_formation_intent(const QString& intent_id) {
  auto parsed = Game::Formation::try_parse_intent(intent_id);
  if (!parsed) {
    return;
  }
  if (m_formation_intent == *parsed) {
    return;
  }
  m_formation_intent = *parsed;
  apply_formation_option_change();
}

auto CommandController::formation_intent() const -> QString {
  return QString::fromLatin1(Game::Formation::intent_to_string(m_formation_intent));
}

auto CommandController::formation_intents() const -> QStringList {

  QStringList out;
  if (m_formation_units.empty()) {
    return out;
  }
  for (auto intent : Game::Formation::all_intents()) {
    out.append(QString::fromLatin1(Game::Formation::intent_to_string(intent)));
  }
  return out;
}

auto CommandController::formation_intent_display_name(const QString& intent_id) const
    -> QString {
  auto parsed = Game::Formation::try_parse_intent(intent_id);
  return Game::Formation::intent_display_name(
      parsed.value_or(Game::Formation::ArmyFormationIntent::FactionDefault));
}

auto CommandController::formation_intent_unavailable_reason(
    const QString& intent_id) const -> QString {
  auto parsed = Game::Formation::try_parse_intent(intent_id);
  if (!parsed || m_world == nullptr || m_formation_units.empty()) {
    return QCoreApplication::translate("Formation", "No units selected.");
  }
  return QString::fromStdString(Game::Formation::ArmyFormationService::availability(
      *m_world, m_formation_units, *parsed, m_formation_doctrine_override));
}

auto CommandController::formation_doctrine() const -> QString {
  if (!m_formation_doctrine_override.empty()) {
    return QString::fromStdString(m_formation_doctrine_override);
  }
  if (m_world == nullptr || m_formation_units.empty()) {
    return QString::fromStdString(Game::Formation::k_neutral_doctrine);
  }
  return QString::fromStdString(
      Game::Formation::ArmyFormationService::doctrine_for_selection(
          *m_world, m_formation_units, m_formation_options.mixed_policy));
}

auto CommandController::formation_doctrine_display_name() const -> QString {
  return Game::Util::tr_asset(Game::Util::k_formations_context,
                              Game::Formation::DoctrineRegistry::instance()
                                  .get_or_neutral(formation_doctrine().toStdString())
                                  .display_name);
}

auto CommandController::formation_doctrine_options() const -> QVariantList {

  QVariantList out;
  QVariantMap automatic;
  automatic["id"] = QString();
  automatic["name"] = tr("Automatic");
  out.append(automatic);

  const auto& registry = Game::Formation::DoctrineRegistry::instance();
  auto ids = registry.ids();
  std::sort(ids.begin(), ids.end());
  for (const auto& id : ids) {
    const auto* doctrine = registry.find(id);
    if (doctrine == nullptr) {
      continue;
    }
    QVariantMap entry;
    entry["id"] = QString::fromStdString(id);
    entry["name"] =
        Game::Util::tr_asset(Game::Util::k_formations_context, doctrine->display_name);
    out.append(entry);
  }
  return out;
}

void CommandController::begin_formation_drag(const QVector3D& start) {
  if (!m_is_placing_formation || m_formation_units.empty()) {
    return;
  }
  m_formation_drag_active = true;
  m_formation_drag_start = start;
  m_formation_placement_position = start;
  m_formation_frontage = 0.0F;
  follow_auto_formation_facing();
  invalidate_formation_layout();
  refresh_formation_preview();
  emit formation_placement_updated(m_formation_placement_position,
                                   m_formation_facing_degrees);
}

void CommandController::update_formation_drag(const QVector3D& current) {
  if (!m_formation_drag_active) {
    return;
  }

  QVector3D along = current - m_formation_drag_start;
  along.setY(0.0F);
  float const length = along.length();

  m_formation_placement_position = m_formation_drag_start + (along * 0.5F);

  if (length > 0.5F) {
    m_formation_frontage = length;
    QVector3D const facing_dir(-along.z(), 0.0F, along.x());
    set_formation_facing(std::atan2(facing_dir.x(), facing_dir.z()) * 180.0F /
                             std::numbers::pi_v<float>,
                         true);

    m_formation_preview_dirty = true;
  } else {
    follow_auto_formation_facing();
  }

  refresh_formation_preview();
  emit formation_placement_updated(m_formation_placement_position,
                                   m_formation_facing_degrees);
}

void CommandController::end_formation_drag() {
  m_formation_drag_active = false;
}

void CommandController::adjust_formation_depth(float wheel_delta) {
  if (!m_is_placing_formation) {
    return;
  }
  float const step = wheel_delta > 0.0F ? 1.15F : (1.0F / 1.15F);
  m_formation_options.depth_scale =
      std::clamp(m_formation_options.depth_scale * step, 0.4F, 3.0F);
  apply_formation_option_change();
}

void CommandController::set_formation_preserve_order(bool preserve) {
  m_formation_options.preserve_member_order = preserve;
  apply_formation_option_change();
}

void CommandController::set_formation_frontage_preset(const QString& preset) {
  m_formation_options.frontage_scale = scale_for_preset(preset, 0.7F, 1.0F, 1.45F);
  apply_formation_option_change();
}

void CommandController::set_formation_depth_preset(const QString& preset) {
  m_formation_options.depth_scale = scale_for_preset(preset, 0.7F, 1.0F, 1.45F);
  apply_formation_option_change();
}

void CommandController::set_formation_spacing_preset(const QString& preset) {
  m_formation_options.spacing_scale = scale_for_preset(preset, 0.75F, 1.0F, 1.35F);
  apply_formation_option_change();
}

void CommandController::set_formation_flank_preference(const QString& preference) {
  if (auto parsed = Game::Formation::try_parse_flank_preference(preference)) {
    m_formation_options.flank_preference = *parsed;
    apply_formation_option_change();
  }
}

void CommandController::set_formation_ranged_placement(const QString& placement) {
  if (auto parsed = Game::Formation::try_parse_ranged_placement(placement)) {
    m_formation_options.ranged_placement = *parsed;
    apply_formation_option_change();
  }
}

void CommandController::set_formation_reserve_rows(int rows) {
  m_formation_options.reserve_rows = std::clamp(rows, -1, 2);
  apply_formation_option_change();
}

void CommandController::set_formation_movement_policy(const QString& policy) {
  if (auto parsed = Game::Formation::try_parse_movement_policy(policy)) {
    m_formation_options.movement_policy = *parsed;
    apply_formation_option_change();
  }
}

void CommandController::set_formation_mixed_policy(const QString& policy) {
  if (auto parsed = Game::Formation::try_parse_mixed_policy(policy)) {
    m_formation_options.mixed_policy = *parsed;
    apply_formation_option_change();
  }
}

void CommandController::set_formation_doctrine_override(const QString& doctrine) {
  const QString trimmed = doctrine.trimmed().toLower();
  if (trimmed.isEmpty() || trimmed == QStringLiteral("automatic")) {
    m_formation_doctrine_override.clear();
    m_formation_options.doctrine_locked = false;
  } else {
    m_formation_doctrine_override = trimmed.toStdString();
    m_formation_options.doctrine_locked = true;
  }
  apply_formation_option_change();
}

auto CommandController::formation_options() const -> QVariantMap {
  QVariantMap map;
  map["intent"] = formation_intent();
  map["doctrine"] = formation_doctrine();
  map["doctrine_display_name"] = formation_doctrine_display_name();
  map["doctrine_locked"] = m_formation_options.doctrine_locked;
  map["frontage_scale"] = m_formation_options.frontage_scale;
  map["depth_scale"] = m_formation_options.depth_scale;
  map["spacing_scale"] = m_formation_options.spacing_scale;
  map["reserve_rows"] = m_formation_options.reserve_rows;
  map["preserve_member_order"] = m_formation_options.preserve_member_order;
  map["flank"] = QString::fromLatin1(Game::Formation::flank_preference_to_string(
      m_formation_options.flank_preference));
  map["ranged"] = QString::fromLatin1(Game::Formation::ranged_placement_to_string(
      m_formation_options.ranged_placement));
  map["movement"] = QString::fromLatin1(
      Game::Formation::movement_policy_to_string(m_formation_options.movement_policy));
  map["mixed"] = QString::fromLatin1(
      Game::Formation::mixed_policy_to_string(m_formation_options.mixed_policy));
  map["frontage"] = m_formation_frontage;
  map["blocked_slots"] = m_formation_preview.blocked_count;
  map["adjusted_slots"] = m_formation_preview.adjusted_count;
  map["warning"] = formation_preview_warning();

  map["frontage_index"] =
      preset_index_for_scale(m_formation_options.frontage_scale, 0.7F, 1.0F, 1.45F);
  map["depth_index"] =
      preset_index_for_scale(m_formation_options.depth_scale, 0.7F, 1.0F, 1.45F);
  map["spacing_index"] =
      preset_index_for_scale(m_formation_options.spacing_scale, 0.75F, 1.0F, 1.35F);
  map["flank_index"] = static_cast<int>(m_formation_options.flank_preference);
  map["ranged_index"] = static_cast<int>(m_formation_options.ranged_placement);
  map["reserve_index"] = std::clamp(m_formation_options.reserve_rows, -1, 2) + 1;
  map["movement_index"] = static_cast<int>(m_formation_options.movement_policy);
  map["mixed_index"] = static_cast<int>(m_formation_options.mixed_policy);

  map["preserve_index"] = m_formation_options.preserve_member_order ? 1 : 0;
  map["intent_display_name"] = Game::Formation::intent_display_name(m_formation_intent);
  map["unit_count"] = static_cast<int>(m_formation_units.size());
  map["placed_count"] = m_formation_preview.placed_count();
  map["slot_count"] = static_cast<int>(m_formation_preview.slot_list.size());
  map["ranks"] = m_formation_preview.rank_count();
  map["files"] = m_formation_preview.file_count();
  map["plan_frontage"] = m_formation_preview.frontage;
  map["plan_depth"] = m_formation_preview.depth;
  map["plan_valid"] = m_formation_preview.valid;
  return map;
}

auto CommandController::selected_formation_status() const -> QVariantMap {
  QVariantMap map;
  map["active"] = false;
  if (m_world == nullptr || m_selection_system == nullptr) {
    return map;
  }

  const auto& selected = m_selection_system->get_selected_units();
  auto& registry = Game::Formation::ArmyFormationRegistry::instance();

  Game::Formation::FormationGroupID group = Game::Formation::k_invalid_group;
  int in_group = 0;
  int group_count = 0;
  for (auto const id : selected) {
    auto const owner = registry.group_of(id);
    if (owner == Game::Formation::k_invalid_group) {
      continue;
    }
    if (group == Game::Formation::k_invalid_group) {
      group = owner;
    }
    if (owner == group) {
      ++in_group;
    } else {
      ++group_count;
    }
  }

  const auto* formation = registry.find(group);
  if (formation == nullptr) {
    return map;
  }

  map["active"] = true;
  map["intent"] =
      QString::fromLatin1(Game::Formation::intent_to_string(formation->intent));
  map["intent_display_name"] = Game::Formation::intent_display_name(formation->intent);
  map["doctrine_display_name"] =
      QString::fromStdString(Game::Formation::DoctrineRegistry::instance()
                                 .get_or_neutral(formation->doctrine)
                                 .display_name);
  map["cohesion"] = formation->cohesion;
  map["phase"] = QString::fromLatin1(phase_to_string(formation->phase));
  map["member_count"] = static_cast<int>(formation->members.size());
  map["selected_in_group"] = in_group;
  map["mixed_groups"] = group_count > 0;
  map["blocked_slots"] = formation->blocked_slot_count();
  return map;
}

void CommandController::reset_formation_options() {
  m_formation_options = Game::Formation::ArmyFormationOptions{};
  m_formation_doctrine_override.clear();
  m_formation_intent = Game::Formation::ArmyFormationIntent::FactionDefault;
  m_formation_frontage = 0.0F;
  apply_formation_option_change();
}

auto CommandController::formation_preview_warning() const -> QString {
  if (!m_formation_preview.rejection_reason.empty()) {
    return QString::fromStdString(m_formation_preview.rejection_reason);
  }
  if (m_formation_preview.blocked_count > 0) {
    return tr("%1 of %2 positions do not fit on this ground.")
        .arg(m_formation_preview.blocked_count)
        .arg(static_cast<int>(m_formation_preview.slot_list.size()));
  }
  return {};
}

void CommandController::invalidate_formation_layout() {
  m_formation_layout_valid = false;
  m_formation_preview_dirty = true;
}

void CommandController::refresh_formation_preview() {
  if (m_world == nullptr || m_formation_units.empty() || !m_is_placing_formation) {
    m_formation_preview = Game::Formation::ArmyFormationPlan{};
    m_formation_members.clear();
    m_formation_layout_valid = false;
    m_formation_preview_dirty = true;
    emit formation_preview_changed();
    return;
  }

  Game::Formation::ArmyFormationRequest request;
  request.members = m_formation_units;
  request.anchor = m_formation_placement_position;
  request.facing = m_formation_facing_degrees;
  request.frontage = m_formation_frontage;
  request.intent = m_formation_intent;
  request.doctrine = m_formation_doctrine_override;
  request.options = m_formation_options;
  request.spacing = Game::GameConfig::instance().gameplay().formation_spacing_default;
  request.group_id = Game::Formation::ArmyFormationRegistry::instance().group_of(
      m_formation_units.front());

  constexpr float k_anchor_epsilon = 0.05F;
  constexpr float k_facing_epsilon = 0.25F;
  if (!m_formation_preview_dirty && m_formation_preview.valid &&
      (request.anchor - m_formation_previewed_anchor).lengthSquared() <
          k_anchor_epsilon * k_anchor_epsilon &&
      std::abs(request.facing - m_formation_previewed_facing) < k_facing_epsilon) {
    return;
  }

  if (m_formation_members.empty()) {
    m_formation_members = Game::Formation::ArmyFormationPlanner::collect_members(
        *m_world, m_formation_units);
  }

  auto const signature = Game::Formation::ArmyFormationPlanner::layout_signature(
      m_formation_members, request);
  if (!m_formation_layout_valid || m_formation_layout.signature != signature) {
    m_formation_layout = Game::Formation::ArmyFormationPlanner::build_layout(
        m_formation_members, request);
    m_formation_layout_valid = true;
  }

  m_formation_preview =
      Game::Formation::ArmyFormationPlanner::place(m_formation_layout, request);
  m_formation_previewed_anchor = request.anchor;
  m_formation_previewed_facing = request.facing;
  m_formation_preview_dirty = false;
  emit formation_preview_changed();
}

void CommandController::apply_formation_option_change() {
  invalidate_formation_layout();
  refresh_formation_preview();
  emit formation_placement_updated(m_formation_placement_position,
                                   m_formation_facing_degrees);
}

} // namespace App::Controllers
