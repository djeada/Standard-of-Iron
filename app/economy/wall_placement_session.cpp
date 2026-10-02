#include "app/economy/wall_placement_session.h"

#include <algorithm>

#include "app/economy/construction_pointer.h"
#include "game/core/world.h"
#include "game/systems/navigation/wall_network_service.h"

namespace App::Economy {

namespace {
constexpr float k_rotation_step_degrees = 90.0F;
}

void WallPlacementSession::reset() {
  m_segments.clear();
  m_drag_active = false;
  m_anchor_set = false;
  m_rotation_y = 0.0F;
  m_rotation_explicit = false;
}

void WallPlacementSession::hover_without_target() {
  m_segments.clear();
  if (!m_drag_active) {
    m_anchor_set = false;
  }
}

void WallPlacementSession::hover_at(const QVector3D& world_position) {
  if (!m_drag_active) {
    m_anchor_set = true;
    m_anchor_world = world_position;
  }
}

void WallPlacementSession::begin_drag(const QVector3D& world_position) {
  m_drag_active = true;
  m_anchor_set = true;
  m_anchor_world = world_position;
}

void WallPlacementSession::rotate(float quarter_turns) {
  m_rotation_y = normalize_rotation_degrees(m_rotation_y +
                                            quarter_turns * k_rotation_step_degrees);
  m_rotation_explicit = true;
}

auto WallPlacementSession::valid_segment_count() const -> int {
  return static_cast<int>(std::count_if(
      m_segments.begin(), m_segments.end(), [](const auto& s) { return s.valid; }));
}

auto WallPlacementSession::plan(Engine::Core::World& world,
                                const QVector3D& pointer_world,
                                int owner_id,
                                bool gate,
                                bool ladder) -> WallPlanSummary {
  using Game::Systems::WallNetworkService;
  m_segments.clear();

  QVector3D preview_world = pointer_world;
  if (!m_drag_active && m_rotation_explicit) {
    preview_world = maybe_snap_rotated_wall_preview(
        &world, pointer_world, wall_preview_is_vertical(m_rotation_y));
  }

  const QVector3D anchor_world = m_drag_active ? m_anchor_world : preview_world;
  const auto anchor =
      WallNetworkService::snap_world_position(anchor_world.x(), anchor_world.z());
  auto target =
      WallNetworkService::snap_world_position(preview_world.x(), preview_world.z());
  if (m_drag_active && m_rotation_explicit) {
    if (wall_preview_is_vertical(m_rotation_y)) {
      target.x = anchor.x;
    } else {
      target.z = anchor.z;
    }
  }

  m_request = Game::Systems::WallPlanRequest{.owner_id = owner_id,
                                             .gate = gate,
                                             .anchor = anchor,
                                             .target = target,
                                             .rotation_y = m_rotation_y};
  if (ladder) {
    // A ladder is aimed, not dragged: it follows the pointer onto the nearest
    // town face of the owner's wall.
    m_request.ladder = true;
    m_request.pointer = pointer_world;
  }
  const auto plan = Game::Systems::WallPlanService::plan(world, m_request);
  m_segments = plan.segments;
  return {.segment_count = static_cast<int>(m_segments.size()),
          .valid_segment_count = plan.valid_count,
          .total_cost = plan.valid_count * plan.wood_per_segment};
}

} // namespace App::Economy
