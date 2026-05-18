#include "minimap_fog_compositor.h"

#include <algorithm>
#include <cmath>

#include "minimap_utils.h"

namespace Game::Map::Minimap {

namespace {
constexpr int k_fog_r = 45;
constexpr int k_fog_g = 38;
constexpr int k_fog_b = 30;
constexpr int k_alpha_unseen = 180;
constexpr int k_alpha_explored = 60;
constexpr int k_alpha_visible = 0;
constexpr float k_alpha_threshold = 0.5F;
} // namespace

void MinimapFogCompositor::reset() {
  m_visibility_version = 0;
  m_lookup_vis_width = 0;
  m_lookup_vis_height = 0;
  m_lookup_img_width = 0;
  m_lookup_img_height = 0;
  m_lookup_cos = std::numeric_limits<float>::quiet_NaN();
  m_lookup_sin = std::numeric_limits<float>::quiet_NaN();
  m_lookup_entries.clear();
}

auto MinimapFogCompositor::apply(const QImage& base_image,
                                 const VisibilityService::Snapshot& snapshot,
                                 QImage& fogged_image) -> bool {
  if (base_image.isNull()) {
    return false;
  }

  if (!snapshot.initialized || snapshot.cells.empty() || snapshot.width <= 0 ||
      snapshot.height <= 0) {
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

  if (!lookup_stale && snapshot.version == m_visibility_version &&
      !fogged_image.isNull() && fogged_image.size() == base_image.size() &&
      fogged_image.format() == base_image.format()) {
    return false;
  }

  if (lookup_stale) {
    rebuild_lookup(snapshot.width, snapshot.height, img_width, img_height);
  }

  if (fogged_image.isNull() || fogged_image.size() != base_image.size() ||
      fogged_image.format() != base_image.format()) {
    fogged_image = base_image.copy();
  }

  const auto visible_state = static_cast<std::uint8_t>(VisibilityState::Visible);
  const auto explored_state = static_cast<std::uint8_t>(VisibilityState::Explored);
  auto alpha_from_cell = [&](std::uint8_t state) -> float {
    if (state == visible_state) {
      return static_cast<float>(k_alpha_visible);
    }
    if (state == explored_state) {
      return static_cast<float>(k_alpha_explored);
    }
    return static_cast<float>(k_alpha_unseen);
  };

  std::size_t lookup_index = 0;
  for (int y = 0; y < img_height; ++y) {
    const auto* base_scanline =
        reinterpret_cast<const QRgb*>(base_image.constScanLine(y));
    auto* fog_scanline = reinterpret_cast<QRgb*>(fogged_image.scanLine(y));
    for (int x = 0; x < img_width; ++x) {
      const LookupEntry& sample = m_lookup_entries[lookup_index++];
      const float a00 =
          alpha_from_cell(snapshot.cells[static_cast<std::size_t>(sample.idx00)]);
      const float a10 =
          alpha_from_cell(snapshot.cells[static_cast<std::size_t>(sample.idx10)]);
      const float a01 =
          alpha_from_cell(snapshot.cells[static_cast<std::size_t>(sample.idx01)]);
      const float a11 =
          alpha_from_cell(snapshot.cells[static_cast<std::size_t>(sample.idx11)]);

      const float alpha_top = a00 + (a10 - a00) * sample.fx;
      const float alpha_bottom = a01 + (a11 - a01) * sample.fx;
      const float fog_alpha =
          std::clamp(alpha_top + (alpha_bottom - alpha_top) * sample.fy, 0.0F, 255.0F);

      if (fog_alpha > k_alpha_threshold) {
        const QRgb original = base_scanline[x];
        const float blend = fog_alpha / 255.0F;
        const float inv_blend = 1.0F - blend;
        const int new_r = std::clamp(
            static_cast<int>(qRed(original) * inv_blend + k_fog_r * blend), 0, 255);
        const int new_g = std::clamp(
            static_cast<int>(qGreen(original) * inv_blend + k_fog_g * blend), 0, 255);
        const int new_b = std::clamp(
            static_cast<int>(qBlue(original) * inv_blend + k_fog_b * blend), 0, 255);
        fog_scanline[x] = qRgba(new_r, new_g, new_b, 255);
      } else {
        fog_scanline[x] = base_scanline[x];
      }
    }
  }

  m_visibility_version = snapshot.version;
  return true;
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
      entry.fx = std::clamp(vis_x - static_cast<float>(base_vx), 0.0F, 1.0F);
      entry.fy = std::clamp(vis_y - static_cast<float>(base_vy), 0.0F, 1.0F);
    }
  }
}

} // namespace Game::Map::Minimap
