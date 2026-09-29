#include "app/orders/movement_commands.h"

#include <QPointF>

#include <vector>

#include "app/orders/mode_toggle_commands.h"
#include "app/orders/movement_utils.h"
#include "app/orders/rts_action_model.h"
#include "game/core/component_core.h"
#include "game/core/world.h"
#include "game/render_bridge/picking_service.h"
#include "game/session/selection_service.h"
#include "game/systems/combat_system/target_rules.h"
#include "game/systems/movement/command_service.h"
#include "scene/camera.h"

namespace App::Controllers {

MovementCommands::MovementCommands(Engine::Core::World* world,
                                   Game::Session::SelectionService* selection,
                                   Game::Systems::PickingService* picking,
                                   App::Orders::OrderIssuer& orders,
                                   ModeToggleCommands& modes)
    : m_world(world)
    , m_selection(selection)
    , m_picking(picking)
    , m_orders(orders)
    , m_modes(modes) {
}

auto MovementCommands::ready_for_pointer(const PointerTarget& target) const -> bool {
  return m_selection != nullptr && m_picking != nullptr && target.camera != nullptr &&
         m_world != nullptr;
}

auto MovementCommands::on_attack_click(const PointerTarget& target,
                                       int local_owner_id) -> CommandResult {
  using App::Core::OrderKind;
  CommandResult result;
  result.reset_cursor_to_normal = true;
  if (!ready_for_pointer(target)) {
    return result;
  }

  const auto& selected = m_selection->get_selected_units();
  if (selected.empty()) {
    result.order = m_orders.reject(OrderKind::Attack, App::Core::no_selection_reason());
    return result;
  }

  Engine::Core::EntityID const target_id =
      Game::Systems::PickingService::pick_unit_first(float(target.sx),
                                                     float(target.sy),
                                                     *m_world,
                                                     *target.camera,
                                                     target.viewport_width,
                                                     target.viewport_height,
                                                     0);

  auto* target_entity = target_id != 0 ? m_world->get_entity(target_id) : nullptr;
  auto* target_unit = target_entity != nullptr
                          ? target_entity->get_component<Engine::Core::UnitComponent>()
                          : nullptr;
  if (target_unit == nullptr ||
      Game::Systems::Combat::is_warded_structure(target_entity)) {
    result = attack_ground(target, selected, local_owner_id);
    result.reset_cursor_to_normal = true;
    return result;
  }

  result = attack_unit(target_id, selected);
  result.reset_cursor_to_normal = true;
  return result;
}

auto MovementCommands::attack_ground(
    const PointerTarget& target,
    const std::vector<Engine::Core::EntityID>& selected,
    int local_owner_id) -> CommandResult {
  using App::Core::OrderKind;
  CommandResult result;
  QVector3D hit;
  if (!Game::Systems::PickingService::screen_to_ground(QPointF(target.sx, target.sy),
                                                       *target.camera,
                                                       target.viewport_width,
                                                       target.viewport_height,
                                                       hit)) {
    result.order = m_orders.reject(
        OrderKind::Attack, App::Core::no_target_under_cursor_reason(OrderKind::Attack));
    return result;
  }

  auto const attackers = App::Core::filter_selected_units_for_action(
      m_world, selected, QStringLiteral("attack"));
  if (attackers.empty()) {
    result.order = m_orders.reject_at(
        OrderKind::Attack, App::Core::no_eligible_units_reason(OrderKind::Attack), hit);
    return result;
  }
  result.order = m_orders.publish(
      App::Utils::submit_ground_move(*m_world,
                                     attackers,
                                     hit,
                                     local_owner_id,
                                     Game::Systems::MoveOrderKind::AttackMove));
  result.input_consumed = result.order.accepted();
  return result;
}

auto MovementCommands::attack_unit(Engine::Core::EntityID target_id,
                                   const std::vector<Engine::Core::EntityID>& selected)
    -> CommandResult {
  using App::Core::OrderKind;
  CommandResult result;
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

auto MovementCommands::on_attack_press(const PointerTarget& target,
                                       int local_owner_id) -> CommandResult {
  CommandResult result;
  if (!ready_for_pointer(target)) {
    return result;
  }

  const auto& selected = m_selection->get_selected_units();
  if (selected.empty()) {
    return result;
  }

  Engine::Core::EntityID const target_id =
      App::Utils::pick_enemy_unit_at_screen(m_world,
                                            target.camera,
                                            target.sx,
                                            target.sy,
                                            target.viewport_width,
                                            target.viewport_height,
                                            local_owner_id);
  if (target_id == 0U) {
    return result;
  }

  m_modes.disable_run_mode_for_selected();
  result.order = m_orders.publish(
      App::Utils::issue_attack_command(m_world, selected, target_id, local_owner_id));
  result.input_consumed = true;
  return result;
}

auto MovementCommands::on_move_or_attack_click(const PointerTarget& target,
                                               int local_owner_id) -> CommandResult {
  CommandResult result;
  if (!ready_for_pointer(target)) {
    return result;
  }

  const auto& selected = m_selection->get_selected_units();
  if (selected.empty()) {
    return result;
  }

  result.order =
      m_orders.publish(App::Utils::issue_move_or_attack_command(m_world,
                                                                selected,
                                                                m_picking,
                                                                target.camera,
                                                                target.sx,
                                                                target.sy,
                                                                target.viewport_width,
                                                                target.viewport_height,
                                                                local_owner_id));
  result.input_consumed = result.order.issued();
  return result;
}

auto MovementCommands::on_minimap_move(const QVector3D& world_target,
                                       int local_owner_id) -> CommandResult {
  CommandResult result;
  if ((m_selection == nullptr) || (m_world == nullptr)) {
    return result;
  }
  const auto& selected = m_selection->get_selected_units();
  if (selected.empty()) {
    return result;
  }

  result.order = m_orders.publish(
      App::Utils::submit_ground_move(*m_world, selected, world_target, local_owner_id));
  result.input_consumed = result.order.accepted();
  return result;
}

auto MovementCommands::refuse_unreachable_move(const QVector3D& destination)
    -> App::Core::OrderOutcome {
  return m_orders.reject_at(
      App::Core::OrderKind::Move, App::Core::unreachable_reason(), destination);
}

} // namespace App::Controllers
