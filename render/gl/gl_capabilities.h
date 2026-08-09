#pragma once

#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QString>

#include "context_requirements.h"

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

    report_feature_tiers();

    qInfo() << "==================================";
  }

  static auto is_extension_supported(const char* extension) -> bool {
    auto* ctx = QOpenGLContext::currentContext();
    if (ctx == nullptr) {
      return false;
    }

    return ctx->hasExtension(QByteArray(extension));
  }

  static constexpr int k_required_major = ContextRequirements::required.major;
  static constexpr int k_required_minor = ContextRequirements::required.minor;

  [[nodiscard]] static auto
  has_core_version(ContextRequirements::Version version) -> bool {
    auto* ctx = QOpenGLContext::currentContext();
    if (ctx == nullptr) {
      return false;
    }
    const auto format = ctx->format();
    return format.profile() == QSurfaceFormat::CoreProfile &&
           ContextRequirements::at_least(
               format.majorVersion(), format.minorVersion(), version);
  }

  [[nodiscard]] static auto meets_minimum_version() -> bool {
    return has_core_version(ContextRequirements::required);
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

    qInfo() << "SOI_GL_FLOOR: PASS -" << found;
  }

  [[nodiscard]] static auto has_core_4_1() -> bool {
    return has_core_version(ContextRequirements::apple_maximum);
  }

  [[nodiscard]] static auto has_core_4_3() -> bool { return has_core_version({4, 3}); }

  [[nodiscard]] static auto has_core_4_4() -> bool { return has_core_version({4, 4}); }

  [[nodiscard]] static auto has_core_4_5() -> bool {
    return has_core_version(ContextRequirements::preferred);
  }

  [[nodiscard]] static auto has_compute_shaders() -> bool { return has_core_4_3(); }

  [[nodiscard]] static auto has_indirect_draw() -> bool { return has_core_4_3(); }

  static void report_feature_tiers() {
    qInfo() << (has_core_4_1() ? "SOI_GL_TIER_41: PASS"
                               : "SOI_GL_TIER_41: UNAVAILABLE");
    qInfo() << (has_core_4_3() ? "SOI_GL_TIER_43: PASS"
                               : "SOI_GL_TIER_43: UNAVAILABLE");
    qInfo() << (has_core_4_4() ? "SOI_GL_TIER_44: PASS"
                               : "SOI_GL_TIER_44: UNAVAILABLE");
    qInfo() << (has_core_4_5() ? "SOI_GL_TIER_45: PASS"
                               : "SOI_GL_TIER_45: UNAVAILABLE");
  }
};

} // namespace Render::GL
