#include "app/session/world_bootstrap.h"

#include <QCoreApplication>
#include <QOpenGLContext>
#include <QSurfaceFormat>

#include "render/gl/bootstrap.h"
#include "render/ground/ground_renderer.h"

namespace App::Core {

auto WorldBootstrap::initialize(Render::GL::Renderer& renderer,
                                Render::GL::Camera& camera,
                                Render::GL::GroundRenderer* ground,
                                QString* out_error) -> bool {
  if (!Render::GL::RenderBootstrap::initialize(renderer, camera)) {
    if (out_error != nullptr) {
      QString detail = QStringLiteral("Detected OpenGL context: unavailable.");
      if (QOpenGLContext* ctx = QOpenGLContext::currentContext(); ctx != nullptr) {
        const auto fmt = ctx->format();
        const auto ver = fmt.version();
        const QString profile =
            fmt.profile() == QSurfaceFormat::CoreProfile
                ? QStringLiteral("Core")
                : (fmt.profile() == QSurfaceFormat::CompatibilityProfile
                       ? QStringLiteral("Compatibility")
                       : QStringLiteral("NoProfile"));
        detail =
            QStringLiteral("Detected OpenGL %1.%2 (%3); required: OpenGL 3.3 Core.")
                .arg(ver.first)
                .arg(ver.second)
                .arg(profile);
      }
      *out_error =
          detail + QStringLiteral("\n\n") +
          QCoreApplication::translate(
              "WorldBootstrap",
              "Failed to initialize OpenGL renderer.\n\n"
              "This usually means:\n"
              "1. Graphics drivers don't support required OpenGL 3.3 Core Profile\n"
              "2. Running in a VM with incomplete OpenGL support\n"
              "3. Qt software OpenGL fallback is unavailable or failed to initialize\n\n"
              "To fix:\n"
              "- Run run_debug_softwaregl.cmd or set QT_OPENGL=software\n"
              "- Update graphics drivers\n"
              "- On VMs: Enable 3D acceleration in VM settings");
    }
    return false;
  }

  if (ground != nullptr) {
    ground->configure_extent(50.0F);
  }

  return true;
}

void WorldBootstrap::ensure_initialized(bool& initialized,
                                        Render::GL::Renderer& renderer,
                                        Render::GL::Camera& camera,
                                        Render::GL::GroundRenderer* ground,
                                        QString* out_error) {
  if (!initialized) {
    initialized = initialize(renderer, camera, ground, out_error);
  }
}

} // namespace App::Core
