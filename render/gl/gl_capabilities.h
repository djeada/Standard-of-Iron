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

  static constexpr int k_required_major = 3;
  static constexpr int k_required_minor = 3;

  [[nodiscard]] static auto meets_minimum_version() -> bool {
    auto* ctx = QOpenGLContext::currentContext();
    if (ctx == nullptr) {
      return false;
    }
    const auto format = ctx->format();
    return format.majorVersion() > k_required_major ||
           (format.majorVersion() == k_required_major &&
            format.minorVersion() >= k_required_minor);
  }

  static void report_minimum_version() {
    auto* ctx = QOpenGLContext::currentContext();
    if (ctx == nullptr) {
      qCritical() << "SOI_GL_FLOOR: FAIL - no current OpenGL context";
      return;
    }
    const auto format = ctx->format();
    const QString found =
        QStringLiteral("%1.%2 %3")
            .arg(format.majorVersion())
            .arg(format.minorVersion())
            .arg(format.profile() == QSurfaceFormat::CoreProfile
                     ? QStringLiteral("Core")
                 : format.profile() == QSurfaceFormat::CompatibilityProfile
                     ? QStringLiteral("Compatibility")
                     : QStringLiteral("NoProfile"));

    if (!meets_minimum_version()) {
      qCritical() << "SOI_GL_FLOOR: FAIL - need OpenGL" << k_required_major << "."
                  << k_required_minor << "Core, got" << found;
      return;
    }

    if (format.profile() == QSurfaceFormat::CompatibilityProfile) {
      qWarning() << "SOI_GL_FLOOR: WARN - compatibility profile:" << found;
      return;
    }

    qInfo() << "SOI_GL_FLOOR: PASS -" << found;
  }

  [[nodiscard]] static auto has_core_4_3() -> bool {
    auto* ctx = QOpenGLContext::currentContext();
    if (ctx == nullptr) {
      return false;
    }
    const auto format = ctx->format();
    return format.majorVersion() > 4 ||
           (format.majorVersion() == 4 && format.minorVersion() >= 3);
  }

  [[nodiscard]] static auto has_compute_shaders() -> bool { return has_core_4_3(); }

  [[nodiscard]] static auto has_indirect_draw() -> bool { return has_core_4_3(); }
};

} // namespace Render::GL
