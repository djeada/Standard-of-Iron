#include "camera_follow_system.h"

#include <qvectornd.h>

#include <vector>

#include "../core/component_core.h"
#include "../core/world.h"
#include "../session/selection_service.h"
#include "scene/camera.h"

namespace Game::Systems {

namespace {

constexpr float k_cluster_radius = 24.0F;
constexpr float k_keep_current_share = 0.7F;

auto selected_positions(Engine::Core::World& world,
                        Game::Session::SelectionService& selection)
    -> std::vector<QVector3D> {
  std::vector<QVector3D> positions;
  for (auto id : selection.get_selected_units()) {
    if (const auto* t = world.try_get<Engine::Core::TransformComponent>(id)) {
      positions.emplace_back(t->position.x, t->position.y, t->position.z);
    }
  }
  return positions;
}

auto cluster_around(const std::vector<QVector3D>& positions,
                    const QVector3D& centre,
                    QVector3D& mean) -> int {
  QVector3D sum;
  int count = 0;
  for (const auto& p : positions) {
    float const dx = p.x() - centre.x();
    float const dz = p.z() - centre.z();
    if ((dx * dx) + (dz * dz) <= k_cluster_radius * k_cluster_radius) {
      sum += p;
      ++count;
    }
  }
  if (count > 0) {
    mean = sum / static_cast<float>(count);
  }
  return count;
}

// A scattered selection's centroid is empty ground between the groups. Follow
// the largest cluster instead, and stay on the one already framed while it still
// holds most of the troops, so the camera does not hop between groups.
auto follow_point(const std::vector<QVector3D>& positions,
                  const QVector3D& current) -> QVector3D {
  QVector3D best = positions.front();
  int best_count = 0;
  for (const auto& p : positions) {
    QVector3D mean;
    int const count = cluster_around(positions, p, mean);
    if (count > best_count) {
      best_count = count;
      best = mean;
    }
  }
  QVector3D kept;
  int const kept_count = cluster_around(positions, current, kept);
  if (kept_count > 0 &&
      static_cast<float>(kept_count) >= k_keep_current_share * best_count) {
    return kept;
  }
  return best;
}

} // namespace

void CameraFollowSystem::update(Engine::Core::World& world,
                                Game::Session::SelectionService& selection,
                                Render::GL::Camera& camera) {
  auto const positions = selected_positions(world, selection);
  if (positions.empty()) {
    return;
  }
  camera.update_follow(follow_point(positions, camera.get_target()));
}

void CameraFollowSystem::snap_to_selection(Engine::Core::World& world,
                                           Game::Session::SelectionService& selection,
                                           Render::GL::Camera& camera) {
  auto const positions = selected_positions(world, selection);
  if (positions.empty()) {
    return;
  }
  QVector3D const target = follow_point(positions, camera.get_target());
  QVector3D const offset = camera.get_position() - camera.get_target();
  camera.look_at(target + offset, target, QVector3D(0.0F, 1.0F, 0.0F));
  camera.capture_follow_offset();
}

} // namespace Game::Systems
