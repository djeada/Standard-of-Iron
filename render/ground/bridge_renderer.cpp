#include "bridge_renderer.h"

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

#include "fog_renderer.h"
#include "game/map/visibility_service.h"
#include "linear_feature_geometry.h"
#include "linear_feature_submission.h"
#include "linear_feature_visibility.h"
#include "map/terrain.h"
#include "render/gl/mesh.h"
#include "render/gl/resources.h"
#include "render/scene_renderer.h"

namespace Render::GL {

BridgeRenderer::BridgeRenderer() = default;
BridgeRenderer::~BridgeRenderer() = default;

void BridgeRenderer::configure(const std::vector<Game::Map::Bridge>& bridges,
                               float tile_size,
                               const Game::Map::TerrainHeightMap& height_map) {
  m_bridges = bridges;
  m_tile_size = tile_size;
  m_height_map = &height_map;
  build_meshes();
}

void BridgeRenderer::set_fog_renderer(FogRenderer* fog) {
  m_fog = fog;
}

void BridgeRenderer::build_meshes() {
  m_meshes.clear();

  if (m_bridges.empty()) {
    return;
  }

  if (m_height_map == nullptr) {
    return;
  }

  for (const auto& bridge : m_bridges) {
    m_meshes.push_back(Ground::build_bridge_mesh(bridge, m_tile_size, *m_height_map));
  }
}

auto BridgeRenderer::segment_cull_options(float longest_segment) const
    -> Ground::LinearFeatureVisibilityOptions {

  Ground::LinearFeatureVisibilityOptions options;
  options.fog_culls_segments = false;
  options.sample_count = Ground::recommended_linear_feature_visibility_sample_count(
      longest_segment, m_tile_size);

  options.explored_alpha = 1.0F;
  options.explored_tint = QVector3D(1.0F, 1.0F, 1.0F);
  return options;
}

void BridgeRenderer::submit(Renderer& renderer, ResourceManager* resources) {
  Q_UNUSED(resources);

  float longest_segment = 0.0F;
  for (const auto& bridge : m_bridges) {
    longest_segment = std::max(longest_segment, (bridge.end - bridge.start).length());
  }

  TerrainSurfaceCmd::VisibilityResources visibility_resources;
  if (renderer.static_world_visibility_filter_enabled()) {
    visibility_resources = renderer.visibility_mask();
  }

  FogMaskResources fog_mask;
  if (m_fog != nullptr && renderer.static_world_visibility_filter_enabled()) {
    fog_mask = m_fog->prepare_mask(renderer);
  }

  Ground::submit_linear_feature_segments(renderer,
                                         m_bridges,
                                         m_meshes,
                                         LinearFeatureKind::Bridge,
                                         QVector3D(0.58F, 0.55F, 0.50F),
                                         segment_cull_options(longest_segment),
                                         visibility_resources,
                                         fog_mask);
}

} // namespace Render::GL
