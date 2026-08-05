#include "placement_view_model.h"

#include "../controllers/command_controller.h"
#include "../core/input_command_handler.h"
#include "../core/production_manager.h"

namespace App::ViewModels {

PlacementViewModel::PlacementViewModel(PlacementHost& host, QObject* parent)
    : QObject(parent)
    , m_host(host) {
}

void PlacementViewModel::on_formation_command() {
  auto* handler = m_host.input_handler();
  if (handler == nullptr) {
    return;
  }
  m_host.ensure_initialized();
  handler->on_formation_command();
}

auto PlacementViewModel::any_selected_in_formation_mode() const -> bool {
  auto* handler = m_host.input_handler();
  return handler != nullptr && handler->any_selected_in_formation_mode();
}

auto PlacementViewModel::is_placing_formation() const -> bool {
  auto* commands = m_host.command_controller();
  return commands != nullptr && commands->is_placing_formation();
}

void PlacementViewModel::on_formation_mouse_move(qreal sx, qreal sy) {
  auto* handler = m_host.input_handler();
  if (handler == nullptr) {
    return;
  }
  m_host.ensure_initialized();
  handler->on_formation_mouse_move(sx, sy, m_host.viewport());
}

void PlacementViewModel::on_formation_scroll(float delta) {
  auto* handler = m_host.input_handler();
  if (handler == nullptr) {
    return;
  }
  m_host.ensure_initialized();
  handler->on_formation_scroll(delta);
}

void PlacementViewModel::on_formation_confirm() {
  auto* handler = m_host.input_handler();
  if (handler == nullptr) {
    return;
  }
  m_host.ensure_initialized();
  handler->on_formation_confirm();
}

void PlacementViewModel::on_formation_cancel() {
  auto* handler = m_host.input_handler();
  if (handler == nullptr) {
    return;
  }
  m_host.ensure_initialized();
  handler->on_formation_cancel();
}

void PlacementViewModel::on_formation_drag_begin(qreal sx, qreal sy) {
  auto* handler = m_host.input_handler();
  if (handler == nullptr) {
    return;
  }
  m_host.ensure_initialized();
  handler->on_formation_drag_begin(sx, sy, m_host.viewport());
  emit formation_options_changed();
}

void PlacementViewModel::on_formation_drag_update(qreal sx, qreal sy) {
  auto* handler = m_host.input_handler();
  if (handler == nullptr) {
    return;
  }
  m_host.ensure_initialized();
  handler->on_formation_drag_update(sx, sy, m_host.viewport());
  emit formation_options_changed();
}

void PlacementViewModel::on_formation_drag_end() {
  auto* handler = m_host.input_handler();
  if (handler == nullptr) {
    return;
  }
  handler->on_formation_drag_end();
  emit formation_options_changed();
}

auto PlacementViewModel::is_dragging_formation() const -> bool {
  auto* handler = m_host.input_handler();
  return handler != nullptr && handler->is_dragging_formation();
}

void PlacementViewModel::set_formation_intent(const QString& intent_id) {
  auto* commands = m_host.command_controller();
  if (commands == nullptr) {
    return;
  }
  commands->set_formation_intent(intent_id);
  emit formation_options_changed();
}

auto PlacementViewModel::formation_intent() const -> QString {
  auto* commands = m_host.command_controller();
  return commands == nullptr ? QString() : commands->formation_intent();
}

auto PlacementViewModel::formation_intents() const -> QStringList {
  auto* commands = m_host.command_controller();
  return commands == nullptr ? QStringList() : commands->formation_intents();
}

auto PlacementViewModel::formation_intent_display_name(const QString& intent_id) const
    -> QString {
  auto* commands = m_host.command_controller();
  return commands == nullptr ? QString()
                             : commands->formation_intent_display_name(intent_id);
}

auto PlacementViewModel::formation_intent_unavailable_reason(
    const QString& intent_id) const -> QString {
  auto* commands = m_host.command_controller();
  return commands == nullptr ? QString()
                             : commands->formation_intent_unavailable_reason(intent_id);
}

auto PlacementViewModel::selected_formation_status() const -> QVariantMap {
  auto* commands = m_host.command_controller();
  return commands == nullptr ? QVariantMap() : commands->selected_formation_status();
}

auto PlacementViewModel::formation_doctrine_options() const -> QVariantList {
  auto* commands = m_host.command_controller();
  return commands == nullptr ? QVariantList() : commands->formation_doctrine_options();
}

auto PlacementViewModel::formation_options() const -> QVariantMap {
  auto* commands = m_host.command_controller();
  return commands == nullptr ? QVariantMap() : commands->formation_options();
}

void PlacementViewModel::reset_formation_options() {
  auto* commands = m_host.command_controller();
  if (commands != nullptr) {
    commands->reset_formation_options();
    emit formation_options_changed();
  }
}

void PlacementViewModel::set_formation_frontage_preset(const QString& preset) {
  auto* commands = m_host.command_controller();
  if (commands != nullptr) {
    commands->set_formation_frontage_preset(preset);
    emit formation_options_changed();
  }
}

void PlacementViewModel::set_formation_depth_preset(const QString& preset) {
  auto* commands = m_host.command_controller();
  if (commands != nullptr) {
    commands->set_formation_depth_preset(preset);
    emit formation_options_changed();
  }
}

void PlacementViewModel::set_formation_spacing_preset(const QString& preset) {
  auto* commands = m_host.command_controller();
  if (commands != nullptr) {
    commands->set_formation_spacing_preset(preset);
    emit formation_options_changed();
  }
}

void PlacementViewModel::set_formation_flank_preference(const QString& preference) {
  auto* commands = m_host.command_controller();
  if (commands != nullptr) {
    commands->set_formation_flank_preference(preference);
    emit formation_options_changed();
  }
}

void PlacementViewModel::set_formation_ranged_placement(const QString& placement) {
  auto* commands = m_host.command_controller();
  if (commands != nullptr) {
    commands->set_formation_ranged_placement(placement);
    emit formation_options_changed();
  }
}

void PlacementViewModel::set_formation_reserve_rows(int rows) {
  auto* commands = m_host.command_controller();
  if (commands != nullptr) {
    commands->set_formation_reserve_rows(rows);
    emit formation_options_changed();
  }
}

void PlacementViewModel::set_formation_movement_policy(const QString& policy) {
  auto* commands = m_host.command_controller();
  if (commands != nullptr) {
    commands->set_formation_movement_policy(policy);
    emit formation_options_changed();
  }
}

void PlacementViewModel::set_formation_mixed_policy(const QString& policy) {
  auto* commands = m_host.command_controller();
  if (commands != nullptr) {
    commands->set_formation_mixed_policy(policy);
    emit formation_options_changed();
  }
}

void PlacementViewModel::set_formation_doctrine_override(const QString& doctrine) {
  auto* commands = m_host.command_controller();
  if (commands != nullptr) {
    commands->set_formation_doctrine_override(doctrine);
    emit formation_options_changed();
  }
}

void PlacementViewModel::set_formation_preserve_order(bool preserve) {
  auto* commands = m_host.command_controller();
  if (commands != nullptr) {
    commands->set_formation_preserve_order(preserve);
    emit formation_options_changed();
  }
}

void PlacementViewModel::adjust_formation_depth(float wheel_delta) {
  auto* commands = m_host.command_controller();
  if (commands != nullptr) {
    commands->adjust_formation_depth(wheel_delta);
    emit formation_options_changed();
  }
}

auto PlacementViewModel::is_placing_construction() const -> bool {
  auto* production = m_host.production_manager();
  return production != nullptr && production->is_placing_construction();
}

auto PlacementViewModel::pending_builder_construction_type() const -> QString {
  auto* production = m_host.production_manager();
  return production != nullptr ? production->pending_builder_construction_type()
                               : QString();
}

auto PlacementViewModel::construction_preview_active() const -> bool {
  auto* production = m_host.production_manager();
  return production != nullptr && production->construction_preview_active();
}

auto PlacementViewModel::construction_preview_valid() const -> bool {
  auto* production = m_host.production_manager();
  return production != nullptr && production->construction_preview_valid();
}

auto PlacementViewModel::construction_preview_rotatable() const -> bool {
  auto* production = m_host.production_manager();
  return production != nullptr && production->construction_preview_rotatable();
}

auto PlacementViewModel::construction_preview_segment_count() const -> int {
  auto* production = m_host.production_manager();
  return production != nullptr ? production->construction_preview_segment_count() : 0;
}

auto PlacementViewModel::construction_preview_valid_segment_count() const -> int {
  auto* production = m_host.production_manager();
  return production != nullptr ? production->construction_preview_valid_segment_count()
                               : 0;
}

auto PlacementViewModel::construction_preview_total_cost() const -> int {
  auto* production = m_host.production_manager();
  return production != nullptr ? production->construction_preview_total_cost() : 0;
}

void PlacementViewModel::on_construction_mouse_move(qreal sx, qreal sy) {
  m_host.ensure_initialized();
  auto* production = m_host.production_manager();
  if (production != nullptr) {
    const QPointF viewport_point = m_host.map_input_to_viewport(sx, sy);
    production->on_construction_mouse_move(
        viewport_point.x(), viewport_point.y(), m_host.viewport());
  }
}

void PlacementViewModel::on_construction_pointer_pressed(qreal sx, qreal sy) {
  m_host.ensure_initialized();
  auto* production = m_host.production_manager();
  if (production != nullptr) {
    const QPointF viewport_point = m_host.map_input_to_viewport(sx, sy);
    production->on_construction_pointer_pressed(
        viewport_point.x(), viewport_point.y(), m_host.viewport());
  }
}

void PlacementViewModel::on_construction_pointer_released(qreal sx, qreal sy) {
  m_host.ensure_initialized();
  auto* production = m_host.production_manager();
  if (production != nullptr) {
    const QPointF viewport_point = m_host.map_input_to_viewport(sx, sy);
    production->on_construction_pointer_released(
        viewport_point.x(), viewport_point.y(), m_host.viewport());
  }
  if (!is_placing_construction()) {
    m_host.set_cursor_mode(CursorMode::Normal);
  }
}

void PlacementViewModel::on_construction_scroll(float delta) {
  m_host.ensure_initialized();
  auto* production = m_host.production_manager();
  if (production != nullptr) {
    production->on_construction_scroll(delta);
  }
}

void PlacementViewModel::on_construction_confirm() {
  m_host.ensure_initialized();
  auto* production = m_host.production_manager();
  if (production != nullptr) {
    production->on_construction_confirm();
  }
  if (!is_placing_construction()) {
    m_host.set_cursor_mode(CursorMode::Normal);
  }
}

void PlacementViewModel::on_construction_cancel() {
  auto* production = m_host.production_manager();
  if (production != nullptr) {
    production->on_construction_cancel();
  }
  if (!is_placing_construction()) {
    m_host.set_cursor_mode(CursorMode::Normal);
  }
}

void PlacementViewModel::start_builder_construction(const QString& item_type) {
  auto* production = m_host.production_manager();
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
  auto* production = m_host.production_manager();
  return production != nullptr ? production->get_construction_info(item_type)
                               : QVariantMap();
}

void PlacementViewModel::start_building_placement(const QString& building_type) {
  m_host.ensure_initialized();
  auto* production = m_host.production_manager();
  if (production != nullptr) {
    production->start_building_placement(building_type, m_host.local_owner_id());
    m_host.set_cursor_mode(CursorMode::PlaceBuilding);
  }
}

void PlacementViewModel::place_building_at_screen(qreal sx, qreal sy) {
  m_host.ensure_initialized();
  auto* production = m_host.production_manager();
  if (production != nullptr) {
    production->place_building_at_screen(
        sx, sy, m_host.local_owner_id(), m_host.viewport());
    m_host.set_cursor_mode(CursorMode::Normal);
  }
}

void PlacementViewModel::cancel_building_placement() {
  auto* production = m_host.production_manager();
  if (production != nullptr) {
    production->cancel_building_placement();
  }
  m_host.set_cursor_mode(CursorMode::Normal);
}

auto PlacementViewModel::pending_building_type() const -> QString {
  auto* production = m_host.production_manager();
  return production != nullptr ? production->pending_building_type() : QString();
}

} // namespace App::ViewModels
