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
  Program,
  Shader,
  Framebuffer,
  Renderbuffer,
  Sampler,
  Query,
  Sync,
};

inline constexpr std::size_t k_gl_object_kind_count =
    static_cast<std::size_t>(DeferredGlObject::Sync) + 1U;

[[nodiscard]] auto gl_object_kind_name(DeferredGlObject kind) noexcept -> const char*;

// Every GL name the renderer generates is counted here when it is created and again
// when it is deleted, destroyed with its share group, or dropped with a dead
// context. `live()` is how many are still owned: the leak test asserts it returns
// to where it started once a renderer is torn down.
struct GlObjectCensus {
  std::uint64_t created = 0;
  std::uint64_t deleted = 0;

  [[nodiscard]] auto live() const noexcept -> std::int64_t {
    return static_cast<std::int64_t>(created) - static_cast<std::int64_t>(deleted);
  }
};

[[nodiscard]] auto gl_object_census(DeferredGlObject kind) noexcept -> GlObjectCensus;

void note_gl_object_created(DeferredGlObject kind, std::size_t count = 1) noexcept;

void defer_gl_delete(DeferredGlObject kind,
                     unsigned int name,
                     GlShareGroup group = k_unknown_share_group) noexcept;
void drain_deferred_gl_deletes();

void release_gl_object(DeferredGlObject kind,
                       unsigned int name,
                       GlShareGroup group) noexcept;

// Sync objects are pointers, not names; they go through the same queue.
void release_gl_sync(void* sync, GlShareGroup group) noexcept;

void forget_gl_share_group(GlShareGroup group) noexcept;

[[nodiscard]] auto deferred_gl_delete_count() noexcept -> std::size_t;

} // namespace Render::GL
