#pragma once

#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QVector2D>
#include <QtGui/qopengl.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "../../game/map/visibility_service.h"
#include "../draw_queue.h"
#include "../gl/texture.h"
#include "visibility_mask_encoder.h"

namespace Render::Ground {

class VisibilityTextureHelper {
public:
  using VisibilityResources = GL::TerrainSurfaceCmd::VisibilityResources;

  void reset() {
    m_texture.reset();
    m_cached_version = 0;
    m_width = 0;
    m_height = 0;
    m_previous_cells.clear();
    m_texels.clear();
  }

  auto update(const Game::Map::VisibilityService::Snapshot& snapshot,
              float tile_size,
              float explored_alpha = 0.82F) -> VisibilityResources {
    const int vis_w = snapshot.width;
    const int vis_h = snapshot.height;
    auto* gl_context = QOpenGLContext::currentContext();
    auto* gl_functions = gl_context != nullptr ? gl_context->functions() : nullptr;
    if (gl_functions == nullptr) {
      qWarning() << "VisibilityTextureHelper: no current OpenGL context for visibility "
                    "texture upload";
      VisibilityResources res;
      res.size = QVector2D(static_cast<float>(vis_w), static_cast<float>(vis_h));
      res.tile_size = tile_size;
      res.explored_alpha = explored_alpha;
      res.enabled = false;
      return res;
    }

    bool const size_changed = (vis_w != m_width) || (vis_h != m_height);

    if (!m_texture || size_changed) {
      m_texture = std::make_unique<GL::Texture>();
      m_texture->create_empty(vis_w, vis_h, GL::Texture::Format::RGBA);
      m_texture->set_filter(GL::Texture::Filter::Linear, GL::Texture::Filter::Linear);
      m_texture->set_wrap(GL::Texture::Wrap::ClampToEdge,
                          GL::Texture::Wrap::ClampToEdge);
      m_width = vis_w;
      m_height = vis_h;
      m_cached_version = 0;
      m_previous_cells.clear();
    }

    const std::uint64_t version = snapshot.version;
    if (version != m_cached_version || size_changed) {

      const MaskRegion dirty = dirty_region(snapshot.cells, vis_w, vis_h);
      if (!dirty.is_empty()) {
        encode_visibility_mask_region(snapshot.cells, vis_w, vis_h, dirty, m_texels);
        if (!m_texels.empty()) {
          m_texture->bind();
          gl_functions->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
          gl_functions->glTexSubImage2D(GL_TEXTURE_2D,
                                        0,
                                        dirty.x,
                                        dirty.z,
                                        dirty.width,
                                        dirty.height,
                                        GL_RGBA,
                                        GL_UNSIGNED_BYTE,
                                        m_texels.data());
        }
      }
      m_previous_cells = snapshot.cells;
      m_cached_version = version;
    }

    VisibilityResources res;
    res.texture = m_texture.get();
    res.size = QVector2D(static_cast<float>(vis_w), static_cast<float>(vis_h));
    res.tile_size = tile_size;
    res.explored_alpha = explored_alpha;
    res.enabled = true;
    return res;
  }

private:
  [[nodiscard]] auto dirty_region(const std::vector<std::uint8_t>& cells,
                                  int width,
                                  int height) const -> MaskRegion {
    const auto cell_count =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (cells.size() != cell_count) {
      return {};
    }
    if (m_previous_cells.size() != cell_count) {
      return MaskRegion::whole(width, height);
    }

    MaskRegion dirty;
    for (int z = 0; z < height; ++z) {
      const std::size_t row =
          static_cast<std::size_t>(z) * static_cast<std::size_t>(width);
      for (int x = 0; x < width; ++x) {
        if (cells[row + static_cast<std::size_t>(x)] !=
            m_previous_cells[row + static_cast<std::size_t>(x)]) {
          dirty.include(x, z, width, height);
        }
      }
    }
    return dirty;
  }

  std::unique_ptr<GL::Texture> m_texture;
  std::vector<std::uint8_t> m_previous_cells;
  std::vector<unsigned char> m_texels;
  std::uint64_t m_cached_version = 0;
  int m_width = 0;
  int m_height = 0;
};

} // namespace Render::Ground
