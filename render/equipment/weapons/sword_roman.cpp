#include "sword_roman.h"

namespace Render::GL {

RomanSwordRenderer::RomanSwordRenderer()
    : SwordRenderer([]() {
      SwordRenderConfig config;

      config.metal_color = {0.76F, 0.79F, 0.88F};
      config.sword_length = 0.84F;
      config.sword_width = 0.088F;
      config.guard_half_width = 0.104F;
      config.handle_radius = 0.018F;
      config.pommel_radius = 0.054F;
      config.pommel_length = 0.050F;
      config.blade_ricasso = 0.10F;
      config.blade_taper_bias = 0.32F;
      config.blade_mid_width_scale = 1.18F;
      config.blade_tip_width_scale = 0.22F;
      config.blade_curve = 0.0F;
      config.guard_curve = 0.03F;
      config.guard_spike_length = 0.045F;
      config.has_scabbard = true;
      return config;
    }()) {
}

} // namespace Render::GL
