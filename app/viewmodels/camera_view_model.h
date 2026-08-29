#pragma once

#include <QObject>
#include <QVariantMap>
#include <QVector3D>

#include "app/core/frame_snapshot.h"

namespace App::Core {
struct ClientContext;
class ClientHost;
} // namespace App::Core

namespace App::ViewModels {

class CameraViewModel : public QObject {
  Q_OBJECT

  Q_PROPERTY(float distance READ distance NOTIFY distance_changed)
  Q_PROPERTY(bool following_selection READ following_selection NOTIFY
                 following_selection_changed)

public:
  CameraViewModel(const App::Core::ClientContext& context,
                  App::Core::ClientHost& host,
                  QObject* parent = nullptr);

  Q_INVOKABLE void move(float dx, float dz);
  Q_INVOKABLE void elevate(float dy);
  Q_INVOKABLE void zoom(float delta);

  Q_INVOKABLE void zoom_at_screen(float delta, float sx, float sy);

  Q_INVOKABLE void drag_pan_begin(float sx, float sy);
  Q_INVOKABLE void drag_pan_update(float sx, float sy);
  Q_INVOKABLE void drag_pan_end();

  Q_INVOKABLE void orbit_drag_begin(float sx, float sy);
  Q_INVOKABLE void orbit_drag_update(float sx, float sy);
  Q_INVOKABLE void orbit_drag_end();
  Q_INVOKABLE void yaw(float degrees);
  Q_INVOKABLE void orbit(float yaw_deg, float pitch_deg);
  Q_INVOKABLE void tilt(int direction, bool shift);
  Q_INVOKABLE void reset();
  Q_INVOKABLE void look_at_world(float x, float z);
  Q_INVOKABLE void follow_selection(bool enable);
  Q_INVOKABLE void set_follow_lerp(float alpha);

  Q_INVOKABLE [[nodiscard]] QVariantMap project_world(float x, float y, float z) const;

  void publish_frame();

  [[nodiscard]] auto distance() const -> float;
  [[nodiscard]] auto following_selection() const -> bool {
    return m_following_selection;
  }

  void update_follow();
  void set_following_selection(bool enable);

signals:
  void distance_changed();
  void following_selection_changed();

  void moved();

private:
  App::Core::Published<App::Core::CameraProjection> m_projection;

  const App::Core::ClientContext& m_context;
  App::Core::ClientHost& m_host;
  bool m_following_selection = false;

  [[nodiscard]] auto
  ground_under_screen(float sx, float sy, QVector3D& out) const -> bool;
  bool m_drag_pan_active = false;
  QVector3D m_drag_pan_anchor;
  bool m_orbit_drag_active = false;
  float m_orbit_drag_last_x = 0.0F;
  float m_orbit_drag_last_y = 0.0F;
};

} // namespace App::ViewModels
