#include "fog_renderer.h"

#include "../scene_renderer.h"
#include <QElapsedTimer>
#include <algorithm>

namespace Render::GL {

namespace {
const QMatrix4x4 kIdentityMatrix;
}

void FogRenderer::updateMask(int width, int height, float tileSize,
                             const std::vector<std::uint8_t> &cells) {
  m_width = std::max(0, width);
  m_height = std::max(0, height);
  m_tileSize = std::max(0.0001f, tileSize);
  m_halfWidth = m_width * 0.5f - 0.5f;
  m_halfHeight = m_height * 0.5f - 0.5f;
  m_cells = cells;
  buildChunks();
}

void FogRenderer::submit(Renderer &renderer, ResourceManager &resources) {
  if (!m_enabled)
    return;
  if (m_width <= 0 || m_height <= 0)
    return;
  if (static_cast<int>(m_cells.size()) != m_width * m_height)
    return;

  (void)resources;

  if (!m_instances.empty()) {
    renderer.fogBatch(m_instances.data(), m_instances.size());
  }
}

void FogRenderer::buildChunks() {
  m_instances.clear();

  if (m_width <= 0 || m_height <= 0)
    return;
  if (static_cast<int>(m_cells.size()) != m_width * m_height)
    return;

  QElapsedTimer timer;
  timer.start();

  const float halfTile = m_tileSize * 0.5f;
  m_instances.reserve(static_cast<std::size_t>(m_width) * m_height);

  std::size_t totalQuads = 0;

  for (int z = 0; z < m_height; ++z) {
    for (int x = 0; x < m_width; ++x) {
      const std::uint8_t state = m_cells[z * m_width + x];
      if (state >= 2)
        continue;

      FogInstance instance;
      const float worldX = (x - m_halfWidth) * m_tileSize;
      const float worldZ = (z - m_halfHeight) * m_tileSize;
      instance.center = QVector3D(worldX, 0.25f, worldZ);
      instance.color = (state == 0) ? QVector3D(0.02f, 0.02f, 0.05f)
                                    : QVector3D(0.05f, 0.05f, 0.05f);
      instance.alpha = (state == 0) ? 0.9f : 0.45f;
      instance.size = m_tileSize;

      m_instances.push_back(instance);
      ++totalQuads;
    }
  }
}

} // namespace Render::GL
