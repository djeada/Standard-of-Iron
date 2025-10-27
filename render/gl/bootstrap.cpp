#include "bootstrap.h"
#include "../scene_renderer.h"
#include "camera.h"
#include <QDebug>
#include <QOpenGLContext>
#include <qglobal.h>

namespace Render::GL {

auto RenderBootstrap::initialize(Renderer &renderer, Camera &camera) -> bool {
  QOpenGLContext *ctx = QOpenGLContext::currentContext();
  if ((ctx == nullptr) || !ctx->isValid()) {
    qWarning() << "RenderBootstrap: no current valid OpenGL context";
    return false;
  }
  if (!renderer.initialize()) {
    qWarning() << "RenderBootstrap: renderer initialize failed";
    return false;
  }
  renderer.setCamera(&camera);
  return true;
}

} // namespace Render::GL
