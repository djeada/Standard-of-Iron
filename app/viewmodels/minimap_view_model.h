#pragma once

#include <QImage>
#include <QObject>
#include <QVector3D>

#include <optional>

namespace App::Core {
struct ClientContext;
class ClientHost;
} // namespace App::Core

namespace App::ViewModels {

class CameraViewModel;

class MinimapViewModel : public QObject {
  Q_OBJECT

  Q_PROPERTY(QImage image READ image NOTIFY image_changed)

public:
  MinimapViewModel(const App::Core::ClientContext& context,
                   App::Core::ClientHost& host,
                   CameraViewModel& camera,
                   QObject* parent = nullptr);

  Q_INVOKABLE void
  on_left_click(qreal mx, qreal my, qreal minimap_width, qreal minimap_height);
  Q_INVOKABLE void
  on_right_click(qreal mx, qreal my, qreal minimap_width, qreal minimap_height);

  [[nodiscard]] auto image() const -> QImage;

  void notify_image_changed() { emit image_changed(); }

signals:
  void image_changed();

private:
  [[nodiscard]] auto world_at(qreal mx,
                              qreal my,
                              qreal minimap_width,
                              qreal minimap_height) const -> std::optional<QVector3D>;

  const App::Core::ClientContext& m_context;
  App::Core::ClientHost& m_host;
  CameraViewModel& m_camera;
};

} // namespace App::ViewModels
