#include "sword_kingdom.h"

namespace Render::GL {

KingdomSwordRenderer::KingdomSwordRenderer() {
  SwordRenderConfig config;

  config.metal_color = {0.72F, 0.73F, 0.78F};
  config.sword_length = 0.85F;
  config.sword_width = 0.068F;
  config.guard_half_width = 0.14F;
  config.handle_radius = 0.015F;
  config.pommel_radius = 0.047F;
  config.blade_ricasso = 0.18F;
  config.blade_taper_bias = 0.65F;
  config.has_scabbard = true;

  setConfig(config);
}

} // namespace Render::GL
