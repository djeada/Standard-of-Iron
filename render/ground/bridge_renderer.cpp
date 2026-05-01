#include "bridge_renderer.h"
#include "../../game/map/visibility_service.h"
#include "../gl/mesh.h"
#include "../gl/resources.h"
#include "../scene_renderer.h"
#include "linear_feature_geometry.h"
#include "linear_feature_submission.h"
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
  Q_UNUSED(resources);
  Ground::submit_linear_feature_segments(renderer, m_bridges, m_meshes,
                                         LinearFeatureKind::Bridge,
                                         QVector3D(0.55F, 0.52F, 0.48F));
}

} // namespace Render::GL
