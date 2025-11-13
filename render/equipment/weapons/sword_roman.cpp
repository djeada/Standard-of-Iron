#include "sword_roman.h"

namespace Render::GL {

RomanSwordRenderer::RomanSwordRenderer() {
  SwordRenderConfig config;
  // Roman gladius: short, practical stabbing sword
  config.metal_color = {0.70F, 0.71F, 0.76F}; // Standard steel
  config.sword_length = 0.65F; // Shorter gladius length
  config.sword_width = 0.058F; // Narrower blade for thrusting
  config.guard_half_width = 0.08F; // Minimal guard
  config.handle_radius = 0.016F; // Standard grip
  config.pommel_radius = 0.048F; // Larger spherical pommel
  config.blade_ricasso = 0.12F; // Short ricasso
  config.blade_taper_bias = 0.60F; // Sharp taper for point
  config.has_scabbard = true;

  setConfig(config);
}

} // namespace Render::GL
