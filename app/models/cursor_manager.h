#pragma once

#include "cursor_mode.h"
#include <QObject>
#include <QString>
#include <QVector3D>
#include <QtGui/QCursor>

class QQuickWindow;

class CursorManager : public QObject {
  Q_OBJECT
public:
  explicit CursorManager(QObject *parent = nullptr);

  [[nodiscard]] auto mode() const -> CursorMode { return m_cursorMode; }
  void setMode(CursorMode mode);
  void setMode(const QString &mode);
  [[nodiscard]] auto modeString() const -> QString {
    return CursorModeUtils::toString(m_cursorMode);
  }

  void updateCursorShape(QQuickWindow *window);

  static auto globalCursorX(QQuickWindow *window) -> qreal;
  static auto globalCursorY(QQuickWindow *window) -> qreal;

  [[nodiscard]] auto hasPatrolFirstWaypoint() const -> bool {
    return m_hasFirstWaypoint;
  }
  void setPatrolFirstWaypoint(const QVector3D &waypoint);
  void clearPatrolFirstWaypoint();
  [[nodiscard]] auto getPatrolFirstWaypoint() const -> QVector3D {
    return m_firstWaypoint;
  }

signals:
  void modeChanged();
  void globalCursorChanged();

private:
  CursorMode m_cursorMode{CursorMode::Normal};
  Qt::CursorShape m_currentCursor = Qt::ArrowCursor;
  bool m_hasFirstWaypoint = false;
  QVector3D m_firstWaypoint;
};
