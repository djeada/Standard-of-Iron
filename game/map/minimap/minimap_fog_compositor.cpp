#include "minimap_fog_compositor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

#include "minimap_utils.h"

namespace Game::Map::Minimap {

namespace {
constexpr int k_fog_r = 45;
constexpr int k_fog_g = 38;
constexpr int k_fog_b = 30;
constexpr int k_alpha_unseen = 180;
constexpr int k_alpha_explored = 60;
constexpr int k_alpha_visible = 0;
constexpr int k_bilinear_scale = 256;

[[nodiscard]] auto alpha_from_cell(std::uint8_t state) noexcept -> int {
  if (state == static_cast<std::uint8_t>(VisibilityState::Visible)) {
    return k_alpha_visible;
  }
  if (state == static_cast<std::uint8_t>(VisibilityState::Explored)) {
    return k_alpha_explored;
  }
  return k_alpha_unseen;
}
} // namespace

void MinimapFogCompositor::reset() {
  m_visibility_version = 0;
  m_snapshot_version = 0;
  m_lookup_vis_width = 0;
  m_lookup_vis_height = 0;
  m_lookup_img_width = 0;
  m_lookup_img_height = 0;
  m_lookup_cos = std::numeric_limits<float>::quiet_NaN();
  m_lookup_sin = std::numeric_limits<float>::quiet_NaN();
  m_lookup_entries.clear();
  m_previous_cells.clear();
  m_reverse_offsets.clear();
  m_reverse_pixels.clear();
  m_dirty_pixel_stamps.clear();
  m_dirty_pixels.clear();
  m_dirty_generation = 0;
}

auto MinimapFogCompositor::apply(const QImage& base_image,
                                 const VisibilityService::Snapshot& snapshot,
                                 QImage& fogged_image) -> bool {
  if (base_image.isNull()) {
    return false;
  }

  const bool valid_dimensions = snapshot.width > 0 && snapshot.height > 0;
  const std::size_t expected_cells = valid_dimensions
                                         ? static_cast<std::size_t>(snapshot.width) *
                                               static_cast<std::size_t>(snapshot.height)
                                         : 0U;
  if (!snapshot.initialized || !valid_dimensions ||
      snapshot.cells.size() != expected_cells) {
    return clear(base_image, fogged_image);
  }

  const auto& orientation = MinimapOrientation::instance();
  const float orient_cos = orientation.cos_yaw();
  const float orient_sin = orientation.sin_yaw();
  const int img_width = base_image.width();
  const int img_height = base_image.height();
  const bool lookup_stale =
      m_lookup_vis_width != snapshot.width || m_lookup_vis_height != snapshot.height ||
      m_lookup_img_width != img_width || m_lookup_img_height != img_height ||
      m_lookup_entries.size() != static_cast<std::size_t>(img_width * img_height) ||
      m_lookup_cos != orient_cos || m_lookup_sin != orient_sin;

  if (!lookup_stale && snapshot.version == m_snapshot_version &&
      !fogged_image.isNull() && fogged_image.size() == base_image.size() &&
      fogged_image.format() == base_image.format()) {
    return false;
  }

  if (lookup_stale) {
    rebuild_lookup(snapshot.width, snapshot.height, img_width, img_height);
  }

  const bool image_stale = fogged_image.isNull() ||
                           fogged_image.size() != base_image.size() ||
                           fogged_image.format() != base_image.format();
  if (image_stale) {
    fogged_image = base_image.copy();
  }

  const bool full_recompose =
      lookup_stale || image_stale || m_previous_cells.size() != snapshot.cells.size();
  if (full_recompose) {
    std::size_t pixel = 0;
    for (int y = 0; y < img_height; ++y) {
      const auto* base_scanline =
          reinterpret_cast<const QRgb*>(base_image.constScanLine(y));
      auto* fog_scanline = reinterpret_cast<QRgb*>(fogged_image.scanLine(y));
      for (int x = 0; x < img_width; ++x, ++pixel) {
        fog_scanline[x] = compose_pixel(pixel, base_scanline[x], snapshot);
      }
    }
    m_previous_cells = snapshot.cells;
    m_snapshot_version = snapshot.version;
    ++m_visibility_version;
    return true;
  }

  ++m_dirty_generation;
  if (m_dirty_generation == 0U) {
    std::fill(m_dirty_pixel_stamps.begin(), m_dirty_pixel_stamps.end(), 0U);
    m_dirty_generation = 1U;
  }

  m_dirty_pixels.clear();
  for (std::size_t cell = 0; cell < snapshot.cells.size(); ++cell) {
    if (snapshot.cells[cell] == m_previous_cells[cell]) {
      continue;
    }
    for (std::uint32_t cursor = m_reverse_offsets[cell];
         cursor < m_reverse_offsets[cell + 1U];
         ++cursor) {
      const std::uint32_t pixel = m_reverse_pixels[cursor];
      if (m_dirty_pixel_stamps[pixel] == m_dirty_generation) {
        continue;
      }
      m_dirty_pixel_stamps[pixel] = m_dirty_generation;
      m_dirty_pixels.push_back(pixel);
    }
  }

  m_previous_cells = snapshot.cells;
  m_snapshot_version = snapshot.version;
  if (m_dirty_pixels.empty()) {
    return false;
  }

  if (m_dirty_pixels.size() * 3U >= m_lookup_entries.size()) {
    std::size_t pixel = 0;
    for (int y = 0; y < img_height; ++y) {
      const auto* base_scanline =
          reinterpret_cast<const QRgb*>(base_image.constScanLine(y));
      auto* fog_scanline = reinterpret_cast<QRgb*>(fogged_image.scanLine(y));
      for (int x = 0; x < img_width; ++x, ++pixel) {
        fog_scanline[x] = compose_pixel(pixel, base_scanline[x], snapshot);
      }
    }
  } else {
    std::sort(m_dirty_pixels.begin(), m_dirty_pixels.end());
    int current_y = -1;
    const QRgb* base_scanline = nullptr;
    QRgb* fog_scanline = nullptr;
    for (const std::uint32_t pixel : m_dirty_pixels) {
      const int y = static_cast<int>(pixel / static_cast<std::uint32_t>(img_width));
      const int x = static_cast<int>(pixel % static_cast<std::uint32_t>(img_width));
      if (y != current_y) {
        current_y = y;
        base_scanline = reinterpret_cast<const QRgb*>(base_image.constScanLine(y));
        fog_scanline = reinterpret_cast<QRgb*>(fogged_image.scanLine(y));
      }
      fog_scanline[x] = compose_pixel(pixel, base_scanline[x], snapshot);
    }
  }
  ++m_visibility_version;
  return true;
}

auto MinimapFogCompositor::compose_pixel(
    std::size_t pixel_index,
    QRgb original,
    const VisibilityService::Snapshot& snapshot) const -> QRgb {
  const LookupEntry& sample = m_lookup_entries[pixel_index];
  const int a00 =
      alpha_from_cell(snapshot.cells[static_cast<std::size_t>(sample.idx00)]);
  const int a10 =
      alpha_from_cell(snapshot.cells[static_cast<std::size_t>(sample.idx10)]);
  const int a01 =
      alpha_from_cell(snapshot.cells[static_cast<std::size_t>(sample.idx01)]);
  const int a11 =
      alpha_from_cell(snapshot.cells[static_cast<std::size_t>(sample.idx11)]);

  const int inverse_x = k_bilinear_scale - static_cast<int>(sample.fx);
  const int inverse_y = k_bilinear_scale - static_cast<int>(sample.fy);
  const int alpha_top = a00 * inverse_x + a10 * static_cast<int>(sample.fx);
  const int alpha_bottom = a01 * inverse_x + a11 * static_cast<int>(sample.fx);
  const int fog_alpha =
      (alpha_top * inverse_y + alpha_bottom * static_cast<int>(sample.fy)) >> 16;
  if (fog_alpha <= 0) {
    return original;
  }

  const int inverse_alpha = 255 - fog_alpha;
  const int new_r = (qRed(original) * inverse_alpha + k_fog_r * fog_alpha) / 255;
  const int new_g = (qGreen(original) * inverse_alpha + k_fog_g * fog_alpha) / 255;
  const int new_b = (qBlue(original) * inverse_alpha + k_fog_b * fog_alpha) / 255;
  return qRgba(new_r, new_g, new_b, 255);
}

auto MinimapFogCompositor::clear(const QImage& base_image,
                                 QImage& fogged_image) -> bool {
  if (base_image.isNull()) {
    return false;
  }

  if (m_visibility_version == 0 && !fogged_image.isNull() &&
      fogged_image.size() == base_image.size() &&
      fogged_image.format() == base_image.format()) {
    return false;
  }

  fogged_image = base_image.copy();
  m_visibility_version = 0;
  m_snapshot_version = 0;
  m_previous_cells.clear();
  return true;
}

void MinimapFogCompositor::rebuild_lookup(int vis_width,
                                          int vis_height,
                                          int img_width,
                                          int img_height) {
  if (vis_width <= 0 || vis_height <= 0 || img_width <= 0 || img_height <= 0) {
    reset();
    return;
  }

  m_lookup_vis_width = vis_width;
  m_lookup_vis_height = vis_height;
  m_lookup_img_width = img_width;
  m_lookup_img_height = img_height;

  const auto& orientation = MinimapOrientation::instance();
  const float inv_cos = orientation.cos_yaw();
  const float inv_sin = -orientation.sin_yaw();
  m_lookup_cos = orientation.cos_yaw();
  m_lookup_sin = orientation.sin_yaw();

  m_lookup_entries.resize(static_cast<std::size_t>(img_width * img_height));

  const float scale_x = static_cast<float>(vis_width) / static_cast<float>(img_width);
  const float scale_y = static_cast<float>(vis_height) / static_cast<float>(img_height);
  const float half_img_w = static_cast<float>(img_width) * 0.5F;
  const float half_img_h = static_cast<float>(img_height) * 0.5F;
  const float half_vis_w = static_cast<float>(vis_width) * 0.5F;
  const float half_vis_h = static_cast<float>(vis_height) * 0.5F;

  std::size_t lookup_index = 0;
  for (int y = 0; y < img_height; ++y) {
    const float centered_y = static_cast<float>(y) - half_img_h;
    for (int x = 0; x < img_width; ++x) {
      const float centered_x = static_cast<float>(x) - half_img_w;
      const float world_x = centered_x * inv_cos - centered_y * inv_sin;
      const float world_y = centered_x * inv_sin + centered_y * inv_cos;

      const float vis_x = (world_x * scale_x) + half_vis_w;
      const float vis_y = (world_y * scale_y) + half_vis_h;

      const int base_vx = static_cast<int>(std::floor(vis_x));
      const int base_vy = static_cast<int>(std::floor(vis_y));
      const int vx0 = std::clamp(base_vx, 0, vis_width - 1);
      const int vx1 = std::clamp(base_vx + 1, 0, vis_width - 1);
      const int vy0 = std::clamp(base_vy, 0, vis_height - 1);
      const int vy1 = std::clamp(base_vy + 1, 0, vis_height - 1);

      LookupEntry& entry = m_lookup_entries[lookup_index++];
      entry.idx00 = vy0 * vis_width + vx0;
      entry.idx10 = vy0 * vis_width + vx1;
      entry.idx01 = vy1 * vis_width + vx0;
      entry.idx11 = vy1 * vis_width + vx1;
      const float fx = std::clamp(vis_x - static_cast<float>(base_vx), 0.0F, 1.0F);
      const float fy = std::clamp(vis_y - static_cast<float>(base_vy), 0.0F, 1.0F);
      entry.fx = static_cast<std::uint16_t>(
          std::lround(fx * static_cast<float>(k_bilinear_scale)));
      entry.fy = static_cast<std::uint16_t>(
          std::lround(fy * static_cast<float>(k_bilinear_scale)));
    }
  }

  const std::size_t visibility_cell_count =
      static_cast<std::size_t>(vis_width) * static_cast<std::size_t>(vis_height);
  m_reverse_offsets.assign(visibility_cell_count + 1U, 0U);
  for (const LookupEntry& entry : m_lookup_entries) {
    const std::array<int, 4> ids{entry.idx00, entry.idx10, entry.idx01, entry.idx11};
    for (std::size_t i = 0; i < ids.size(); ++i) {
      bool duplicate = false;
      for (std::size_t j = 0; j < i; ++j) {
        duplicate = duplicate || ids[j] == ids[i];
      }
      if (duplicate) {
        continue;
      }
      ++m_reverse_offsets[static_cast<std::size_t>(ids[i]) + 1U];
    }
  }
  for (std::size_t cell = 1; cell < m_reverse_offsets.size(); ++cell) {
    m_reverse_offsets[cell] += m_reverse_offsets[cell - 1U];
  }
  m_reverse_pixels.resize(m_reverse_offsets.back());
  auto cursors = m_reverse_offsets;
  for (std::uint32_t pixel = 0;
       pixel < static_cast<std::uint32_t>(m_lookup_entries.size());
       ++pixel) {
    const LookupEntry& entry = m_lookup_entries[pixel];
    const std::array<int, 4> ids{entry.idx00, entry.idx10, entry.idx01, entry.idx11};
    for (std::size_t i = 0; i < ids.size(); ++i) {
      bool duplicate = false;
      for (std::size_t j = 0; j < i; ++j) {
        duplicate = duplicate || ids[j] == ids[i];
      }
      if (duplicate) {
        continue;
      }
      const auto cell = static_cast<std::size_t>(ids[i]);
      m_reverse_pixels[cursors[cell]++] = pixel;
    }
  }
  m_dirty_pixel_stamps.assign(m_lookup_entries.size(), 0U);
  m_dirty_pixels.clear();
  m_dirty_pixels.reserve(m_lookup_entries.size() / 3U);
  m_previous_cells.clear();
}

} // namespace Game::Map::Minimap
