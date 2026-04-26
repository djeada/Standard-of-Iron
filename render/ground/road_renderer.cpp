#include "road_renderer.h"
#include "../../game/map/terrain.h"
#include "../../game/map/visibility_service.h"
#include "../gl/mesh.h"
#include "../gl/resources.h"
#include "../scene_renderer.h"
#include "ground_utils.h"
#include "linear_feature_geometry.h"
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

  for (const auto &segment : m_road_segments) {
    m_meshes.push_back(Ground::build_linear_ribbon_mesh(
        {segment.start, segment.end, segment.width}, m_tile_size, settings));
  }
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

    QVector3D dir = segment.end - segment.start;
    float const length = dir.length();

    float alpha = 1.0F;
    QVector3D color_multiplier(1.0F, 1.0F, 1.0F);

    if (use_visibility) {
      int max_visibility_state = 0;
      dir.normalize();

      int const samples_per_segment = 5;
      for (int i = 0; i < samples_per_segment; ++i) {
        float const t =
            static_cast<float>(i) / static_cast<float>(samples_per_segment - 1);
        QVector3D const pos = segment.start + dir * (length * t);

        if (visibility_snapshot.isVisibleWorld(pos.x(), pos.z())) {
          max_visibility_state = 2;
          break;
        }
        if (visibility_snapshot.isExploredWorld(pos.x(), pos.z())) {
          max_visibility_state = std::max(max_visibility_state, 1);
        }
      }

      if (max_visibility_state == 0) {
        continue;
      }
      if (max_visibility_state == 1) {
        alpha = 0.5F;
        color_multiplier = QVector3D(0.4F, 0.4F, 0.45F);
      }
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
