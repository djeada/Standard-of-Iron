#include "sword_kingdom.h"

namespace Render::GL {

KingdomSwordRenderer::KingdomSwordRenderer() {
  SwordRenderConfig config;
  // Kingdom longsword: classic medieval knight's sword
  config.metal_color = {0.72F, 0.73F, 0.78F}; // Polished steel
  config.sword_length = 0.85F; // Longer blade for reach
  config.sword_width = 0.068F; // Medium width for balance
  config.guard_half_width = 0.14F; // Large crossguard
  config.handle_radius = 0.015F; // Standard grip
  config.pommel_radius = 0.047F; // Medium pommel
  config.blade_ricasso = 0.18F; // Longer ricasso
  config.blade_taper_bias = 0.65F; // Balanced taper
  config.has_scabbard = true;

  setConfig(config);
}

} // namespace Render::GL
