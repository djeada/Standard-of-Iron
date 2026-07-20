#include "sword_carthage.h"

namespace Render::GL {

CarthageSwordRenderer::CarthageSwordRenderer()
    : SwordRenderer([]() {
      SwordRenderConfig config;

      config.metal_color = {0.52F, 0.42F, 0.30F};
      config.grip_color = {0.22F, 0.045F, 0.17F};
      config.fuller_color = {0.10F, 0.075F, 0.065F};
      config.sword_length = 1.00F;
      config.sword_width = 0.090F;
      config.guard_half_width = 0.150F;
      config.handle_radius = 0.018F;
      config.pommel_radius = 0.050F;
      config.pommel_length = 0.050F;
      config.blade_ricasso = 0.14F;
      config.blade_taper_bias = 0.84F;
      config.blade_mid_width_scale = 1.34F;
      config.blade_tip_width_scale = 0.10F;
      config.blade_curve = 0.20F;
      config.guard_curve = -0.065F;
      config.guard_spike_length = 0.095F;
      config.blade_back_spike_count = 3;
      config.blade_back_spike_length = 0.085F;
      config.has_scabbard = true;
      return config;
    }()) {
}

} // namespace Render::GL
