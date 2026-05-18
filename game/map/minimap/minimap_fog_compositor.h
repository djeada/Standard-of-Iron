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
    float fx = 0.0F;
    float fy = 0.0F;
  };

  void rebuild_lookup(int vis_width, int vis_height, int img_width, int img_height);

  std::uint64_t m_visibility_version = 0;
  int m_lookup_vis_width = 0;
  int m_lookup_vis_height = 0;
  int m_lookup_img_width = 0;
  int m_lookup_img_height = 0;
  float m_lookup_cos = std::numeric_limits<float>::quiet_NaN();
  float m_lookup_sin = std::numeric_limits<float>::quiet_NaN();
  std::vector<LookupEntry> m_lookup_entries;
};

} // namespace Game::Map::Minimap
