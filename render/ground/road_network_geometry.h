#pragma once

#include <QString>
#include <QVector3D>

#include <memory>
#include <vector>

#include "../../game/map/terrain.h"
#include "../gl/mesh.h"

namespace Render::Ground {

struct RoadNetworkSettings {
  const Game::Map::TerrainHeightMap* height_map = nullptr;
  const std::vector<Game::Map::Bridge>* bridges = nullptr;
  float tile_size = 1.0F;
  float y_offset = Game::Map::k_road_surface_y_offset;
};

struct RoadNetworkSurface {
  std::unique_ptr<Render::GL::Mesh> mesh;
  QString style;

  QVector3D visibility_start;
  QVector3D visibility_end;
  float visibility_width = 1.0F;
  bool junction = false;
};

[[nodiscard]] auto road_edge_fade_width(float road_width) -> float;

[[nodiscard]] auto build_road_network_surfaces(
    const std::vector<Game::Map::RoadSegment>& segments,
    const RoadNetworkSettings& settings) -> std::vector<RoadNetworkSurface>;

} // namespace Render::Ground
