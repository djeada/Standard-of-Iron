#pragma once

#include <cstdint>

namespace Render {

// Animation cache cleanup runs every k_animation_cache_prune_interval frames.
// Reduced from 256 (0xFF mask) to 64 (0x3F mask) for more aggressive pruning.
inline constexpr std::uint32_t k_animation_cache_prune_interval = 64;
inline constexpr std::uint32_t k_animation_cache_cleanup_mask = k_animation_cache_prune_interval - 1;

} // namespace Render
