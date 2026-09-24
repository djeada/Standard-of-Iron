#include "river_renderer.h"

#include <QVector2D>
#include <QVector3D>
#include <qglobal.h>
#include <qmatrix4x4.h>
#include <qvectornd.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <vector>

#include "game/map/scatter/ground_utils.h"
#include "game/map/visibility_service.h"
#include "linear_feature_geometry.h"
#include "linear_feature_visibility.h"
#include "map/terrain.h"
#include "render/draw_commands.h"
#include "render/gl/mesh.h"
#include "render/gl/mesh_prewarmer.h"
#include "render/gl/resources.h"
#include "render/scene_renderer.h"

namespace Render::GL {

auto WaterRenderer::prewarm_gpu_resources() -> bool {
  return prewarm_mesh_buffers(m_meshes, [](auto& entry) { return entry.mesh.get(); });
}

namespace {

// Height of every water surface above its datum (the river network's level or
// the lake's centre height); the carved bed sits 0.10 below the datum.
constexpr float k_water_surface_lift = 0.02F;
constexpr float k_lake_surface_lift = k_water_surface_lift + 0.006F;

} // namespace

WaterRenderer::WaterRenderer() = default;
WaterRenderer::~WaterRenderer() = default;

void WaterRenderer::configure(
    const std::vector<Game::Map::RiverSegment>& river_segments,
    const std::vector<Game::Map::Lake>& lakes,
    const Game::Map::TerrainHeightMap& height_map,
    const Game::Map::BiomeSettings& biome_settings) {
  m_river_segments = river_segments;
  m_lakes = lakes;
  m_tile_size = height_map.get_tile_size();
  m_height_map = &height_map;
  m_biome_settings = biome_settings;
  build_meshes();
}

void WaterRenderer::build_meshes() {
  m_meshes.clear();

  if (m_river_segments.empty() && m_lakes.empty()) {
    return;
  }

  Ground::LinearFeatureRibbonSettings settings = Ground::make_river_ribbon_settings();
  settings.height_map = m_height_map;
  settings.use_segment_elevation_profile = true;
  settings.y_offset = k_water_surface_lift;
  settings.shared_junction_drop = 0.008F;

  // An end that meets another segment, or runs into a lake, is not a shore.
  const float join_distance = std::max(m_tile_size * 0.30F, 0.05F);
  auto continues_into_water = [&](const QVector3D& point, std::size_t self) {
    for (std::size_t other = 0; other < m_river_segments.size(); ++other) {
      if (other == self) {
        continue;
      }
      const auto& segment = m_river_segments[other];
      if ((segment.start - point).toVector2D().length() <= join_distance ||
          (segment.end - point).toVector2D().length() <= join_distance) {
        return true;
      }
    }
    return std::any_of(m_lakes.begin(), m_lakes.end(), [&](const auto& lake) {
      return Game::Map::point_in_lake(lake, point.x(), point.z());
    });
  };

  std::vector<Ground::LinearFeatureRibbonSegment> segments;
  segments.reserve(m_river_segments.size());
  for (std::size_t index = 0; index < m_river_segments.size(); ++index) {
    const auto& segment = m_river_segments[index];
    Ground::LinearFeatureRibbonSegment ribbon{
        segment.start, segment.end, segment.width};
    ribbon.start_is_joint = continues_into_water(segment.start, index);
    ribbon.end_is_joint = continues_into_water(segment.end, index);
    segments.push_back(ribbon);
  }

  auto river_meshes =
      Ground::build_linear_ribbon_meshes(segments, m_tile_size, settings);
  auto river_junctions =
      Ground::build_linear_feature_junction_meshes(segments, m_tile_size, settings);
  m_meshes.reserve(river_meshes.size() + river_junctions.size() + m_lakes.size());
  for (std::size_t index = 0; index < river_meshes.size(); ++index) {
    m_meshes.push_back({std::move(river_meshes[index]),
                        WaterSurfaceKind::River,
                        m_river_segments[index].start,
                        m_river_segments[index].end});
  }
  for (auto& junction : river_junctions) {
    m_meshes.push_back({std::move(junction.mesh),
                        WaterSurfaceKind::River,
                        junction.center,
                        junction.center});
  }
  for (const auto& lake : m_lakes) {
    // Lakes sit a hair above the rivers (not the 10 cm they used to), so a
    // river mouth runs level into the lake and the lake covers the river's
    // end where the two overlap.
    m_meshes.push_back({Ground::build_lake_surface_mesh(
                            lake, m_tile_size, k_lake_surface_lift, &m_river_segments),
                        WaterSurfaceKind::Lake,
                        lake.center,
                        lake.center});
  }
}

void WaterRenderer::submit(Renderer& renderer, ResourceManager* resources) {
  Q_UNUSED(resources);

  if (m_meshes.empty()) {
    return;
  }

  const auto* vis_snapshot = renderer.static_world_visibility_filter_enabled()
                                 ? renderer.submission_visibility().snapshot()
                                 : nullptr;

  TerrainSurfaceCmd::VisibilityResources vis_res;
  if (vis_snapshot != nullptr) {
    vis_res = renderer.visibility_mask();
  }

  QMatrix4x4 model;
  model.setToIdentity();
  const auto surface_profile = Game::Map::make_surface_profile(m_biome_settings);
  const auto climate = Game::Map::make_climate_profile(m_biome_settings);

  for (const auto& surface : m_meshes) {
    auto* mesh = surface.mesh.get();
    if (mesh == nullptr) {
      continue;
    }

    if (surface.kind == WaterSurfaceKind::River &&
        !renderer.submission_visibility().accepts_segment(surface.visibility_start,
                                                          surface.visibility_end,
                                                          m_tile_size,
                                                          SubmissionFogMode::Ignore)) {
      continue;
    }

    TerrainFeatureCmd cmd;
    cmd.mesh = mesh;
    cmd.kind = LinearFeatureKind::Water;
    cmd.water_kind = surface.kind;
    cmd.model = model;
    cmd.color = QVector3D(1.0F, 1.0F, 1.0F);
    cmd.biome_soil_color = surface_profile.soil_color;
    cmd.biome_moisture = climate.moisture_level;
    cmd.biome_snow_coverage = climate.snow_coverage;
    cmd.alpha = 1.0F;
    cmd.visibility = vis_res;
    cmd.height = renderer.terrain_height_resources();
    renderer.terrain_feature(cmd);
  }
}

} // namespace Render::GL
