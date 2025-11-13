#include "sword_roman.h"

namespace Render::GL {

RomanSwordRenderer::RomanSwordRenderer() {
  SwordRenderConfig config;

  config.metal_color = {0.70F, 0.71F, 0.76F};
  config.sword_length = 0.65F;
  config.sword_width = 0.058F;
  config.guard_half_width = 0.08F;
  config.handle_radius = 0.016F;
  config.pommel_radius = 0.048F;
  config.blade_ricasso = 0.12F;
  config.blade_taper_bias = 0.60F;
  config.has_scabbard = true;

  setConfig(config);
}

} // namespace Render::GL
