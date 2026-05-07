#include "map_boundary_fog_renderer.h"

#include "../scene_renderer.h"
#include <QVector3D>
#include <algorithm>
#include <array>
#include <cstddef>

namespace Render::GL {

namespace {

constexpr int k_clear_outside_tiles = 10;
constexpr int k_band_outside = 6;
constexpr int k_fog_layers = 1;
constexpr float k_fog_y = 0.06F;

constexpr float k_size_scale = 2.4F;

constexpr float k_fog_r = 0.58F;
constexpr float k_fog_g = 0.61F;
constexpr float k_fog_b = 0.54F;

constexpr std::array<float, k_fog_layers> k_layer_alpha = {0.10F};

constexpr std::array<float, k_band_outside> k_dist_alpha = {
    0.26F,
    0.36F,
    0.50F,
    0.66F,
    0.82F,
    1.00F,
};

} // namespace

void MapBoundaryFogRenderer::configure(int width, int height, float tile_size) {
  m_width = std::max(0, width);
  m_height = std::max(0, height);
  m_tile_size = std::max(0.0001F, tile_size);
  build_instances();
}

void MapBoundaryFogRenderer::submit(Renderer &renderer,
                                    ResourceManager *resources) {
  (void)resources;
  if (!m_instances.empty()) {
    renderer.fog_batch(m_instances.data(), m_instances.size());
  }
}

void MapBoundaryFogRenderer::build_instances() {
  m_instances.clear();

  if (m_width <= 0 || m_height <= 0) {
    return;
  }

  const float half_w = m_width * 0.5F - 0.5F;
  const float half_h = m_height * 0.5F - 0.5F;

  const int outer = k_clear_outside_tiles + k_band_outside;
  const auto exp_w = static_cast<std::size_t>(m_width + 2 * outer);
  const auto exp_h = static_cast<std::size_t>(m_height + 2 * outer);
  const auto inner_w =
      static_cast<std::size_t>(m_width + 2 * k_clear_outside_tiles);
  const auto inner_h =
      static_cast<std::size_t>(m_height + 2 * k_clear_outside_tiles);
  const auto outside_tiles = exp_w * exp_h - inner_w * inner_h;
  m_instances.reserve(outside_tiles * static_cast<std::size_t>(k_fog_layers));

  const float quad_size = m_tile_size * k_size_scale;

  for (int tz = -outer; tz < m_height + outer; ++tz) {
    for (int tx = -outer; tx < m_width + outer; ++tx) {

      const int dx = std::min(tx, m_width - 1 - tx);
      const int dz = std::min(tz, m_height - 1 - tz);
      const int dist = std::min(dx, dz);

      if (dist >= -k_clear_outside_tiles) {
        continue;
      }
      if (dist < -outer) {
        continue;
      }

      const auto dist_idx =
          static_cast<std::size_t>((-dist) - k_clear_outside_tiles - 1);
      const float dist_mult = k_dist_alpha[dist_idx];

      const float world_x = (static_cast<float>(tx) - half_w) * m_tile_size;
      const float world_z = (static_cast<float>(tz) - half_h) * m_tile_size;

      for (int layer = 0; layer < k_fog_layers; ++layer) {
        const float alpha =
            k_layer_alpha[static_cast<std::size_t>(layer)] * dist_mult;

        FogInstanceData inst;
        inst.center = QVector3D(world_x, k_fog_y, world_z);
        inst.color = QVector3D(k_fog_r, k_fog_g, k_fog_b);
        inst.alpha = alpha;
        inst.size = quad_size;

        m_instances.push_back(inst);
      }
    }
  }
}

} // namespace Render::GL
