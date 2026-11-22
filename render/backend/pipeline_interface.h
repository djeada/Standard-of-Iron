#pragma once

#include <QOpenGLFunctions_3_3_Core>

#include "render/gl/shader.h"

namespace Render::GL::BackendPipelines {

class IPipeline : protected QOpenGLFunctions_3_3_Core {
public:
  ~IPipeline() override = default;

  // Resolves this pipeline's GL entry points on the current context. The
  // backend calls it for every pipeline before initialize(), so a pipeline's
  // draw methods never call through an unresolved function table.
  void initialize_gl_functions() { initializeOpenGLFunctions(); }

  virtual auto initialize() -> bool = 0;

  virtual void shutdown() = 0;

  virtual void cache_uniforms() = 0;

  [[nodiscard]] virtual auto is_initialized() const -> bool = 0;
};

} // namespace Render::GL::BackendPipelines
