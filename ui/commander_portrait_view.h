#pragma once

#include <QQmlEngine>
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
  QML_ELEMENT

  Q_PROPERTY(
      QString troopType READ troop_type WRITE set_troop_type NOTIFY troop_type_changed)
  Q_PROPERTY(QString nation READ nation WRITE set_nation NOTIFY nation_changed)
  Q_PROPERTY(QString pose READ pose WRITE set_pose NOTIFY pose_changed)
  Q_PROPERTY(bool speaking READ speaking WRITE set_speaking NOTIFY speaking_changed)
  Q_PROPERTY(bool talking READ talking WRITE set_talking NOTIFY talking_changed)

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

  [[nodiscard]] auto talking() const -> bool { return m_talking; }
  void set_talking(bool value);

signals:
  void troop_type_changed();
  void nation_changed();
  void pose_changed();
  void speaking_changed();
  void talking_changed();

private:
  class PortraitRenderer;

  QString m_troop_type;
  QString m_nation;
  QString m_pose;
  bool m_speaking = false;
  bool m_talking = false;
};
