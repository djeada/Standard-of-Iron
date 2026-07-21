#pragma once

#include <QVector3D>

#include <array>
#include <cstddef>
#include <memory>
#include <vector>

#include "../../game/map/terrain.h"
#include "../gl/mesh.h"

namespace Render::Ground {

struct LinearFeatureRibbonSegment {
  QVector3D start;
  QVector3D end;
  float width = 1.0F;
};

struct LinearFeatureRibbonSettings {
  float sample_step = 0.5F;
  int min_length_steps = 8;
  int cross_section_segments = 1;
  std::array<float, 3> edge_noise_frequencies{0.0F, 0.0F, 0.0F};
  std::array<float, 3> edge_noise_weights{1.0F, 0.0F, 0.0F};
  float width_scale = 1.0F;
  float width_variation_scale = 0.0F;
  float meander_frequency = 0.0F;
  float meander_length_scale = 0.1F;
  float meander_amplitude = 0.0F;
  float y_offset = 0.0F;
  bool sample_terrain_envelope = false;
  bool follow_terrain_centerline = false;
  const Game::Map::TerrainHeightMap* height_map = nullptr;
};

struct LinearFeatureCrossSection {
  QVector3D center;
  float half_width = 0.5F;
};

struct LinearFeatureJunctionMesh {
  std::unique_ptr<Render::GL::Mesh> mesh;
  QVector3D center;
};

[[nodiscard]] auto make_river_ribbon_settings() -> LinearFeatureRibbonSettings;

[[nodiscard]] auto sample_linear_feature_cross_section(
    const LinearFeatureRibbonSegment& segment,
    float t,
    const LinearFeatureRibbonSettings& settings) -> LinearFeatureCrossSection;

struct RiverbankMeshBuildResult {
  std::unique_ptr<Render::GL::Mesh> mesh;
  std::vector<QVector3D> visibility_samples;
};

[[nodiscard]] auto build_linear_ribbon_mesh(const LinearFeatureRibbonSegment& segment,
                                            float tile_size,
                                            const LinearFeatureRibbonSettings& settings)
    -> std::unique_ptr<Render::GL::Mesh>;

[[nodiscard]] auto
build_linear_ribbon_meshes(const std::vector<LinearFeatureRibbonSegment>& segments,
                           float tile_size,
                           const LinearFeatureRibbonSettings& settings)
    -> std::vector<std::unique_ptr<Render::GL::Mesh>>;

[[nodiscard]] auto build_linear_feature_junction_meshes(
    const std::vector<LinearFeatureRibbonSegment>& segments,
    float tile_size,
    const LinearFeatureRibbonSettings& settings)
    -> std::vector<LinearFeatureJunctionMesh>;

[[nodiscard]] auto build_lake_surface_mesh(const Game::Map::Lake& lake,
                                           float tile_size,
                                           float y_offset = 0.12F)
    -> std::unique_ptr<Render::GL::Mesh>;

[[nodiscard]] auto build_bridge_mesh(const Game::Map::Bridge& bridge, float tile_size)
    -> std::unique_ptr<Render::GL::Mesh>;

[[nodiscard]] auto build_riverbank_mesh(const Game::Map::RiverSegment& segment,
                                        const Game::Map::TerrainHeightMap& height_map)
    -> RiverbankMeshBuildResult;

[[nodiscard]] auto build_riverbank_mesh(
    const std::vector<Game::Map::RiverSegment>& river_network,
    std::size_t segment_index,
    const Game::Map::TerrainHeightMap& height_map) -> RiverbankMeshBuildResult;

[[nodiscard]] auto build_lake_shore_mesh(const Game::Map::Lake& lake,
                                         const Game::Map::TerrainHeightMap& height_map)
    -> RiverbankMeshBuildResult;

} // namespace Render::Ground
