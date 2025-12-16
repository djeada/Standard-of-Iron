#include "cursor_manager.h"
#include "app/models/cursor_mode.h"
#include <QCursor>
#include <QPoint>
#include <QQuickWindow>
#include <qglobal.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qpoint.h>
#include <qtmetamacros.h>
#include <qvectornd.h>

CursorManager::CursorManager(QObject *parent) : QObject(parent) {}

void CursorManager::set_mode(CursorMode mode) {
  if (m_cursor_mode == mode) {
    return;
  }

  if (m_cursor_mode == CursorMode::Patrol && mode != CursorMode::Patrol) {
    m_has_first_waypoint = false;
  }

  m_cursor_mode = mode;

  emit mode_changed();
  emit global_cursor_changed();
}

void CursorManager::set_mode(const QString &mode) {
  set_mode(CursorModeUtils::fromString(mode));
}

void CursorManager::update_cursor_shape(QQuickWindow *window) {
  if (window == nullptr) {
    return;
  }

  Qt::CursorShape const desired_cursor =
      (m_cursor_mode == CursorMode::Normal) ? Qt::ArrowCursor : Qt::BlankCursor;

  if (m_current_cursor != desired_cursor) {
    m_current_cursor = desired_cursor;
    window->setCursor(desired_cursor);
  }
}

auto CursorManager::global_cursor_x(QQuickWindow *window) -> qreal {
  if (window == nullptr) {
    return 0;
  }
  QPoint const global_pos = QCursor::pos();
  QPoint const local_pos = window->mapFromGlobal(global_pos);
  return local_pos.x();
}

auto CursorManager::global_cursor_y(QQuickWindow *window) -> qreal {
  if (window == nullptr) {
    return 0;
  }
  QPoint const global_pos = QCursor::pos();
  QPoint const local_pos = window->mapFromGlobal(global_pos);
  return local_pos.y();
}

void CursorManager::set_patrol_first_waypoint(const QVector3D &waypoint) {
  m_first_waypoint = waypoint;
  m_has_first_waypoint = true;
}

void CursorManager::clear_patrol_first_waypoint() { m_has_first_waypoint = false; }
