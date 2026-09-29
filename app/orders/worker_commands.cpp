#include "app/orders/worker_commands.h"

#include <utility>
#include <vector>

#include "app/orders/movement_utils.h"
#include "app/orders/order_submission.h"
#include "app/orders/rts_action_model.h"
#include "app/orders/worker_orders.h"
#include "game/core/component_gameplay.h"
#include "game/core/world.h"
#include "game/render_bridge/picking_service.h"
#include "game/session/selection_service.h"
#include "game/systems/economy/food_targets.h"
#include "scene/camera.h"

namespace App::Controllers {

WorkerCommands::WorkerCommands(Engine::Core::World* world,
                               Game::Session::SelectionService* selection,
                               Game::Systems::PickingService* picking,
                               App::Orders::OrderIssuer& orders,
                               QObject* parent)
    : QObject(parent)
    , m_world(world)
    , m_selection(selection)
    , m_picking(picking)
    , m_orders(orders) {
}

auto WorkerCommands::eligible_gatherers() const -> std::vector<Engine::Core::EntityID> {
  return App::Core::filter_selected_units_for_action(
      m_world, m_selection->get_selected_units(), QStringLiteral("auto_gather"));
}

auto WorkerCommands::reject_no_gatherers() -> CommandResult {
  CommandResult result;
  result.order = m_orders.reject(
      App::Core::OrderKind::Gather,
      m_selection->get_selected_units().empty()
          ? App::Core::no_selection_reason()
          : App::Core::no_eligible_units_reason(App::Core::OrderKind::Gather));
  return result;
}

auto WorkerCommands::on_auto_gather_command(const QString& priority_product_type)
    -> CommandResult {
  if ((m_selection == nullptr) || (m_world == nullptr)) {
    return {};
  }

  auto const builders = eligible_gatherers();
  if (builders.empty()) {
    return reject_no_gatherers();
  }

  int already_gathering = 0;
  for (auto const id : builders) {
    const auto* builder =
        m_world->try_get<Engine::Core::BuilderProductionComponent>(id);
    already_gathering += (builder != nullptr && builder->auto_gather) ? 1 : 0;
  }

  const bool should_enable = already_gathering < static_cast<int>(builders.size());
  return issue_auto_gather(builders, should_enable, priority_product_type);
}

auto WorkerCommands::set_auto_gather(bool active, const QString& priority_product_type)
    -> CommandResult {
  if ((m_selection == nullptr) || (m_world == nullptr)) {
    return {};
  }

  auto const builders = eligible_gatherers();
  if (builders.empty()) {
    return reject_no_gatherers();
  }
  return issue_auto_gather(builders, active, priority_product_type);
}

auto WorkerCommands::issue_auto_gather(
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

auto WorkerCommands::start_food_harvest(Engine::Core::EntityID target,
                                        const QString& product_type,
                                        int local_owner_id) -> CommandResult {
  CommandResult result;
  if (m_selection == nullptr || m_world == nullptr || target == 0 ||
      product_type.isEmpty()) {
    return result;
  }

  const auto& selected = m_selection->get_selected_units();
  if (selected.empty()) {
    result.order =
        m_orders.reject(App::Core::OrderKind::Gather, App::Core::no_selection_reason());
    return result;
  }

  auto crew = App::Orders::builder_crew_of(*m_world, selected);
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

  result.order = m_orders.publish(App::Core::submit_player_order(
      *m_world,
      local_owner_id,
      App::Orders::harvest_order(std::move(crew),
                                 product_type.toStdString(),
                                 target,
                                 food_target->x,
                                 food_target->z)));
  result.input_consumed = result.order.accepted();
  return result;
}

template <class Issue>
auto WorkerCommands::pointer_order(const PointerTarget& target,
                                   App::Core::OrderKind empty_selection_kind,
                                   CursorReset reset,
                                   Issue&& issue) -> CommandResult {
  CommandResult result;
  result.reset_cursor_to_normal = true;
  if ((m_selection == nullptr) || (m_picking == nullptr) ||
      (target.camera == nullptr) || (m_world == nullptr)) {
    return result;
  }

  const auto& selected = m_selection->get_selected_units();
  if (selected.empty()) {
    result.order =
        m_orders.reject(empty_selection_kind, App::Core::no_selection_reason());
    return result;
  }

  result.order = m_orders.publish(issue(selected));
  result.input_consumed = result.order.accepted();
  result.reset_cursor_to_normal = reset == CursorReset::Always || result.input_consumed;
  return result;
}

auto WorkerCommands::on_civilian_delivery_click(const PointerTarget& target,
                                                int local_owner_id) -> CommandResult {
  return pointer_order(target,
                       App::Core::OrderKind::Deliver,
                       CursorReset::Always,
                       [&](const std::vector<Engine::Core::EntityID>& selected) {
                         return App::Utils::issue_civilian_delivery_command(
                             m_world,
                             selected,
                             m_picking,
                             target.camera,
                             target.sx,
                             target.sy,
                             target.viewport_width,
                             target.viewport_height,
                             local_owner_id);
                       });
}

auto WorkerCommands::on_builder_repair_click(const PointerTarget& target,
                                             int local_owner_id) -> CommandResult {
  return pointer_order(target,
                       App::Core::OrderKind::Repair,
                       CursorReset::WhenAccepted,
                       [&](const std::vector<Engine::Core::EntityID>& selected) {
                         return App::Utils::issue_builder_repair_command(
                             m_world,
                             selected,
                             m_picking,
                             target.camera,
                             target.sx,
                             target.sy,
                             target.viewport_width,
                             target.viewport_height,
                             local_owner_id);
                       });
}

auto WorkerCommands::on_builder_dismantle_click(const PointerTarget& target,
                                                int local_owner_id) -> CommandResult {
  return pointer_order(target,
                       App::Core::OrderKind::Build,
                       CursorReset::WhenAccepted,
                       [&](const std::vector<Engine::Core::EntityID>& selected) {
                         return App::Utils::issue_builder_dismantle_command(
                             m_world,
                             selected,
                             m_picking,
                             target.camera,
                             target.sx,
                             target.sy,
                             target.viewport_width,
                             target.viewport_height,
                             local_owner_id);
                       });
}

} // namespace App::Controllers
