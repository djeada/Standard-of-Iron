#include "sword_carthage.h"

namespace Render::GL {

CarthageSwordRenderer::CarthageSwordRenderer()
    : SwordRenderer([]() {
      SwordRenderConfig config;

      config.metal_color = {0.78F, 0.70F, 0.56F};
      config.sword_length = 0.96F;
      config.sword_width = 0.076F;
      config.guard_half_width = 0.116F;
      config.handle_radius = 0.015F;
      config.pommel_radius = 0.042F;
      config.pommel_length = 0.035F;
      config.blade_ricasso = 0.14F;
      config.blade_taper_bias = 0.80F;
      config.blade_mid_width_scale = 1.10F;
      config.blade_tip_width_scale = 0.12F;
      config.blade_curve = 0.14F;
      config.guard_curve = -0.04F;
      config.guard_spike_length = 0.070F;
      config.has_scabbard = true;
      return config;
    }()) {
}

} // namespace Render::GL
