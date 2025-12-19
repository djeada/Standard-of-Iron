#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QVector3D>
#include <vector>

namespace Engine::Core {
class World;
using EntityID = unsigned int;
} // namespace Engine::Core

namespace Render::GL {
class Camera;
} // namespace Render::GL

namespace Game::Systems {
class PickingService;
} // namespace Game::Systems

// Forward declare ViewportState from input_command_handler.h
struct ViewportState;

class ProductionManager : public QObject {
  Q_OBJECT

public:
  explicit ProductionManager(Engine::Core::World *world,
                            Game::Systems::PickingService *picking_service,
                            Render::GL::Camera *camera,
                            QObject *parent = nullptr);

  void start_building_placement(const QString &building_type);
  void place_building_at_screen(qreal sx, qreal sy, int local_owner_id,
                                const ViewportState &viewport);
  void cancel_building_placement();
  [[nodiscard]] QString pending_building_type() const { return m_pending_building_type; }

  [[nodiscard]] bool is_placing_construction() const { return m_is_placing_construction; }
  void on_construction_mouse_move(qreal sx, qreal sy, const ViewportState &viewport);
  void on_construction_confirm();
  void on_construction_cancel();
  void start_builder_construction(const QString &item_type);

  [[nodiscard]] QVariantMap get_selected_production_state(int local_owner_id) const;
  [[nodiscard]] QVariantMap get_selected_builder_production_state() const;
  [[nodiscard]] static QVariantMap get_unit_production_info(const QString &unit_type);

  void set_rally_at_screen(qreal sx, qreal sy, int local_owner_id, const ViewportState &viewport);

signals:
  void placing_construction_changed();

private:
  std::vector<Engine::Core::EntityID> collect_available_builders();
  QVector3D calculate_builder_center_position(const std::vector<Engine::Core::EntityID> &builder_ids);
  static float get_construction_build_time(const std::string &item_type);

  Engine::Core::World *m_world;
  Game::Systems::PickingService *m_picking_service;
  Render::GL::Camera *m_camera;

  QString m_pending_building_type;
  QString m_pending_construction_type;
  std::vector<Engine::Core::EntityID> m_pending_construction_builders;
  QVector3D m_construction_placement_position;
  bool m_is_placing_construction = false;
};
