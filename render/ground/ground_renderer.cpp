#include "ground_renderer.h"
#include "../draw_queue.h"
#include "../gl/resources.h"
#include "../scene_renderer.h"

namespace Render {
namespace GL {

void GroundRenderer::recomputeModel() {
  m_model.setToIdentity();

  m_model.translate(0.0f, -0.02f, 0.0f);
  if (m_width > 0 && m_height > 0) {

    float scaleX = float(m_width) * m_tileSize;
    float scaleZ = float(m_height) * m_tileSize;
    m_model.scale(scaleX, 1.0f, scaleZ);
  } else {
    m_model.scale(m_extent, 1.0f, m_extent);
  }
}

void GroundRenderer::submit(Renderer &renderer, ResourceManager &resources) {

  float cell = m_tileSize > 0.0f ? m_tileSize : 1.0f;
  float extent = (m_width > 0 && m_height > 0)
                     ? std::max(m_width, m_height) * m_tileSize * 0.5f
                     : m_extent;
  renderer.grid(m_model, m_color, cell, 0.06f, extent);
}

} // namespace GL
} // namespace Render
