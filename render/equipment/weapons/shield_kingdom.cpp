#include "shield_kingdom.h"

namespace Render::GL {

KingdomShieldRenderer::KingdomShieldRenderer() {
  ShieldRenderConfig config;
  config.shield_color = {0.85F, 0.85F, 0.90F};
  config.trim_color = {0.72F, 0.73F, 0.78F};
  config.metal_color = {0.72F, 0.73F, 0.78F};
  config.shield_radius = 0.18F;
  config.shield_aspect = 1.1F;
  config.has_cross_decal = true;

  setConfig(config);
}

} // namespace Render::GL
