#include "roman_shoulder_cover.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_specs.h"
#include "../../submitter.h"
#include <QMatrix4x4>
#include <QVector3D>

namespace Render::GL {

using Render::Geom::sphereAt;

void RomanShoulderCoverRenderer::render(const DrawContext &ctx,
                                        const BodyFrames &frames,
                                        const HumanoidPalette &palette,
                                        const HumanoidAnimationContext &anim,
                                        ISubmitter &submitter) {
  (void)anim;
  (void)palette;

  using HP = HumanProportions;

  const QVector3D &right_axis = frames.torso.right;

  QVector3D metal_base = QVector3D(0.76F, 0.77F, 0.80F);
  QVector3D metal_dark = metal_base * 0.82F;
  QVector3D edge_highlight = QVector3D(0.90F, 0.90F, 0.94F);

  auto draw_shoulder_cover = [&](const QVector3D &shoulder_pos,
                                 const QVector3D &outward) {
    float const upper_arm_r = HP::UPPER_ARM_R;

    QVector3D top_pos =
        shoulder_pos + outward * 0.032F + QVector3D(0.0F, 0.110F, 0.0F);
    QMatrix4x4 top = ctx.model;
    top.translate(top_pos);
    top.scale(upper_arm_r * 1.70F, upper_arm_r * 0.42F, upper_arm_r * 1.50F);
    submitter.mesh(getUnitSphere(), top, metal_base, nullptr, 1.0F, 1);

    QVector3D under_pos =
        top_pos - QVector3D(0.0F, 0.040F, 0.0F) + outward * 0.020F;
    QMatrix4x4 under = ctx.model;
    under.translate(under_pos);
    under.scale(upper_arm_r * 1.55F, upper_arm_r * 0.30F, upper_arm_r * 1.32F);
    submitter.mesh(getUnitSphere(), under, metal_dark, nullptr, 1.0F, 1);

    QVector3D rim_pos =
        under_pos - QVector3D(0.0F, 0.025F, 0.0F) + outward * 0.014F;
    QMatrix4x4 rim = ctx.model;
    rim.translate(rim_pos);
    rim.scale(upper_arm_r * 1.40F, upper_arm_r * 0.14F, upper_arm_r * 1.18F);
    submitter.mesh(getUnitSphere(), rim, edge_highlight, nullptr, 1.0F, 1);
  };

  draw_shoulder_cover(frames.shoulder_l.origin, -right_axis);
  draw_shoulder_cover(frames.shoulder_r.origin, right_axis);
}

} // namespace Render::GL
