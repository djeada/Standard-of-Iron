#pragma once

#include <QObject>
#include <QString>
#include <QVector3D>

#include <cstdint>
#include <optional>
#include <vector>

#include "app/economy/construction_preview.h"
#include "app/economy/placement_refusal.h"
#include "app/economy/placement_session.h"
#include "app/economy/wall_placement_session.h"
#include "app/orders/order_feedback.h"

namespace Engine::Core {
class World;
using EntityID = std::uint64_t;
} // namespace Engine::Core

namespace Render::GL {
class Camera;
}

namespace Game::Systems {
class PickingService;
}

struct ViewportState;

namespace App::Core {
struct OrderRequest;
}

namespace App::Economy {
struct ConstructionPointerHit;
}

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
    return m_session.pending_building_type();
  }
  [[nodiscard]] QString pending_builder_construction_type() const {
    return m_session.construction_type();
  }

  [[nodiscard]] bool is_placing_construction() const { return m_session.active(); }
  [[nodiscard]] bool construction_preview_valid() const { return m_preview.valid(); }
  [[nodiscard]] QString construction_preview_reason() const {
    return m_preview.reason();
  }
  [[nodiscard]] bool construction_preview_active() const { return m_preview.active(); }
  [[nodiscard]] bool construction_preview_rotatable() const;
  [[nodiscard]] int construction_preview_segment_count() const {
    return m_preview.segment_count();
  }
  [[nodiscard]] int construction_preview_valid_segment_count() const {
    return m_preview.valid_segment_count();
  }
  [[nodiscard]] int construction_preview_total_cost() const {
    return m_preview.total_cost();
  }
  [[nodiscard]] std::uint64_t pending_harvest_target_id() const {
    return m_preview.valid() ? m_session.harvest_target_id() : 0;
  }
  [[nodiscard]] Engine::Core::EntityID pending_food_target_id() const {
    return m_preview.valid() ? m_session.food_target_id() : 0;
  }
  [[nodiscard]] std::optional<QVector3D> release_position() const {
    return m_release_position;
  }
  [[nodiscard]] auto placement_phase() const -> App::Economy::PlacementPhase;

  void on_construction_mouse_move(qreal sx, qreal sy, const ViewportState& viewport);
  void
  on_construction_pointer_pressed(qreal sx, qreal sy, const ViewportState& viewport);
  void
  on_construction_pointer_released(qreal sx, qreal sy, const ViewportState& viewport);
  void on_construction_scroll(float delta);
  void on_construction_confirm();
  void on_construction_cancel();
  void start_builder_construction(const QString& item_type);

  auto set_rally_at_screen(qreal sx,
                           qreal sy,
                           int local_owner_id,
                           const ViewportState& viewport) -> bool;

signals:
  void placing_construction_changed();
  void construction_preview_active_changed();
  void construction_preview_valid_changed();
  void construction_preview_reason_changed();
  void construction_preview_summary_changed();
  void construction_placement_rejected(const QString& reason);
  void order_feedback(const App::Core::OrderOutcome& outcome);

private:
  enum class Handoff : std::uint8_t {
    Discard,
    BecomeSiteGhost
  };

  [[nodiscard]] auto ready_for_pointer() const -> bool;
  [[nodiscard]] auto refusal_context() const -> App::Economy::RefusalContext;
  [[nodiscard]] auto ground_refusal(const QVector3D& site) const -> QString;
  [[nodiscard]] auto nearest_legal_site(const QVector3D& wanted) const -> QVector3D;
  [[nodiscard]] auto hover_refusal(const QVector3D& site) -> QString;
  [[nodiscard]] auto pointer_hit(qreal sx, qreal sy, const ViewportState& viewport)
      -> std::optional<App::Economy::ConstructionPointerHit>;

  void hover_wall(const QVector3D& world_position);
  void hover_structure(const App::Economy::ConstructionPointerHit& hit);
  void hover_nothing();
  void release_structure(qreal sx, qreal sy, const ViewportState& viewport);
  void release_wall(qreal sx, qreal sy, const ViewportState& viewport);
  void replan_wall(const QVector3D& pointer_world);
  void show_structure_preview(const QVector3D& world_position);
  void show_structure_hover(const QVector3D& world_position);
  void drop_structure_preview();

  void confirm_wall();
  void confirm_direct_building();
  void confirm_food_harvest();
  void confirm_harvest();
  void confirm_builder_structure();

  void reject(const QString& reason);
  void reject_and_invalidate(const QString& reason);
  void submit_order(int owner_id, App::Core::OrderRequest request);
  void end_placement(Handoff handoff = Handoff::Discard);

  [[nodiscard]] auto
  collect_available_builders(bool include_busy) -> std::vector<Engine::Core::EntityID>;
  [[nodiscard]] auto
  builder_center(const std::vector<Engine::Core::EntityID>& builder_ids) -> QVector3D;
  [[nodiscard]] auto preview_owner() const -> App::Economy::PreviewOwner;

  Engine::Core::World* m_world;
  Game::Systems::PickingService* m_picking_service;
  Render::GL::Camera* m_camera;

  App::Economy::PlacementSession m_session;
  App::Economy::WallPlacementSession m_wall;
  App::Economy::ConstructionPreview m_preview;
  std::optional<QVector3D> m_release_position;
};
