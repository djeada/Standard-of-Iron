#include "map_boundary_fog_renderer.h"

#include "../scene_renderer.h"
#include <QVector3D>
#include <algorithm>
#include <array>
#include <cstddef>

namespace Render::GL {

namespace {

// Number of tile bands outside the map edge that receive fog.
constexpr int k_band_outside = 4;

// Number of stacked horizontal quad layers per fog tile.
// Each layer sits at a different Y height, building a volumetric fog wall
// that is clearly visible from any camera angle rather than a thin stripe.
constexpr int k_fog_layers = 7;

// Y spacing between consecutive fog layers (world units).
constexpr float k_layer_y_step = 0.5F;

// Fog quad size relative to tile size.  Slightly wider so adjacent quads
// overlap and avoid visible gaps in the fog wall.
constexpr float k_size_scale = 1.1F;

// Dense white-grey mist colour.
constexpr float k_fog_r = 0.82F;
constexpr float k_fog_g = 0.85F;
constexpr float k_fog_b = 0.90F;

// Per-layer alpha profile (index 0 = ground, index k_fog_layers-1 = top).
// Layers stack through additive alpha blending, so individual values are kept
// modest while the combined column reaches high opacity near the map edge.
constexpr std::array<float, k_fog_layers> k_layer_alpha = {
    0.45F, // layer 0 – ground level
    0.55F, // layer 1
    0.60F, // layer 2 – peak density
    0.55F, // layer 3
    0.40F, // layer 4
    0.25F, // layer 5
    0.12F, // layer 6 – top, fades into sky
};

// Per-distance-band alpha multiplier.
// Index 0 = one tile outside the map edge (densest), index k_band_outside-1 =
// furthest outside (most transparent), creating a gradient into the void.
constexpr std::array<float, k_band_outside> k_dist_alpha = {
    1.00F, // |dist| == 1  (closest to the map edge)
    0.80F, // |dist| == 2
    0.55F, // |dist| == 3
    0.30F, // |dist| == 4  (furthest outside)
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

  // Pre-compute exact outside-tile count for the reservation.
  const auto exp_w =
      static_cast<std::size_t>(m_width + 2 * k_band_outside);
  const auto exp_h =
      static_cast<std::size_t>(m_height + 2 * k_band_outside);
  const auto outside_tiles =
      exp_w * exp_h -
      static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height);
  m_instances.reserve(outside_tiles * static_cast<std::size_t>(k_fog_layers));

  const float quad_size = m_tile_size * k_size_scale;

  for (int tz = -k_band_outside; tz < m_height + k_band_outside; ++tz) {
    for (int tx = -k_band_outside; tx < m_width + k_band_outside; ++tx) {
      // Signed distance from the nearest map edge.
      //   negative → outside the map
      //   zero / positive → on or inside the map (skip)
      const int dx = std::min(tx, m_width - 1 - tx);
      const int dz = std::min(tz, m_height - 1 - tz);
      const int dist = std::min(dx, dz);

      // Only render fog outside the map boundary.
      if (dist >= 0) {
        continue;
      }
      if (dist < -k_band_outside) {
        continue;
      }

      // Distance index: 0 = one tile outside edge, k_band_outside-1 = furthest.
      const auto dist_idx = static_cast<std::size_t>((-dist) - 1);
      const float dist_mult = k_dist_alpha[dist_idx];

      const float world_x =
          (static_cast<float>(tx) - half_w) * m_tile_size;
      const float world_z =
          (static_cast<float>(tz) - half_h) * m_tile_size;

      // Emit one quad per vertical layer to build a volumetric fog column.
      for (int layer = 0; layer < k_fog_layers; ++layer) {
        const float y =
            static_cast<float>(layer) * k_layer_y_step;
        const float alpha =
            k_layer_alpha[static_cast<std::size_t>(layer)] * dist_mult;

        FogInstanceData inst;
        inst.center = QVector3D(world_x, y, world_z);
        inst.color = QVector3D(k_fog_r, k_fog_g, k_fog_b);
        inst.alpha = alpha;
        inst.size = quad_size;

        m_instances.push_back(inst);
      }
    }
  }
}

} // namespace Render::GL
