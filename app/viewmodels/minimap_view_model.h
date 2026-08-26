#pragma once

#include <QElapsedTimer>
#include <QImage>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVector3D>

#include <array>
#include <cstdint>
#include <optional>

namespace App::Core {
struct ClientContext;
class ClientHost;
struct OrderMarker;
} // namespace App::Core

namespace App::ViewModels {

class CameraViewModel;

enum class MinimapAlert : std::uint8_t {
  TroopsAttacked = 0,
  StructureAttacked = 1,
  CaptureStarted = 2,
  CaptureContested = 3,
  CaptureFinished = 4,
  ShrineStirred = 5,
};

class MinimapViewModel : public QObject {
  Q_OBJECT

  Q_PROPERTY(QImage image READ image NOTIFY image_changed)
  Q_PROPERTY(QVariantList destinations READ destinations NOTIFY destinations_changed)
  Q_PROPERTY(QVariantList landmarks READ landmarks NOTIFY landmarks_changed)

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
  [[nodiscard]] auto destinations() const -> QVariantList { return m_destinations; }
  [[nodiscard]] auto landmarks() const -> QVariantList { return m_landmarks; }

  void note_order_marker(const App::Core::OrderMarker& marker);

  [[nodiscard]] auto consume_alert_budget() -> bool;

  void note_alert(MinimapAlert kind,
                  float world_x,
                  float world_z,
                  int subject_owner_id,
                  int actor_owner_id);

  void set_destinations(QVariantList destinations);
  void set_landmarks(QVariantList landmarks);
  void clear_overlays();

  void notify_image_changed() { emit image_changed(); }

signals:
  void image_changed();
  void destinations_changed();
  void landmarks_changed();

  void order_ping(
      qreal nx, qreal ny, const QString& color, bool rejected, qreal lifetime_seconds);

  void event_blip(qreal nx,
                  qreal ny,
                  const QString& kind,
                  const QString& relation,
                  qreal lifetime_seconds);

private:
  struct AlertSlot {
    std::uint32_t key = 0;
    qint64 last_ms = 0;
    bool used = false;
  };

  static constexpr std::size_t k_alert_slot_count = 32;

  [[nodiscard]] auto world_at(qreal mx,
                              qreal my,
                              qreal minimap_width,
                              qreal minimap_height) const -> std::optional<QVector3D>;

  [[nodiscard]] auto relation_for(int owner_id) const -> QString;
  [[nodiscard]] auto accept_alert(MinimapAlert kind, float nx, float ny) -> bool;

  const App::Core::ClientContext& m_context;
  App::Core::ClientHost& m_host;
  CameraViewModel& m_camera;

  QVariantList m_destinations;
  QVariantList m_landmarks;

  QElapsedTimer m_alert_clock;
  std::array<AlertSlot, k_alert_slot_count> m_alert_slots{};
  qint64 m_last_considered_ms = 0;
};

} // namespace App::ViewModels
