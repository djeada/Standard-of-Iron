#pragma once

#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QDebug>

// Platform-specific OpenGL helpers for Windows/Linux compatibility
namespace Render::GL::Platform {

// Check if persistent mapped buffers are supported (ARB_buffer_storage)
inline auto supportsPersistentMapping() -> bool {
  auto *ctx = QOpenGLContext::currentContext();
  if (ctx == nullptr) {
    return false;
  }

  // Check for GL 4.4+ or ARB_buffer_storage extension
  const auto glVersion = ctx->format().version();
  const int majorVersion = glVersion.first;
  const int minorVersion = glVersion.second;
  
  if (majorVersion > 4 || (majorVersion == 4 && minorVersion >= 4)) {
    return true;
  }

  // Check for extension on older OpenGL versions
  const auto extensions = QString::fromLatin1(
      reinterpret_cast<const char*>(ctx->extraFunctions()->glGetString(GL_EXTENSIONS)));
  return extensions.contains("GL_ARB_buffer_storage");
}

// Platform-agnostic way to get glBufferStorage function pointer
inline auto getBufferStorageFunction() -> void* {
  auto *ctx = QOpenGLContext::currentContext();
  if (ctx == nullptr) {
    return nullptr;
  }

  // Try core function first (GL 4.4+)
  void *func = reinterpret_cast<void*>(ctx->getProcAddress("glBufferStorage"));
  
  // Fallback to ARB extension on older drivers
  if (func == nullptr) {
    func = reinterpret_cast<void*>(ctx->getProcAddress("glBufferStorageARB"));
  }

#ifdef Q_OS_WIN
  // Windows may need explicit wglGetProcAddress fallback
  if (func == nullptr) {
    qWarning() << "Platform::getBufferStorageFunction: glBufferStorage not available on Windows";
  }
#endif

  return func;
}

// GL constants for persistent mapping (may not be defined on all platforms)
#ifndef GL_MAP_PERSISTENT_BIT
#define GL_MAP_PERSISTENT_BIT 0x0040
#endif

#ifndef GL_MAP_COHERENT_BIT
#define GL_MAP_COHERENT_BIT 0x0080
#endif

#ifndef GL_MAP_WRITE_BIT
#define GL_MAP_WRITE_BIT 0x0002
#endif

#ifndef GL_DYNAMIC_STORAGE_BIT
#define GL_DYNAMIC_STORAGE_BIT 0x0100
#endif

// Wrapper for buffer storage creation with fallback
class BufferStorageHelper {
public:
  enum class Mode {
    Persistent,  // Use persistent mapping (GL 4.4+)
    Fallback     // Use traditional glBufferData + map/unmap
  };

  static auto createBuffer(GLuint buffer, GLsizeiptr size, Mode *outMode) -> bool {
    if (supportsPersistentMapping()) {
      typedef void(QOPENGLF_APIENTRYP type_glBufferStorage)(
          GLenum target, GLsizeiptr size, const void *data, GLbitfield flags);
      
      auto glBufferStorage = reinterpret_cast<type_glBufferStorage>(
          getBufferStorageFunction());

      if (glBufferStorage != nullptr) {
        const GLbitfield storageFlags = GL_DYNAMIC_STORAGE_BIT;
        const GLbitfield mapFlags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
        
        glBufferStorage(GL_ARRAY_BUFFER, size, nullptr, storageFlags | mapFlags);
        
        GLenum err = QOpenGLContext::currentContext()->extraFunctions()->glGetError();
        if (err == GL_NO_ERROR) {
          if (outMode != nullptr) {
            *outMode = Mode::Persistent;
          }
          return true;
        }
        qWarning() << "BufferStorageHelper: glBufferStorage failed with error:" << err;
      }
    }

    // Fallback to traditional buffer allocation
    qInfo() << "BufferStorageHelper: Using fallback buffer mode (glBufferData)";
    QOpenGLContext::currentContext()->extraFunctions()->glBufferData(
        GL_ARRAY_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
    
    if (outMode != nullptr) {
      *outMode = Mode::Fallback;
    }
    return true;
  }

  static auto mapBuffer(GLsizeiptr size, Mode mode) -> void* {
    auto *gl = QOpenGLContext::currentContext()->extraFunctions();
    
    if (mode == Mode::Persistent) {
      const GLbitfield mapFlags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
      void *ptr = gl->glMapBufferRange(GL_ARRAY_BUFFER, 0, size, mapFlags);
      if (ptr != nullptr) {
        return ptr;
      }
      qWarning() << "BufferStorageHelper: Persistent mapping failed, falling back";
    }

    // Fallback mapping (non-persistent)
    return gl->glMapBufferRange(GL_ARRAY_BUFFER, 0, size, GL_MAP_WRITE_BIT);
  }
};

} // namespace Render::GL::Platform
