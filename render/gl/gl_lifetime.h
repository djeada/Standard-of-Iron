#pragma once

#include <cstddef>
#include <cstdint>

namespace Render::GL {

[[nodiscard]] auto gl_objects_can_be_released() noexcept -> bool;

// A GL object can outlive the thread that owns the context: a match teardown
// runs on the GUI thread while the render thread holds the context, and every
// buffer and vertex array freed there used to be abandoned with a warning -
// 3,620 of them in one session, which is a VRAM leak per match loaded.
//
// The name is kept until a thread with a current context can delete it. Whoever
// owns the context calls `drain_deferred_gl_deletes()` once a frame.
enum class DeferredGlObject : std::uint8_t {
  Buffer,
  VertexArray,
  Texture,
};

void defer_gl_delete(DeferredGlObject kind, unsigned int name) noexcept;
void drain_deferred_gl_deletes();

// How many names are still waiting. Zero after a drain on a context thread.
[[nodiscard]] auto deferred_gl_delete_count() noexcept -> std::size_t;

} // namespace Render::GL
