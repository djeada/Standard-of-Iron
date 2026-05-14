#include "sword_carthage.h"

namespace Render::GL {

CarthageSwordRenderer::CarthageSwordRenderer()
    : SwordRenderer([]() {
      SwordRenderConfig config;

      config.metal_color = {0.76F, 0.70F, 0.58F};
      config.sword_length = 0.96F;
      config.sword_width = 0.070F;
      config.guard_half_width = 0.118F;
      config.handle_radius = 0.015F;
      config.pommel_radius = 0.044F;
      config.blade_ricasso = 0.16F;
      config.blade_taper_bias = 0.72F;
      config.has_scabbard = true;
      return config;
    }()) {
}

} // namespace Render::GL
