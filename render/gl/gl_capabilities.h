#pragma once

#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QString>

namespace Render::GL {

class GLCapabilities {
public:
  static void log_capabilities() {
    auto* ctx = QOpenGLContext::currentContext();
    if (ctx == nullptr) {
      qWarning() << "GLCapabilities: No current OpenGL context";
      return;
    }

    auto* gl = ctx->extraFunctions();
    const auto format = ctx->format();

    qInfo() << "=== OpenGL Context Information ===";
    qInfo() << "Vendor:" << reinterpret_cast<const char*>(gl->glGetString(GL_VENDOR));
    qInfo() << "Renderer:"
            << reinterpret_cast<const char*>(gl->glGetString(GL_RENDERER));
    qInfo() << "Version:" << reinterpret_cast<const char*>(gl->glGetString(GL_VERSION));
    qInfo() << "GLSL Version:"
            << reinterpret_cast<const char*>(
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

    qInfo() << "=== Extension Support ===";
    qInfo() << "GL_ARB_buffer_storage:"
            << ctx->hasExtension(QByteArrayLiteral("GL_ARB_buffer_storage"));
    qInfo() << "GL_ARB_direct_state_access:"
            << ctx->hasExtension(QByteArrayLiteral("GL_ARB_direct_state_access"));
    qInfo() << "GL_ARB_vertex_array_object:"
            << ctx->hasExtension(QByteArrayLiteral("GL_ARB_vertex_array_object"));
    qInfo() << "GL_ARB_uniform_buffer_object:"
            << ctx->hasExtension(QByteArrayLiteral("GL_ARB_uniform_buffer_object"));

    const bool has_persistent_mapping =
        (format.majorVersion() > 4 ||
         (format.majorVersion() == 4 && format.minorVersion() >= 4)) ||
        ctx->hasExtension(QByteArrayLiteral("GL_ARB_buffer_storage"));

    qInfo() << "Persistent Buffer Mapping:"
            << (has_persistent_mapping ? "Supported" : "Not Supported");
    qInfo() << "Compute Shaders:"
            << (has_compute_shaders() ? "Supported" : "Not Supported");
    qInfo() << "Indirect Draw + SSBO:"
            << (has_indirect_draw() ? "Supported" : "Not Supported");

    qInfo() << "==================================";
  }

  static auto is_extension_supported(const char* extension) -> bool {
    auto* ctx = QOpenGLContext::currentContext();
    if (ctx == nullptr) {
      return false;
    }

    return ctx->hasExtension(QByteArray(extension));
  }

  [[nodiscard]] static auto has_compute_shaders() -> bool {
    auto* ctx = QOpenGLContext::currentContext();
    if (ctx == nullptr) {
      return false;
    }
    const auto format = ctx->format();
    const bool core_43 = format.majorVersion() > 4 ||
                         (format.majorVersion() == 4 && format.minorVersion() >= 3);
    return core_43 || ctx->hasExtension(QByteArrayLiteral("GL_ARB_compute_shader"));
  }

  [[nodiscard]] static auto has_indirect_draw() -> bool {
    auto* ctx = QOpenGLContext::currentContext();
    if (ctx == nullptr) {
      return false;
    }
    const auto format = ctx->format();
    const bool core_43 = format.majorVersion() > 4 ||
                         (format.majorVersion() == 4 && format.minorVersion() >= 3);
    return core_43 || (ctx->hasExtension(QByteArrayLiteral("GL_ARB_draw_indirect")) &&
                       ctx->hasExtension(
                           QByteArrayLiteral("GL_ARB_shader_storage_buffer_object")));
  }
};

} // namespace Render::GL
