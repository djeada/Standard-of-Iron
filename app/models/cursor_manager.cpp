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

void CursorManager::setMode(CursorMode mode) {
  if (m_cursorMode == mode) {
    return;
  }

  if (m_cursorMode == CursorMode::Patrol && mode != CursorMode::Patrol) {
    m_hasFirstWaypoint = false;
  }

  m_cursorMode = mode;

  emit modeChanged();
  emit globalCursorChanged();
}

void CursorManager::setMode(const QString &mode) {
  setMode(CursorModeUtils::fromString(mode));
}

void CursorManager::updateCursorShape(QQuickWindow *window) {
  if (window == nullptr) {
    return;
  }

  Qt::CursorShape const desired_cursor =
      (m_cursorMode == CursorMode::Normal) ? Qt::ArrowCursor : Qt::BlankCursor;

  if (m_currentCursor != desired_cursor) {
    m_currentCursor = desired_cursor;
    window->setCursor(desired_cursor);
  }
}

auto CursorManager::globalCursorX(QQuickWindow *window) -> qreal {
  if (window == nullptr) {
    return 0;
  }
  QPoint const global_pos = QCursor::pos();
  QPoint const local_pos = window->mapFromGlobal(global_pos);
  return local_pos.x();
}

auto CursorManager::globalCursorY(QQuickWindow *window) -> qreal {
  if (window == nullptr) {
    return 0;
  }
  QPoint const global_pos = QCursor::pos();
  QPoint const local_pos = window->mapFromGlobal(global_pos);
  return local_pos.y();
}

void CursorManager::setPatrolFirstWaypoint(const QVector3D &waypoint) {
  m_firstWaypoint = waypoint;
  m_hasFirstWaypoint = true;
}

void CursorManager::clearPatrolFirstWaypoint() { m_hasFirstWaypoint = false; }
