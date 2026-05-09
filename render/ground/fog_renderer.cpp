#include "fog_renderer.h"

#include "../scene_renderer.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Render::GL {

void FogRenderer::update_mask(int width, int height, float tile_size,
                              const std::vector<std::uint8_t> &cells) {
  const int new_width = std::max(0, width);
  const int new_height = std::max(0, height);
  const float new_tile_size = std::max(0.0001F, tile_size);
  const bool geometry_changed = (new_width != m_width) ||
                                (new_height != m_height) ||
                                (new_tile_size != m_tile_size);

  m_width = new_width;
  m_height = new_height;
  m_tile_size = new_tile_size;
  m_half_width = m_width * 0.5F - 0.5F;
  m_half_height = m_height * 0.5F - 0.5F;
  if (m_width <= 0 || m_height <= 0) {
    m_cells.clear();
    m_instances.clear();
    m_instance_buffer.reset();
    m_instances_dirty = false;
    return;
  }
  if (!geometry_changed && m_cells == cells) {
    return;
  }
  m_cells = cells;
  build_chunks();
  m_instances_dirty = true;
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

  upload_instances();
  if (!m_instances.empty()) {
    if (m_instance_buffer) {
      renderer.fog_batch(m_instance_buffer.get(), m_instances.size());
    } else {
      renderer.fog_batch(m_instances.data(), m_instances.size());
    }
  }
}

void FogRenderer::upload_instances() {
  if (m_instances.empty()) {
    m_instance_buffer.reset();
    m_instances_dirty = false;
    return;
  }
  if (!m_instance_buffer) {
    m_instance_buffer = std::make_unique<Buffer>(Buffer::Type::Vertex);
    m_instances_dirty = true;
  }
  if (m_instances_dirty) {
    m_instance_buffer->set_data(m_instances, Buffer::Usage::Static);
    m_instances_dirty = false;
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

  constexpr int k_chunk_cells = 14;
  constexpr float k_size_pad_tiles = 2.2F;
  constexpr float k_fog_y = 0.14F;

  struct StateAgg {
    int count = 0;
    int min_x = 0;
    int max_x = 0;
    int min_z = 0;
    int max_z = 0;
  };

  m_instances.reserve(((static_cast<std::size_t>(m_width + k_chunk_cells - 1) /
                        static_cast<std::size_t>(k_chunk_cells)) *
                       (static_cast<std::size_t>(m_height + k_chunk_cells - 1) /
                        static_cast<std::size_t>(k_chunk_cells))) *
                      2U);

  const QVector3D unexplored_color(0.08F, 0.10F, 0.15F);
  const QVector3D explored_color(0.24F, 0.24F, 0.22F);

  auto reset_agg = [](StateAgg &agg) {
    agg.count = 0;
    agg.min_x = agg.max_x = agg.min_z = agg.max_z = 0;
  };

  auto accumulate = [](StateAgg &agg, int x, int z) {
    if (agg.count == 0) {
      agg.min_x = agg.max_x = x;
      agg.min_z = agg.max_z = z;
    } else {
      agg.min_x = std::min(agg.min_x, x);
      agg.max_x = std::max(agg.max_x, x);
      agg.min_z = std::min(agg.min_z, z);
      agg.max_z = std::max(agg.max_z, z);
    }
    ++agg.count;
  };

  auto emit_patch = [&](const StateAgg &agg, int chunk_area,
                        std::uint8_t state) {
    if (agg.count == 0 || chunk_area <= 0) {
      return;
    }

    const float coverage =
        static_cast<float>(agg.count) / static_cast<float>(chunk_area);
    const float center_x =
        (static_cast<float>(agg.min_x + agg.max_x) * 0.5F - m_half_width) *
        m_tile_size;
    const float center_z =
        (static_cast<float>(agg.min_z + agg.max_z) * 0.5F - m_half_height) *
        m_tile_size;
    const int span_x = agg.max_x - agg.min_x + 1;
    const int span_z = agg.max_z - agg.min_z + 1;
    const float span_tiles =
        static_cast<float>(std::max(span_x, span_z)) + k_size_pad_tiles;

    FogInstance inst;
    inst.center = QVector3D(center_x, k_fog_y, center_z);
    inst.size = span_tiles * m_tile_size;
    if (state == 0U) {
      inst.color = unexplored_color;
      inst.alpha = 0.52F + 0.34F * std::sqrt(std::max(coverage, 0.0F));
    } else {
      inst.color = explored_color;
      inst.alpha = 0.22F + 0.20F * std::sqrt(std::max(coverage, 0.0F));
    }
    m_instances.push_back(inst);
  };

  for (int chunk_z = 0; chunk_z < m_height; chunk_z += k_chunk_cells) {
    for (int chunk_x = 0; chunk_x < m_width; chunk_x += k_chunk_cells) {
      StateAgg states[2];
      reset_agg(states[0]);
      reset_agg(states[1]);

      const int end_z = std::min(chunk_z + k_chunk_cells, m_height);
      const int end_x = std::min(chunk_x + k_chunk_cells, m_width);
      const int chunk_area = (end_z - chunk_z) * (end_x - chunk_x);

      for (int z = chunk_z; z < end_z; ++z) {
        for (int x = chunk_x; x < end_x; ++x) {
          const std::uint8_t state = m_cells[z * m_width + x];
          if (state < 2U) {
            accumulate(states[state], x, z);
          }
        }
      }

      emit_patch(states[0], chunk_area, 0U);
      emit_patch(states[1], chunk_area, 1U);
    }
  }
}

} // namespace Render::GL
