#include "road_renderer.h"
#include "../../game/map/terrain.h"
#include "../../game/map/visibility_service.h"
#include "../gl/mesh.h"
#include "../gl/resources.h"
#include "../scene_renderer.h"
#include "ground_utils.h"
#include "linear_feature_geometry.h"
#include "linear_feature_visibility.h"
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

RoadRenderer::RoadRenderer() = default;
RoadRenderer::~RoadRenderer() = default;

void RoadRenderer::configure(
    const std::vector<Game::Map::RoadSegment> &road_segments, float tile_size) {
  m_road_segments = road_segments;
  m_tile_size = tile_size;
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
  settings.edge_noise_frequencies = {1.5F, 4.0F, 0.0F};
  settings.edge_noise_weights = {0.6F, 0.4F, 0.0F};
  settings.width_variation_scale = 0.15F;
  settings.meander_frequency = 0.0F;
  settings.meander_length_scale = 0.1F;
  settings.meander_amplitude = 0.0F;
  settings.y_offset = 0.02F;

  std::vector<Ground::LinearFeatureRibbonSegment> segments;
  segments.reserve(m_road_segments.size());
  for (const auto &segment : m_road_segments) {
    segments.push_back({segment.start, segment.end, segment.width});
  }

  m_meshes =
      Ground::build_linear_ribbon_meshes(segments, m_tile_size, settings);
}

void RoadRenderer::submit(Renderer &renderer, ResourceManager *resources) {
  if (m_meshes.empty() || m_road_segments.empty()) {
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

  QVector3D const road_base_color(0.45F, 0.42F, 0.38F);

  size_t mesh_index = 0;
  for (const auto &segment : m_road_segments) {
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
      const auto visibility_result = Ground::evaluate_linear_feature_visibility(
          &visibility_snapshot, segment.start, segment.end);
      if (!visibility_result.visible) {
        continue;
      }
      alpha = visibility_result.alpha;
      color_multiplier = visibility_result.color_multiplier;
    }

    QVector3D const final_color(road_base_color.x() * color_multiplier.x(),
                                road_base_color.y() * color_multiplier.y(),
                                road_base_color.z() * color_multiplier.z());

    TerrainFeatureCmd cmd;
    cmd.mesh = mesh;
    cmd.kind = TerrainFeatureCmd::Kind::Road;
    cmd.model = model;
    cmd.color = final_color;
    cmd.alpha = alpha;
    renderer.terrain_feature(cmd);
  }
}

} // namespace Render::GL
