#include "carthage_shoulder_cover.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_specs.h"
#include "../../submitter.h"
#include <QMatrix4x4>
#include <QVector3D>

namespace Render::GL {

using Render::Geom::sphereAt;

void CarthageShoulderCoverRenderer::render(const DrawContext &ctx,
                                           const BodyFrames &frames,
                                           const HumanoidPalette &palette,
                                           const HumanoidAnimationContext &anim,
                                           ISubmitter &submitter) {
  (void)anim;
  (void)palette;

  using HP = HumanProportions;

  const QVector3D &right_axis = frames.torso.right;

  QVector3D leather_color = QVector3D(0.44F, 0.30F, 0.19F);
  QVector3D leather_dark = leather_color * 0.78F;

  auto draw_shoulder_cover = [&](const QVector3D &shoulder_pos,
                                 const QVector3D &outward) {
    float const upper_arm_r = HP::UPPER_ARM_R;

    QVector3D upper_pos =
        shoulder_pos + outward * 0.012F + QVector3D(0.0F, 0.108F, 0.0F);
    QMatrix4x4 upper = ctx.model;
    upper.translate(upper_pos);
    upper.scale(upper_arm_r * 1.75F, upper_arm_r * 0.38F, upper_arm_r * 1.55F);
    submitter.mesh(getUnitSphere(), upper, leather_color * 1.05F, nullptr, 1.0F,
                   1);

    QVector3D lower_pos =
        upper_pos - QVector3D(0.0F, 0.045F, 0.0F) + outward * 0.010F;
    QMatrix4x4 lower = ctx.model;
    lower.translate(lower_pos);
    lower.scale(upper_arm_r * 1.58F, upper_arm_r * 0.34F, upper_arm_r * 1.40F);
    submitter.mesh(getUnitSphere(), lower, leather_color * 0.96F, nullptr, 1.0F,
                   1);

    QVector3D trim_pos =
        lower_pos - QVector3D(0.0F, 0.025F, 0.0F) + outward * 0.006F;
    QMatrix4x4 trim = ctx.model;
    trim.translate(trim_pos);
    trim.scale(upper_arm_r * 1.42F, upper_arm_r * 0.18F, upper_arm_r * 1.25F);
    submitter.mesh(getUnitSphere(), trim, leather_dark, nullptr, 1.0F, 1);
  };

  draw_shoulder_cover(frames.shoulder_l.origin, -right_axis);
  draw_shoulder_cover(frames.shoulder_r.origin, right_axis);
}

} // namespace Render::GL
