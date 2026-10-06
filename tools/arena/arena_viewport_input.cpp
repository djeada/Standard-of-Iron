#include <QEvent>
#include <QFocusEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMap>
#include <QMouseEvent>
#include <QVector3D>
#include <QWheelEvent>

#include <algorithm>
#include <string>
#include <vector>

#include "app/commander/commander_control_controller.h"
#include "app/orders/movement_utils.h"
#include "arena_viewport.h"
#include "arena_viewport_internal.h"
#include "game/core/world.h"
#include "game/render_bridge/camera_service.h"
#include "game/render_bridge/picking_service.h"
#include "game/session/selection_service.h"
#include "game/session/session_context.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/formation_combat_geometry.h"
#include "game/systems/nation_id.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_type.h"
#include "game/units/unit.h"
#include "render/scene_renderer.h"
#include "scene/camera.h"

using namespace arena_viewport_internal;

namespace {

auto local_controllable_selection(
    Engine::Core::World* world, const std::vector<Engine::Core::EntityID>& selected_ids)
    -> std::vector<Engine::Core::EntityID> {
  std::vector<Engine::Core::EntityID> controllable_ids;
  if (world == nullptr) {
    return controllable_ids;
  }

  controllable_ids.reserve(selected_ids.size());
  for (auto entity_id : selected_ids) {
    auto* entity = world->get_entity(entity_id);
    auto* unit_component = entity != nullptr
                               ? entity->get_component<Engine::Core::UnitComponent>()
                               : nullptr;
    if ((unit_component == nullptr) || (unit_component->owner_id != k_local_owner_id) ||
        (unit_component->health <= 0) ||
        (entity != nullptr &&
         entity->has_component<Engine::Core::PendingRemovalComponent>())) {
      continue;
    }
    controllable_ids.push_back(entity_id);
  }
  return controllable_ids;
}

auto prettify_identifier(const QString& value) -> QString {
  QString label = value;
  label.replace(QLatin1Char('_'), QLatin1Char(' '));
  QStringList parts = label.split(QLatin1Char(' '), Qt::SkipEmptyParts);
  for (QString& part : parts) {
    if (!part.isEmpty()) {
      part[0] = part[0].toUpper();
    }
  }
  return parts.join(QLatin1Char(' '));
}

auto is_mounted_spawn_type(Game::Units::SpawnType spawn_type) -> bool {
  using Game::Units::SpawnType;
  return spawn_type == SpawnType::MountedSwordsman ||
         spawn_type == SpawnType::HorseArcher || spawn_type == SpawnType::HorseSpearman;
}

auto resolved_individuals_per_unit(const Engine::Core::UnitComponent& unit) -> int {
  return Game::Systems::FormationCombat::resolve_definition(unit).total_count;
}

} // namespace

auto ArenaViewport::focusNextPrevChild(bool next) -> bool {
  static_cast<void>(next);
  return false;
}

void ArenaViewport::keyPressEvent(QKeyEvent* event) {
  if (event == nullptr) {
    return;
  }

  if (rpg_interactive_key_press(event)) {
    event->accept();
    return;
  }

  if (event->key() == Qt::Key_Tab && !event->isAutoRepeat()) {
    if (enter_rpg_interactive_control()) {
      event->accept();
      return;
    }
  }

  switch (event->key()) {
  case Qt::Key_Up:
    m_pan_up_pressed = true;
    event->accept();
    return;
  case Qt::Key_Down:
    m_pan_down_pressed = true;
    event->accept();
    return;
  case Qt::Key_Left:
    m_pan_left_pressed = true;
    event->accept();
    return;
  case Qt::Key_Right:
    m_pan_right_pressed = true;
    event->accept();
    return;
  case Qt::Key_Home:
    if (!event->isAutoRepeat()) {
      if (m_terrain_review_mode) {
        set_terrain_review_overview_camera();
      } else {
        reset_camera();
      }
    }
    event->accept();
    return;
  case Qt::Key_Q:
    if (m_camera_service != nullptr && m_camera != nullptr) {
      float const yaw_step =
          (event->modifiers() & Qt::ShiftModifier) != 0 ? 8.0F : 4.0F;
      m_camera_service->yaw(*m_camera, -yaw_step);
      update();
    }
    event->accept();
    return;
  case Qt::Key_E:
    if (m_camera_service != nullptr && m_camera != nullptr) {
      float const yaw_step =
          (event->modifiers() & Qt::ShiftModifier) != 0 ? 8.0F : 4.0F;
      m_camera_service->yaw(*m_camera, yaw_step);
      update();
    }
    event->accept();
    return;
  case Qt::Key_R:
    if (m_camera_service != nullptr && m_camera != nullptr) {
      m_camera_service->tilt(
          *m_camera, 1, (event->modifiers() & Qt::ShiftModifier) != 0);
      update();
    }
    event->accept();
    return;
  case Qt::Key_F:
    if (m_camera_service != nullptr && m_camera != nullptr) {
      m_camera_service->tilt(
          *m_camera, -1, (event->modifiers() & Qt::ShiftModifier) != 0);
      update();
    }
    event->accept();
    return;
  case Qt::Key_X:
    if (!event->isAutoRepeat()) {
      select_all_local_units();
    }
    event->accept();
    return;
  case Qt::Key_Space:
    if (!event->isAutoRepeat()) {
      pause_simulation(!m_paused);
    }
    event->accept();
    return;
  case Qt::Key_F1:
  case Qt::Key_Question:
    if (!event->isAutoRepeat()) {
      m_controls_overlay_visible = !m_controls_overlay_visible;
      update();
    }
    event->accept();
    return;
  default:
    break;
  }

  QOpenGLWidget::keyPressEvent(event);
}

void ArenaViewport::keyReleaseEvent(QKeyEvent* event) {
  if (event == nullptr) {
    return;
  }

  if (rpg_interactive_key_release(event)) {
    event->accept();
    return;
  }

  switch (event->key()) {
  case Qt::Key_Up:
    m_pan_up_pressed = false;
    event->accept();
    return;
  case Qt::Key_Down:
    m_pan_down_pressed = false;
    event->accept();
    return;
  case Qt::Key_Left:
    m_pan_left_pressed = false;
    event->accept();
    return;
  case Qt::Key_Right:
    m_pan_right_pressed = false;
    event->accept();
    return;
  default:
    break;
  }

  QOpenGLWidget::keyReleaseEvent(event);
}

void ArenaViewport::focusOutEvent(QFocusEvent* event) {
  clear_camera_key_state();
  if (m_rpg_interactive) {

    m_rpg_mouse_captured = false;
    unsetCursor();
    if (m_rpg_commander_controller != nullptr) {
      m_rpg_commander_controller->release_all_input();
    }
  }
  QOpenGLWidget::focusOutEvent(event);
}

void ArenaViewport::mousePressEvent(QMouseEvent* event) {
  setFocus(Qt::MouseFocusReason);
  if (m_rpg_interactive && m_rpg_commander_controller != nullptr) {
    if (event->button() == Qt::LeftButton) {
      m_rpg_commander_controller->primary_action_down();
    } else if (event->button() == Qt::MiddleButton) {
      m_rpg_commander_controller->request_heavy_action();
    } else if (event->button() == Qt::RightButton) {
      m_rpg_commander_controller->secondary_action_down();
    }
    event->accept();
    return;
  }
  m_last_mouse_pos = event->pos();
  m_last_mouse_pos_valid = true;
  if (event->button() == Qt::LeftButton) {
    m_selection_anchor = event->pos();
    m_selection_current = event->pos();
    m_selection_drag_active = true;
  } else if (event->button() == Qt::RightButton &&
             (event->modifiers() & Qt::AltModifier) == 0) {
    issue_move_order(event->position());
  }
  QOpenGLWidget::mousePressEvent(event);
}

void ArenaViewport::mouseMoveEvent(QMouseEvent* event) {
  if (m_rpg_interactive && m_rpg_commander_controller != nullptr) {
    if (m_rpg_mouse_captured) {
      QPoint const global_pos = event->globalPosition().toPoint();
      QPoint const look_delta = global_pos - m_rpg_mouse_center;
      if (!look_delta.isNull()) {
        m_rpg_commander_controller->mouse_move(look_delta.x(), look_delta.y());
        QCursor::setPos(m_rpg_mouse_center);
      }
      update();
    }
    event->accept();
    return;
  }

  QPoint const delta = event->pos() - m_last_mouse_pos;
  m_last_mouse_pos = event->pos();
  m_last_mouse_pos_valid = true;

  if (m_camera != nullptr && (event->buttons() & Qt::MiddleButton) != 0) {
    if ((event->modifiers() & Qt::ShiftModifier) != 0) {
      const float pan_scale =
          m_terrain_review_mode
              ? std::clamp(m_camera->get_distance() * 0.004F, 0.10F, 1.4F)
              : 0.05F;
      m_camera->pan(-static_cast<float>(delta.x()) * pan_scale,
                    static_cast<float>(delta.y()) * pan_scale);
    } else {
      m_camera->orbit(-static_cast<float>(delta.x()) * 0.45F,
                      -static_cast<float>(delta.y()) * 0.25F);
    }
    update();
  } else if (m_camera != nullptr && (event->buttons() & Qt::RightButton) != 0 &&
             (event->modifiers() & Qt::AltModifier) != 0) {
    const float pan_scale =
        m_terrain_review_mode
            ? std::clamp(m_camera->get_distance() * 0.004F, 0.10F, 1.4F)
            : 0.05F;
    m_camera->pan(-static_cast<float>(delta.x()) * pan_scale,
                  static_cast<float>(delta.y()) * pan_scale);
    update();
  } else if ((event->buttons() & Qt::LeftButton) != 0 && m_selection_drag_active) {
    m_selection_current = event->pos();
    update();
  } else {
    update_hover(event->pos());
  }

  QOpenGLWidget::mouseMoveEvent(event);
}

void ArenaViewport::mouseReleaseEvent(QMouseEvent* event) {
  if (m_rpg_interactive && m_rpg_commander_controller != nullptr) {
    if (event->button() == Qt::LeftButton) {
      m_rpg_commander_controller->primary_action_up();
    } else if (event->button() == Qt::RightButton) {
      m_rpg_commander_controller->secondary_action_up();
    }
    event->accept();
    return;
  }

  if (event->button() == Qt::LeftButton && m_camera != nullptr && m_world != nullptr &&
      width() > 0 && height() > 0) {
    m_selection_current = event->pos();
    bool const additive = (event->modifiers() & Qt::ShiftModifier) != 0;
    bool const dragged = (m_selection_current - m_selection_anchor).manhattanLength() >=
                         k_selection_drag_threshold;
    if (dragged) {
      select_entities_in_rect(QRect(m_selection_anchor, m_selection_current), additive);
    } else {
      QPointF const click_pos = event->position();
      update_spawn_anchor_from_click(click_pos);
      Engine::Core::EntityID const picked =
          Game::Systems::PickingService::pick_single(static_cast<float>(click_pos.x()),
                                                     static_cast<float>(click_pos.y()),
                                                     *m_world,
                                                     *m_camera,
                                                     width(),
                                                     height(),
                                                     k_local_owner_id,
                                                     true);
      select_entity(picked, additive);
    }
    m_selection_drag_active = false;
    update();
  }

  QOpenGLWidget::mouseReleaseEvent(event);
}

void ArenaViewport::wheelEvent(QWheelEvent* event) {
  if (m_rpg_interactive) {
    event->accept();
    return;
  }
  if (m_camera != nullptr) {
    const float wheel_divisor =
        m_terrain_review_mode
            ? ((event->modifiers() & Qt::ShiftModifier) != 0 ? 180.0F : 420.0F)
            : 1200.0F;
    const float delta = static_cast<float>(event->angleDelta().y()) / wheel_divisor;
    if (m_terrain_review_mode) {
      m_camera->zoom_distance(delta, 1.0F, terrain_review_max_camera_distance());
    } else {
      m_camera->zoom_distance(delta);
    }
    update();
  }
  event->accept();
}

void ArenaViewport::leaveEvent(QEvent* event) {
  m_hovered_entity_id = 0;
  m_selection_drag_active = false;
  QOpenGLWidget::leaveEvent(event);
}

void ArenaViewport::update_hover(const QPoint& pos) {
  if (m_picking_service == nullptr || m_world == nullptr || m_camera == nullptr ||
      width() <= 0 || height() <= 0) {
    m_hovered_entity_id = 0;
    return;
  }

  m_hovered_entity_id = m_picking_service->update_hover(static_cast<float>(pos.x()),
                                                        static_cast<float>(pos.y()),
                                                        *m_world,
                                                        *m_camera,
                                                        width(),
                                                        height());
}

auto ArenaViewport::selection_system() const -> Game::Session::SelectionService* {
  return m_world != nullptr ? &Game::Session::session_for(*m_world).selection()
                            : nullptr;
}

void ArenaViewport::select_entity(Engine::Core::EntityID entity_id, bool additive) {
  auto* selection = selection_system();
  if (selection == nullptr) {
    return;
  }

  if (!additive) {
    selection->clear_selection();
  }
  if (entity_id != 0U) {
    selection->select_unit(entity_id);
  }
  update();
}

void ArenaViewport::select_entities_in_rect(const QRect& selection_rect,
                                            bool additive) {
  auto* selection = selection_system();
  if (selection == nullptr || m_world == nullptr || m_camera == nullptr ||
      width() <= 0 || height() <= 0) {
    return;
  }

  if (!additive) {
    selection->clear_selection();
  }

  QRect const normalized = selection_rect.normalized();
  auto picked = Game::Systems::PickingService::pick_in_rect(
      static_cast<float>(normalized.left()),
      static_cast<float>(normalized.top()),
      static_cast<float>(normalized.right()),
      static_cast<float>(normalized.bottom()),
      *m_world,
      *m_camera,
      width(),
      height(),
      k_local_owner_id);
  for (auto entity_id : picked) {
    selection->select_unit(entity_id);
  }
}

void ArenaViewport::select_all_local_units() {
  auto* selection = selection_system();
  if (selection == nullptr || m_world == nullptr) {
    return;
  }

  selection->clear_selection();
  for (const auto& unit : m_units) {
    if (unit == nullptr) {
      continue;
    }

    auto* entity = m_world->get_entity(unit->id());
    auto* unit_component = entity != nullptr
                               ? entity->get_component<Engine::Core::UnitComponent>()
                               : nullptr;
    if ((unit_component == nullptr) || (unit_component->owner_id != k_local_owner_id) ||
        (unit_component->health <= 0) ||
        (entity != nullptr &&
         entity->has_component<Engine::Core::PendingRemovalComponent>())) {
      continue;
    }

    selection->select_unit(entity->get_id());
  }
  update();
}

void ArenaViewport::issue_move_order(const QPointF& screen_pos) {
  if (m_world == nullptr || m_camera == nullptr || width() <= 0 || height() <= 0) {
    return;
  }

  sanitize_selection();
  auto* selection = selection_system();
  if (selection == nullptr || selection->get_selected_units().empty()) {
    return;
  }

  std::vector<Engine::Core::EntityID> const ids =
      local_controllable_selection(m_world.get(), selection->get_selected_units());
  if (ids.empty()) {
    return;
  }
  clear_forced_animation_state(ids);
  App::Utils::issue_move_or_attack_command(m_world.get(),
                                           ids,
                                           m_picking_service.get(),
                                           m_camera.get(),
                                           screen_pos.x(),
                                           screen_pos.y(),
                                           width(),
                                           height(),
                                           k_local_owner_id);
  update();
}

void ArenaViewport::update_spawn_anchor_from_click(const QPointF& screen_pos) {
  if (m_camera == nullptr || width() <= 0 || height() <= 0) {
    return;
  }

  QVector3D spawn_anchor;
  if (!Game::Systems::PickingService::screen_to_ground(
          *m_camera, width(), height(), screen_pos, spawn_anchor)) {
    return;
  }

  m_spawn_anchor_world = App::Utils::snap_to_walkable_ground(spawn_anchor);
  m_spawn_anchor_world_valid = true;
}

void ArenaViewport::apply_keyboard_camera_controls(float real_dt) {
  if (m_camera_service == nullptr || m_camera == nullptr) {
    return;
  }

  int dx = 0;
  int dz = 0;
  if (m_pan_up_pressed) {
    dz += 1;
  }
  if (m_pan_down_pressed) {
    dz -= 1;
  }
  if (m_pan_left_pressed) {
    dx -= 1;
  }
  if (m_pan_right_pressed) {
    dx += 1;
  }
  if (dx == 0 && dz == 0) {
    return;
  }

  const bool fast = (QGuiApplication::keyboardModifiers() & Qt::ShiftModifier) != 0;
  float const step =
      m_terrain_review_mode ? (fast ? 6.0F : 2.5F) : (fast ? 2.0F : 1.0F);
  float const frame_scale = std::clamp(real_dt * 60.0F, 0.5F, 2.0F);
  m_camera_service->move(*m_camera,
                         static_cast<float>(dx) * step * frame_scale,
                         static_cast<float>(dz) * step * frame_scale);
}

void ArenaViewport::clear_camera_key_state() {
  m_pan_up_pressed = false;
  m_pan_down_pressed = false;
  m_pan_left_pressed = false;
  m_pan_right_pressed = false;
}

auto ArenaViewport::owner_display_name(int owner_id) const -> QString {
  std::string const owner_name = m_session.owners().get_owner_name(owner_id);
  return owner_name.empty() ? QStringLiteral("Owner %1").arg(owner_id)
                            : QString::fromStdString(owner_name);
}

auto ArenaViewport::nation_display_name(Game::Systems::NationID nation_id) const
    -> QString {
  const auto* nation = m_session.nations().get_nation(nation_id);
  if (nation == nullptr) {
    return prettify_identifier(Game::Systems::nation_id_to_qstring(nation_id));
  }

  QString const label = QString::fromStdString(nation->display_name).trimmed();
  return label.isEmpty()
             ? prettify_identifier(Game::Systems::nation_id_to_qstring(nation_id))
             : label;
}

auto ArenaViewport::troop_display_name(Game::Systems::NationID nation_id,
                                       Game::Units::SpawnType spawnType) const
    -> QString {
  auto troop_type = Game::Units::spawn_typeToTroopType(spawnType);
  if (!troop_type.has_value()) {
    return QStringLiteral("Unknown");
  }

  const auto* nation = m_session.nations().get_nation(nation_id);
  if (nation != nullptr) {
    auto it = std::find_if(nation->available_troops.begin(),
                           nation->available_troops.end(),
                           [&troop_type](const Game::Systems::TroopType& entry) {
                             return entry.unit_type == troop_type.value();
                           });
    if (it != nation->available_troops.end()) {
      QString const label = QString::fromStdString(it->display_name).trimmed();
      if (!label.isEmpty()) {
        return label;
      }
    }
  }

  return prettify_identifier(Game::Units::troop_typeToQString(*troop_type));
}

void ArenaViewport::sanitize_selection() {
  auto* selection = selection_system();
  if (selection == nullptr || m_world == nullptr) {
    return;
  }

  auto ids = selection->get_selected_units();
  for (auto entity_id : ids) {
    auto* entity = m_world->get_entity(entity_id);
    auto* unit = entity != nullptr
                     ? entity->get_component<Engine::Core::UnitComponent>()
                     : nullptr;
    if (entity == nullptr || unit == nullptr || unit->health <= 0 ||
        entity->has_component<Engine::Core::PendingRemovalComponent>()) {
      selection->deselect_unit(entity_id);
    }
  }
}

void ArenaViewport::update_selected_entities() {
  auto* selection = selection_system();
  if (selection == nullptr || m_renderer == nullptr) {
    return;
  }
  const auto& selected = selection->get_selected_units();
  const std::vector<Engine::Core::EntityID> ids(selected.begin(), selected.end());
  m_renderer->set_selected_entities(ids);
}

void ArenaViewport::sync_selection_summary() {
  QString const summary = build_selection_summary();
  if (summary == m_last_selection_summary) {
    return;
  }
  m_last_selection_summary = summary;
  emit selection_summary_changed(summary);
}

auto ArenaViewport::build_selection_summary() const -> QString {
  auto* selection = selection_system();
  if (selection == nullptr || m_world == nullptr) {
    return QStringLiteral("No units selected.");
  }

  const auto& selected = selection->get_selected_units();
  if (selected.empty()) {
    return QStringLiteral("No units selected.");
  }

  int living_count = 0;
  int total_individuals = 0;
  int health_sum = 0;
  int max_health_sum = 0;
  int riderless_mounts = 0;
  float center_x = 0.0F;
  float center_z = 0.0F;
  QMap<QString, int> owner_counts;
  QMap<QString, int> composition_counts;

  for (auto entity_id : selected) {
    auto* entity = m_world->get_entity(entity_id);
    auto* unit = entity != nullptr
                     ? entity->get_component<Engine::Core::UnitComponent>()
                     : nullptr;
    auto* transform = entity != nullptr
                          ? entity->get_component<Engine::Core::TransformComponent>()
                          : nullptr;
    if (entity == nullptr || unit == nullptr || transform == nullptr ||
        unit->health <= 0 ||
        entity->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    ++living_count;
    total_individuals += resolved_individuals_per_unit(*unit);
    health_sum += std::max(0, unit->health);
    max_health_sum += std::max(1, unit->max_health);
    center_x += transform->position.x;
    center_z += transform->position.z;
    if (is_mounted_spawn_type(unit->spawn_type) && !unit->render_rider) {
      ++riderless_mounts;
    }
    owner_counts[owner_display_name(unit->owner_id)] += 1;
    composition_counts[QStringLiteral("%1 %2").arg(
        nation_display_name(unit->nation_id),
        troop_display_name(unit->nation_id, unit->spawn_type))] += 1;
  }

  if (living_count == 0) {
    return QStringLiteral("No units selected.");
  }

  QStringList owner_parts;
  for (auto it = owner_counts.cbegin(); it != owner_counts.cend(); ++it) {
    owner_parts << QStringLiteral("%1 x%2").arg(it.key()).arg(it.value());
  }

  QStringList composition_parts;
  for (auto it = composition_counts.cbegin(); it != composition_counts.cend(); ++it) {
    composition_parts << QStringLiteral("%1 x%2").arg(it.key()).arg(it.value());
  }
  if (composition_parts.size() > 4) {
    int const hidden_count = composition_parts.size() - 4;
    composition_parts = composition_parts.mid(0, 4);
    composition_parts << QStringLiteral("+%1 more").arg(hidden_count);
  }

  center_x /= static_cast<float>(living_count);
  center_z /= static_cast<float>(living_count);
  int const health_percent =
      max_health_sum > 0 ? (health_sum * 100) / max_health_sum : 0;

  QStringList lines;
  lines << QStringLiteral("Selected: %1").arg(living_count);
  lines << QStringLiteral("Sides: %1").arg(owner_parts.join(QStringLiteral(", ")));
  lines << QStringLiteral("Health: %1%").arg(health_percent);
  lines << QStringLiteral("Members: %1 total").arg(total_individuals);
  lines << QStringLiteral("Center: (%1, %2)")
               .arg(center_x, 0, 'f', 1)
               .arg(center_z, 0, 'f', 1);
  if (riderless_mounts > 0) {
    lines << QStringLiteral("Riderless mounts: %1").arg(riderless_mounts);
  }
  lines
      << QStringLiteral("Units: %1").arg(composition_parts.join(QStringLiteral(", ")));
  return lines.join(QLatin1Char('\n'));
}
