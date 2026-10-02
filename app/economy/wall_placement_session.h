#pragma once

#include <QVector3D>

#include <vector>

#include "game/systems/wall_plan_service.h"

namespace Engine::Core {
class World;
}

namespace App::Economy {

struct WallPlanSummary {
  int segment_count = 0;
  int valid_segment_count = 0;
  int total_cost = 0;
};

class WallPlacementSession {
public:
  void reset();
  void clear_plan() { m_segments.clear(); }

  void hover_without_target();
  void hover_at(const QVector3D& world_position);
  void begin_drag(const QVector3D& world_position);
  void rotate(float quarter_turns);

  [[nodiscard]] auto plan(Engine::Core::World& world,
                          const QVector3D& pointer_world,
                          int owner_id,
                          bool gate,
                          bool ladder = false) -> WallPlanSummary;

  [[nodiscard]] auto drag_active() const -> bool { return m_drag_active; }
  [[nodiscard]] auto anchor_set() const -> bool { return m_anchor_set; }
  [[nodiscard]] auto rotation_y() const -> float { return m_rotation_y; }
  [[nodiscard]] auto request() const -> const Game::Systems::WallPlanRequest& {
    return m_request;
  }
  [[nodiscard]] auto
  segments() const -> const std::vector<Game::Systems::PlannedWallSegment>& {
    return m_segments;
  }
  [[nodiscard]] auto valid_segment_count() const -> int;

private:
  bool m_drag_active = false;
  bool m_anchor_set = false;
  QVector3D m_anchor_world;
  float m_rotation_y = 0.0F;
  bool m_rotation_explicit = false;
  Game::Systems::WallPlanRequest m_request;
  std::vector<Game::Systems::PlannedWallSegment> m_segments;
};

} // namespace App::Economy
