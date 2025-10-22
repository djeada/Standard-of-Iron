#pragma once

#include <cstddef>

namespace Render::GL {

// Maximum size for extras caches in entity renderers
// This prevents unbounded memory growth in renderer caches
static constexpr std::size_t MAX_EXTRAS_CACHE_SIZE = 10000;

} // namespace Render::GL
