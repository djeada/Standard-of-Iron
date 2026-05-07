#include "map_boundary_fog_renderer.h"

#include "../scene_renderer.h"
#include <QVector3D>
#include <algorithm>
#include <array>
#include <cstddef>

namespace Render::GL {

namespace {

// Number of tile layers that the boundary band extends INSIDE the map edge.
// Set to 3 so the first three rows of playable tiles receive dense fog, making
// the world boundary clearly visible when the camera faces the edge.
constexpr int k_band_inside = 3;
// Number of tile layers outside the map edge that receive fog.
constexpr int k_band_outside = 3;

// Y height of each fog quad – raised so the fog forms a visible wall rather
// than hugging the ground.
constexpr float k_fog_y = 1.5F;

// Cool blue-grey colour to evoke an ethereal world edge.
constexpr float k_fog_r = 0.55F;
constexpr float k_fog_g = 0.60F;
constexpr float k_fog_b = 0.72F;

// Alpha values indexed by (signed_dist + k_band_outside).
//   signed_dist < 0  → outside the map  (more negative = further outside)
//   signed_dist == 0 → map edge tile
//   signed_dist > 0  → inside the map (up to k_band_inside - 1)
// Total entries = k_band_outside + k_band_inside = 3 + 3 = 6.
constexpr std::array<float, k_band_outside + k_band_inside> k_alpha_table = {
    0.15F, // signed_dist == -3  (3 tiles outside the map)
    0.30F, // signed_dist == -2  (2 tiles outside the map)
    0.55F, // signed_dist == -1  (1 tile outside the map)
    0.90F, // signed_dist ==  0  (map edge tile)
    0.70F, // signed_dist ==  1  (1 tile inside the map)
    0.40F, // signed_dist ==  2  (2 tiles inside the map)
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
