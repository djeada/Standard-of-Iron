#include "riverbank_renderer.h"

#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QVector2D>
#include <QVector3D>
#include <qglobal.h>
#include <qmatrix4x4.h>
#include <qvectornd.h>

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "game/map/render_visibility_rules.h"
#include "game/map/visibility_service.h"
#include "linear_feature_geometry.h"
#include "map/terrain.h"
#include "render/gl/mesh.h"
#include "render/gl/resources.h"
#include "render/scene_renderer.h"

namespace Render::GL {

ShorelineRenderer::ShorelineRenderer() = default;
ShorelineRenderer::~ShorelineRenderer() = default;

void ShorelineRenderer::configure(
    const std::vector<Game::Map::RiverSegment>& river_segments,
    const std::vector<Game::Map::Lake>& lakes,
    const Game::Map::TerrainHeightMap& height_map,
    const Game::Map::BiomeSettings& biome_settings) {
  m_river_segments = river_segments;
  m_lakes = lakes;
  m_tile_size = height_map.get_tile_size();
  m_biome_settings = biome_settings;
  m_visibility_width = 0;
  m_visibility_height = 0;
  build_meshes(height_map);
}

void ShorelineRenderer::build_meshes(const Game::Map::TerrainHeightMap& height_map) {
  m_meshes.clear();
  m_visibility_samples.clear();
  m_water_kinds.clear();

  if (m_river_segments.empty() && m_lakes.empty()) {
    return;
  }

  for (std::size_t segment_index = 0; segment_index < m_river_segments.size();
       ++segment_index) {
    auto mesh_result =
        Ground::build_riverbank_mesh(m_river_segments, segment_index, height_map);
    m_meshes.push_back(std::move(mesh_result.mesh));
    m_visibility_samples.push_back(std::move(mesh_result.visibility_samples));
    m_water_kinds.push_back(WaterSurfaceKind::River);
  }
  for (const auto& lake : m_lakes) {
    auto mesh_result = Ground::build_lake_shore_mesh(lake, height_map);
    m_meshes.push_back(std::move(mesh_result.mesh));
    m_visibility_samples.push_back(std::move(mesh_result.visibility_samples));
    m_water_kinds.push_back(WaterSurfaceKind::Lake);
  }
}

void ShorelineRenderer::submit(Renderer& renderer, ResourceManager* resources) {
  if (m_meshes.empty()) {
    return;
  }

  Q_UNUSED(resources);

  const bool use_visibility = renderer.static_world_visibility_filter_enabled();
  const auto* visibility_snapshot =
      use_visibility ? renderer.submission_visibility().snapshot() : nullptr;
  QMatrix4x4 model;
  model.setToIdentity();

  TerrainSurfaceCmd::VisibilityResources visibility_resources;
  if (visibility_snapshot != nullptr) {
    visibility_resources = renderer.visibility_mask();
    visibility_resources.explored_alpha = m_explored_dim_factor;
  }

  const auto surface = Game::Map::make_surface_profile(m_biome_settings);
  const auto climate = Game::Map::make_climate_profile(m_biome_settings);
  size_t mesh_index = 0;
  for (const auto& mesh_owner : m_meshes) {
    auto* mesh = mesh_owner.get();
    ++mesh_index;

    if (mesh == nullptr) {
      continue;
    }

    const auto& cull_samples = m_visibility_samples[mesh_index - 1];
    if (!cull_samples.empty()) {
      const auto fog_mode = renderer.static_world_visibility_filter_enabled()
                                ? SubmissionFogMode::Revealed
                                : SubmissionFogMode::Ignore;
      if (!renderer.submission_visibility().accepts_segment(
              cull_samples.front(), cull_samples.back(), m_tile_size, fog_mode)) {
        continue;
      }
    }

    float segment_visibility = 1.0F;
    if (visibility_snapshot != nullptr) {
      const auto& samples = m_visibility_samples[mesh_index - 1];
      if (samples.empty()) {
        segment_visibility = 1.0F;
      } else {
        auto state = Game::Map::RenderVisibilityState::Hidden;
        for (const auto& sample : samples) {
          const auto sample_state = Game::Map::classify_world_visibility(
              *visibility_snapshot, sample.x(), sample.z());
          if (sample_state == Game::Map::RenderVisibilityState::Visible) {
            state = Game::Map::RenderVisibilityState::Visible;
            break;
          }
          if (sample_state == Game::Map::RenderVisibilityState::Explored) {
            state = Game::Map::RenderVisibilityState::Explored;
          }
        }

        if (state == Game::Map::RenderVisibilityState::Hidden) {
          continue;
        }
        segment_visibility = state == Game::Map::RenderVisibilityState::Visible
                                 ? 1.0F
                                 : m_explored_dim_factor;
      }
    }

    TerrainFeatureCmd cmd;
    cmd.mesh = mesh;
    cmd.kind = LinearFeatureKind::Shoreline;
    cmd.water_kind = m_water_kinds[mesh_index - 1];
    cmd.model = model;

    cmd.color = surface.grass_primary;
    cmd.biome_grass_secondary = surface.grass_secondary;
    cmd.biome_grass_dry = surface.grass_dry;
    cmd.biome_soil_color = surface.soil_color;
    cmd.biome_rock_low = surface.rock_low;
    cmd.biome_rock_high = surface.rock_high;
    cmd.biome_snow_color = climate.snow_color;
    cmd.biome_moisture = climate.moisture_level;
    cmd.biome_rock_exposure = climate.rock_exposure;
    cmd.biome_snow_coverage = climate.snow_coverage;
    cmd.biome_ground_type = static_cast<int>(m_biome_settings.ground_type);
    cmd.ambient_boost = surface.terrain_ambient_boost * 0.95F;
    cmd.alpha = segment_visibility;
    cmd.visibility = visibility_resources;
    renderer.terrain_feature(cmd);
  }
}

} // namespace Render::GL
