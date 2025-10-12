#pragma once

#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QDebug>
#include <cstddef>
#include <cstring>

namespace Render::GL {

// Persistent mapped ring buffer for high-frequency data uploads
// Uses ARB_buffer_storage to eliminate per-frame glBufferData/glMapBuffer churn
template <typename T> class PersistentRingBuffer : protected QOpenGLExtraFunctions {
public:
  PersistentRingBuffer() = default;
  ~PersistentRingBuffer() { destroy(); }

  // Non-copyable
  PersistentRingBuffer(const PersistentRingBuffer &) = delete;
  PersistentRingBuffer &operator=(const PersistentRingBuffer &) = delete;

  // Initialize with specified capacity
  // buffersInFlight: how many frames worth of data to buffer (usually 2-3)
  bool initialize(std::size_t capacity, int buffersInFlight = 3) {
    if (m_buffer != 0)
      return false;

    initializeOpenGLFunctions();

    // Check if glBufferStorage is available (OpenGL 4.4+ or ARB_buffer_storage)
    if (!hasOpenGLFeature(QOpenGLFunctions::Buffers)) {
      qDebug() << "PersistentRingBuffer: OpenGL buffers not supported";
      return false;
    }

    m_capacity = capacity;
    m_buffersInFlight = buffersInFlight;
    m_totalSize = capacity * sizeof(T) * buffersInFlight;
    m_currentFrame = 0;
    m_frameOffset = 0;

    glGenBuffers(1, &m_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, m_buffer);

    // GL_MAP_WRITE_BIT: We will write to it
    // GL_MAP_PERSISTENT_BIT: Mapping persists (0x0040)
    // GL_MAP_COHERENT_BIT: No need for explicit flush/barrier (0x0080)
    // GL_DYNAMIC_STORAGE_BIT: Content will be updated (0x0100)
    const GLbitfield storageFlags = 0x0100; // GL_DYNAMIC_STORAGE_BIT
    const GLbitfield mapFlags = 0x0002 | 0x0040 | 0x0080; // WRITE | PERSISTENT | COHERENT

    // Try to use glBufferStorage (OpenGL 4.4+)
    // We need to call it via function pointer since Qt doesn't wrap it in 3.3 core
    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    if (!ctx) {
      qWarning() << "PersistentRingBuffer: No current OpenGL context";
      glBindBuffer(GL_ARRAY_BUFFER, 0);
      glDeleteBuffers(1, &m_buffer);
      m_buffer = 0;
      return false;
    }
    
    typedef void (QOPENGLF_APIENTRYP type_glBufferStorage)(GLenum target, GLsizeiptr size, 
                                                            const void *data, GLbitfield flags);
    type_glBufferStorage glBufferStorage = 
        reinterpret_cast<type_glBufferStorage>(ctx->getProcAddress("glBufferStorage"));
    
    if (!glBufferStorage) {
      qDebug() << "PersistentRingBuffer: glBufferStorage not available (OpenGL < 4.4)";
      glBindBuffer(GL_ARRAY_BUFFER, 0);
      glDeleteBuffers(1, &m_buffer);
      m_buffer = 0;
      return false;
    }

    glBufferStorage(GL_ARRAY_BUFFER, m_totalSize, nullptr, storageFlags | mapFlags);
    
    // Check for GL errors
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
      qWarning() << "PersistentRingBuffer: glBufferStorage failed with error:" << err;
      glBindBuffer(GL_ARRAY_BUFFER, 0);
      glDeleteBuffers(1, &m_buffer);
      m_buffer = 0;
      return false;
    }

    m_mappedPtr = glMapBufferRange(GL_ARRAY_BUFFER, 0, m_totalSize, mapFlags);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    if (!m_mappedPtr) {
      qWarning() << "PersistentRingBuffer: glMapBufferRange failed";
      destroy();
      return false;
    }

    return true;
  }

  void destroy() {
    if (m_buffer == 0)
      return;

    initializeOpenGLFunctions();

    if (m_mappedPtr) {
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

  // Begin a new frame - advances the ring buffer
  void beginFrame() {
    m_currentFrame = (m_currentFrame + 1) % m_buffersInFlight;
    m_frameOffset = m_currentFrame * m_capacity * sizeof(T);
    m_currentCount = 0;
  }

  // Write data to current frame's section of the ring buffer
  // Returns the offset (in elements, not bytes) where data was written
  std::size_t write(const T *data, std::size_t count) {
    if (!m_mappedPtr || count == 0 || count > m_capacity)
      return 0;

    std::size_t writeOffset = m_frameOffset + m_currentCount * sizeof(T);
    void *dest = static_cast<char *>(m_mappedPtr) + writeOffset;
    std::memcpy(dest, data, count * sizeof(T));

    std::size_t elementOffset = m_currentCount;
    m_currentCount += count;
    
    return elementOffset;
  }

  // Get the GPU buffer handle
  GLuint buffer() const { return m_buffer; }

  // Get current frame's byte offset in the buffer
  std::size_t currentOffset() const { return m_frameOffset; }

  // Get capacity per frame
  std::size_t capacity() const { return m_capacity; }

  // Get number of elements written this frame
  std::size_t count() const { return m_currentCount; }

  // Check if buffer is initialized
  bool isValid() const { return m_buffer != 0 && m_mappedPtr != nullptr; }

private:
  GLuint m_buffer = 0;
  void *m_mappedPtr = nullptr;
  std::size_t m_capacity = 0;
  std::size_t m_totalSize = 0;
  std::size_t m_frameOffset = 0;
  std::size_t m_currentCount = 0;
  int m_buffersInFlight = 3;
  int m_currentFrame = 0;
};

} // namespace Render::GL
