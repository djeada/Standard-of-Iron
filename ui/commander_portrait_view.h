#pragma once

#include <QQuickFramebufferObject>
#include <QString>

#include <cstdint>
#include <memory>

namespace Engine::Core {
class World;
using EntityID = std::uint64_t;
} // namespace Engine::Core

namespace Render::GL {
class Renderer;
class Camera;
} // namespace Render::GL

namespace Game::Units {
class Unit;
} // namespace Game::Units

class CommanderPortraitView : public QQuickFramebufferObject {
  Q_OBJECT

  Q_PROPERTY(
      QString troopType READ troop_type WRITE set_troop_type NOTIFY troop_type_changed)
  Q_PROPERTY(QString nation READ nation WRITE set_nation NOTIFY nation_changed)
  Q_PROPERTY(QString pose READ pose WRITE set_pose NOTIFY pose_changed)
  Q_PROPERTY(bool speaking READ speaking WRITE set_speaking NOTIFY speaking_changed)

  Q_PROPERTY(bool faceValid READ face_valid NOTIFY face_anchor_changed)
  Q_PROPERTY(qreal faceX READ face_x NOTIFY face_anchor_changed)
  Q_PROPERTY(qreal faceY READ face_y NOTIFY face_anchor_changed)
  Q_PROPERTY(qreal faceRadius READ face_radius NOTIFY face_anchor_changed)
  Q_PROPERTY(qreal faceRoll READ face_roll NOTIFY face_anchor_changed)
  Q_PROPERTY(qreal faceTurn READ face_turn NOTIFY face_anchor_changed)
  Q_PROPERTY(qreal faceTilt READ face_tilt NOTIFY face_anchor_changed)
  Q_PROPERTY(qreal faceFacing READ face_facing NOTIFY face_anchor_changed)

public:
  CommanderPortraitView();
  ~CommanderPortraitView() override;

  [[nodiscard]] auto createRenderer() const -> Renderer* override;

  [[nodiscard]] auto troop_type() const -> QString { return m_troop_type; }
  void set_troop_type(const QString& value);

  [[nodiscard]] auto nation() const -> QString { return m_nation; }
  void set_nation(const QString& value);

  [[nodiscard]] auto pose() const -> QString { return m_pose; }
  void set_pose(const QString& value);

  [[nodiscard]] auto speaking() const -> bool { return m_speaking; }
  void set_speaking(bool value);

  struct FaceAnchor {
    bool valid{false};

    qreal x{0.0};
    qreal y{0.0};

    qreal radius{0.0};

    qreal roll{0.0};

    qreal turn{0.0};

    qreal tilt{0.0};

    qreal facing{0.0};
  };

  void publish_face_anchor(const FaceAnchor& anchor);

  [[nodiscard]] auto face_valid() const -> bool { return m_face.valid; }
  [[nodiscard]] auto face_x() const -> qreal { return m_face.x; }
  [[nodiscard]] auto face_y() const -> qreal { return m_face.y; }
  [[nodiscard]] auto face_radius() const -> qreal { return m_face.radius; }
  [[nodiscard]] auto face_roll() const -> qreal { return m_face.roll; }
  [[nodiscard]] auto face_turn() const -> qreal { return m_face.turn; }
  [[nodiscard]] auto face_tilt() const -> qreal { return m_face.tilt; }
  [[nodiscard]] auto face_facing() const -> qreal { return m_face.facing; }

signals:
  void troop_type_changed();
  void nation_changed();
  void pose_changed();
  void speaking_changed();
  void face_anchor_changed();

private:
  class PortraitRenderer;

  QString m_troop_type;
  QString m_nation;
  QString m_pose;
  bool m_speaking = false;
  FaceAnchor m_face{};
};
