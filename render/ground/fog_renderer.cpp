#include "fog_renderer.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "../scene_renderer.h"

namespace Render::GL {

namespace {

constexpr int k_chunk_cells = 14;
constexpr float k_size_pad_tiles = 2.2F;
constexpr float k_fog_y = 0.14F;
constexpr float k_soft_reveal_duration = 0.30F;

struct StateAgg {
  int count = 0;
  int min_x = 0;
  int max_x = 0;
  int min_z = 0;
  int max_z = 0;
};

void reset_agg(StateAgg& agg) {
  agg.count = 0;
  agg.min_x = agg.max_x = agg.min_z = agg.max_z = 0;
}

void accumulate(StateAgg& agg, int x, int z) {
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
}

auto make_patch(const StateAgg& agg,
                int chunk_area,
                std::uint8_t state,
                int width,
                int height,
                float tile_size) -> FogInstanceData {
  FogInstanceData inst;
  if (agg.count == 0 || chunk_area <= 0) {
    return inst;
  }

  const float half_width = static_cast<float>(width) * 0.5F - 0.5F;
  const float half_height = static_cast<float>(height) * 0.5F - 0.5F;
  const float coverage = static_cast<float>(agg.count) / static_cast<float>(chunk_area);
  const float center_x =
      (static_cast<float>(agg.min_x + agg.max_x) * 0.5F - half_width) * tile_size;
  const float center_z =
      (static_cast<float>(agg.min_z + agg.max_z) * 0.5F - half_height) * tile_size;
  const int span_x = agg.max_x - agg.min_x + 1;
  const int span_z = agg.max_z - agg.min_z + 1;
  const float span_tiles =
      static_cast<float>(std::max(span_x, span_z)) + k_size_pad_tiles;

  inst.center = QVector3D(center_x, k_fog_y, center_z);
  inst.size = span_tiles * tile_size;
  if (state == 0U) {
    inst.color = QVector3D(0.08F, 0.10F, 0.15F);
    inst.alpha = 0.52F + 0.34F * std::sqrt(std::max(coverage, 0.0F));
  } else {
    inst.color = QVector3D(0.24F, 0.24F, 0.22F);
    inst.alpha = 0.22F + 0.20F * std::sqrt(std::max(coverage, 0.0F));
  }
  return inst;
}

} // namespace

void FogRenderer::set_soft_reveal_enabled(bool enabled) {
  if (m_soft_reveal_enabled == enabled) {
    return;
  }
  m_soft_reveal_enabled = enabled;
  m_transitions.clear();
  m_submit_instances.clear();
}

void FogRenderer::update_mask(int width,
                              int height,
                              float tile_size,
                              const std::vector<std::uint8_t>& cells) {
  const int new_width = std::max(0, width);
  const int new_height = std::max(0, height);
  const float new_tile_size = std::max(0.0001F, tile_size);
  const bool geometry_changed = (new_width != m_width) || (new_height != m_height) ||
                                (new_tile_size != m_tile_size);

  m_width = new_width;
  m_height = new_height;
  m_tile_size = new_tile_size;
  m_half_width = m_width * 0.5F - 0.5F;
  m_half_height = m_height * 0.5F - 0.5F;
  if (m_width <= 0 || m_height <= 0) {
    m_cells.clear();
    m_instances.clear();
    m_transitions.clear();
    m_submit_instances.clear();
    m_instance_buffer.reset();
    m_instances_dirty = false;
    return;
  }
  if (!geometry_changed && m_cells == cells) {
    return;
  }
  const std::vector<std::uint8_t> previous_cells = m_cells;
  m_cells = cells;
  if (geometry_changed) {
    m_transitions.clear();
  } else if (m_soft_reveal_enabled) {
    build_transition_chunks(previous_cells, m_cells, 0.0F);
  }
  build_chunks();
  m_instances_dirty = true;
}

void FogRenderer::submit(Renderer& renderer, ResourceManager* resources) {
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

  if (m_soft_reveal_enabled && !m_transitions.empty()) {
    const float now = renderer.get_animation_time();
    m_submit_instances = m_instances;
    auto out = m_transitions.begin();
    for (auto transition : m_transitions) {
      if (transition.start_time <= 0.0F) {
        transition.start_time = now;
      }
      const float t =
          std::clamp((now - transition.start_time) / transition.duration, 0.0F, 1.0F);
      const float fade = (1.0F - t) * (1.0F - t);
      if (fade > 0.002F) {
        FogInstance inst = transition.instance;
        inst.alpha *= fade;
        m_submit_instances.push_back(inst);
        *out++ = transition;
      }
    }
    m_transitions.erase(out, m_transitions.end());
    if (!m_submit_instances.empty()) {
      if (!m_instance_buffer) {
        m_instance_buffer = std::make_unique<Buffer>(Buffer::Type::Vertex);
      }
      m_instance_buffer->set_data(m_submit_instances, Buffer::Usage::Dynamic);
      m_instances_dirty = true;
      renderer.fog_batch(m_instance_buffer.get(), m_submit_instances.size());
    }
    return;
  }

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

  m_instances.reserve(((static_cast<std::size_t>(m_width + k_chunk_cells - 1) /
                        static_cast<std::size_t>(k_chunk_cells)) *
                       (static_cast<std::size_t>(m_height + k_chunk_cells - 1) /
                        static_cast<std::size_t>(k_chunk_cells))) *
                      2U);

  auto emit_patch = [&](const StateAgg& agg, int chunk_area, std::uint8_t state) {
    if (agg.count == 0 || chunk_area <= 0) {
      return;
    }
    m_instances.push_back(
        make_patch(agg, chunk_area, state, m_width, m_height, m_tile_size));
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

void FogRenderer::build_transition_chunks(
    const std::vector<std::uint8_t>& previous_cells,
    const std::vector<std::uint8_t>& next_cells,
    float now) {
  if (previous_cells.size() != next_cells.size()) {
    return;
  }
  if (static_cast<int>(next_cells.size()) != m_width * m_height) {
    return;
  }

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
          const int idx = z * m_width + x;
          const std::uint8_t previous = previous_cells[idx];
          const std::uint8_t next = next_cells[idx];
          if (previous < 2U && next == 2U) {
            accumulate(states[previous], x, z);
          }
        }
      }

      for (std::uint8_t state = 0U; state < 2U; ++state) {
        if (states[state].count == 0) {
          continue;
        }
        FogTransition transition;
        transition.instance = make_patch(
            states[state], chunk_area, state, m_width, m_height, m_tile_size);
        transition.start_time = now;
        transition.duration = k_soft_reveal_duration;
        m_transitions.push_back(transition);
      }
    }
  }
}

} // namespace Render::GL
