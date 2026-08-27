#pragma once

#include <cstddef>
#include <cstdint>

namespace Render::GL {

[[nodiscard]] auto gl_objects_can_be_released() noexcept -> bool;

enum class DeferredGlObject : std::uint8_t {
  Buffer,
  VertexArray,
  Texture,
};

void defer_gl_delete(DeferredGlObject kind, unsigned int name) noexcept;
void drain_deferred_gl_deletes();

[[nodiscard]] auto deferred_gl_delete_count() noexcept -> std::size_t;

} // namespace Render::GL
