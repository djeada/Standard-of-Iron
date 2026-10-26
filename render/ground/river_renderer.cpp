#include "river_renderer.h"

#include <QVector2D>
#include <QVector3D>
#include <qglobal.h>
#include <qmatrix4x4.h>
#include <qvectornd.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <vector>

#include "game/map/bridge_geometry.h"
#include "game/map/scatter/ground_utils.h"
#include "game/map/visibility_service.h"
#include "linear_feature_geometry.h"
#include "linear_feature_visibility.h"
#include "map/biome_settings.h"
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

auto WaterRenderer::fords_near(const QVector3D& start,
                               const QVector3D& end,
                               float width) const -> std::vector<FordPatch> {
  std::vector<FordPatch> patches;
  if (m_height_map == nullptr) {
    return patches;
  }
  QVector3D const mid = (start + end) * 0.5F;
  float const half_span =
      std::hypot(end.x() - start.x(), end.z() - start.z()) * 0.5F + width;

  for (const auto& segment : m_river_segments) {
    if (!segment.ford.has_value()) {
      continue;
    }
    QVector3D along = segment.end - segment.start;
    along.setY(0.0F);
    float const length = along.length();
    if (length < 0.01F) {
      continue;
    }
    along /= length;
    QVector3D const centre = (segment.start + segment.end) * 0.5F;
    if (std::hypot(centre.x() - mid.x(), centre.z() - mid.z()) >
        half_span + length * 0.5F) {
      continue;
    }
    patches.push_back(
        {QVector4D(centre.x(), centre.z(), along.x(), along.z()),
         QVector4D(length * 0.5F + 1.0F,
                   Game::Map::river_bank_standing_half_width(segment.width) + 1.0F,
                   0.0F,
                   0.0F)});
  }

  for (const auto& ford : m_height_map->get_fords()) {
    const Game::Map::RiverSegment* host = nullptr;
    float best = std::numeric_limits<float>::max();
    for (const auto& segment : m_river_segments) {
      QVector3D const delta = segment.end - segment.start;
      float const length_sq = delta.x() * delta.x() + delta.z() * delta.z();
      if (length_sq < 1.0e-4F) {
        continue;
      }
      float const t = std::clamp(((ford.position.x() - segment.start.x()) * delta.x() +
                                  (ford.position.z() - segment.start.z()) * delta.z()) /
                                     length_sq,
                                 0.0F,
                                 1.0F);
      float const distance =
          std::hypot(ford.position.x() - (segment.start.x() + delta.x() * t),
                     ford.position.z() - (segment.start.z() + delta.z() * t));
      if (distance < best) {
        best = distance;
        host = &segment;
      }
    }
    if (host == nullptr ||
        std::hypot(ford.position.x() - mid.x(), ford.position.z() - mid.z()) >
            half_span + ford.length * 0.5F) {
      continue;
    }
    QVector3D along = host->end - host->start;
    along.setY(0.0F);
    along.normalize();
    patches.push_back(
        {QVector4D(ford.position.x(), ford.position.z(), along.x(), along.z()),
         QVector4D(ford.length * 0.5F,
                   Game::Map::river_bank_standing_half_width(host->width) + 1.0F,
                   0.0F,
                   0.0F)});
  }

  if (patches.size() >
      static_cast<std::size_t>(TerrainFeatureCmd::k_max_ford_patches)) {
    patches.resize(TerrainFeatureCmd::k_max_ford_patches);
  }
  return patches;
}

namespace {

constexpr float k_river_neighbor_core = 0.48F;
constexpr float k_lake_neighbor_core = 0.40F;
constexpr float k_neighbor_search_margin = 2.0F;

auto distance_to_segment_xz(const QVector3D& point,
                            const QVector3D& start,
                            const QVector3D& end) -> float {
  float const dx = end.x() - start.x();
  float const dz = end.z() - start.z();
  float const length_sq = dx * dx + dz * dz;
  float const t =
      length_sq > 1.0e-6F
          ? std::clamp(((point.x() - start.x()) * dx + (point.z() - start.z()) * dz) /
                           length_sq,
                       0.0F,
                       1.0F)
          : 0.0F;
  return std::hypot(point.x() - (start.x() + dx * t), point.z() - (start.z() + dz * t));
}

auto segment_gap_xz(const QVector3D& a0,
                    const QVector3D& a1,
                    const QVector3D& b0,
                    const QVector3D& b1) -> float {
  return std::min({distance_to_segment_xz(a0, b0, b1),
                   distance_to_segment_xz(a1, b0, b1),
                   distance_to_segment_xz(b0, a0, a1),
                   distance_to_segment_xz(b1, a0, a1)});
}

} // namespace

auto WaterRenderer::water_neighbors_near(const QVector3D& start,
                                         const QVector3D& end,
                                         float reach,
                                         std::ptrdiff_t own_river,
                                         std::ptrdiff_t own_lake) const
    -> std::vector<WaterNeighbor> {
  struct Candidate {
    WaterNeighbor neighbor;
    float gap = 0.0F;
  };
  std::vector<Candidate> candidates;
  for (std::size_t index = 0; index < m_river_segments.size(); ++index) {
    if (static_cast<std::ptrdiff_t>(index) == own_river) {
      continue;
    }
    const auto& segment = m_river_segments[index];
    float const radius = segment.width * k_river_neighbor_core;
    float const gap = segment_gap_xz(start, end, segment.start, segment.end);
    if (gap > reach + radius + k_neighbor_search_margin) {
      continue;
    }
    candidates.push_back(
        {{QVector4D(
              segment.start.x(), segment.start.z(), segment.end.x(), segment.end.z()),
          radius},
         gap});
  }
  for (std::size_t index = 0; index < m_lakes.size(); ++index) {
    if (static_cast<std::ptrdiff_t>(index) == own_lake) {
      continue;
    }
    const auto& lake = m_lakes[index];
    float const radius = std::min(lake.width, lake.depth) * k_lake_neighbor_core;
    float const gap = distance_to_segment_xz(lake.center, start, end);
    if (gap > reach + radius + k_neighbor_search_margin) {
      continue;
    }
    candidates.push_back(
        {{QVector4D(lake.center.x(), lake.center.z(), lake.center.x(), lake.center.z()),
          radius},
         gap});
  }
  std::sort(candidates.begin(), candidates.end(), [](const auto& lhs, const auto& rhs) {
    return lhs.gap < rhs.gap;
  });
  std::vector<WaterNeighbor> neighbors;
  for (const auto& candidate : candidates) {
    if (neighbors.size() >=
        static_cast<std::size_t>(TerrainFeatureCmd::k_max_water_neighbors)) {
      break;
    }
    neighbors.push_back(candidate.neighbor);
  }
  return neighbors;
}

void WaterRenderer::build_meshes() {
  m_meshes.clear();

  if (m_river_segments.empty() && m_lakes.empty()) {
    return;
  }

  const Ground::LinearFeatureRibbonSettings settings =
      Ground::make_water_surface_ribbon_settings(*m_height_map);

  std::vector<Ground::LinearFeatureRibbonSegment> segments;
  segments.reserve(m_river_segments.size());
  for (const auto& segment : m_river_segments) {
    segments.push_back({segment.start, segment.end, segment.width});
  }

  auto river_meshes =
      Ground::build_linear_ribbon_meshes(segments, m_tile_size, settings);
  auto river_junctions =
      Ground::build_linear_feature_junction_meshes(segments, m_tile_size, settings);
  m_meshes.reserve(river_meshes.size() + river_junctions.size() + m_lakes.size());
  for (std::size_t index = 0; index < river_meshes.size(); ++index) {
    const auto& segment = m_river_segments[index];
    m_meshes.push_back(
        {std::move(river_meshes[index]),
         WaterSurfaceKind::River,
         segment.start,
         segment.end,
         fords_near(segment.start, segment.end, segment.width),
         water_neighbors_near(segment.start,
                              segment.end,
                              Game::Map::river_drawn_half_width(segment.width),
                              static_cast<std::ptrdiff_t>(index),
                              -1)});
  }
  for (auto& junction : river_junctions) {
    m_meshes.push_back(
        {std::move(junction.mesh),
         WaterSurfaceKind::River,
         junction.center,
         junction.center,
         fords_near(junction.center, junction.center, 4.0F),
         water_neighbors_near(junction.center, junction.center, 4.0F, -1, -1)});
  }
  for (std::size_t index = 0; index < m_lakes.size(); ++index) {
    const auto& lake = m_lakes[index];
    m_meshes.push_back({Ground::build_lake_surface_mesh(lake, m_tile_size),
                        WaterSurfaceKind::Lake,
                        lake.center,
                        lake.center,
                        {},
                        water_neighbors_near(lake.center,
                                             lake.center,
                                             std::max(lake.width, lake.depth) * 0.55F,
                                             -1,
                                             static_cast<std::ptrdiff_t>(index))});
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
    cmd.ford_patch_count = static_cast<int>(surface.fords.size());
    for (std::size_t patch = 0; patch < surface.fords.size(); ++patch) {
      cmd.ford_patch_a[patch] = surface.fords[patch].a;
      cmd.ford_patch_b[patch] = surface.fords[patch].b;
    }
    cmd.water_neighbor_count = static_cast<int>(surface.neighbors.size());
    for (std::size_t neighbor = 0; neighbor < surface.neighbors.size(); ++neighbor) {
      cmd.water_neighbor_segment[neighbor] = surface.neighbors[neighbor].segment;
      cmd.water_neighbor_radius[neighbor] = surface.neighbors[neighbor].radius;
    }
    cmd.visibility = vis_res;
    renderer.terrain_feature(cmd);
  }
}

} // namespace Render::GL
