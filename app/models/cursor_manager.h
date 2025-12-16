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

  [[nodiscard]] auto mode() const -> CursorMode { return m_cursor_mode; }
  void set_mode(CursorMode mode);
  void set_mode(const QString &mode);
  [[nodiscard]] auto mode_string() const -> QString {
    return CursorModeUtils::toString(m_cursor_mode);
  }

  void update_cursor_shape(QQuickWindow *window);

  static auto global_cursor_x(QQuickWindow *window) -> qreal;
  static auto global_cursor_y(QQuickWindow *window) -> qreal;

  [[nodiscard]] auto has_patrol_first_waypoint() const -> bool {
    return m_has_first_waypoint;
  }
  void set_patrol_first_waypoint(const QVector3D &waypoint);
  void clear_patrol_first_waypoint();
  [[nodiscard]] auto get_patrol_first_waypoint() const -> QVector3D {
    return m_first_waypoint;
  }

signals:
  void mode_changed();
  void global_cursor_changed();

private:
  CursorMode m_cursor_mode{CursorMode::Normal};
  Qt::CursorShape m_current_cursor = Qt::ArrowCursor;
  bool m_has_first_waypoint = false;
  QVector3D m_first_waypoint;
};
