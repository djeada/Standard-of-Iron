#include "sword_roman.h"

namespace Render::GL {

RomanSwordRenderer::RomanSwordRenderer()
    : SwordRenderer([]() {
      SwordRenderConfig config;

      config.metal_color = {0.58F, 0.62F, 0.72F};
      config.grip_color = {0.34F, 0.055F, 0.045F};
      config.fuller_color = {0.13F, 0.15F, 0.20F};
      config.sword_length = 0.86F;
      config.sword_width = 0.100F;
      config.guard_half_width = 0.140F;
      config.handle_radius = 0.020F;
      config.pommel_radius = 0.062F;
      config.pommel_length = 0.060F;
      config.blade_ricasso = 0.095F;
      config.blade_taper_bias = 0.28F;
      config.blade_mid_width_scale = 1.32F;
      config.blade_tip_width_scale = 0.15F;
      config.blade_curve = 0.0F;
      config.guard_curve = 0.045F;
      config.guard_spike_length = 0.080F;
      config.has_scabbard = true;
      return config;
    }()) {
}

} // namespace Render::GL
