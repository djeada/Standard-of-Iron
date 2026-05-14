#include "sword_roman.h"

namespace Render::GL {

RomanSwordRenderer::RomanSwordRenderer()
    : SwordRenderer([]() {
      SwordRenderConfig config;

      config.metal_color = {0.72F, 0.74F, 0.83F};
      config.sword_length = 0.82F;
      config.sword_width = 0.082F;
      config.guard_half_width = 0.095F;
      config.handle_radius = 0.018F;
      config.pommel_radius = 0.058F;
      config.blade_ricasso = 0.10F;
      config.blade_taper_bias = 0.60F;
      config.has_scabbard = true;
      return config;
    }()) {
}

} // namespace Render::GL
