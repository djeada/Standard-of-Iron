#pragma once

#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QString>

namespace Render::GL {

class GLCapabilities {
public:
  static void log_capabilities() {
    auto *ctx = QOpenGLContext::currentContext();
    if (ctx == nullptr) {
      qWarning() << "GLCapabilities: No current OpenGL context";
      return;
    }

    auto *gl = ctx->extraFunctions();
    const auto format = ctx->format();

    qInfo() << "=== OpenGL Context Information ===";
    qInfo() << "Vendor:"
            << reinterpret_cast<const char *>(gl->glGetString(GL_VENDOR));
    qInfo() << "Renderer:"
            << reinterpret_cast<const char *>(gl->glGetString(GL_RENDERER));
    qInfo() << "Version:"
            << reinterpret_cast<const char *>(gl->glGetString(GL_VERSION));
    qInfo() << "GLSL Version:"
            << reinterpret_cast<const char *>(
                   gl->glGetString(GL_SHADING_LANGUAGE_VERSION));
    qInfo() << "Context Version:" << format.majorVersion() << "."
            << format.minorVersion();
    qInfo() << "Profile:"
            << (format.profile() == QSurfaceFormat::CoreProfile ? "Core"
                : format.profile() == QSurfaceFormat::CompatibilityProfile
                    ? "Compatibility"
                    : "NoProfile");

#ifdef Q_OS_WIN
    qInfo() << "Platform: Windows";
#elif defined(Q_OS_LINUX)
    qInfo() << "Platform: Linux";
#elif defined(Q_OS_MAC)
    qInfo() << "Platform: macOS";
#else
    qInfo() << "Platform: Unknown";
#endif

    const QString extensions = QString::fromLatin1(
        reinterpret_cast<const char *>(gl->glGetString(GL_EXTENSIONS)));

    qInfo() << "=== Extension Support ===";
    qInfo() << "GL_ARB_buffer_storage:"
            << extensions.contains("GL_ARB_buffer_storage");
    qInfo() << "GL_ARB_direct_state_access:"
            << extensions.contains("GL_ARB_direct_state_access");
    qInfo() << "GL_ARB_vertex_array_object:"
            << extensions.contains("GL_ARB_vertex_array_object");
    qInfo() << "GL_ARB_uniform_buffer_object:"
            << extensions.contains("GL_ARB_uniform_buffer_object");

    const bool hasPersistentMapping =
        (format.majorVersion() > 4 ||
         (format.majorVersion() == 4 && format.minorVersion() >= 4)) ||
        extensions.contains("GL_ARB_buffer_storage");

    qInfo() << "Persistent Buffer Mapping:"
            << (hasPersistentMapping ? "Supported" : "Not Supported");

    qInfo() << "==================================";
  }

  static auto is_extension_supported(const char *extension) -> bool {
    auto *ctx = QOpenGLContext::currentContext();
    if (ctx == nullptr) {
      return false;
    }

    auto *gl = ctx->extraFunctions();
    const QString extensions = QString::fromLatin1(
        reinterpret_cast<const char *>(gl->glGetString(GL_EXTENSIONS)));

    return extensions.contains(extension);
  }
};

} 
