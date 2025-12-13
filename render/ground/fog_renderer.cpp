#include "fog_renderer.h"

#include "../scene_renderer.h"
#include <QElapsedTimer>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <qelapsedtimer.h>
#include <qmatrix4x4.h>
#include <vector>

namespace Render::GL {

namespace {
const QMatrix4x4 k_identity_matrix;
}

void FogRenderer::update_mask(int width, int height, float tile_size,
                             const std::vector<std::uint8_t> &cells) {
  m_width = std::max(0, width);
  m_height = std::max(0, height);
  m_tile_size = std::max(0.0001F, tile_size);
  m_half_width = m_width * 0.5F - 0.5F;
  m_half_height = m_height * 0.5F - 0.5F;
  m_cells = cells;
  build_chunks();
}

void FogRenderer::submit(Renderer &renderer, ResourceManager *resources) {
  if (!m_enabled) {
    return;
  }
  if (m_width <= 0 || m_height <= 0) {
    return;
  }
  if (static_cast<int>(m_cells.size()) != m_width * m_height) {
    return;
  }

  (void)resources;

  if (!m_instances.empty()) {
    renderer.fog_batch(m_instances.data(), m_instances.size());
  }
}

void FogRenderer::build_chunks() {
  m_instances.clear();

  if (m_width <= 0 || m_height <= 0) {
    return;
  }
  if (static_cast<int>(m_cells.size()) != m_width * m_height) {
    return;
  }

  QElapsedTimer timer;
  timer.start();

  const float half_tile = m_tile_size * 0.5F;
  m_instances.reserve(static_cast<std::size_t>(m_width) * m_height);

  std::size_t total_quads = 0;

  for (int z = 0; z < m_height; ++z) {
    for (int x = 0; x < m_width; ++x) {
      const std::uint8_t state = m_cells[z * m_width + x];
      if (state >= 2) {
        continue;
      }

      FogInstance instance;
      const float world_x = (x - m_half_width) * m_tile_size;
      const float world_z = (z - m_half_height) * m_tile_size;
      instance.center = QVector3D(world_x, 0.25F, world_z);
      instance.color = (state == 0) ? QVector3D(0.02F, 0.02F, 0.05F)
                                    : QVector3D(0.05F, 0.05F, 0.05F);
      instance.alpha = (state == 0) ? 0.9F : 0.45F;
      instance.size = m_tile_size;

      m_instances.push_back(instance);
      ++total_quads;
    }
  }
}

} // namespace Render::GL
