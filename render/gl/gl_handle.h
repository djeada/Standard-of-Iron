#pragma once

#include <cstddef>
#include <utility>

#include "gl_lifetime.h"

namespace Render::GL {

// Generates one name of `kind` on the current context and counts it (profiling
// counters and the live-object census). Returns 0 without a current context.
// Program and Shader names come from glCreateProgram/glCreateShader; pass the
// shader stage in `shader_type` for the latter.
[[nodiscard]] auto generate_gl_object(DeferredGlObject kind,
                                      unsigned int shader_type = 0U) -> unsigned int;

// Move-only owner of one GL object name. The name is released through
// release_gl_object(): deleted at once when its share group's context is current,
// otherwise queued for drain_deferred_gl_deletes(), so a handle may be destroyed on
// any thread. This and FenceHandle are the only places render/ may call
// glGen*/glCreate*/glDelete* (scripts/check-gl-ownership.py).
template <DeferredGlObject Kind>
class GlHandle {
public:
  GlHandle() noexcept = default;
  ~GlHandle() { reset(); }

  GlHandle(const GlHandle&) = delete;
  auto operator=(const GlHandle&) -> GlHandle& = delete;

  GlHandle(GlHandle&& other) noexcept
      : m_name(std::exchange(other.m_name, 0U)),
        m_group(std::exchange(other.m_group, k_unknown_share_group)) {}

  auto operator=(GlHandle&& other) noexcept -> GlHandle& {
    if (this != &other) {
      reset();
      m_name = std::exchange(other.m_name, 0U);
      m_group = std::exchange(other.m_group, k_unknown_share_group);
    }
    return *this;
  }

  // A fresh object on the current context; empty when there is none or the
  // driver refused.
  [[nodiscard]] static auto create(unsigned int shader_type = 0U) -> GlHandle {
    GlHandle handle;
    handle.m_name = generate_gl_object(Kind, shader_type);
    if (handle.m_name != 0U) {
      handle.m_group = current_gl_share_group();
    }
    return handle;
  }

  [[nodiscard]] auto id() const noexcept -> unsigned int { return m_name; }
  [[nodiscard]] auto valid() const noexcept -> bool { return m_name != 0U; }
  [[nodiscard]] explicit operator bool() const noexcept { return m_name != 0U; }
  [[nodiscard]] auto share_group() const noexcept -> GlShareGroup { return m_group; }

  // Deletes the object now if its context is current, else queues it.
  void reset() noexcept {
    if (m_name == 0U) {
      return;
    }
    release_gl_object(Kind, std::exchange(m_name, 0U), m_group);
    m_group = k_unknown_share_group;
  }

  // Replaces the object with a fresh one.
  auto recreate(unsigned int shader_type = 0U) -> bool {
    *this = create(shader_type);
    return valid();
  }

private:
  unsigned int m_name = 0U;
  GlShareGroup m_group = k_unknown_share_group;
};

using BufferHandle = GlHandle<DeferredGlObject::Buffer>;
using VertexArrayHandle = GlHandle<DeferredGlObject::VertexArray>;
using TextureHandle = GlHandle<DeferredGlObject::Texture>;
using ProgramHandle = GlHandle<DeferredGlObject::Program>;
using ShaderHandle = GlHandle<DeferredGlObject::Shader>;
using FramebufferHandle = GlHandle<DeferredGlObject::Framebuffer>;
using RenderbufferHandle = GlHandle<DeferredGlObject::Renderbuffer>;
using SamplerHandle = GlHandle<DeferredGlObject::Sampler>;
using QueryHandle = GlHandle<DeferredGlObject::Query>;

// Move-only owner of a GL fence (glFenceSync). Released like a GlHandle.
class FenceHandle {
public:
  FenceHandle() noexcept = default;
  ~FenceHandle() { reset(); }

  FenceHandle(const FenceHandle&) = delete;
  auto operator=(const FenceHandle&) -> FenceHandle& = delete;

  FenceHandle(FenceHandle&& other) noexcept
      : m_sync(std::exchange(other.m_sync, nullptr)),
        m_group(std::exchange(other.m_group, k_unknown_share_group)) {}

  auto operator=(FenceHandle&& other) noexcept -> FenceHandle& {
    if (this != &other) {
      reset();
      m_sync = std::exchange(other.m_sync, nullptr);
      m_group = std::exchange(other.m_group, k_unknown_share_group);
    }
    return *this;
  }

  // Inserts a fence after the commands issued so far on the current context.
  [[nodiscard]] static auto insert() -> FenceHandle;

  // The GLsync, as an opaque pointer (cast with static_cast<GLsync>).
  [[nodiscard]] auto get() const noexcept -> void* { return m_sync; }
  [[nodiscard]] auto valid() const noexcept -> bool { return m_sync != nullptr; }
  [[nodiscard]] explicit operator bool() const noexcept { return m_sync != nullptr; }

  void reset() noexcept {
    if (m_sync == nullptr) {
      return;
    }
    release_gl_sync(std::exchange(m_sync, nullptr), m_group);
    m_group = k_unknown_share_group;
  }

private:
  void* m_sync = nullptr;
  GlShareGroup m_group = k_unknown_share_group;
};

} // namespace Render::GL
