#pragma once

#include <QObject>
#include <QString>
#include <QVector3D>
#include <QtGui/QCursor>

class QQuickWindow;

class CursorManager : public QObject {
  Q_OBJECT
public:
  explicit CursorManager(QObject *parent = nullptr);

  QString mode() const { return m_cursorMode; }
  void setMode(const QString &mode);

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
  QString m_cursorMode = "normal";
  Qt::CursorShape m_currentCursor = Qt::ArrowCursor;
  bool m_hasFirstWaypoint = false;
  QVector3D m_firstWaypoint;
};
