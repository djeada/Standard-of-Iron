#include "armor_heavy_carthage.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_math.h"
#include "../../humanoid/humanoid_specs.h"
#include "../../humanoid/rig.h"
#include "../../submitter.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>

namespace Render::GL {

using Render::Geom::cylinder_between;

void ArmorHeavyCarthageRenderer::render(const DrawContext &ctx,
                                        const BodyFrames &frames,
                                        const HumanoidPalette &palette,
                                        const HumanoidAnimationContext &anim,
                                        ISubmitter &submitter) {
  (void)anim;
  (void)palette;

  const AttachmentFrame &torso = frames.torso;
  const AttachmentFrame &waist = frames.waist;
  const AttachmentFrame &head = frames.head;

  if (torso.radius <= 0.0F) {
    return;
  }

  auto safeNormal = [](const QVector3D &v, const QVector3D &fallback) {
    return (v.lengthSquared() > 1e-6F) ? v.normalized() : fallback;
  };

  QVector3D up = safeNormal(torso.up, QVector3D(0.0F, 1.0F, 0.0F));
  QVector3D right = safeNormal(torso.right, QVector3D(1.0F, 0.0F, 0.0F));
  QVector3D forward = safeNormal(torso.forward, QVector3D(0.0F, 0.0F, 1.0F));
  QVector3D waist_up = safeNormal(waist.up, up);
  QVector3D head_up = safeNormal(head.up, up);

  float const torso_r = torso.radius;
  float const torso_depth =
      (torso.depth > 0.0F) ? torso.depth : torso.radius * 0.75F;
  auto depth_scale_for = [&](float base) {
    float const ratio = torso_depth / std::max(0.001F, torso_r);
    return std::max(0.08F, base * ratio);
  };
  float const waist_r =
      waist.radius > 0.0F ? waist.radius : torso.radius * 0.90F;
  float const head_r = head.radius > 0.0F ? head.radius : torso.radius * 0.60F;

  QVector3D top = torso.origin + up * (torso_r * 0.64F);
  QVector3D head_guard = head.origin - head_up * (head_r * 1.35F);
  if (QVector3D::dotProduct(top - head_guard, up) > 0.0F) {
    top = head_guard - up * (torso_r * 0.06F);
  }

  QVector3D bottom = waist.origin - waist_up * (waist_r * 1.60F);
  QVector3D chainmail_bottom = waist.origin - waist_up * (waist_r * 1.52F);

  top += forward * (torso_r * 0.010F);
  bottom += forward * (torso_r * 0.010F);
  chainmail_bottom += forward * (torso_r * 0.010F);

  QVector3D bronze_color = QVector3D(0.72F, 0.53F, 0.28F);
  QVector3D bronze_core = bronze_color * 0.92F;
  QVector3D chainmail_color = QVector3D(0.50F, 0.52F, 0.58F);

  auto draw_torso = [&](const QVector3D &a, const QVector3D &b, float radius,
                        const QVector3D &color, float scale_x, float base_z,
                        int material_id = 1) {
    QMatrix4x4 m = cylinder_between(ctx.model, a, b, radius);
    m.scale(scale_x, 1.0F, depth_scale_for(base_z));
    Mesh *torso_mesh = torso_mesh_without_bottom_cap();
    submitter.mesh(torso_mesh != nullptr ? torso_mesh : get_unit_torso(), m,
                   color, nullptr, 1.0F, material_id);
  };

  draw_torso(top, chainmail_bottom, torso_r * 1.10F, chainmail_color, 1.07F,
             1.04F, 1);

  draw_torso(top + forward * (torso_r * 0.02F),
             bottom + forward * (torso_r * 0.02F), torso_r * 1.16F,
             bronze_color, 1.10F, 1.04F, 1);

  draw_torso(top, bottom, torso_r * 1.10F, bronze_core, 1.05F, 1.00F, 1);
}

} // namespace Render::GL
