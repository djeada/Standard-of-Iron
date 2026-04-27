#include "bridge_renderer.h"
#include "../../game/map/visibility_service.h"
#include "../gl/mesh.h"
#include "../gl/resources.h"
#include "../scene_renderer.h"
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

BridgeRenderer::BridgeRenderer() = default;
BridgeRenderer::~BridgeRenderer() = default;

void BridgeRenderer::configure(const std::vector<Game::Map::Bridge> &bridges,
                               float tile_size) {
  m_bridges = bridges;
  m_tile_size = tile_size;
  build_meshes();
}

void BridgeRenderer::build_meshes() {
  m_meshes.clear();

  if (m_bridges.empty()) {
    return;
  }

  for (const auto &bridge : m_bridges) {
    m_meshes.push_back(Ground::build_bridge_mesh(bridge, m_tile_size));
  }
}

void BridgeRenderer::submit(Renderer &renderer, ResourceManager *resources) {
  if (m_meshes.empty() || m_bridges.empty()) {
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

  QVector3D const stone_color(0.55F, 0.52F, 0.48F);

  size_t mesh_index = 0;
  for (const auto &bridge : m_bridges) {
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
          &visibility_snapshot, bridge.start, bridge.end);
      if (!visibility_result.visible) {
        continue;
      }
      alpha = visibility_result.alpha;
      color_multiplier = visibility_result.color_multiplier;
    }

    QVector3D const final_color(stone_color.x() * color_multiplier.x(),
                                stone_color.y() * color_multiplier.y(),
                                stone_color.z() * color_multiplier.z());

    TerrainFeatureCmd cmd;
    cmd.mesh = mesh;
    cmd.kind = TerrainFeatureCmd::Kind::Bridge;
    cmd.model = model;
    cmd.color = final_color;
    cmd.alpha = alpha;
    renderer.terrain_feature(cmd);
  }
}

} // namespace Render::GL
