#pragma once

#include <QPointF>
#include <QString>
#include <QVector3D>

#include <cstdint>
#include <optional>
#include <vector>

namespace Engine::Core {
class World;
using EntityID = std::uint64_t;
} // namespace Engine::Core

namespace Render::GL {
class Camera;
}

struct ViewportState;

namespace App::Economy {

struct ConstructionPointerHit {
  QVector3D world_position;
  std::uint64_t harvest_target_id = 0;
  Engine::Core::EntityID food_target_id = 0;
};

[[nodiscard]] auto resolve_construction_pointer_hit(
    Engine::Core::World* world,
    const QString& item_type,
    int owner_id,
    const std::vector<Engine::Core::EntityID>& crew,
    const Render::GL::Camera& camera,
    const ViewportState& viewport,
    const QPointF& screen_point) -> std::optional<ConstructionPointerHit>;

[[nodiscard]] auto normalize_rotation_degrees(float angle) -> float;

[[nodiscard]] auto maybe_snap_rotated_wall_preview(Engine::Core::World* world,
                                                   const QVector3D& world_position,
                                                   bool vertical) -> QVector3D;

[[nodiscard]] auto wall_preview_is_vertical(float angle) -> bool;

[[nodiscard]] auto
nearest_clear_site(Engine::Core::World& world,
                   const QString& item_type,
                   const QVector3D& wanted,
                   float rotation_y,
                   const std::vector<Engine::Core::EntityID>& builders) -> QVector3D;

} // namespace App::Economy
