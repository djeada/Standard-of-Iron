#include "app/orders/patrol_commands.h"

#include <QPointF>

#include "app/orders/rts_action_model.h"
#include "game/core/world.h"
#include "game/render_bridge/picking_service.h"
#include "game/session/selection_service.h"
#include "scene/camera.h"

namespace App::Controllers {

PatrolCommands::PatrolCommands(Engine::Core::World* world,
                               Game::Session::SelectionService* selection,
                               Game::Systems::PickingService* picking,
                               App::Orders::OrderIssuer& orders)
    : m_world(world)
    , m_selection(selection)
    , m_picking(picking)
    , m_orders(orders) {
}

void PatrolCommands::reset() {
  m_has_first_waypoint = false;
  m_first_waypoint = QVector3D();
}

auto PatrolCommands::on_patrol_click(const PointerTarget& target) -> CommandResult {
  CommandResult result;
  const auto abandon_pending = [&](bool reset_cursor) {
    if (m_has_first_waypoint) {
      clear_first_waypoint();
      result.reset_cursor_to_normal = reset_cursor;
    }
  };

  if ((m_selection == nullptr) || (m_world == nullptr) || (m_picking == nullptr) ||
      (target.camera == nullptr)) {
    abandon_pending(true);
    return result;
  }

  const auto& selected = m_selection->get_selected_units();
  if (selected.empty()) {
    abandon_pending(true);
    result.order =
        m_orders.reject(App::Core::OrderKind::Patrol, App::Core::no_selection_reason());
    return result;
  }

  auto const patrol_units = App::Core::filter_selected_units_for_action(
      m_world, selected, QStringLiteral("patrol"));
  if (patrol_units.empty()) {
    if (m_has_first_waypoint) {
      clear_first_waypoint();
    }
    result.order = m_orders.reject(
        App::Core::OrderKind::Patrol,
        App::Core::no_eligible_units_reason(App::Core::OrderKind::Patrol));
    result.reset_cursor_to_normal = true;
    return result;
  }

  QVector3D hit;
  if (!Game::Systems::PickingService::screen_to_ground(QPointF(target.sx, target.sy),
                                                       *target.camera,
                                                       target.viewport_width,
                                                       target.viewport_height,
                                                       hit)) {
    abandon_pending(true);
    return result;
  }

  if (!m_has_first_waypoint) {
    m_has_first_waypoint = true;
    m_first_waypoint = hit;
    result.input_consumed = true;
    return result;
  }

  result.order = m_orders.issue(
      App::Core::OrderKind::Patrol,
      Game::Command::Patrol{.units = {patrol_units.begin(), patrol_units.end()},
                            .first_waypoint = m_first_waypoint,
                            .second_waypoint = hit},
      0,
      &hit);

  clear_first_waypoint();
  result.input_consumed = result.order.accepted();
  result.reset_cursor_to_normal = true;
  return result;
}

} // namespace App::Controllers
