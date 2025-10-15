#include "cursor_manager.h"
#include <QCursor>
#include <QPoint>
#include <QQuickWindow>

CursorManager::CursorManager(QObject *parent) : QObject(parent) {}

void CursorManager::setMode(CursorMode mode) {
  if (m_cursorMode == mode)
    return;

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
  if (!window)
    return;

  Qt::CursorShape desiredCursor =
      (m_cursorMode == CursorMode::Normal) ? Qt::ArrowCursor : Qt::BlankCursor;

  if (m_currentCursor != desiredCursor) {
    m_currentCursor = desiredCursor;
    window->setCursor(desiredCursor);
  }
}

qreal CursorManager::globalCursorX(QQuickWindow *window) const {
  if (!window)
    return 0;
  QPoint globalPos = QCursor::pos();
  QPoint localPos = window->mapFromGlobal(globalPos);
  return localPos.x();
}

qreal CursorManager::globalCursorY(QQuickWindow *window) const {
  if (!window)
    return 0;
  QPoint globalPos = QCursor::pos();
  QPoint localPos = window->mapFromGlobal(globalPos);
  return localPos.y();
}

void CursorManager::setPatrolFirstWaypoint(const QVector3D &waypoint) {
  m_firstWaypoint = waypoint;
  m_hasFirstWaypoint = true;
}

void CursorManager::clearPatrolFirstWaypoint() { m_hasFirstWaypoint = false; }
