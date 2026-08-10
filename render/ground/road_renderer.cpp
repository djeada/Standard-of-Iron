#include "road_renderer.h"

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
#include "game/map/terrain.h"
#include "game/map/visibility_service.h"
#include "linear_feature_visibility.h"
#include "render/draw_commands.h"
#include "render/gl/mesh.h"
#include "render/gl/resources.h"
#include "render/scene_renderer.h"
#include "road_network_geometry.h"

namespace Render::GL {

namespace {

auto road_color_for_style(const QString& authored_style) -> QVector3D {
  const QString style = authored_style.trimmed().toLower();
  if (style == QStringLiteral("rough")) {
    return {0.47F, 0.41F, 0.31F};
  }
  if (style == QStringLiteral("stone") || style == QStringLiteral("paved")) {
    return {0.50F, 0.49F, 0.46F};
  }
  return {0.52F, 0.44F, 0.30F};
}

auto road_surface_for_style(const QString& authored_style) -> RoadSurfaceKind {
  const QString style = authored_style.trimmed().toLower();
  if (style == QStringLiteral("rough")) {
    return RoadSurfaceKind::RoughTrack;
  }
  if (style == QStringLiteral("stone") || style == QStringLiteral("paved")) {
    return RoadSurfaceKind::Paved;
  }
  return RoadSurfaceKind::PackedEarth;
}

} // namespace

RoadRenderer::RoadRenderer() = default;
RoadRenderer::~RoadRenderer() = default;

void RoadRenderer::configure(const std::vector<Game::Map::RoadSegment>& road_segments,
                             const Game::Map::TerrainHeightMap& height_map) {
  m_road_segments = road_segments;
  m_height_map = &height_map;
  m_tile_size = height_map.get_tile_size();
  build_meshes();
}

void RoadRenderer::build_meshes() {
  m_surfaces.clear();

  if (m_road_segments.empty() || m_height_map == nullptr) {
    return;
  }

  Ground::RoadNetworkSettings settings;
  settings.height_map = m_height_map;
  settings.bridges = &m_height_map->get_bridges();
  settings.tile_size = m_tile_size;
  settings.y_offset = Game::Map::k_road_surface_y_offset;

  m_surfaces = Ground::build_road_network_surfaces(m_road_segments, settings);
}

void RoadRenderer::submit(Renderer& renderer, ResourceManager* resources) {
  Q_UNUSED(resources);

  if (m_surfaces.empty()) {
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
  for (const auto& surface : m_surfaces) {
    auto* mesh = surface.mesh.get();
    if (mesh == nullptr) {
      continue;
    }

    const auto fog_mode = renderer.static_world_visibility_filter_enabled()
                              ? SubmissionFogMode::Revealed
                              : SubmissionFogMode::Ignore;
    if (!renderer.submission_visibility().accepts_segment(surface.visibility_start,
                                                          surface.visibility_end,
                                                          surface.visibility_width,
                                                          fog_mode)) {
      continue;
    }

    if (vis_snapshot != nullptr) {
      Ground::LinearFeatureVisibilityOptions vis_opts;
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
    cmd.kind = LinearFeatureKind::Road;
    cmd.model = model;
    cmd.color = road_color_for_style(surface.style);
    cmd.road_surface_kind = road_surface_for_style(surface.style);

    cmd.alpha = 0.995F;
    cmd.visibility = vis_res;
    renderer.terrain_feature(cmd);
  }
}

} // namespace Render::GL
