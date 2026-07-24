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

#include "../../game/map/visibility_service.h"
#include "../draw_queue.h"
#include "../gl/mesh.h"
#include "../gl/resources.h"
#include "../scene_renderer.h"
#include "ground_utils.h"
#include "linear_feature_geometry.h"
#include "linear_feature_visibility.h"
#include "map/terrain.h"

namespace Render::GL {

WaterRenderer::WaterRenderer() = default;
WaterRenderer::~WaterRenderer() = default;

void WaterRenderer::configure(
    const std::vector<Game::Map::RiverSegment>& river_segments,
    const std::vector<Game::Map::Lake>& lakes,
    const Game::Map::TerrainHeightMap& height_map) {
  m_river_segments = river_segments;
  m_lakes = lakes;
  m_tile_size = height_map.get_tile_size();
  m_height_map = &height_map;
  m_vis_helper.reset();
  build_meshes();
}

void WaterRenderer::build_meshes() {
  m_meshes.clear();

  if (m_river_segments.empty() && m_lakes.empty()) {
    return;
  }

  Ground::LinearFeatureRibbonSettings settings = Ground::make_river_ribbon_settings();
  settings.height_map = m_height_map;
  settings.follow_terrain_centerline = true;

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
    m_meshes.push_back({Ground::build_lake_surface_mesh(lake, m_tile_size),
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
    vis_res = m_vis_helper.update(*vis_snapshot, m_tile_size);
  }

  QMatrix4x4 model;
  model.setToIdentity();

  Ground::LinearFeatureVisibilityOptions vis_opts;
  vis_opts.treat_out_of_bounds_as_visible = true;

  for (const auto& surface : m_meshes) {
    auto* mesh = surface.mesh.get();
    if (mesh == nullptr) {
      continue;
    }

    if (surface.kind == WaterSurfaceKind::River) {
      const auto fog_mode = renderer.static_world_visibility_filter_enabled()
                                ? SubmissionFogMode::Revealed
                                : SubmissionFogMode::Ignore;
      if (!renderer.submission_visibility().accepts_segment(surface.visibility_start,
                                                            surface.visibility_end,
                                                            m_tile_size,
                                                            fog_mode)) {
        continue;
      }
    }

    if (vis_snapshot != nullptr && surface.kind == WaterSurfaceKind::River) {
      vis_opts.sample_count =
          Ground::recommended_linear_feature_visibility_sample_count(
              (surface.visibility_end - surface.visibility_start).length(),
              m_tile_size);
      const auto vis_result = Ground::evaluate_linear_feature_visibility(
          vis_snapshot, surface.visibility_start, surface.visibility_end, vis_opts);
      if (!vis_result.visible) {
        continue;
      }
    }

    TerrainFeatureCmd cmd;
    cmd.mesh = mesh;
    cmd.kind = LinearFeatureKind::Water;
    cmd.water_kind = surface.kind;
    cmd.model = model;
    cmd.color = QVector3D(1.0F, 1.0F, 1.0F);
    cmd.alpha = 1.0F;
    cmd.visibility = vis_res;
    renderer.terrain_feature(cmd);
  }
}

} // namespace Render::GL
