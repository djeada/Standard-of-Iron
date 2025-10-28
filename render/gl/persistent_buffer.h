#pragma once

#include "platform_gl.h"
#include "render_constants.h"
#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <cstddef>
#include <cstring>

namespace Render::GL {

template <typename T>
class PersistentRingBuffer : protected QOpenGLExtraFunctions {
public:
  PersistentRingBuffer() = default;
  ~PersistentRingBuffer() { destroy(); }

  PersistentRingBuffer(const PersistentRingBuffer &) = delete;
  auto
  operator=(const PersistentRingBuffer &) -> PersistentRingBuffer & = delete;

  auto
  initialize(std::size_t capacity,
             int buffersInFlight = BufferCapacity::BuffersInFlight) -> bool {
    if (m_buffer != 0) {
      return false;
    }

    initializeOpenGLFunctions();

    if (!hasOpenGLFeature(QOpenGLFunctions::Buffers)) {
      return false;
    }

    m_capacity = capacity;
    m_buffersInFlight = buffersInFlight;
    m_totalSize = capacity * sizeof(T) * buffersInFlight;
    m_currentFrame = 0;
    m_frameOffset = 0;

    glGenBuffers(1, &m_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, m_buffer);

    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    if (ctx == nullptr) {
      qWarning() << "PersistentRingBuffer: No current OpenGL context";
      glBindBuffer(GL_ARRAY_BUFFER, 0);
      glDeleteBuffers(1, &m_buffer);
      m_buffer = 0;
      return false;
    }

    // Try to create buffer using platform-aware helper with fallback
    Platform::BufferStorageHelper::Mode mode;
    if (!Platform::BufferStorageHelper::createBuffer(m_buffer, m_totalSize, &mode)) {
      qWarning() << "PersistentRingBuffer: Failed to create buffer storage";
      glBindBuffer(GL_ARRAY_BUFFER, 0);
      glDeleteBuffers(1, &m_buffer);
      m_buffer = 0;
      return false;
    }

    m_bufferMode = mode;

    // Map the buffer with appropriate mode
    m_mappedPtr = Platform::BufferStorageHelper::mapBuffer(m_totalSize, mode);

    if (m_mappedPtr == nullptr) {
      qWarning() << "PersistentRingBuffer: Failed to map buffer";
      glBindBuffer(GL_ARRAY_BUFFER, 0);
      destroy();
      return false;
    }

    // For fallback mode, we need to unmap after each write
    if (mode == Platform::BufferStorageHelper::Mode::Fallback) {
      qInfo() << "PersistentRingBuffer: Running in fallback mode (non-persistent mapping)";
      glUnmapBuffer(GL_ARRAY_BUFFER);
      m_mappedPtr = nullptr;  // Will be remapped on each write
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return true;
  }

  void destroy() {
    if (m_buffer == 0) {
      return;
    }

    initializeOpenGLFunctions();

    if (m_mappedPtr != nullptr) {
      glBindBuffer(GL_ARRAY_BUFFER, m_buffer);
      glUnmapBuffer(GL_ARRAY_BUFFER);
      glBindBuffer(GL_ARRAY_BUFFER, 0);
      m_mappedPtr = nullptr;
    }

    glDeleteBuffers(1, &m_buffer);
    m_buffer = 0;
    m_capacity = 0;
    m_totalSize = 0;
  }

  void beginFrame() {
    m_currentFrame = (m_currentFrame + 1) % m_buffersInFlight;
    m_frameOffset = m_currentFrame * m_capacity * sizeof(T);
    m_currentCount = 0;
  }

  auto write(const T *data, std::size_t count) -> std::size_t {
    if (count == 0 || count > m_capacity || m_buffer == 0) {
      return 0;
    }

    // For fallback mode, we need to map/unmap on each write
    if (m_bufferMode == Platform::BufferStorageHelper::Mode::Fallback) {
      glBindBuffer(GL_ARRAY_BUFFER, m_buffer);
      
      std::size_t const writeOffset = m_frameOffset + m_currentCount * sizeof(T);
      void *ptr = glMapBufferRange(GL_ARRAY_BUFFER, writeOffset, count * sizeof(T), 
                                    GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT);
      
      if (ptr == nullptr) {
        qWarning() << "PersistentRingBuffer: Failed to map buffer for write";
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        return 0;
      }
      
      std::memcpy(ptr, data, count * sizeof(T));
      glUnmapBuffer(GL_ARRAY_BUFFER);
      glBindBuffer(GL_ARRAY_BUFFER, 0);
      
      std::size_t const elementOffset = m_currentCount;
      m_currentCount += count;
      return elementOffset;
    }

    // Persistent mode: direct write to mapped memory
    if (m_mappedPtr == nullptr) {
      return 0;
    }

    std::size_t const writeOffset = m_frameOffset + m_currentCount * sizeof(T);
    void *dest = static_cast<char *>(m_mappedPtr) + writeOffset;
    std::memcpy(dest, data, count * sizeof(T));

    std::size_t const elementOffset = m_currentCount;
    m_currentCount += count;

    return elementOffset;
  }

  [[nodiscard]] auto buffer() const -> GLuint { return m_buffer; }

  [[nodiscard]] auto currentOffset() const -> std::size_t {
    return m_frameOffset;
  }

  [[nodiscard]] auto capacity() const -> std::size_t { return m_capacity; }

  [[nodiscard]] auto count() const -> std::size_t { return m_currentCount; }

  [[nodiscard]] auto isValid() const -> bool {
    return m_buffer != 0 && 
           (m_bufferMode == Platform::BufferStorageHelper::Mode::Fallback || 
            m_mappedPtr != nullptr);
  }

private:
  GLuint m_buffer = 0;
  void *m_mappedPtr = nullptr;
  std::size_t m_capacity = 0;
  std::size_t m_totalSize = 0;
  std::size_t m_frameOffset = 0;
  std::size_t m_currentCount = 0;
  int m_buffersInFlight = BufferCapacity::BuffersInFlight;
  int m_currentFrame = 0;
  Platform::BufferStorageHelper::Mode m_bufferMode = Platform::BufferStorageHelper::Mode::Persistent;
};

} // namespace Render::GL
