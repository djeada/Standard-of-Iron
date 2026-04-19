#pragma once

#include <cstdint>

namespace Render {

inline constexpr std::uint32_t k_animation_cache_prune_interval = 64;
inline constexpr std::uint32_t k_animation_cache_cleanup_mask =
    k_animation_cache_prune_interval - 1;

} // namespace Render
