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

  CursorMode mode() const { return m_cursorMode; }
  void setMode(CursorMode mode);
  void setMode(const QString &mode);
  QString modeString() const { return CursorModeUtils::toString(m_cursorMode); }

  void updateCursorShape(QQuickWindow *window);

  qreal globalCursorX(QQuickWindow *window) const;
  qreal globalCursorY(QQuickWindow *window) const;

  bool hasPatrolFirstWaypoint() const { return m_hasFirstWaypoint; }
  void setPatrolFirstWaypoint(const QVector3D &waypoint);
  void clearPatrolFirstWaypoint();
  QVector3D getPatrolFirstWaypoint() const { return m_firstWaypoint; }

signals:
  void modeChanged();
  void globalCursorChanged();

private:
  CursorMode m_cursorMode{CursorMode::Normal};
  Qt::CursorShape m_currentCursor = Qt::ArrowCursor;
  bool m_hasFirstWaypoint = false;
  QVector3D m_firstWaypoint;
};
