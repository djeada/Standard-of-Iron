#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "../../game/map/visibility_service.h"

namespace Render::Ground {

struct VisibilityMaskChannels {
  static constexpr int k_seen = 0;
  static constexpr int k_known = 1;
  static constexpr int k_stride = 4;

  static constexpr int k_blur_radius = 1;
};

struct MaskRegion {
  int x = 0;
  int z = 0;
  int width = 0;
  int height = 0;

  [[nodiscard]] auto is_empty() const -> bool { return width <= 0 || height <= 0; }

  [[nodiscard]] static auto whole(int grid_width, int grid_height) -> MaskRegion {
    return {0, 0, std::max(0, grid_width), std::max(0, grid_height)};
  }

  void include(int tile_x, int tile_z, int grid_width, int grid_height) {
    const int min_x = std::max(0, tile_x - VisibilityMaskChannels::k_blur_radius);
    const int min_z = std::max(0, tile_z - VisibilityMaskChannels::k_blur_radius);
    const int max_x =
        std::min(grid_width - 1, tile_x + VisibilityMaskChannels::k_blur_radius);
    const int max_z =
        std::min(grid_height - 1, tile_z + VisibilityMaskChannels::k_blur_radius);
    if (max_x < min_x || max_z < min_z) {
      return;
    }
    if (is_empty()) {
      x = min_x;
      z = min_z;
      width = max_x - min_x + 1;
      height = max_z - min_z + 1;
      return;
    }
    const int new_min_x = std::min(x, min_x);
    const int new_min_z = std::min(z, min_z);
    const int new_max_x = std::max(x + width - 1, max_x);
    const int new_max_z = std::max(z + height - 1, max_z);
    x = new_min_x;
    z = new_min_z;
    width = new_max_x - new_min_x + 1;
    height = new_max_z - new_min_z + 1;
  }
};

namespace detail {

inline auto to_byte(float value) -> unsigned char {
  const float clamped = std::clamp(value, 0.0F, 1.0F);
  return static_cast<unsigned char>(clamped * 255.0F + 0.5F);
}

template <typename Sampler>
auto tent_sample(Sampler&& value_at, int width, int height, int x, int z) -> float {
  const int x0 = std::max(0, x - 1);
  const int x1 = x;
  const int x2 = std::min(width - 1, x + 1);
  const int z0 = std::max(0, z - 1);
  const int z1 = z;
  const int z2 = std::min(height - 1, z + 1);

  const float row0 = value_at(x0, z0) + 2.0F * value_at(x1, z0) + value_at(x2, z0);
  const float row1 = value_at(x0, z1) + 2.0F * value_at(x1, z1) + value_at(x2, z1);
  const float row2 = value_at(x0, z2) + 2.0F * value_at(x1, z2) + value_at(x2, z2);

  return (row0 + 2.0F * row1 + row2) * 0.0625F;
}

[[nodiscard]] inline auto
clamp_region(const MaskRegion& region, int width, int height) -> MaskRegion {
  MaskRegion clamped;
  clamped.x = std::clamp(region.x, 0, std::max(0, width - 1));
  clamped.z = std::clamp(region.z, 0, std::max(0, height - 1));
  clamped.width = std::min(region.width, width - clamped.x);
  clamped.height = std::min(region.height, height - clamped.z);
  clamped.width = std::max(0, clamped.width);
  clamped.height = std::max(0, clamped.height);
  return clamped;
}

} // namespace detail

inline void encode_visibility_mask_region(const std::vector<std::uint8_t>& cells,
                                          int width,
                                          int height,
                                          const MaskRegion& region,
                                          std::vector<unsigned char>& out_rgba) {
  const auto cell_count = static_cast<std::size_t>(std::max(0, width)) *
                          static_cast<std::size_t>(std::max(0, height));
  const MaskRegion clamped = detail::clamp_region(region, width, height);
  if (cell_count == 0 || cells.size() != cell_count || clamped.is_empty()) {
    out_rgba.clear();
    return;
  }

  out_rgba.assign(static_cast<std::size_t>(clamped.width) *
                      static_cast<std::size_t>(clamped.height) *
                      VisibilityMaskChannels::k_stride,
                  0U);

  const auto seen_at = [&cells, width](int x, int z) -> float {
    return static_cast<Game::Map::VisibilityState>(
               cells[static_cast<std::size_t>(z) * static_cast<std::size_t>(width) +
                     static_cast<std::size_t>(x)]) ==
                   Game::Map::VisibilityState::Visible
               ? 1.0F
               : 0.0F;
  };
  const auto known_at = [&cells, width](int x, int z) -> float {
    return static_cast<Game::Map::VisibilityState>(
               cells[static_cast<std::size_t>(z) * static_cast<std::size_t>(width) +
                     static_cast<std::size_t>(x)]) != Game::Map::VisibilityState::Unseen
               ? 1.0F
               : 0.0F;
  };

  for (int row = 0; row < clamped.height; ++row) {
    const int z = clamped.z + row;
    for (int column = 0; column < clamped.width; ++column) {
      const int x = clamped.x + column;
      const std::size_t base =
          (static_cast<std::size_t>(row) * static_cast<std::size_t>(clamped.width) +
           static_cast<std::size_t>(column)) *
          VisibilityMaskChannels::k_stride;
      out_rgba[base + VisibilityMaskChannels::k_seen] =
          detail::to_byte(detail::tent_sample(seen_at, width, height, x, z));
      out_rgba[base + VisibilityMaskChannels::k_known] =
          detail::to_byte(detail::tent_sample(known_at, width, height, x, z));
      out_rgba[base + 3] = 255U;
    }
  }
}

inline void encode_visibility_mask(const std::vector<std::uint8_t>& cells,
                                   int width,
                                   int height,
                                   std::vector<unsigned char>& out_rgba) {
  const auto cell_count = static_cast<std::size_t>(std::max(0, width)) *
                          static_cast<std::size_t>(std::max(0, height));
  if (cell_count == 0 || cells.size() != cell_count) {
    out_rgba.assign(cell_count * VisibilityMaskChannels::k_stride, 0U);
    return;
  }
  encode_visibility_mask_region(
      cells, width, height, MaskRegion::whole(width, height), out_rgba);
}

inline void encode_fog_mask_region(const std::vector<float>& fog_amount,
                                   const std::vector<float>& seen_amount,
                                   int width,
                                   int height,
                                   const MaskRegion& region,
                                   std::vector<unsigned char>& out_rgba) {
  const auto cell_count = static_cast<std::size_t>(std::max(0, width)) *
                          static_cast<std::size_t>(std::max(0, height));
  const MaskRegion clamped = detail::clamp_region(region, width, height);
  if (cell_count == 0 || fog_amount.size() != cell_count ||
      seen_amount.size() != cell_count || clamped.is_empty()) {
    out_rgba.clear();
    return;
  }

  out_rgba.assign(static_cast<std::size_t>(clamped.width) *
                      static_cast<std::size_t>(clamped.height) *
                      VisibilityMaskChannels::k_stride,
                  0U);

  const auto fog_at = [&fog_amount, width](int x, int z) -> float {
    return fog_amount[static_cast<std::size_t>(z) * static_cast<std::size_t>(width) +
                      static_cast<std::size_t>(x)];
  };
  const auto seen_at = [&seen_amount, width](int x, int z) -> float {
    return seen_amount[static_cast<std::size_t>(z) * static_cast<std::size_t>(width) +
                       static_cast<std::size_t>(x)];
  };

  for (int row = 0; row < clamped.height; ++row) {
    const int z = clamped.z + row;
    for (int column = 0; column < clamped.width; ++column) {
      const int x = clamped.x + column;
      const std::size_t base =
          (static_cast<std::size_t>(row) * static_cast<std::size_t>(clamped.width) +
           static_cast<std::size_t>(column)) *
          VisibilityMaskChannels::k_stride;
      out_rgba[base] =
          detail::to_byte(detail::tent_sample(fog_at, width, height, x, z));
      out_rgba[base + 1] =
          detail::to_byte(detail::tent_sample(seen_at, width, height, x, z));
      out_rgba[base + 3] = 255U;
    }
  }
}

inline void encode_fog_mask(const std::vector<float>& fog_amount,
                            const std::vector<float>& seen_amount,
                            int width,
                            int height,
                            std::vector<unsigned char>& out_rgba) {
  const auto cell_count = static_cast<std::size_t>(std::max(0, width)) *
                          static_cast<std::size_t>(std::max(0, height));
  if (cell_count == 0 || fog_amount.size() != cell_count ||
      seen_amount.size() != cell_count) {
    out_rgba.assign(cell_count * VisibilityMaskChannels::k_stride, 0U);
    return;
  }
  encode_fog_mask_region(fog_amount,
                         seen_amount,
                         width,
                         height,
                         MaskRegion::whole(width, height),
                         out_rgba);
}

} // namespace Render::Ground
