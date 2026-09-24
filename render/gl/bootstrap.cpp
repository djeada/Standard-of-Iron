#include "bootstrap.h"

#include <QDebug>
#include <QOpenGLContext>
#include <qglobal.h>

#include "gl_capabilities.h"
#include "render/graphics_settings.h"
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
  RenderBootstrap::remember_adapter(GLCapabilities::describe_adapter());
  qInfo() << "RenderBootstrap: Capabilities logged";

  if (!GLCapabilities::meets_minimum_version()) {
    qCritical() << "RenderBootstrap: the driver is below the OpenGL"
                << GLCapabilities::k_required_major << "."
                << GLCapabilities::k_required_minor
                << "Core floor the renderer requires";
    return false;
  }

  auto& graphics = Render::GraphicsSettings::instance();
  if (!graphics.quality_chosen_by_user() &&
      graphics.quality() != Render::GraphicsQuality::Low &&
      GLCapabilities::is_low_end_adapter(RenderBootstrap::adapter())) {
    qInfo() << "RenderBootstrap: no saved graphics preset and a low-end adapter ("
            << RenderBootstrap::adapter().renderer << "); starting on the Low preset";
    graphics.set_quality(Render::GraphicsQuality::Low);
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

namespace {
GLCapabilities::AdapterDescription g_adapter;
} // namespace

void RenderBootstrap::remember_adapter(
    const GLCapabilities::AdapterDescription& adapter) {
  g_adapter = adapter;
}

auto RenderBootstrap::adapter() -> const GLCapabilities::AdapterDescription& {
  return g_adapter;
}

} // namespace Render::GL
