#pragma once

#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>

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
    m_buffers_in_flight = buffers_in_flight;
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

  void begin_frame() {
    m_current_frame = (m_current_frame + 1) % m_buffers_in_flight;
    m_frame_offset = m_current_frame * m_capacity * sizeof(T);
    m_current_count = 0;
  }

  auto write(const T* data, std::size_t count) -> std::size_t {
    if (count == 0 || count > m_capacity || m_buffer == 0) {
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
    return m_buffer != 0 &&
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
  Platform::BufferStorageHelper::Mode m_buffer_mode =
      Platform::BufferStorageHelper::Mode::Persistent;
};

} // namespace Render::GL
