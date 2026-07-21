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

#include "../../game/map/terrain.h"
#include "../../game/map/visibility_service.h"
#include "../draw_queue.h"
#include "../gl/mesh.h"
#include "../gl/resources.h"
#include "../scene_renderer.h"
#include "ground_utils.h"
#include "linear_feature_geometry.h"
#include "linear_feature_visibility.h"

namespace Render::GL {

namespace {

auto road_color_for_style(const QString& authored_style) -> QVector3D {
  const QString style = authored_style.trimmed().toLower();
  if (style == QStringLiteral("rough")) {
    return {0.34F, 0.29F, 0.22F};
  }
  if (style == QStringLiteral("stone") || style == QStringLiteral("paved")) {
    return {0.48F, 0.47F, 0.44F};
  }
  return {0.45F, 0.42F, 0.38F};
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
  m_vis_helper.reset();
  build_meshes();
}

void RoadRenderer::build_meshes() {
  m_meshes.clear();

  if (m_road_segments.empty()) {
    return;
  }

  Ground::LinearFeatureRibbonSettings settings;
  settings.sample_step = 0.5F;
  settings.min_length_steps = 8;
  settings.cross_section_segments = 4;
  settings.edge_noise_frequencies = {0.18F, 0.55F, 1.35F};
  settings.edge_noise_weights = {0.62F, 0.28F, 0.10F};
  settings.width_variation_scale = 0.09F;
  settings.meander_frequency = 0.0F;
  settings.meander_length_scale = 0.1F;
  settings.meander_amplitude = 0.0F;
  settings.y_offset = Game::Map::k_road_surface_y_offset;
  settings.sample_terrain_envelope = false;
  settings.height_map = m_height_map;

  std::vector<Ground::LinearFeatureRibbonSegment> segments;
  segments.reserve(m_road_segments.size());
  for (const auto& segment : m_road_segments) {
    QVector3D direction = segment.end - segment.start;
    direction.setY(0.0F);
    if (direction.lengthSquared() > 0.0001F) {
      direction.normalize();
    }
    const float join_overlap = segment.width * 0.45F;
    segments.push_back({segment.start - direction * join_overlap,
                        segment.end + direction * join_overlap,
                        segment.width});
  }

  m_meshes = Ground::build_linear_ribbon_meshes(segments, m_tile_size, settings);
}

void RoadRenderer::submit(Renderer& renderer, ResourceManager* resources) {
  Q_UNUSED(resources);

  if (m_road_segments.empty() || m_meshes.empty()) {
    return;
  }

  auto& visibility = Game::Map::VisibilityService::instance();
  const bool use_visibility =
      renderer.static_world_visibility_filter_enabled() && visibility.is_initialized();

  auto vis_snapshot = use_visibility ? visibility.snapshot_ptr() : nullptr;

  TerrainSurfaceCmd::VisibilityResources vis_res;
  if (vis_snapshot != nullptr) {
    vis_res = m_vis_helper.update(*vis_snapshot, m_tile_size);
  }

  QMatrix4x4 model;
  model.setToIdentity();
  std::size_t mesh_index = 0;
  for (const auto& segment : m_road_segments) {
    if (mesh_index >= m_meshes.size()) {
      break;
    }

    auto* mesh = m_meshes[mesh_index].get();
    ++mesh_index;
    if (mesh == nullptr) {
      continue;
    }

    if (vis_snapshot != nullptr) {
      Ground::LinearFeatureVisibilityOptions vis_opts;
      vis_opts.sample_count =
          Ground::recommended_linear_feature_visibility_sample_count(
              (segment.end - segment.start).length(), m_tile_size);
      const auto vis_result = Ground::evaluate_linear_feature_visibility(
          vis_snapshot.get(), segment.start, segment.end, vis_opts);
      if (!vis_result.visible) {
        continue;
      }
    }

    TerrainFeatureCmd cmd;
    cmd.mesh = mesh;
    cmd.kind = LinearFeatureKind::Road;
    cmd.model = model;
    cmd.color = road_color_for_style(segment.style);
    cmd.road_surface_kind = road_surface_for_style(segment.style);
    cmd.alpha = 1.0F;
    cmd.visibility = vis_res;
    renderer.terrain_feature(cmd);
  }
}

} // namespace Render::GL
