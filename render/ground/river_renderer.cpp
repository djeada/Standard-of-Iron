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

RiverRenderer::RiverRenderer() = default;
RiverRenderer::~RiverRenderer() = default;

void RiverRenderer::configure(
    const std::vector<Game::Map::RiverSegment>& river_segments, float tile_size) {
  m_river_segments = river_segments;
  m_tile_size = tile_size;
  m_vis_helper.reset();
  build_meshes();
}

void RiverRenderer::build_meshes() {
  m_meshes.clear();

  if (m_river_segments.empty()) {
    return;
  }

  Ground::LinearFeatureRibbonSettings settings;
  settings.sample_step = 0.5F;
  settings.min_length_steps = 8;
  settings.edge_noise_frequencies = {2.0F, 5.0F, 10.0F};
  settings.edge_noise_weights = {0.5F, 0.3F, 0.2F};
  settings.width_variation_scale = 0.35F;
  settings.meander_frequency = 3.0F;
  settings.meander_length_scale = 0.1F;
  settings.meander_amplitude = 0.3F;
  settings.y_offset = 0.30F;

  std::vector<Ground::LinearFeatureRibbonSegment> segments;
  segments.reserve(m_river_segments.size());
  for (const auto& segment : m_river_segments) {
    segments.push_back({segment.start, segment.end, segment.width});
  }

  m_meshes = Ground::build_linear_ribbon_meshes(segments, m_tile_size, settings);
}

void RiverRenderer::submit(Renderer& renderer, ResourceManager* resources) {
  Q_UNUSED(resources);

  if (m_river_segments.empty() || m_meshes.empty()) {
    return;
  }

  auto& visibility = Game::Map::VisibilityService::instance();
  const bool use_visibility = visibility.is_initialized();

  Game::Map::VisibilityService::Snapshot vis_snapshot;
  if (use_visibility) {
    vis_snapshot = visibility.snapshot();
  }

  TerrainFeatureCmd::VisibilityResources vis_res;
  if (use_visibility) {
    vis_res = m_vis_helper.update(vis_snapshot, m_tile_size);
  }

  QMatrix4x4 model;
  model.setToIdentity();

  Ground::LinearFeatureVisibilityOptions vis_opts;
  vis_opts.treat_out_of_bounds_as_visible = true;

  std::size_t mesh_index = 0;
  for (const auto& segment : m_river_segments) {
    if (mesh_index >= m_meshes.size()) {
      break;
    }

    auto* mesh = m_meshes[mesh_index].get();
    ++mesh_index;
    if (mesh == nullptr) {
      continue;
    }

    if (use_visibility) {
      const auto vis_result = Ground::evaluate_linear_feature_visibility(
          &vis_snapshot, segment.start, segment.end, vis_opts);
      if (!vis_result.visible) {
        continue;
      }
    }

    TerrainFeatureCmd cmd;
    cmd.mesh = mesh;
    cmd.kind = LinearFeatureKind::River;
    cmd.model = model;
    cmd.color = QVector3D(1.0F, 1.0F, 1.0F);
    cmd.alpha = 1.0F;
    cmd.visibility = vis_res;
    renderer.terrain_feature(cmd);
  }
}

} // namespace Render::GL
