#include "shield_roman.h"

namespace Render::GL {

RomanShieldRenderer::RomanShieldRenderer() {
  ShieldRenderConfig config;
  config.shield_color = {0.65F, 0.15F, 0.15F};      // Roman red
  config.trim_color = {0.78F, 0.70F, 0.45F};        // Gold trim
  config.metal_color = {0.72F, 0.73F, 0.78F};       // Silver metal
  config.shield_radius = 0.18F;                     // Standard size
  config.shield_aspect = 1.0F;                      // Round shield
  config.has_cross_decal = false;                   // No cross for Romans

  setConfig(config);
}

} // namespace Render::GL
