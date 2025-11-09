#include "shield_kingdom.h"

namespace Render::GL {

KingdomShieldRenderer::KingdomShieldRenderer() {
  ShieldRenderConfig config;
  config.shield_color = {0.85F, 0.85F, 0.90F};      // Light/white
  config.trim_color = {0.72F, 0.73F, 0.78F};        // Silver trim
  config.metal_color = {0.72F, 0.73F, 0.78F};       // Silver metal
  config.shield_radius = 0.18F;                     // Standard size
  config.shield_aspect = 1.1F;                      // Slightly tall
  config.has_cross_decal = true;                    // Kingdom has cross

  setConfig(config);
}

} // namespace Render::GL
