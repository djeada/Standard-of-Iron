#pragma once

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

    const GLbitfield storageFlags = 0x0100;
    const GLbitfield mapFlags = 0x0002 | 0x0040 | 0x0080;

    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    if (ctx == nullptr) {
      qWarning() << "PersistentRingBuffer: No current OpenGL context";
      glBindBuffer(GL_ARRAY_BUFFER, 0);
      glDeleteBuffers(1, &m_buffer);
      m_buffer = 0;
      return false;
    }

    typedef void(QOPENGLF_APIENTRYP type_glBufferStorage)(
        GLenum target, GLsizeiptr size, const void *data, GLbitfield flags);
    auto glBufferStorage = reinterpret_cast<type_glBufferStorage>(
        ctx->getProcAddress("glBufferStorage"));

    if (glBufferStorage == nullptr) {
      glBindBuffer(GL_ARRAY_BUFFER, 0);
      glDeleteBuffers(1, &m_buffer);
      m_buffer = 0;
      return false;
    }

    glBufferStorage(GL_ARRAY_BUFFER, m_totalSize, nullptr,
                    storageFlags | mapFlags);

    GLenum const err = glGetError();
    if (err != GL_NO_ERROR) {
      qWarning() << "PersistentRingBuffer: glBufferStorage failed with error:"
                 << err;
      glBindBuffer(GL_ARRAY_BUFFER, 0);
      glDeleteBuffers(1, &m_buffer);
      m_buffer = 0;
      return false;
    }

    m_mappedPtr = glMapBufferRange(GL_ARRAY_BUFFER, 0, m_totalSize, mapFlags);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    if (m_mappedPtr == nullptr) {
      qWarning() << "PersistentRingBuffer: glMapBufferRange failed";
      destroy();
      return false;
    }

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
    if ((m_mappedPtr == nullptr) || count == 0 || count > m_capacity) {
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
    return m_buffer != 0 && m_mappedPtr != nullptr;
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
};

} // namespace Render::GL
