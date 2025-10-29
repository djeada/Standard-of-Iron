#include "bootstrap.h"
#include "../scene_renderer.h"
#include "camera.h"
#include "gl_capabilities.h"
#include <QDebug>
#include <QOpenGLContext>
#include <qglobal.h>

namespace Render::GL {

auto RenderBootstrap::initialize(Renderer &renderer, Camera &camera) -> bool {
  qInfo()
      << "RenderBootstrap::initialize() - Starting OpenGL initialization...";

  QOpenGLContext *ctx = QOpenGLContext::currentContext();
  if ((ctx == nullptr) || !ctx->isValid()) {
    qCritical() << "RenderBootstrap: no current valid OpenGL context";
    return false;
  }
  qInfo() << "RenderBootstrap: OpenGL context is valid";

  qInfo() << "RenderBootstrap: Logging OpenGL capabilities...";
  GLCapabilities::logCapabilities();
  qInfo() << "RenderBootstrap: Capabilities logged";

  qInfo() << "RenderBootstrap: Calling renderer.initialize()...";
  if (!renderer.initialize()) {
    qCritical() << "RenderBootstrap: renderer initialize failed";
    return false;
  }
  qInfo() << "RenderBootstrap: Renderer initialized successfully";

  qInfo() << "RenderBootstrap: Setting camera...";
  renderer.setCamera(&camera);
  qInfo() << "RenderBootstrap: Camera set, initialization complete";

  return true;
}

} // namespace Render::GL
