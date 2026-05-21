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

namespace Render::Ground {

class VisibilityTextureHelper {
public:
  using VisibilityResources = GL::TerrainSurfaceCmd::VisibilityResources;

  void reset() {
    m_texture.reset();
    m_cached_version = 0;
    m_width = 0;
    m_height = 0;
  }

  auto update(const Game::Map::VisibilityService::Snapshot& snapshot,
              float tile_size,
              float explored_alpha = 0.6F) -> VisibilityResources {
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
      m_texture->set_filter(GL::Texture::Filter::Nearest, GL::Texture::Filter::Nearest);
      m_texture->set_wrap(GL::Texture::Wrap::ClampToEdge,
                          GL::Texture::Wrap::ClampToEdge);
      m_width = vis_w;
      m_height = vis_h;
      m_cached_version = 0;
    }

    const std::uint64_t version = snapshot.version;
    if (version != m_cached_version || size_changed) {
      const auto& cells = snapshot.cells;
      std::vector<unsigned char> texels(static_cast<std::size_t>(vis_w * vis_h * 4),
                                        0U);

      for (int z = 0; z < vis_h; ++z) {
        for (int x = 0; x < vis_w; ++x) {
          int const idx = z * vis_w + x;
          unsigned char val = 0U;
          switch (static_cast<Game::Map::VisibilityState>(cells[idx])) {
          case Game::Map::VisibilityState::Visible:
            val = 255U;
            break;
          case Game::Map::VisibilityState::Explored:
            val = 128U;
            break;
          case Game::Map::VisibilityState::Unseen:
          default:
            val = 0U;
            break;
          }
          texels[static_cast<std::size_t>(idx * 4)] = val;
          texels[static_cast<std::size_t>(idx * 4 + 3)] = 255U;
        }
      }

      m_texture->bind();
      gl_functions->glTexSubImage2D(GL_TEXTURE_2D,
                                    0,
                                    0,
                                    0,
                                    vis_w,
                                    vis_h,
                                    GL_RGBA,
                                    GL_UNSIGNED_BYTE,
                                    texels.data());
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
  std::unique_ptr<GL::Texture> m_texture;
  std::uint64_t m_cached_version = 0;
  int m_width = 0;
  int m_height = 0;
};

} // namespace Render::Ground
