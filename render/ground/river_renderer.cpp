#include "river_renderer.h"
#include "../../game/map/visibility_service.h"
#include "../gl/mesh.h"
#include "../gl/resources.h"
#include "../scene_renderer.h"
#include "ground_utils.h"
#include "linear_feature_geometry.h"
#include "linear_feature_visibility.h"
#include "map/terrain.h"
#include <QVector2D>
#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <qglobal.h>
#include <qmatrix4x4.h>
#include <qvectornd.h>
#include <vector>

namespace Render::GL {

RiverRenderer::RiverRenderer() = default;
RiverRenderer::~RiverRenderer() = default;

void RiverRenderer::configure(
    const std::vector<Game::Map::RiverSegment> &river_segments,
    float tile_size) {
  m_riverSegments = river_segments;
  m_tile_size = tile_size;
  build_meshes();
}

void RiverRenderer::build_meshes() {
  m_meshes.clear();

  if (m_riverSegments.empty()) {
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
  settings.y_offset = 0.0F;

  std::vector<Ground::LinearFeatureRibbonSegment> segments;
  segments.reserve(m_riverSegments.size());
  for (const auto &segment : m_riverSegments) {
    segments.push_back({segment.start, segment.end, segment.width});
  }

  m_meshes =
      Ground::build_linear_ribbon_meshes(segments, m_tile_size, settings);
}

void RiverRenderer::submit(Renderer &renderer, ResourceManager *resources) {
  if (m_meshes.empty() || m_riverSegments.empty()) {
    return;
  }

  Q_UNUSED(resources);

  auto &visibility = Game::Map::VisibilityService::instance();
  const bool use_visibility = visibility.is_initialized();
  Game::Map::VisibilityService::Snapshot visibility_snapshot;
  if (use_visibility) {
    visibility_snapshot = visibility.snapshot();
  }

  QMatrix4x4 model;
  model.setToIdentity();

  size_t mesh_index = 0;
  for (const auto &segment : m_riverSegments) {
    if (mesh_index >= m_meshes.size()) {
      break;
    }

    auto *mesh = m_meshes[mesh_index].get();
    ++mesh_index;

    if (mesh == nullptr) {
      continue;
    }

    float alpha = 1.0F;
    QVector3D color_multiplier(1.0F, 1.0F, 1.0F);

    if (use_visibility) {
      Ground::LinearFeatureVisibilityOptions visibility_options;
      visibility_options.treat_out_of_bounds_as_visible = true;
      const auto visibility_result = Ground::evaluate_linear_feature_visibility(
          &visibility_snapshot, segment.start, segment.end, visibility_options);
      if (!visibility_result.visible) {
        continue;
      }
      alpha = visibility_result.alpha;
      color_multiplier = visibility_result.color_multiplier;
    }

    QVector3D const final_color(color_multiplier.x(), color_multiplier.y(),
                                color_multiplier.z());

    TerrainFeatureCmd cmd;
    cmd.mesh = mesh;
    cmd.kind = TerrainFeatureCmd::Kind::River;
    cmd.model = model;
    cmd.color = final_color;
    cmd.alpha = alpha;
    renderer.terrain_feature(cmd);
  }
}

} // namespace Render::GL
