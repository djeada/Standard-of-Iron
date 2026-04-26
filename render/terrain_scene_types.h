#pragma once

#include "../game/map/terrain.h"
#include "i_render_pass.h"
#include <cstddef>

namespace Render::GL {

enum class TerrainSurfaceKind { GroundPlane, TerrainMesh };

struct TerrainSurfaceShaderParams {
  const Game::Map::BiomeSettings *biome_settings = nullptr;
  const Game::Map::TerrainField *field = nullptr;
  bool is_ground_plane = false;
};

struct TerrainSurfaceChunk {
  TerrainSurfaceKind kind = TerrainSurfaceKind::GroundPlane;
  IRenderPass *pass = nullptr;
  TerrainSurfaceShaderParams params;
};

enum class LinearFeatureKind { River, Road, Riverbank, Bridge };

enum class LinearFeatureVisibilityMode { None, SegmentSampled, TextureDriven };

struct LinearFeatureChunk {
  LinearFeatureKind kind = LinearFeatureKind::Road;
  LinearFeatureVisibilityMode visibility_mode =
      LinearFeatureVisibilityMode::None;
  IRenderPass *pass = nullptr;
  std::size_t geometry_count = 0;
};

} // namespace Render::GL
