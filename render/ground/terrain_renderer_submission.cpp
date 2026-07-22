#include <QMatrix4x4>
#include <QtGlobal>

#include <cstddef>

#include "../../game/map/render_visibility_rules.h"
#include "../../game/map/visibility_service.h"
#include "../scene_renderer.h"
#include "terrain_renderer.h"

namespace {

const QMatrix4x4 k_identity_matrix;

}

namespace Render::GL {

void TerrainRenderer::submit(Renderer& renderer, ResourceManager* resources) {
  if (m_chunks.empty()) {
    return;
  }

  Q_UNUSED(resources);

  const auto* visibility_snapshot = renderer.static_world_visibility_filter_enabled()
                                        ? renderer.submission_visibility().snapshot()
                                        : nullptr;
  TerrainSurfaceCmd::VisibilityResources visibility_resources;
  if (visibility_snapshot != nullptr) {
    visibility_resources =
        m_visibility_helper.update(*visibility_snapshot, m_tile_size);
    if (m_chunk_visibility_cache.size() != m_chunks.size()) {
      m_chunk_visibility_cache.assign(m_chunks.size(), {});
    }
  }

  for (std::size_t chunk_index = 0; chunk_index < m_chunks.size(); ++chunk_index) {
    const auto& chunk = m_chunks[chunk_index];
    if (!chunk.mesh) {
      continue;
    }

    if (!renderer.submission_visibility().accepts_sphere(
            chunk.cull_center,
            chunk.cull_radius,
            SubmissionFogMode::Ignore)) {
      continue;
    }

    if (visibility_snapshot != nullptr) {
      auto& cache = m_chunk_visibility_cache[chunk_index];
      if (cache.visibility_version != visibility_snapshot->version) {
        bool any_revealed = false;
        for (int gz = chunk.min_z; gz <= chunk.max_z && !any_revealed; ++gz) {
          for (int gx = chunk.min_x; gx <= chunk.max_x; ++gx) {
            if (Game::Map::classify_static_world_cell_visibility(
                    *visibility_snapshot, gx, gz) !=
                Game::Map::RenderVisibilityState::Hidden) {
              any_revealed = true;
              break;
            }
          }
        }
        cache.any_revealed = any_revealed;
        cache.visibility_version = visibility_snapshot->version;
      }
      if (!cache.any_revealed) {
        continue;
      }
    }

    TerrainSurfaceCmd cmd;
    cmd.mesh = chunk.mesh.get();
    cmd.model = k_identity_matrix;
    cmd.params = chunk.params;
    cmd.visibility = visibility_resources;
    cmd.params.light_direction = m_light_direction;
    cmd.sort_key = 0x0080U;
    cmd.depth_write = true;
    cmd.wireframe = m_wireframe;
    cmd.depth_bias = 0.0F;
    renderer.terrain_surface(cmd);
  }
}

} // namespace Render::GL
