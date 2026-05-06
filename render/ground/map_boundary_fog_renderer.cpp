#include "map_boundary_fog_renderer.h"

#include "../scene_renderer.h"
#include <QVector3D>
#include <algorithm>
#include <array>
#include <cstddef>

namespace Render::GL {

namespace {

// Number of tile layers inside the map edge that receive fog.
constexpr int k_band_inside = 2;
// Number of tile layers outside the map edge that receive fog.
constexpr int k_band_outside = 3;

// Y height of each fog quad (low to the ground so it doesn't obscure units).
constexpr float k_fog_y = 0.15F;

// Subtle warm-gray fog colour.
constexpr float k_fog_r = 0.60F;
constexpr float k_fog_g = 0.58F;
constexpr float k_fog_b = 0.55F;

// Alpha values indexed by (signed_dist + k_band_outside).
//   signed_dist < 0  → outside the map  (more negative = further outside)
//   signed_dist == 0 → map edge tile
//   signed_dist > 0  → inside the map
// Total entries = k_band_outside + k_band_inside.
constexpr std::array<float, k_band_outside + k_band_inside> k_alpha_table = {
    0.08F, // 3 tiles outside
    0.18F, // 2 tiles outside
    0.24F, // 1 tile outside
    0.16F, // map edge (distance 0)
    0.08F, // 1 tile inside edge
};

} // namespace

void MapBoundaryFogRenderer::configure(int width, int height,
                                       float tile_size) {
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

  // Centre offset used to convert tile indices to world coordinates.
  const float half_w = m_width * 0.5F - 0.5F;
  const float half_h = m_height * 0.5F - 0.5F;

  // Conservative upper bound for reservation.
  const auto perimeter =
      static_cast<std::size_t>(2 * (m_width + m_height) + 4);
  const auto band_total = static_cast<std::size_t>(k_band_inside + k_band_outside);
  m_instances.reserve(perimeter * band_total);

  for (int tz = -k_band_outside; tz < m_height + k_band_outside; ++tz) {
    for (int tx = -k_band_outside; tx < m_width + k_band_outside; ++tx) {
      // Signed distance from the nearest map edge.
      //   negative → outside the map
      //   zero     → on the edge
      //   positive → inside the map
      const int dx = std::min(tx, m_width - 1 - tx);
      const int dz = std::min(tz, m_height - 1 - tz);
      const int dist = std::min(dx, dz);

      // Skip tiles that are deep inside the playable area.
      if (dist >= k_band_inside) {
        continue;
      }
      // Skip tiles further outside than our band (shouldn't happen given the
      // loop bounds, but guard for correctness).
      if (dist < -k_band_outside) {
        continue;
      }

      const int table_index = dist + k_band_outside;
      const float alpha = k_alpha_table[static_cast<std::size_t>(table_index)];

      FogInstanceData inst;
      inst.center =
          QVector3D((static_cast<float>(tx) - half_w) * m_tile_size, k_fog_y,
                    (static_cast<float>(tz) - half_h) * m_tile_size);
      inst.color = QVector3D(k_fog_r, k_fog_g, k_fog_b);
      inst.alpha = alpha;
      inst.size = m_tile_size;

      m_instances.push_back(inst);
    }
  }
}

} // namespace Render::GL
