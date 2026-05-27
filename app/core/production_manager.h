#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QVector3D>

#include <cstdint>
#include <string>
#include <vector>

#include "game/systems/nation_id.h"

namespace Engine::Core {
class World;
using EntityID = unsigned int;
} // namespace Engine::Core

namespace Render::GL {
class Camera;
}

namespace Game::Systems {
class PickingService;
}

struct ViewportState;

class ProductionManager : public QObject {
  Q_OBJECT

public:
  explicit ProductionManager(Engine::Core::World* world,
                             Game::Systems::PickingService* picking_service,
                             Render::GL::Camera* camera,
                             QObject* parent = nullptr);

  void start_building_placement(const QString& building_type, int local_owner_id);
  void place_building_at_screen(qreal sx,
                                qreal sy,
                                int local_owner_id,
                                const ViewportState& viewport);
  void cancel_building_placement();
  void reset_transient_state();
  [[nodiscard]] QString pending_building_type() const {
    return m_pending_building_type;
  }
  [[nodiscard]] QString pending_builder_construction_type() const {
    return m_pending_construction_type;
  }

  [[nodiscard]] bool is_placing_construction() const {
    return m_is_placing_construction;
  }
  [[nodiscard]] bool construction_preview_valid() const {
    return m_construction_preview_valid;
  }
  [[nodiscard]] bool construction_preview_active() const {
    return m_construction_preview_active;
  }
  [[nodiscard]] bool construction_preview_rotatable() const;
  [[nodiscard]] int construction_preview_segment_count() const {
    return m_construction_preview_segment_count;
  }
  [[nodiscard]] int construction_preview_valid_segment_count() const {
    return m_construction_preview_valid_segment_count;
  }
  [[nodiscard]] int construction_preview_total_cost() const {
    return m_construction_preview_total_cost;
  }
  void on_construction_mouse_move(qreal sx, qreal sy, const ViewportState& viewport);
  void
  on_construction_pointer_pressed(qreal sx, qreal sy, const ViewportState& viewport);
  void
  on_construction_pointer_released(qreal sx, qreal sy, const ViewportState& viewport);
  void on_construction_scroll(float delta);
  void on_construction_confirm();
  void on_construction_cancel();
  void start_builder_construction(const QString& item_type);

  [[nodiscard]] QVariantMap get_selected_production_state(int local_owner_id) const;
  [[nodiscard]] QVariantMap
  get_selected_home_production_state(int local_owner_id) const;
  [[nodiscard]] QVariantMap get_selected_builder_production_state() const;
  [[nodiscard]] QVariantMap
  get_selected_marketplace_state(int local_owner_id) const;
  [[nodiscard]] QVariantMap get_unit_production_info(const QString& unit_type,
                                                     const QString& nation_id) const;
  [[nodiscard]] QVariantMap get_construction_info(const QString& item_type) const;

  auto set_rally_at_screen(qreal sx,
                           qreal sy,
                           int local_owner_id,
                           const ViewportState& viewport) -> bool;

signals:
  void placing_construction_changed();
  void construction_preview_active_changed();
  void construction_preview_valid_changed();
  void construction_preview_summary_changed();
  void construction_placement_rejected(const QString& reason);

private:
  struct WallPlacementSegment {
    int grid_x{0};
    int grid_z{0};
    QVector3D world_position;
    bool valid{false};
    std::string failure_reason;
    std::uint8_t connection_mask{0};
    float rotation_y{0.0F};
  };

  std::vector<Engine::Core::EntityID> collect_available_builders();
  QVector3D calculate_builder_center_position(
      const std::vector<Engine::Core::EntityID>& builder_ids);
  static float get_construction_build_time(const std::string& item_type);
  void set_construction_preview_active(bool active);
  void set_construction_preview_valid(bool valid);
  void clear_construction_preview_summary();
  void set_construction_preview_summary(int segment_count,
                                        int valid_segment_count,
                                        int total_cost);
  void clear_preview_entities();
  void clear_builder_preview_sites();
  void update_non_wall_construction_preview(const QVector3D& world_position);
  void clear_non_wall_construction_preview();
  void rebuild_non_wall_preview_entity(const QVector3D& world_position);
  void rebuild_wall_preview_entities();
  void rebuild_wall_preview_plan(const QVector3D& current_world_position);
  void append_preview_entity(const QString& item_type,
                             const QVector3D& world_position,
                             bool valid,
                             float rotation_y,
                             const std::string* renderer_override = nullptr);
  void confirm_wall_construction_plan();
  void confirm_direct_building_placement();
  [[nodiscard]] auto pending_construction_owner_id() const -> int;
  [[nodiscard]] auto pending_construction_nation_id() const -> Game::Systems::NationID;
  [[nodiscard]] auto is_wall_construction_mode() const -> bool;

  Engine::Core::World* m_world;
  Game::Systems::PickingService* m_picking_service;
  Render::GL::Camera* m_camera;

  QString m_pending_building_type;
  QString m_pending_construction_type;
  std::vector<Engine::Core::EntityID> m_pending_construction_builders;
  QVector3D m_construction_placement_position;
  bool m_is_placing_construction = false;
  bool m_is_direct_building_placement = false;
  int m_active_placement_owner_id = 0;
  Game::Systems::NationID m_active_placement_nation_id{
      Game::Systems::NationID::RomanRepublic};
  float m_construction_preview_rotation_y = 0.0F;
  float m_wall_preview_rotation_y = 0.0F;
  bool m_wall_preview_rotation_explicit = false;
  bool m_construction_preview_active = false;
  bool m_construction_preview_valid = false;
  int m_construction_preview_segment_count = 0;
  int m_construction_preview_valid_segment_count = 0;
  int m_construction_preview_total_cost = 0;
  std::uint64_t m_pending_harvest_target_id = 0;

  bool m_wall_drag_active = false;
  bool m_wall_drag_anchor_set = false;
  QVector3D m_wall_drag_anchor_world;
  std::vector<WallPlacementSegment> m_wall_preview_segments;
  std::vector<Engine::Core::EntityID> m_preview_entity_ids;
};
