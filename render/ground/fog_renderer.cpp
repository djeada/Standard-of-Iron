#include "fog_renderer.h"

#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QVector2D>
#include <QtGui/qopengl.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "../../game/map/visibility_service.h"
#include "../scene_renderer.h"
#include "visibility_mask_encoder.h"

namespace Render::GL {

namespace {

constexpr int k_chunk_cells = 14;
constexpr float k_fog_y = 0.14F;
constexpr float k_fog_alpha = 0.78F;
constexpr float k_reveal_seconds = 0.22F;
constexpr float k_soft_reveal_seconds = 0.55F;

constexpr float k_fog_epsilon = 0.004F;

const QVector3D k_fog_color{0.12F, 0.15F, 0.20F};

auto chunk_count(int cells) -> int {
  return (std::max(0, cells) + k_chunk_cells - 1) / k_chunk_cells;
}

} // namespace

void FogRenderer::set_soft_reveal_enabled(bool enabled) {
  m_soft_reveal_enabled = enabled;
}

void FogRenderer::clear_state() {
  m_fog_amount.clear();
  m_target_fog.clear();
  m_seen_amount.clear();
  m_instances.clear();
  m_mask_texels.clear();
  m_instance_buffer.reset();
  m_mask_texture.reset();
  m_mask_texture_width = 0;
  m_mask_texture_height = 0;
  m_instances_dirty = false;
  m_patches_dirty = false;
  m_mask_dirty = {};
  m_fade_region = {};
  m_settled = true;
  m_last_time = -1.0F;
}

void FogRenderer::update_mask(int width,
                              int height,
                              float tile_size,
                              const std::vector<std::uint8_t>& cells) {
  const int new_width = std::max(0, width);
  const int new_height = std::max(0, height);
  const float new_tile_size = std::max(0.0001F, tile_size);
  const auto cell_count =
      static_cast<std::size_t>(new_width) * static_cast<std::size_t>(new_height);

  const bool geometry_changed = (new_width != m_width) || (new_height != m_height) ||
                                (new_tile_size != m_tile_size);

  m_width = new_width;
  m_height = new_height;
  m_tile_size = new_tile_size;

  if (cell_count == 0 || cells.size() != cell_count) {
    clear_state();
    return;
  }

  if (geometry_changed || m_target_fog.size() != cell_count) {
    m_target_fog.assign(cell_count, 1.0F);
    m_fog_amount.assign(cell_count, 1.0F);
    m_seen_amount.assign(cell_count, 0.0F);
    m_instance_buffer.reset();
    m_mask_texture.reset();
    m_mask_texture_width = 0;
    m_mask_texture_height = 0;
    m_last_time = -1.0F;
  }

  bool targets_changed = false;
  for (std::size_t idx = 0; idx < cell_count; ++idx) {
    const auto state = static_cast<Game::Map::VisibilityState>(cells[idx]);
    const float target = state == Game::Map::VisibilityState::Unseen ? 1.0F : 0.0F;
    const float seen = state == Game::Map::VisibilityState::Visible ? 1.0F : 0.0F;
    const bool target_moved = m_target_fog[idx] != target;
    const bool seen_moved = m_seen_amount[idx] != seen;
    if (!target_moved && !seen_moved) {
      continue;
    }
    const int x = static_cast<int>(idx % static_cast<std::size_t>(m_width));
    const int z = static_cast<int>(idx / static_cast<std::size_t>(m_width));
    if (target_moved) {
      m_target_fog[idx] = target;
      targets_changed = true;
      m_fade_region.include(x, z, m_width, m_height);
    }
    if (seen_moved) {
      m_seen_amount[idx] = seen;
    }
    m_mask_dirty.include(x, z, m_width, m_height);
  }

  if (geometry_changed) {

    m_fog_amount = m_target_fog;
    m_settled = true;
    m_patches_dirty = true;
    m_mask_dirty = Ground::MaskRegion::whole(m_width, m_height);
    m_fade_region = {};
    rebuild_patches();
    return;
  }

  if (targets_changed) {
    m_settled = false;
    m_patches_dirty = true;
    rebuild_patches();
  }
}

void FogRenderer::advance_reveal(float dt_seconds) {
  if (m_settled || m_fog_amount.size() != m_target_fog.size()) {
    return;
  }

  const float duration =
      m_soft_reveal_enabled ? k_soft_reveal_seconds : k_reveal_seconds;
  const float step = std::clamp(dt_seconds / duration, 0.0F, 1.0F);

  const Ground::MaskRegion region = m_fade_region;
  Ground::MaskRegion still_fading;
  for (int row = 0; row < region.height; ++row) {
    const int z = region.z + row;
    for (int column = 0; column < region.width; ++column) {
      const int x = region.x + column;
      const auto idx = static_cast<std::size_t>(z) * static_cast<std::size_t>(m_width) +
                       static_cast<std::size_t>(x);
      const float target = m_target_fog[idx];
      float& current = m_fog_amount[idx];
      if (current == target) {
        continue;
      }
      const float delta = target - current;
      if (std::abs(delta) <= k_fog_epsilon || step >= 1.0F) {
        current = target;
      } else {
        current += delta * step;
        still_fading.include(x, z, m_width, m_height);
      }
      m_mask_dirty.include(x, z, m_width, m_height);
    }
  }

  m_fade_region = still_fading;
  m_settled = still_fading.is_empty();

  if (m_settled) {
    m_patches_dirty = true;
    rebuild_patches();
  }
}

auto FogRenderer::fog_amount_at(int grid_x, int grid_z) const -> float {
  if (grid_x < 0 || grid_z < 0 || grid_x >= m_width || grid_z >= m_height) {
    return 1.0F;
  }
  const auto idx =
      static_cast<std::size_t>(grid_z) * static_cast<std::size_t>(m_width) +
      static_cast<std::size_t>(grid_x);
  if (idx >= m_fog_amount.size()) {
    return 1.0F;
  }
  return m_fog_amount[idx];
}

void FogRenderer::rebuild_patches() {
  m_instances.clear();
  m_patches_dirty = false;
  if (m_width <= 0 || m_height <= 0) {
    return;
  }

  const int chunks_x = chunk_count(m_width);
  const int chunks_z = chunk_count(m_height);
  m_instances.reserve(static_cast<std::size_t>(chunks_x) *
                      static_cast<std::size_t>(chunks_z));

  const float half_width = static_cast<float>(m_width) * 0.5F - 0.5F;
  const float half_height = static_cast<float>(m_height) * 0.5F - 0.5F;
  const float chunk_span = static_cast<float>(k_chunk_cells) * m_tile_size;

  for (int chunk_z = 0; chunk_z < chunks_z; ++chunk_z) {
    for (int chunk_x = 0; chunk_x < chunks_x; ++chunk_x) {
      const int start_x = chunk_x * k_chunk_cells;
      const int start_z = chunk_z * k_chunk_cells;
      const int end_x = std::min(start_x + k_chunk_cells, m_width);
      const int end_z = std::min(start_z + k_chunk_cells, m_height);

      bool has_fog = false;
      for (int z = std::max(0, start_z - 1);
           z < std::min(m_height, end_z + 1) && !has_fog;
           ++z) {
        for (int x = std::max(0, start_x - 1); x < std::min(m_width, end_x + 1); ++x) {
          if (m_fog_amount[static_cast<std::size_t>(z) *
                               static_cast<std::size_t>(m_width) +
                           static_cast<std::size_t>(x)] > k_fog_epsilon) {
            has_fog = true;
            break;
          }
        }
      }
      if (!has_fog) {
        continue;
      }

      const float center_cell_x =
          static_cast<float>(start_x) + static_cast<float>(k_chunk_cells - 1) * 0.5F;
      const float center_cell_z =
          static_cast<float>(start_z) + static_cast<float>(k_chunk_cells - 1) * 0.5F;

      FogInstance inst;
      inst.center = QVector3D((center_cell_x - half_width) * m_tile_size,
                              k_fog_y,
                              (center_cell_z - half_height) * m_tile_size);
      inst.size = chunk_span;
      inst.color = k_fog_color;
      inst.alpha = k_fog_alpha;
      m_instances.push_back(inst);
    }
  }

  m_instances_dirty = true;
}

void FogRenderer::upload_mask(Renderer& renderer) {
  (void)renderer;
  if (m_width <= 0 || m_height <= 0 || m_fog_amount.empty()) {
    return;
  }

  auto* gl_context = QOpenGLContext::currentContext();
  auto* gl_functions = gl_context != nullptr ? gl_context->functions() : nullptr;
  if (gl_functions == nullptr) {
    return;
  }

  const bool size_changed =
      (m_mask_texture_width != m_width) || (m_mask_texture_height != m_height);
  if (!m_mask_texture || size_changed) {
    m_mask_texture = std::make_unique<Texture>();
    m_mask_texture->create_empty(m_width, m_height, Texture::Format::RGBA);
    m_mask_texture->set_filter(Texture::Filter::Linear, Texture::Filter::Linear);
    m_mask_texture->set_wrap(Texture::Wrap::ClampToEdge, Texture::Wrap::ClampToEdge);
    m_mask_texture_width = m_width;
    m_mask_texture_height = m_height;
    m_mask_dirty = Ground::MaskRegion::whole(m_width, m_height);
  }

  if (m_mask_dirty.is_empty()) {
    return;
  }

  Ground::encode_fog_mask_region(
      m_fog_amount, m_seen_amount, m_width, m_height, m_mask_dirty, m_mask_texels);
  if (m_mask_texels.empty()) {
    return;
  }

  m_mask_texture->bind();
  gl_functions->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  gl_functions->glTexSubImage2D(GL_TEXTURE_2D,
                                0,
                                m_mask_dirty.x,
                                m_mask_dirty.z,
                                m_mask_dirty.width,
                                m_mask_dirty.height,
                                GL_RGBA,
                                GL_UNSIGNED_BYTE,
                                m_mask_texels.data());
  m_mask_dirty = {};
}

void FogRenderer::submit(Renderer& renderer, ResourceManager* resources) {
  (void)resources;
  if (!m_enabled || m_width <= 0 || m_height <= 0) {
    return;
  }
  if (m_fog_amount.size() !=
      static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height)) {
    return;
  }

  const float now = renderer.get_animation_time();
  if (m_last_time >= 0.0F && now > m_last_time) {
    advance_reveal(now - m_last_time);
  }
  m_last_time = now;

  if (m_patches_dirty) {
    rebuild_patches();
  }
  if (m_instances.empty()) {
    return;
  }

  upload_mask(renderer);
  if (m_mask_texture == nullptr) {
    return;
  }

  upload_instances();

  FogMaskResources mask;
  mask.texture = m_mask_texture.get();
  mask.size = QVector2D(static_cast<float>(m_width), static_cast<float>(m_height));
  mask.tile_size = m_tile_size;
  mask.enabled = true;

  if (m_instance_buffer) {
    renderer.fog_batch(m_instance_buffer.get(), m_instances.size(), mask);
  } else {
    renderer.fog_batch(m_instances.data(), m_instances.size(), mask);
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
    m_instance_buffer->set_data(m_instances, Buffer::Usage::Dynamic);
    m_instances_dirty = false;
  }
}

} // namespace Render::GL
