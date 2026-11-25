#include "render/scene/bootstrap.h"

#include <QDebug>
#include <QOpenGLContext>
#include <qglobal.h>

#include "render/backend/adapter_class.h"
#include "render/frame/graphics_settings.h"
#include "render/gl/gl_capabilities.h"
#include "render/scene/scene_renderer.h"
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
  // First launch only: with no preset of the player's yet, cap the preset for
  // this adapter. The engine then saves the result as the player's choice, so
  // this never runs again. SOI_IGNORE_ADAPTER_CLASS=1 skips the cap, so a
  // developer can try presets above Low on a software or integrated GPU.
  if (!graphics.quality_chosen_by_user() &&
      qEnvironmentVariableIntValue("SOI_IGNORE_ADAPTER_CLASS") == 0) {
    const auto& adapter = RenderBootstrap::adapter();
    const AdapterClass adapter_class = classify_adapter(adapter.renderer);
    Render::GraphicsQuality cap = first_run_quality_cap(adapter_class);
    if (GLCapabilities::is_low_end_adapter(adapter)) {
      cap = Render::GraphicsQuality::Low;
    }
    if (cap < graphics.quality()) {
      qInfo() << "RenderBootstrap: no saved graphics preset; starting on"
              << Render::graphics_quality_key(cap) << "for" << adapter.renderer;
      graphics.set_quality(cap);
    }
  }

  qInfo() << "RenderBootstrap: Calling renderer.initialize()...";
  if (!renderer.initialize()) {
    qCritical() << "RenderBootstrap: renderer initialize failed";
    return false;
  }

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
