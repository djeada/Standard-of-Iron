#pragma once

#include <QObject>
#include <QVariantMap>

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
  Q_INVOKABLE void yaw(float degrees);
  Q_INVOKABLE void orbit(float yaw_deg, float pitch_deg);
  Q_INVOKABLE void tilt(int direction, bool shift);
  Q_INVOKABLE void reset();
  Q_INVOKABLE void follow_selection(bool enable);
  Q_INVOKABLE void set_follow_lerp(float alpha);

  Q_INVOKABLE [[nodiscard]] QVariantMap project_world(float x, float y, float z) const;

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
  const App::Core::ClientContext& m_context;
  App::Core::ClientHost& m_host;
  bool m_following_selection = false;
};

} // namespace App::ViewModels
