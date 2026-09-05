#pragma once

#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>

#include "platform_gl.h"
#include "render_constants.h"

namespace Render::GL {

template <typename T>
class PersistentRingBuffer : protected QOpenGLExtraFunctions {
public:
  PersistentRingBuffer() = default;
  ~PersistentRingBuffer() { destroy(); }

  PersistentRingBuffer(const PersistentRingBuffer&) = delete;
  auto operator=(const PersistentRingBuffer&) -> PersistentRingBuffer& = delete;

  auto initialize(std::size_t capacity,
                  int buffers_in_flight = BufferCapacity::buffers_in_flight) -> bool {
    if (m_buffer != 0) {
      return false;
    }

    initializeOpenGLFunctions();

    if (!hasOpenGLFeature(QOpenGLFunctions::Buffers)) {
      return false;
    }

    m_capacity = capacity;
    m_buffers_in_flight =
        std::min(buffers_in_flight, BufferCapacity::max_buffers_in_flight);
    m_total_size = capacity * sizeof(T) * buffers_in_flight;
    m_current_frame = 0;
    m_frame_offset = 0;

    glGenBuffers(1, &m_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, m_buffer);

    QOpenGLContext* ctx = QOpenGLContext::currentContext();
    if (ctx == nullptr) {
      qWarning() << "PersistentRingBuffer: No current OpenGL context";
      glBindBuffer(GL_ARRAY_BUFFER, 0);
      glDeleteBuffers(1, &m_buffer);
      m_buffer = 0;
      return false;
    }

    Platform::BufferStorageHelper::Mode mode;
    if (!Platform::BufferStorageHelper::create_buffer(m_buffer, m_total_size, &mode)) {
      qWarning() << "PersistentRingBuffer: Failed to create buffer storage";
      glBindBuffer(GL_ARRAY_BUFFER, 0);
      glDeleteBuffers(1, &m_buffer);
      m_buffer = 0;
      return false;
    }

    m_buffer_mode = mode;

    m_mapped_ptr = Platform::BufferStorageHelper::map_buffer(m_total_size, mode);

    if (m_mapped_ptr == nullptr) {
      qWarning() << "PersistentRingBuffer: Failed to map buffer";
      glBindBuffer(GL_ARRAY_BUFFER, 0);
      destroy();
      return false;
    }

    if (mode == Platform::BufferStorageHelper::Mode::Fallback) {
      qInfo() << "PersistentRingBuffer: Running in fallback mode "
                 "(non-persistent mapping)";
      glUnmapBuffer(GL_ARRAY_BUFFER);
      m_mapped_ptr = nullptr;
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return true;
  }

  void destroy() {
    if (m_buffer == 0) {
      return;
    }

    if (QOpenGLContext::currentContext() == nullptr) {

      m_buffer = 0;
      m_mapped_ptr = nullptr;
      m_capacity = 0;
      m_total_size = 0;
      return;
    }

    initializeOpenGLFunctions();

    for (auto& fence : m_fences) {
      if (fence != nullptr) {
        glDeleteSync(fence);
        fence = nullptr;
      }
    }

    if (m_mapped_ptr != nullptr) {
      glBindBuffer(GL_ARRAY_BUFFER, m_buffer);
      glUnmapBuffer(GL_ARRAY_BUFFER);
      glBindBuffer(GL_ARRAY_BUFFER, 0);
      m_mapped_ptr = nullptr;
    }

    glDeleteBuffers(1, &m_buffer);
    m_buffer = 0;
    m_capacity = 0;
    m_total_size = 0;
  }

  auto begin_frame() -> bool {
    const int previous_frame = m_current_frame;
    const std::size_t previous_offset = m_frame_offset;
    m_current_frame = (m_current_frame + 1) % m_buffers_in_flight;
    m_frame_offset = m_current_frame * m_capacity * sizeof(T);
    m_current_count = 0;
    m_slot_writable = true;

    if (m_fences[m_current_frame] == nullptr) {
      return true;
    }

    constexpr GLuint64 k_wait_timeout_ns = 1'000'000'000ULL;
    const GLenum waited = glClientWaitSync(
        m_fences[m_current_frame], GL_SYNC_FLUSH_COMMANDS_BIT, k_wait_timeout_ns);
    if (waited == GL_ALREADY_SIGNALED || waited == GL_CONDITION_SATISFIED) {
      glDeleteSync(m_fences[m_current_frame]);
      m_fences[m_current_frame] = nullptr;
      return true;
    }

    if (waited == GL_WAIT_FAILED) {
      qWarning() << "PersistentRingBuffer: glClientWaitSync failed; disabling "
                    "persistent mapping for this buffer";
      glDeleteSync(m_fences[m_current_frame]);
      m_fences[m_current_frame] = nullptr;
      m_device_error = true;
    } else {
      ++m_slot_timeouts;
      qWarning() << "PersistentRingBuffer: slot" << m_current_frame
                 << "is still read by the GPU after"
                 << static_cast<double>(k_wait_timeout_ns) / 1.0e6
                 << "ms; skipping the persistent path this frame (timeouts"
                 << m_slot_timeouts << ")";
    }

    m_current_frame = previous_frame;
    m_frame_offset = previous_offset;
    m_slot_writable = false;
    return false;
  }

  [[nodiscard]] auto slot_writable() const -> bool { return m_slot_writable; }

  [[nodiscard]] auto slot_timeouts() const -> std::size_t { return m_slot_timeouts; }

  // Called once the frame's draws have been submitted, so the region can be

  void end_frame() {
    if (m_buffer == 0 || !m_slot_writable) {
      return;
    }
    if (m_fences[m_current_frame] != nullptr) {
      glDeleteSync(m_fences[m_current_frame]);
    }
    m_fences[m_current_frame] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  }

  [[nodiscard]] auto remaining() const -> std::size_t {
    return m_capacity > m_current_count ? m_capacity - m_current_count : 0;
  }

  auto write(const T* data, std::size_t count) -> std::size_t {
    if (count == 0 || count > remaining() || m_buffer == 0) {
      return 0;
    }

    if (m_buffer_mode == Platform::BufferStorageHelper::Mode::Fallback) {
      glBindBuffer(GL_ARRAY_BUFFER, m_buffer);

      std::size_t const write_offset = m_frame_offset + m_current_count * sizeof(T);
      void* ptr = glMapBufferRange(GL_ARRAY_BUFFER,
                                   write_offset,
                                   count * sizeof(T),
                                   GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT);

      if (ptr == nullptr) {
        qWarning() << "PersistentRingBuffer: Failed to map buffer for write";
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        return 0;
      }

      std::memcpy(ptr, data, count * sizeof(T));
      glUnmapBuffer(GL_ARRAY_BUFFER);
      glBindBuffer(GL_ARRAY_BUFFER, 0);

      std::size_t const element_offset = m_current_count;
      m_current_count += count;
      return element_offset;
    }

    if (m_mapped_ptr == nullptr) {
      return 0;
    }

    std::size_t const write_offset = m_frame_offset + m_current_count * sizeof(T);
    void* dest = static_cast<char*>(m_mapped_ptr) + write_offset;
    std::memcpy(dest, data, count * sizeof(T));

    std::size_t const element_offset = m_current_count;
    m_current_count += count;

    return element_offset;
  }

  [[nodiscard]] auto buffer() const -> GLuint { return m_buffer; }

  [[nodiscard]] auto current_offset() const -> std::size_t { return m_frame_offset; }

  [[nodiscard]] auto capacity() const -> std::size_t { return m_capacity; }

  [[nodiscard]] auto count() const -> std::size_t { return m_current_count; }

  [[nodiscard]] auto is_valid() const -> bool {
    return m_buffer != 0 && !m_device_error &&
           (m_buffer_mode == Platform::BufferStorageHelper::Mode::Fallback ||
            m_mapped_ptr != nullptr);
  }

private:
  GLuint m_buffer = 0;
  void* m_mapped_ptr = nullptr;
  std::size_t m_capacity = 0;
  std::size_t m_total_size = 0;
  std::size_t m_frame_offset = 0;
  std::size_t m_current_count = 0;
  int m_buffers_in_flight = BufferCapacity::buffers_in_flight;
  int m_current_frame = 0;
  bool m_slot_writable = true;
  bool m_device_error = false;
  std::size_t m_slot_timeouts = 0;
  std::array<GLsync, BufferCapacity::max_buffers_in_flight> m_fences{};
  Platform::BufferStorageHelper::Mode m_buffer_mode =
      Platform::BufferStorageHelper::Mode::Persistent;
};

} // namespace Render::GL
