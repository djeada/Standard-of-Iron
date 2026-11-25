#pragma once

#include <cstddef>
#include <utility>

#include "render/gl/gl_lifetime.h"

namespace Render::GL {

[[nodiscard]] auto generate_gl_object(DeferredGlObject kind,
                                      unsigned int shader_type = 0U) -> unsigned int;

template <DeferredGlObject Kind>
class GlHandle {
public:
  GlHandle() noexcept = default;
  ~GlHandle() { reset(); }

  GlHandle(const GlHandle&) = delete;
  auto operator=(const GlHandle&) -> GlHandle& = delete;

  GlHandle(GlHandle&& other) noexcept
      : m_name(std::exchange(other.m_name, 0U))
      , m_group(std::exchange(other.m_group, k_unknown_share_group)) {}

  auto operator=(GlHandle&& other) noexcept -> GlHandle& {
    if (this != &other) {
      reset();
      m_name = std::exchange(other.m_name, 0U);
      m_group = std::exchange(other.m_group, k_unknown_share_group);
    }
    return *this;
  }

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

  void reset() noexcept {
    if (m_name == 0U) {
      return;
    }
    release_gl_object(Kind, std::exchange(m_name, 0U), m_group);
    m_group = k_unknown_share_group;
  }

  auto recreate(unsigned int shader_type = 0U) -> bool {
    *this = create(shader_type);
    return valid();
  }

private:
  unsigned int m_name = 0U;
  GlShareGroup m_group = k_unknown_share_group;
};

using BorrowedGlName = unsigned int;

using BufferHandle = GlHandle<DeferredGlObject::Buffer>;
using VertexArrayHandle = GlHandle<DeferredGlObject::VertexArray>;
using TextureHandle = GlHandle<DeferredGlObject::Texture>;
using ProgramHandle = GlHandle<DeferredGlObject::Program>;
using ShaderHandle = GlHandle<DeferredGlObject::Shader>;
using FramebufferHandle = GlHandle<DeferredGlObject::Framebuffer>;
using RenderbufferHandle = GlHandle<DeferredGlObject::Renderbuffer>;
using SamplerHandle = GlHandle<DeferredGlObject::Sampler>;
using QueryHandle = GlHandle<DeferredGlObject::Query>;

class FenceHandle {
public:
  FenceHandle() noexcept = default;
  ~FenceHandle() { reset(); }

  FenceHandle(const FenceHandle&) = delete;
  auto operator=(const FenceHandle&) -> FenceHandle& = delete;

  FenceHandle(FenceHandle&& other) noexcept
      : m_sync(std::exchange(other.m_sync, nullptr))
      , m_group(std::exchange(other.m_group, k_unknown_share_group)) {}

  auto operator=(FenceHandle&& other) noexcept -> FenceHandle& {
    if (this != &other) {
      reset();
      m_sync = std::exchange(other.m_sync, nullptr);
      m_group = std::exchange(other.m_group, k_unknown_share_group);
    }
    return *this;
  }

  [[nodiscard]] static auto insert() -> FenceHandle;

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
