#include "roman_shoulder_cover.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_specs.h"
#include "../../submitter.h"
#include <QMatrix4x4>
#include <QVector3D>

namespace Render::GL {

using Render::Geom::sphere_at;

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

    QVector3D outward_n = outward.normalized();
    QVector3D up_n = frames.torso.up.normalized();
    QVector3D forward_n = QVector3D::crossProduct(outward_n, up_n).normalized();
    if (forward_n.lengthSquared() < 1e-5F) {
      forward_n = QVector3D(0.0F, 0.0F, 1.0F);
    }

    auto oriented_transform = [&](const QVector3D &center,
                                  const QVector3D &scale) -> QMatrix4x4 {
      QMatrix4x4 local;
      local.setColumn(0, QVector4D(outward_n * scale.x(), 0.0F));
      local.setColumn(1, QVector4D(up_n * scale.y(), 0.0F));
      local.setColumn(2, QVector4D(forward_n * scale.z(), 0.0F));
      local.setColumn(3, QVector4D(center, 1.0F));
      return ctx.model * local;
    };

    bool const is_infantry = m_outward_scale <= 1.1F;
    float const outward_offset =
        (is_infantry ? 0.003F : 0.009F) * m_outward_scale;
    float const upward_offset = is_infantry ? 0.052F : 0.054F;
    float const back_offset =
        (is_infantry ? -0.018F : -0.012F) * m_outward_scale;

    QVector3D anchor = shoulder_pos + outward_n * outward_offset +
                       up_n * upward_offset + forward_n * back_offset;

    QMatrix4x4 dome =
        oriented_transform(anchor, {upper_arm_r * 1.38F, upper_arm_r * 1.10F,
                                    upper_arm_r * 1.22F});
    submitter.mesh(get_unit_sphere(), dome, metal_base, nullptr, 1.0F, 1);

    QVector3D inner_center =
        anchor + up_n * (-0.030F) + outward_n * (0.006F * m_outward_scale);
    QMatrix4x4 inner = oriented_transform(
        inner_center,
        {upper_arm_r * 1.22F, upper_arm_r * 0.94F, upper_arm_r * 1.05F});
    submitter.mesh(get_unit_sphere(), inner, metal_dark, nullptr, 1.0F, 1);

    QVector3D rim_center = inner_center + up_n * (-0.028F) +
                           outward_n * (0.006F * m_outward_scale);
    QMatrix4x4 rim = oriented_transform(
        rim_center,
        {upper_arm_r * 1.10F, upper_arm_r * 0.40F, upper_arm_r * 0.98F});
    submitter.mesh(get_unit_sphere(), rim, edge_highlight, nullptr, 1.0F, 1);
  };

  draw_shoulder_cover(frames.shoulder_l.origin, -right_axis);
  draw_shoulder_cover(frames.shoulder_r.origin, right_axis);
}

} // namespace Render::GL
