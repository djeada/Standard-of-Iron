#include "bootstrap.h"
#include "../scene_renderer.h"
#include "camera.h"
#include "resources.h"
#include <QDebug>
#include <QOpenGLContext>

namespace Render {
namespace GL {

bool RenderBootstrap::initialize(Renderer &renderer, Camera &camera) {
  QOpenGLContext *ctx = QOpenGLContext::currentContext();
  if (!ctx || !ctx->isValid()) {
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

} 
} 
