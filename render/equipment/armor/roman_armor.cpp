#include "roman_armor.h"
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

void RomanHeavyArmorRenderer::render(const DrawContext &ctx,
                                      const BodyFrames &frames,
                                      const HumanoidPalette &palette,
                                      const HumanoidAnimationContext &anim,
                                      ISubmitter &submitter) {
  // Roman heavy armor - lorica segmentata style
  TunicConfig config;
  config.torso_scale = 1.08F;
  config.shoulder_width_scale = 1.18F; // More compact shoulders
  config.chest_depth_scale = 0.82F; // Deeper chest for segmented plates
  config.waist_taper = 0.90F;
  config.include_pauldrons = true;
  config.include_gorget = true;
  config.include_belt = true;

  TunicRenderer renderer(config);
  renderer.render(ctx, frames, palette, anim, submitter);
}

void RomanLightArmorRenderer::render(const DrawContext &ctx,
                                      const BodyFrames &frames,
                                      const HumanoidPalette &palette,
                                      const HumanoidAnimationContext &anim,
                                      ISubmitter &submitter) {
  // Roman light armor - lighter version
  TunicConfig config;
  config.torso_scale = 1.04F;
  config.shoulder_width_scale = 1.12F;
  config.chest_depth_scale = 0.86F;
  config.waist_taper = 0.93F;
  config.include_pauldrons = false;
  config.include_gorget = false;
  config.include_belt = true;

  TunicRenderer renderer(config);
  renderer.render(ctx, frames, palette, anim, submitter);
}

} // namespace Render::GL
