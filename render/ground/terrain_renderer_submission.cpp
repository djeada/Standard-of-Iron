#include <QMatrix4x4>
#include <QOpenGLContext>
#include <QtGlobal>
#include <QtGui/qopengl.h>

#include <algorithm>
#include <cstddef>

#include "game/map/render_visibility_rules.h"
#include "game/map/visibility_service.h"
#include "render/gl/texture.h"
#include "render/scene_renderer.h"
#include "terrain_renderer.h"

namespace {

const QMatrix4x4 k_identity_matrix;

}

namespace Render::GL {

auto TerrainRenderer::update_height_texture() -> TerrainSurfaceCmd::HeightResources {
  TerrainSurfaceCmd::HeightResources resources;
  if (m_width <= 0 || m_height <= 0 || m_height_data.empty()) {
    return resources;
  }

  auto* context = QOpenGLContext::currentContext();
  auto* gl = context != nullptr ? context->functions() : nullptr;
  if (gl == nullptr) {
    return resources;
  }

  const bool size_changed = m_height_texture == nullptr ||
                            m_height_texture->get_width() != m_width ||
                            m_height_texture->get_height() != m_height;
  if (size_changed) {
    m_height_texture = std::make_unique<Texture>();
    if (!m_height_texture->create_empty(m_width, m_height, Texture::Format::R32F)) {
      m_height_texture.reset();
      return resources;
    }
    m_height_texture->set_filter(Texture::Filter::Linear, Texture::Filter::Linear);
    m_height_texture->set_wrap(Texture::Wrap::ClampToEdge, Texture::Wrap::ClampToEdge);
    m_height_texture_dirty = true;
  }

  if (m_height_texture_dirty) {
    m_height_texture->bind();
    gl->glTexSubImage2D(GL_TEXTURE_2D,
                        0,
                        0,
                        0,
                        m_width,
                        m_height,
                        GL_RED,
                        GL_FLOAT,
                        m_height_data.data());
    m_height_texture_dirty = false;
  }

  resources.texture = m_height_texture.get();
  resources.texel_size = QVector2D(1.0F / static_cast<float>(m_width),
                                   1.0F / static_cast<float>(m_height));
  resources.uv_scale = QVector2D(1.0F / (std::max(1, m_width) * m_tile_size),
                                 1.0F / (std::max(1, m_height) * m_tile_size));
  resources.uv_offset = QVector2D(0.5F, 0.5F);
  resources.to_world = 1.0F;
  resources.enabled = true;
  return resources;
}

void TerrainRenderer::submit(Renderer& renderer, ResourceManager* resources) {
  if (m_chunks.empty()) {
    return;
  }

  Q_UNUSED(resources);

  const auto* visibility_snapshot = renderer.static_world_visibility_filter_enabled()
                                        ? renderer.submission_visibility().snapshot()
                                        : nullptr;
  TerrainSurfaceCmd::VisibilityResources visibility_resources;
  const TerrainSurfaceCmd::HeightResources height_resources = update_height_texture();
  renderer.set_terrain_height_resources(height_resources);
  if (visibility_snapshot != nullptr) {
    visibility_resources = renderer.visibility_mask();
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
            chunk.cull_center, chunk.cull_radius, SubmissionFogMode::Ignore)) {
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
    cmd.height = height_resources;
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
