#pragma once

#include <QVector3D>

namespace Game::Map {
class TerrainService;
}

namespace Game::Systems {

class BuildingCollisionRegistry;

struct CameraObstructionField {
  const BuildingCollisionRegistry* buildings{nullptr};
  const Game::Map::TerrainService* terrain{nullptr};

  float radius{0.0F};
};

[[nodiscard]] auto camera_boom_clear_fraction(const CameraObstructionField& field,
                                              const QVector3D& pivot,
                                              const QVector3D& eye) -> float;

[[nodiscard]] auto camera_body_clearance(const CameraObstructionField& field,
                                         const QVector3D& point) -> float;

[[nodiscard]] auto camera_depenetrated_point(const CameraObstructionField& field,
                                             const QVector3D& point) -> QVector3D;

} // namespace Game::Systems
