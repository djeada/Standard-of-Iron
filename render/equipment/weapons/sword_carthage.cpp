#include "sword_carthage.h"

namespace Render::GL {

CarthageSwordRenderer::CarthageSwordRenderer() {
  SwordRenderConfig config;
  // Carthaginian sword: curved falcata-style blade
  config.metal_color = {0.75F, 0.76F, 0.80F}; // Slightly brighter steel
  config.sword_length = 0.75F; // Shorter, more curved blade
  config.sword_width = 0.070F; // Wider blade for chopping
  config.guard_half_width = 0.10F; // Smaller guard
  config.handle_radius = 0.017F; // Standard grip
  config.pommel_radius = 0.042F; // Smaller pommel
  config.blade_ricasso = 0.14F; // Less ricasso
  config.blade_taper_bias = 0.70F; // More aggressive taper
  config.has_scabbard = true;

  setConfig(config);
}

} // namespace Render::GL
