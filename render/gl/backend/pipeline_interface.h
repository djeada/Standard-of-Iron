#pragma once

#include "../shader.h"
#include <QOpenGLFunctions_3_3_Core>

namespace Render::GL::BackendPipelines {

class IPipeline : protected QOpenGLFunctions_3_3_Core {
public:
  ~IPipeline() override = default;

  virtual auto initialize() -> bool = 0;

  virtual void shutdown() = 0;

  virtual void cache_uniforms() = 0;

  [[nodiscard]] virtual auto is_initialized() const -> bool = 0;
};

} 
