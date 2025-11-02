#include "kingdom_armor.h"
#include "tunic_renderer.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_math.h"
#include "../../humanoid/humanoid_specs.h"
#include "../../humanoid/rig.h"
#include "../../humanoid/style_palette.h"
#include "../../submitter.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <cmath>
#include <numbers>

namespace Render::GL {

void KingdomHeavyArmorRenderer::render(const DrawContext &ctx,
                                        const BodyFrames &frames,
                                        const HumanoidPalette &palette,
                                        const HumanoidAnimationContext &anim,
                                        ISubmitter &submitter) {
  // Kingdom heavy armor - full plate with all components
  TunicConfig config;
  config.torso_scale = 1.08F;
  config.shoulder_width_scale = 1.25F; // Broader shoulders for Kingdom style
  config.chest_depth_scale = 0.85F;
  config.waist_taper = 0.92F;
  config.include_pauldrons = true;
  config.include_gorget = true;
  config.include_belt = true;

  TunicRenderer renderer(config);
  renderer.render(ctx, frames, palette, anim, submitter);
}

void KingdomLightArmorRenderer::render(const DrawContext &ctx,
                                        const BodyFrames &frames,
                                        const HumanoidPalette &palette,
                                        const HumanoidAnimationContext &anim,
                                        ISubmitter &submitter) {
  // Kingdom light armor - lighter breastplate, no pauldrons
  TunicConfig config;
  config.torso_scale = 1.04F;
  config.shoulder_width_scale = 1.15F;
  config.chest_depth_scale = 0.88F;
  config.waist_taper = 0.94F;
  config.include_pauldrons = false; // Light armor has no pauldrons
  config.include_gorget = false;
  config.include_belt = true;

  TunicRenderer renderer(config);
  renderer.render(ctx, frames, palette, anim, submitter);
}

} // namespace Render::GL
