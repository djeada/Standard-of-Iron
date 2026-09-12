#pragma once

#include <cstddef>
#include <cstdint>

namespace Render::GL {

using GlShareGroup = std::uint64_t;

inline constexpr GlShareGroup k_unknown_share_group = 0;

[[nodiscard]] auto current_gl_share_group() noexcept -> GlShareGroup;

[[nodiscard]] auto gl_objects_can_be_released() noexcept -> bool;

[[nodiscard]] auto gl_objects_can_be_released(GlShareGroup group) noexcept -> bool;

enum class DeferredGlObject : std::uint8_t {
  Buffer,
  VertexArray,
  Texture,
};

void defer_gl_delete(DeferredGlObject kind,
                     unsigned int name,
                     GlShareGroup group = k_unknown_share_group) noexcept;
void drain_deferred_gl_deletes();

void forget_gl_share_group(GlShareGroup group) noexcept;

[[nodiscard]] auto deferred_gl_delete_count() noexcept -> std::size_t;

} // namespace Render::GL
