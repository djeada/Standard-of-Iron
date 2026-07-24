#pragma once

#include <QImage>

#include <cstdint>
#include <limits>
#include <vector>

#include "../visibility_service.h"

namespace Game::Map::Minimap {

class MinimapFogCompositor {
public:
  void reset();

  [[nodiscard]] auto apply(const QImage& base_image,
                           const VisibilityService::Snapshot& snapshot,
                           QImage& fogged_image) -> bool;

  [[nodiscard]] auto clear(const QImage& base_image, QImage& fogged_image) -> bool;

  [[nodiscard]] auto version() const -> std::uint64_t { return m_visibility_version; }

private:
  struct LookupEntry {
    int idx00 = 0;
    int idx10 = 0;
    int idx01 = 0;
    int idx11 = 0;
    std::uint16_t fx = 0;
    std::uint16_t fy = 0;
  };

  void rebuild_lookup(int vis_width, int vis_height, int img_width, int img_height);
  [[nodiscard]] auto
  compose_pixel(std::size_t pixel_index,
                QRgb original,
                const VisibilityService::Snapshot& snapshot) const -> QRgb;

  std::uint64_t m_visibility_version = 0;
  std::uint64_t m_snapshot_version = 0;
  int m_lookup_vis_width = 0;
  int m_lookup_vis_height = 0;
  int m_lookup_img_width = 0;
  int m_lookup_img_height = 0;
  float m_lookup_cos = std::numeric_limits<float>::quiet_NaN();
  float m_lookup_sin = std::numeric_limits<float>::quiet_NaN();
  std::vector<LookupEntry> m_lookup_entries;
  std::vector<std::uint8_t> m_previous_cells;
  std::vector<std::uint32_t> m_reverse_offsets;
  std::vector<std::uint32_t> m_reverse_pixels;
  std::vector<std::uint32_t> m_dirty_pixel_stamps;
  std::vector<std::uint32_t> m_dirty_pixels;
  std::uint32_t m_dirty_generation = 0;
};

} // namespace Game::Map::Minimap
