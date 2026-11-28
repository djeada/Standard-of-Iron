#pragma once

#include <QVector3D>

#include <span>

#include "game/core/entity_id.h"
#include "nav_grid_types.h"

namespace Engine::Core {
class World;
}

namespace Game::Map {
class TerrainService;
}

namespace Game::Systems {

class BuildingCollisionRegistry;

struct CameraObstructionField {
  const BuildingCollisionRegistry* buildings{nullptr};
  const Game::Map::TerrainService* terrain{nullptr};

  std::span<const GateBlocker> shut_gates{};

  float radius{0.0F};
  const Engine::Core::World* world{nullptr};
};

[[nodiscard]] auto
first_wagon_intersection_fraction(const Engine::Core::World* world,
                                  const QVector3D& start,
                                  const QVector3D& end,
                                  float radius,
                                  Engine::Core::EntityID ignore = 0) -> float;
[[nodiscard]] auto depenetrate_wagon_bodies(const Engine::Core::World* world,
                                            const QVector3D& point,
                                            float radius) -> QVector3D;
[[nodiscard]] auto slide_along_wagon_bodies(const Engine::Core::World* world,
                                            const QVector3D& from,
                                            const QVector3D& to,
                                            float radius) -> QVector3D;

[[nodiscard]] auto camera_boom_clear_fraction(const CameraObstructionField& field,
                                              const QVector3D& pivot,
                                              const QVector3D& eye) -> float;

[[nodiscard]] auto camera_body_clearance(const CameraObstructionField& field,
                                         const QVector3D& point) -> float;

[[nodiscard]] auto camera_depenetrated_point(const CameraObstructionField& field,
                                             const QVector3D& point) -> QVector3D;

} // namespace Game::Systems
