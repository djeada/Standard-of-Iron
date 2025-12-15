#include "fog_of_war_mask.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace Game::Map::Minimap {

FogOfWarMask::FogOfWarMask(int map_width, int map_height, float tile_size,
                           const FogOfWarConfig &config)
    : m_config(config), m_map_width(map_width), m_map_height(map_height),
      m_tile_size(tile_size) {

  m_fog_width = std::max(1, map_width / m_config.resolution_divisor);
  m_fog_height = std::max(1, map_height / m_config.resolution_divisor);

  m_fog_cell_size = tile_size * static_cast<float>(m_config.resolution_divisor);

  const size_t total_cells =
      static_cast<size_t>(m_fog_width) * static_cast<size_t>(m_fog_height);
  const size_t bytes_needed = (total_cells + 3) / 4;

  m_visibility_data.resize(bytes_needed, 0);
}

FogOfWarMask::~FogOfWarMask() = default;

FogOfWarMask::FogOfWarMask(FogOfWarMask &&) noexcept = default;
auto FogOfWarMask::operator=(FogOfWarMask &&) noexcept -> FogOfWarMask & =
                                                              default;

void FogOfWarMask::set_cell(int fog_x, int fog_y, VisibilityState state) {
  if (fog_x < 0 || fog_x >= m_fog_width || fog_y < 0 || fog_y >= m_fog_height) {
    return;
  }

  const size_t cell_index =
      static_cast<size_t>(fog_y) * static_cast<size_t>(m_fog_width) +
      static_cast<size_t>(fog_x);
  const size_t byte_index = cell_index / 4;
  const int bit_offset = static_cast<int>((cell_index % 4) * 2);

  const uint8_t mask = static_cast<uint8_t>(~(0x03 << bit_offset));
  const uint8_t value =
      static_cast<uint8_t>(static_cast<uint8_t>(state) << bit_offset);

  m_visibility_data[byte_index] =
      static_cast<uint8_t>((m_visibility_data[byte_index] & mask) | value);
}

auto FogOfWarMask::get_cell(int fog_x, int fog_y) const -> VisibilityState {
  if (fog_x < 0 || fog_x >= m_fog_width || fog_y < 0 || fog_y >= m_fog_height) {
    return VisibilityState::Unseen;
  }

  const size_t cell_index =
      static_cast<size_t>(fog_y) * static_cast<size_t>(m_fog_width) +
      static_cast<size_t>(fog_x);
  const size_t byte_index = cell_index / 4;
  const int bit_offset = static_cast<int>((cell_index % 4) * 2);

  const uint8_t value = static_cast<uint8_t>(
      (m_visibility_data[byte_index] >> bit_offset) & 0x03);
  return static_cast<VisibilityState>(value);
}

auto FogOfWarMask::world_to_fog(float world_x,
                                float world_z) const -> std::pair<int, int> {

  const float world_width = static_cast<float>(m_map_width) * m_tile_size;
  const float world_height = static_cast<float>(m_map_height) * m_tile_size;

  const float norm_x = (world_x + world_width * 0.5F) / world_width;
  const float norm_z = (world_z + world_height * 0.5F) / world_height;

  const int fog_x =
      std::clamp(static_cast<int>(norm_x * static_cast<float>(m_fog_width)), 0,
                 m_fog_width - 1);
  const int fog_y =
      std::clamp(static_cast<int>(norm_z * static_cast<float>(m_fog_height)), 0,
                 m_fog_height - 1);

  return {fog_x, fog_y};
}

void FogOfWarMask::clear_current_visibility() {
  for (size_t byte_idx = 0; byte_idx < m_visibility_data.size(); ++byte_idx) {
    uint8_t byte_val = m_visibility_data[byte_idx];
    if (byte_val == 0) {
      continue;
    }

    uint8_t new_val = 0;
    for (int cell = 0; cell < 4; ++cell) {
      const int bit_offset = cell * 2;
      const uint8_t cell_val = (byte_val >> bit_offset) & 0x03;

      if (cell_val == static_cast<uint8_t>(VisibilityState::Visible)) {
        new_val |= static_cast<uint8_t>(
            static_cast<uint8_t>(VisibilityState::Revealed) << bit_offset);
      } else {
        new_val |= static_cast<uint8_t>(cell_val << bit_offset);
      }
    }
    m_visibility_data[byte_idx] = new_val;
  }
}

void FogOfWarMask::reveal_circle(int center_x, int center_y,
                                 float radius_cells) {

  const int radius_int = static_cast<int>(std::ceil(radius_cells));
  const float radius_sq = radius_cells * radius_cells;

  const int min_y = std::max(0, center_y - radius_int);
  const int max_y = std::min(m_fog_height - 1, center_y + radius_int);
  const int min_x = std::max(0, center_x - radius_int);
  const int max_x = std::min(m_fog_width - 1, center_x + radius_int);

  for (int y = min_y; y <= max_y; ++y) {
    const float dy = static_cast<float>(y - center_y);
    const float dy_sq = dy * dy;

    for (int x = min_x; x <= max_x; ++x) {
      const float dx = static_cast<float>(x - center_x);
      const float dist_sq = dx * dx + dy_sq;

      if (dist_sq <= radius_sq) {
        set_cell(x, y, VisibilityState::Visible);
      }
    }
  }
}

void FogOfWarMask::update_vision(const std::vector<VisionSource> &sources,
                                 int player_id) {

  clear_current_visibility();

  for (const auto &source : sources) {
    if (source.player_id != player_id) {
      continue;
    }

    const auto [fog_x, fog_y] = world_to_fog(source.world_x, source.world_z);

    const float radius_cells = source.vision_radius / m_fog_cell_size;

    reveal_circle(fog_x, fog_y, radius_cells);
  }

  m_dirty = true;
}

auto FogOfWarMask::tick(const std::vector<VisionSource> &sources,
                        int player_id) -> bool {
  ++m_frame_counter;

  if (m_frame_counter >= m_config.update_interval) {
    m_frame_counter = 0;
    update_vision(sources, player_id);
    return true;
  }

  return false;
}

auto FogOfWarMask::get_visibility(int fog_x,
                                  int fog_y) const -> VisibilityState {
  return get_cell(fog_x, fog_y);
}

auto FogOfWarMask::is_revealed(float world_x, float world_z) const -> bool {
  const auto [fog_x, fog_y] = world_to_fog(world_x, world_z);
  const auto state = get_cell(fog_x, fog_y);
  return state != VisibilityState::Unseen;
}

auto FogOfWarMask::is_visible(float world_x, float world_z) const -> bool {
  const auto [fog_x, fog_y] = world_to_fog(world_x, world_z);
  return get_cell(fog_x, fog_y) == VisibilityState::Visible;
}

void FogOfWarMask::reset() {
  std::memset(m_visibility_data.data(), 0, m_visibility_data.size());
  m_dirty = true;
}

void FogOfWarMask::reveal_all() {
  std::memset(m_visibility_data.data(), 0x55, m_visibility_data.size());

  m_dirty = true;
}

auto FogOfWarMask::memory_usage() const -> size_t {
  size_t total = sizeof(*this);
  total += m_visibility_data.capacity();
  total += static_cast<size_t>(m_cached_mask.sizeInBytes());
  return total;
}

void FogOfWarMask::apply_gaussian_blur(std::vector<uint8_t> &alpha_buffer,
                                       int width, int height) const {
  if (m_config.blur_radius <= 0) {
    return;
  }

  const int radius = m_config.blur_radius;

  const int kernel_size = radius * 2 + 1;
  std::vector<float> kernel(static_cast<size_t>(kernel_size));
  float kernel_sum = 0.0F;

  const float sigma = static_cast<float>(radius) / 2.0F;
  const float sigma_sq_2 = 2.0F * sigma * sigma;

  for (int i = 0; i < kernel_size; ++i) {
    const float x = static_cast<float>(i - radius);
    kernel[static_cast<size_t>(i)] = std::exp(-(x * x) / sigma_sq_2);
    kernel_sum += kernel[static_cast<size_t>(i)];
  }

  for (auto &k : kernel) {
    k /= kernel_sum;
  }

  std::vector<float> temp(static_cast<size_t>(width * height));

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      float sum = 0.0F;

      for (int k = -radius; k <= radius; ++k) {
        const int sample_x = std::clamp(x + k, 0, width - 1);
        const size_t src_idx = static_cast<size_t>(y * width + sample_x);
        sum += static_cast<float>(alpha_buffer[src_idx]) *
               kernel[static_cast<size_t>(k + radius)];
      }

      temp[static_cast<size_t>(y * width + x)] = sum;
    }
  }

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      float sum = 0.0F;

      for (int k = -radius; k <= radius; ++k) {
        const int sample_y = std::clamp(y + k, 0, height - 1);
        const size_t src_idx = static_cast<size_t>(sample_y * width + x);
        sum += temp[src_idx] * kernel[static_cast<size_t>(k + radius)];
      }

      alpha_buffer[static_cast<size_t>(y * width + x)] =
          static_cast<uint8_t>(std::clamp(sum, 0.0F, 255.0F));
    }
  }
}

auto FogOfWarMask::generate_mask(int target_width,
                                 int target_height) const -> QImage {

  if (!m_dirty && m_cached_width == target_width &&
      m_cached_height == target_height && !m_cached_mask.isNull()) {
    return m_cached_mask;
  }

  std::vector<uint8_t> fog_alpha(static_cast<size_t>(m_fog_width) *
                                 static_cast<size_t>(m_fog_height));

  for (int y = 0; y < m_fog_height; ++y) {
    for (int x = 0; x < m_fog_width; ++x) {
      const auto state = get_cell(x, y);
      uint8_t alpha = m_config.alpha_unseen;

      switch (state) {
      case VisibilityState::Unseen:
        alpha = m_config.alpha_unseen;
        break;
      case VisibilityState::Revealed:
        alpha = m_config.alpha_revealed;
        break;
      case VisibilityState::Visible:
        alpha = m_config.alpha_visible;
        break;
      }

      fog_alpha[static_cast<size_t>(y * m_fog_width + x)] = alpha;
    }
  }

  apply_gaussian_blur(fog_alpha, m_fog_width, m_fog_height);

  QImage mask(target_width, target_height, QImage::Format_RGBA8888);

  const float scale_x =
      static_cast<float>(m_fog_width) / static_cast<float>(target_width);
  const float scale_y =
      static_cast<float>(m_fog_height) / static_cast<float>(target_height);

  for (int y = 0; y < target_height; ++y) {
    auto *scanline = reinterpret_cast<uint32_t *>(mask.scanLine(y));

    for (int x = 0; x < target_width; ++x) {

      const float fog_x = static_cast<float>(x) * scale_x;
      const float fog_y = static_cast<float>(y) * scale_y;

      const int x0 = std::min(static_cast<int>(fog_x), m_fog_width - 1);
      const int y0 = std::min(static_cast<int>(fog_y), m_fog_height - 1);
      const int x1 = std::min(x0 + 1, m_fog_width - 1);
      const int y1 = std::min(y0 + 1, m_fog_height - 1);

      const float fx = fog_x - static_cast<float>(x0);
      const float fy = fog_y - static_cast<float>(y0);

      const float a00 = static_cast<float>(
          fog_alpha[static_cast<size_t>(y0 * m_fog_width + x0)]);
      const float a10 = static_cast<float>(
          fog_alpha[static_cast<size_t>(y0 * m_fog_width + x1)]);
      const float a01 = static_cast<float>(
          fog_alpha[static_cast<size_t>(y1 * m_fog_width + x0)]);
      const float a11 = static_cast<float>(
          fog_alpha[static_cast<size_t>(y1 * m_fog_width + x1)]);

      const float alpha_top = a00 + (a10 - a00) * fx;
      const float alpha_bot = a01 + (a11 - a01) * fx;
      const float alpha = alpha_top + (alpha_bot - alpha_top) * fy;

      const uint8_t final_alpha =
          static_cast<uint8_t>(std::clamp(alpha, 0.0F, 255.0F));

      scanline[x] = (static_cast<uint32_t>(final_alpha) << 24) |
                    (static_cast<uint32_t>(m_config.fog_color_b) << 16) |
                    (static_cast<uint32_t>(m_config.fog_color_g) << 8) |
                    static_cast<uint32_t>(m_config.fog_color_r);
    }
  }

  m_cached_mask = mask;
  m_cached_width = target_width;
  m_cached_height = target_height;

  return mask;
}

} // namespace Game::Map::Minimap
