#include "shield_carthage.h"

namespace Render::GL {

CarthageShieldRenderer::CarthageShieldRenderer() {
  ShieldRenderConfig config;
  config.shield_color = {0.20F, 0.46F, 0.62F};      // Carthage blue
  config.trim_color = {0.76F, 0.68F, 0.42F};        // Carthage gold trim
  config.metal_color = {0.70F, 0.68F, 0.52F};       // Carthage metal
  config.shield_radius = 0.18F * 0.9F;              // Slightly smaller
  config.shield_aspect = 0.85F;                     // Rectangular aspect
  config.has_cross_decal = false;                   // No cross for Carthage

  setConfig(config);
}

} // namespace Render::GL
