#include "sword_carthage.h"

namespace Render::GL {

CarthageSwordRenderer::CarthageSwordRenderer() {
  SwordRenderConfig config;

  config.metal_color = {0.75F, 0.76F, 0.80F};
  config.sword_length = 0.75F;
  config.sword_width = 0.070F;
  config.guard_half_width = 0.10F;
  config.handle_radius = 0.017F;
  config.pommel_radius = 0.042F;
  config.blade_ricasso = 0.14F;
  config.blade_taper_bias = 0.70F;
  config.has_scabbard = true;

  set_config(config);
}

} 
