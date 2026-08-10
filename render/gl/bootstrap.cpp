#include "bootstrap.h"

#include <QDebug>
#include <QOpenGLContext>
#include <qglobal.h>

#include "gl_capabilities.h"
#include "render/scene_renderer.h"
#include "scene/camera.h"

namespace Render::GL {

auto RenderBootstrap::initialize(Renderer& renderer, Camera& camera) -> bool {
  qInfo() << "RenderBootstrap::initialize() - Starting OpenGL initialization...";

  QOpenGLContext* ctx = QOpenGLContext::currentContext();
  if ((ctx == nullptr) || !ctx->isValid()) {
    qCritical() << "RenderBootstrap: no current valid OpenGL context";
    return false;
  }
  qInfo() << "RenderBootstrap: OpenGL context is valid";

  qInfo() << "RenderBootstrap: Logging OpenGL capabilities...";
  GLCapabilities::log_capabilities();
  GLCapabilities::report_minimum_version();
  qInfo() << "RenderBootstrap: Capabilities logged";

  if (!GLCapabilities::meets_minimum_version()) {
    qCritical() << "RenderBootstrap: the driver is below the OpenGL"
                << GLCapabilities::k_required_major << "."
                << GLCapabilities::k_required_minor
                << "Core floor the renderer requires";
    return false;
  }

  qInfo() << "RenderBootstrap: Calling renderer.initialize()...";
  if (!renderer.initialize()) {
    qCritical() << "RenderBootstrap: renderer initialize failed";
    return false;
  }
  qInfo() << "RenderBootstrap: Renderer initialized successfully";

  qInfo() << "RenderBootstrap: Setting camera...";
  renderer.set_camera(&camera);
  qInfo() << "RenderBootstrap: Camera set, initialization complete";

  return true;
}

} // namespace Render::GL
