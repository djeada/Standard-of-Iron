#include "app/viewmodels/placement_view_model.h"

#include <utility>

#include "app/core/client_context.h"
#include "app/economy/production_manager.h"
#include "app/economy/production_readouts.h"
#include "app/input/input_command_handler.h"
#include "app/orders/command_controller.h"

namespace App::ViewModels {

PlacementViewModel::PlacementViewModel(const App::Core::ClientContext& context,
                                       App::Core::ClientHost& host,
                                       QObject* parent)
    : QObject(parent)
    , m_context(context)
    , m_host(host) {
}

void PlacementViewModel::on_formation_command() {
  auto* handler = m_context.input;
  if (handler == nullptr) {
    return;
  }
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  handler->on_formation_command();
}

void PlacementViewModel::publish_frame() {
  App::Core::PlacementReadout readout;
  readout.any_selected_in_formation_mode = [this]() -> bool {
    auto* handler = m_context.input;
    return handler != nullptr && handler->any_selected_in_formation_mode();
  }();
  readout.placing_formation = [this]() -> bool {
    auto* commands = m_context.commands;
    return commands != nullptr && commands->formation().is_placing_formation();
  }();
  readout.dragging_formation = [this]() -> bool {
    auto* handler = m_context.input;
    return handler != nullptr && handler->is_dragging_formation();
  }();
  readout.formation_intent = [this]() -> QString {
    auto* commands = m_context.commands;
    return commands == nullptr ? QString() : commands->formation().formation_intent();
  }();
  readout.formation_intents = [this]() -> QStringList {
    auto* commands = m_context.commands;
    return commands == nullptr ? QStringList()
                               : commands->formation().formation_intents();
  }();
  readout.formation_doctrine_options = [this]() -> QVariantList {
    auto* commands = m_context.commands;
    return commands == nullptr ? QVariantList()
                               : commands->formation().formation_doctrine_options();
  }();
  readout.formation_options = [this]() -> QVariantMap {
    auto* commands = m_context.commands;
    return commands == nullptr ? QVariantMap()
                               : commands->formation().formation_options();
  }();
  readout.selected_formation_status = [this]() -> QVariantMap {
    auto* commands = m_context.commands;
    return commands == nullptr ? QVariantMap()
                               : commands->formation().selected_formation_status();
  }();
  readout.placing_construction = [this]() -> bool {
    auto* production = m_context.production;
    return production != nullptr && production->is_placing_construction();
  }();
  readout.construction_preview_active = [this]() -> bool {
    auto* production = m_context.production;
    return production != nullptr && production->construction_preview_active();
  }();
  readout.construction_preview_valid = [this]() -> bool {
    auto* production = m_context.production;
    return production != nullptr && production->construction_preview_valid();
  }();
  readout.construction_preview_reason = [this]() -> QString {
    auto* production = m_context.production;
    return production != nullptr ? production->construction_preview_reason()
                                 : QString();
  }();
  readout.construction_preview_rotatable = [this]() -> bool {
    auto* production = m_context.production;
    return production != nullptr && production->construction_preview_rotatable();
  }();
  readout.construction_preview_segment_count = [this]() -> int {
    auto* production = m_context.production;
    return production != nullptr ? production->construction_preview_segment_count() : 0;
  }();
  readout.construction_preview_valid_segment_count = [this]() -> int {
    auto* production = m_context.production;
    return production != nullptr
               ? production->construction_preview_valid_segment_count()
               : 0;
  }();
  readout.construction_preview_total_cost = [this]() -> int {
    auto* production = m_context.production;
    return production != nullptr ? production->construction_preview_total_cost() : 0;
  }();
  readout.pending_builder_construction_type = [this]() -> QString {
    auto* production = m_context.production;
    return production != nullptr ? production->pending_builder_construction_type()
                                 : QString();
  }();
  readout.pending_building_type = [this]() -> QString {
    auto* production = m_context.production;
    return production != nullptr ? production->pending_building_type() : QString();
  }();
  m_readout.publish(std::move(readout));
}

auto PlacementViewModel::any_selected_in_formation_mode() const -> bool {
  const auto readout = m_readout.read();
  return readout ? readout->any_selected_in_formation_mode : false;
}

auto PlacementViewModel::is_placing_formation() const -> bool {
  const auto readout = m_readout.read();
  return readout ? readout->placing_formation : false;
}

void PlacementViewModel::on_formation_mouse_move(qreal sx, qreal sy) {
  auto* handler = m_context.input;
  if (handler == nullptr) {
    return;
  }
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  handler->on_formation_mouse_move(sx, sy, *m_context.viewport);
}

void PlacementViewModel::on_formation_scroll(float delta) {
  auto* handler = m_context.input;
  if (handler == nullptr) {
    return;
  }
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  handler->on_formation_scroll(delta);
}

void PlacementViewModel::on_formation_confirm() {
  auto* handler = m_context.input;
  if (handler == nullptr) {
    return;
  }
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  handler->on_formation_confirm();
}

void PlacementViewModel::on_formation_cancel() {
  auto* handler = m_context.input;
  if (handler == nullptr) {
    return;
  }
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  handler->on_formation_cancel();
}

void PlacementViewModel::on_formation_drag_begin(qreal sx, qreal sy) {
  auto* handler = m_context.input;
  if (handler == nullptr) {
    return;
  }
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  handler->on_formation_drag_begin(sx, sy, *m_context.viewport);
  emit formation_options_changed();
}

void PlacementViewModel::on_formation_drag_update(qreal sx, qreal sy) {
  auto* handler = m_context.input;
  if (handler == nullptr) {
    return;
  }
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  handler->on_formation_drag_update(sx, sy, *m_context.viewport);
  emit formation_options_changed();
}

void PlacementViewModel::on_formation_drag_end() {
  const auto frame_lock = m_host.lock_frame();
  auto* handler = m_context.input;
  if (handler == nullptr) {
    return;
  }
  handler->on_formation_drag_end();
  emit formation_options_changed();
}

auto PlacementViewModel::is_dragging_formation() const -> bool {
  const auto readout = m_readout.read();
  return readout ? readout->dragging_formation : false;
}

void PlacementViewModel::set_formation_intent(const QString& intent_id) {
  const auto frame_lock = m_host.lock_frame();
  auto* commands = m_context.commands;
  if (commands == nullptr) {
    return;
  }
  commands->formation().set_formation_intent(intent_id);
  emit formation_options_changed();
}

auto PlacementViewModel::formation_intent() const -> QString {
  const auto readout = m_readout.read();
  return readout ? readout->formation_intent : QString{};
}

auto PlacementViewModel::formation_intents() const -> QStringList {
  const auto readout = m_readout.read();
  return readout ? readout->formation_intents : QStringList{};
}

auto PlacementViewModel::formation_intent_display_name(const QString& intent_id) const
    -> QString {
  const auto frame_lock = m_host.lock_frame();
  auto* commands = m_context.commands;
  return commands == nullptr
             ? QString()
             : commands->formation().formation_intent_display_name(intent_id);
}

auto PlacementViewModel::formation_intent_unavailable_reason(
    const QString& intent_id) const -> QString {
  const auto frame_lock = m_host.lock_frame();
  auto* commands = m_context.commands;
  return commands == nullptr
             ? QString()
             : commands->formation().formation_intent_unavailable_reason(intent_id);
}

auto PlacementViewModel::selected_formation_status() const -> QVariantMap {
  const auto readout = m_readout.read();
  return readout ? readout->selected_formation_status : QVariantMap{};
}

auto PlacementViewModel::formation_doctrine_options() const -> QVariantList {
  const auto readout = m_readout.read();
  return readout ? readout->formation_doctrine_options : QVariantList{};
}

auto PlacementViewModel::formation_options() const -> QVariantMap {
  const auto readout = m_readout.read();
  return readout ? readout->formation_options : QVariantMap{};
}

void PlacementViewModel::reset_formation_options() {
  const auto frame_lock = m_host.lock_frame();
  auto* commands = m_context.commands;
  if (commands != nullptr) {
    commands->formation().reset_formation_options();
    emit formation_options_changed();
  }
}

void PlacementViewModel::set_formation_frontage_preset(const QString& preset) {
  const auto frame_lock = m_host.lock_frame();
  auto* commands = m_context.commands;
  if (commands != nullptr) {
    commands->formation().set_formation_frontage_preset(preset);
    emit formation_options_changed();
  }
}

void PlacementViewModel::set_formation_depth_preset(const QString& preset) {
  const auto frame_lock = m_host.lock_frame();
  auto* commands = m_context.commands;
  if (commands != nullptr) {
    commands->formation().set_formation_depth_preset(preset);
    emit formation_options_changed();
  }
}

void PlacementViewModel::set_formation_spacing_preset(const QString& preset) {
  const auto frame_lock = m_host.lock_frame();
  auto* commands = m_context.commands;
  if (commands != nullptr) {
    commands->formation().set_formation_spacing_preset(preset);
    emit formation_options_changed();
  }
}

void PlacementViewModel::set_formation_flank_preference(const QString& preference) {
  const auto frame_lock = m_host.lock_frame();
  auto* commands = m_context.commands;
  if (commands != nullptr) {
    commands->formation().set_formation_flank_preference(preference);
    emit formation_options_changed();
  }
}

void PlacementViewModel::set_formation_ranged_placement(const QString& placement) {
  const auto frame_lock = m_host.lock_frame();
  auto* commands = m_context.commands;
  if (commands != nullptr) {
    commands->formation().set_formation_ranged_placement(placement);
    emit formation_options_changed();
  }
}

void PlacementViewModel::set_formation_reserve_rows(int rows) {
  const auto frame_lock = m_host.lock_frame();
  auto* commands = m_context.commands;
  if (commands != nullptr) {
    commands->formation().set_formation_reserve_rows(rows);
    emit formation_options_changed();
  }
}

void PlacementViewModel::set_formation_movement_policy(const QString& policy) {
  const auto frame_lock = m_host.lock_frame();
  auto* commands = m_context.commands;
  if (commands != nullptr) {
    commands->formation().set_formation_movement_policy(policy);
    emit formation_options_changed();
  }
}

void PlacementViewModel::set_formation_mixed_policy(const QString& policy) {
  const auto frame_lock = m_host.lock_frame();
  auto* commands = m_context.commands;
  if (commands != nullptr) {
    commands->formation().set_formation_mixed_policy(policy);
    emit formation_options_changed();
  }
}

void PlacementViewModel::set_formation_doctrine_override(const QString& doctrine) {
  const auto frame_lock = m_host.lock_frame();
  auto* commands = m_context.commands;
  if (commands != nullptr) {
    commands->formation().set_formation_doctrine_override(doctrine);
    emit formation_options_changed();
  }
}

void PlacementViewModel::set_formation_preserve_order(bool preserve) {
  const auto frame_lock = m_host.lock_frame();
  auto* commands = m_context.commands;
  if (commands != nullptr) {
    commands->formation().set_formation_preserve_order(preserve);
    emit formation_options_changed();
  }
}

void PlacementViewModel::adjust_formation_depth(float wheel_delta) {
  const auto frame_lock = m_host.lock_frame();
  auto* commands = m_context.commands;
  if (commands != nullptr) {
    commands->formation().adjust_formation_depth(wheel_delta);
    emit formation_options_changed();
  }
}

auto PlacementViewModel::is_placing_construction() const -> bool {
  const auto readout = m_readout.read();
  return readout ? readout->placing_construction : false;
}

auto PlacementViewModel::pending_builder_construction_type() const -> QString {
  const auto readout = m_readout.read();
  return readout ? readout->pending_builder_construction_type : QString{};
}

auto PlacementViewModel::construction_preview_active() const -> bool {
  const auto readout = m_readout.read();
  return readout ? readout->construction_preview_active : false;
}

auto PlacementViewModel::construction_preview_valid() const -> bool {
  const auto readout = m_readout.read();
  return readout ? readout->construction_preview_valid : false;
}

auto PlacementViewModel::construction_preview_reason() const -> QString {
  const auto readout = m_readout.read();
  return readout ? readout->construction_preview_reason : QString();
}

auto PlacementViewModel::construction_preview_rotatable() const -> bool {
  const auto readout = m_readout.read();
  return readout ? readout->construction_preview_rotatable : false;
}

auto PlacementViewModel::construction_preview_segment_count() const -> int {
  const auto readout = m_readout.read();
  return readout ? readout->construction_preview_segment_count : 0;
}

auto PlacementViewModel::construction_preview_valid_segment_count() const -> int {
  const auto readout = m_readout.read();
  return readout ? readout->construction_preview_valid_segment_count : 0;
}

auto PlacementViewModel::construction_preview_total_cost() const -> int {
  const auto readout = m_readout.read();
  return readout ? readout->construction_preview_total_cost : 0;
}

void PlacementViewModel::on_construction_mouse_move(qreal sx, qreal sy) {
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  auto* production = m_context.production;
  if (production != nullptr) {
    const QPointF viewport_point = m_context.viewport->map_input(sx, sy);
    production->on_construction_mouse_move(
        viewport_point.x(), viewport_point.y(), *m_context.viewport);
  }
}

void PlacementViewModel::on_construction_pointer_pressed(qreal sx, qreal sy) {
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  auto* production = m_context.production;
  if (production != nullptr) {
    const QPointF viewport_point = m_context.viewport->map_input(sx, sy);
    production->on_construction_pointer_pressed(
        viewport_point.x(), viewport_point.y(), *m_context.viewport);
  }
}

void PlacementViewModel::on_construction_pointer_released(qreal sx, qreal sy) {
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  auto* production = m_context.production;
  if (production != nullptr) {
    const QPointF viewport_point = m_context.viewport->map_input(sx, sy);
    production->on_construction_pointer_released(
        viewport_point.x(), viewport_point.y(), *m_context.viewport);
  }
  if (!is_placing_construction()) {
    m_host.set_cursor_mode(CursorMode::Normal);
  }
}

void PlacementViewModel::on_construction_scroll(float delta) {
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  auto* production = m_context.production;
  if (production != nullptr) {
    production->on_construction_scroll(delta);
  }
}

void PlacementViewModel::on_construction_confirm() {
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  auto* production = m_context.production;
  if (production != nullptr) {
    production->on_construction_confirm();
  }
  if (!is_placing_construction()) {
    m_host.set_cursor_mode(CursorMode::Normal);
  }
}

void PlacementViewModel::on_construction_cancel() {
  const auto frame_lock = m_host.lock_frame();
  auto* production = m_context.production;
  if (production != nullptr) {
    production->on_construction_cancel();
  }
  if (!is_placing_construction()) {
    m_host.set_cursor_mode(CursorMode::Normal);
  }
}

void PlacementViewModel::start_builder_construction(const QString& item_type) {
  const auto frame_lock = m_host.lock_frame();
  auto* production = m_context.production;
  if (production == nullptr) {
    return;
  }
  production->start_builder_construction(item_type);
  if (production->is_placing_construction()) {
    m_host.set_cursor_mode(item_type == QStringLiteral("collect") ? CursorMode::Collect
                                                                  : CursorMode::Build);
  }
}

auto PlacementViewModel::get_construction_info(const QString& item_type) const
    -> QVariantMap {
  return App::Economy::construction_info(item_type);
}

void PlacementViewModel::start_building_placement(const QString& building_type) {
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  auto* production = m_context.production;
  if (production != nullptr) {
    production->start_building_placement(building_type, m_context.local_owner_id);
    m_host.set_cursor_mode(CursorMode::PlaceBuilding);
  }
}

void PlacementViewModel::place_building_at_screen(qreal sx, qreal sy) {
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  auto* production = m_context.production;
  if (production != nullptr) {
    production->place_building_at_screen(
        sx, sy, m_context.local_owner_id, *m_context.viewport);
    m_host.set_cursor_mode(CursorMode::Normal);
  }
}

void PlacementViewModel::cancel_building_placement() {
  const auto frame_lock = m_host.lock_frame();
  auto* production = m_context.production;
  if (production != nullptr) {
    production->cancel_building_placement();
  }
  m_host.set_cursor_mode(CursorMode::Normal);
}

auto PlacementViewModel::pending_building_type() const -> QString {
  const auto readout = m_readout.read();
  return readout ? readout->pending_building_type : QString{};
}

} // namespace App::ViewModels
