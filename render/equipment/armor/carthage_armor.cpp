#include "carthage_armor.h"
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

void CarthageHeavyArmorRenderer::render(const DrawContext &ctx,
                                         const BodyFrames &frames,
                                         const HumanoidPalette &palette,
                                         const HumanoidAnimationContext &anim,
                                         ISubmitter &submitter) {
  // Carthage heavy armor - mixed style with distinctive features
  TunicConfig config;
  config.torso_scale = 1.07F;
  config.shoulder_width_scale = 1.22F; // Unique Carthaginian style
  config.chest_depth_scale = 0.84F;
  config.waist_taper = 0.91F;
  config.include_pauldrons = true;
  config.include_gorget = true;
  config.include_belt = true;

  TunicRenderer renderer(config);
  renderer.render(ctx, frames, palette, anim, submitter);
}

void CarthageLightArmorRenderer::render(const DrawContext &ctx,
                                         const BodyFrames &frames,
                                         const HumanoidPalette &palette,
                                         const HumanoidAnimationContext &anim,
                                         ISubmitter &submitter) {
  // Carthage light armor - minimal, mostly leather-based
  TunicConfig config;
  config.torso_scale = 1.03F; // Lightest of all nations
  config.shoulder_width_scale = 1.14F;
  config.chest_depth_scale = 0.89F;
  config.waist_taper = 0.95F;
  config.include_pauldrons = false;
  config.include_gorget = false;
  config.include_belt = true;

  TunicRenderer renderer(config);
  renderer.render(ctx, frames, palette, anim, submitter);
}

} // namespace Render::GL
