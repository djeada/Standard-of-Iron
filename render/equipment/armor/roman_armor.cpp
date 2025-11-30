#include "roman_armor.h"
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

using Render::Geom::coneFromTo;
using Render::Geom::cylinderBetween;
using Render::Geom::sphereAt;
using Render::GL::Humanoid::saturate_color;

void RomanHeavyArmorRenderer::render(const DrawContext &ctx,
                                     const BodyFrames &frames,
                                     const HumanoidPalette &palette,
                                     const HumanoidAnimationContext &anim,
                                     ISubmitter &submitter) {
  (void)anim;

  const AttachmentFrame &torso = frames.torso;
  const AttachmentFrame &waist = frames.waist;
  const AttachmentFrame &head = frames.head;

  if (torso.radius <= 0.0F) {
    return;
  }

  using HP = HumanProportions;

  QVector3D const steel_color =
      saturate_color(palette.metal * QVector3D(0.88F, 0.92F, 1.08F));
  QVector3D const brass_color =
      saturate_color(palette.metal * QVector3D(1.25F, 1.05F, 0.62F));
  QVector3D const leather_color =
      saturate_color(palette.leather * QVector3D(0.58F, 0.38F, 0.26F));

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
      (torso.depth > 0.0F) ? torso.depth : torso_r * 0.75F;
  auto depth_scale_for = [&](float base) {
    float const ratio = torso_depth / std::max(0.001F, torso_r);
    return std::max(0.08F, base * ratio);
  };
  float const waist_r =
      waist.radius > 0.0F ? waist.radius : torso.radius * 0.88F;
  float const head_r = head.radius > 0.0F ? head.radius : torso.radius * 0.58F;

  QVector3D top = torso.origin + up * (torso_r * 0.48F);
  QVector3D head_guard = head.origin - head_up * (head_r * 1.30F);
  if (QVector3D::dotProduct(top - head_guard, up) > 0.0F) {
    top = head_guard - up * (torso_r * 0.05F);
  }

  QVector3D bottom =
      waist.origin + waist_up * (waist_r * 0.08F) - forward * (torso_r * 0.01F);

  QMatrix4x4 plates = cylinderBetween(ctx.model, top, bottom, torso_r * 1.02F);
  plates.scale(1.05F, 1.0F, depth_scale_for(0.86F));
  Mesh *torso_mesh = torso_mesh_without_bottom_cap();
  submitter.mesh(torso_mesh != nullptr ? torso_mesh : getUnitTorso(), plates,
                 steel_color, nullptr, 1.0F, 1);

  auto renderShoulderGuard = [&](const QVector3D &shoulder_pos,
                                 const QVector3D &outward) {
    QVector3D upper_pos = shoulder_pos + outward * 0.03F + forward * 0.01F;
    QMatrix4x4 upper = ctx.model;
    upper.translate(upper_pos);
    upper.scale(HP::UPPER_ARM_R * 1.90F, HP::UPPER_ARM_R * 0.42F,
                HP::UPPER_ARM_R * 1.65F);
    submitter.mesh(getUnitSphere(), upper, steel_color * 0.98F, nullptr, 1.0F,
                   1);

    QVector3D lower_pos = upper_pos - up * 0.06F + outward * 0.02F;
    QMatrix4x4 lower = ctx.model;
    lower.translate(lower_pos);
    lower.scale(HP::UPPER_ARM_R * 1.68F, HP::UPPER_ARM_R * 0.38F,
                HP::UPPER_ARM_R * 1.48F);
    submitter.mesh(getUnitSphere(), lower, steel_color * 0.94F, nullptr, 1.0F,
                   1);

    QMatrix4x4 rivet = ctx.model;
    rivet.translate(upper_pos + forward * 0.04F);
    rivet.scale(0.012F);
    submitter.mesh(getUnitSphere(), rivet, brass_color, nullptr, 1.0F);
  };

  renderShoulderGuard(frames.shoulder_l.origin, -right);
  renderShoulderGuard(frames.shoulder_r.origin, right);
}

void RomanLightArmorRenderer::render(const DrawContext &ctx,
                                     const BodyFrames &frames,
                                     const HumanoidPalette &palette,
                                     const HumanoidAnimationContext &anim,
                                     ISubmitter &submitter) {
  (void)anim;

  const AttachmentFrame &torso = frames.torso;
  const AttachmentFrame &waist = frames.waist;
  const AttachmentFrame &head = frames.head;

  if (torso.radius <= 0.0F) {
    return;
  }

  using HP = HumanProportions;

  QVector3D const chainmail_color =
      saturate_color(palette.metal * QVector3D(0.68F, 0.72F, 0.78F));
  QVector3D const leather_color =
      saturate_color(palette.leather * QVector3D(0.52F, 0.36F, 0.24F));
  QVector3D const steel_color =
      saturate_color(palette.metal * QVector3D(0.82F, 0.86F, 0.94F));

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
      (torso.depth > 0.0F) ? torso.depth : torso_r * 0.75F;
  auto depth_scale_for = [&](float base) {
    float const ratio = torso_depth / std::max(0.001F, torso_r);
    return std::max(0.08F, base * ratio);
  };
  float const waist_r =
      waist.radius > 0.0F ? waist.radius : torso.radius * 0.86F;
  float const head_r = head.radius > 0.0F ? head.radius : torso.radius * 0.58F;

  QVector3D top = torso.origin + up * (torso_r * 0.42F);
  QVector3D head_guard = head.origin - head_up * (head_r * 1.35F);
  if (QVector3D::dotProduct(top - head_guard, up) > 0.0F) {
    top = head_guard - up * (torso_r * 0.06F);
  }

  QVector3D bottom = waist.origin - waist_up * (waist_r * 0.15F);

  QMatrix4x4 chainmail =
      cylinderBetween(ctx.model, top, bottom, torso_r * 0.98F);
  chainmail.scale(1.02F, 1.0F, depth_scale_for(0.82F));
  Mesh *torso_mesh = torso_mesh_without_bottom_cap();
  submitter.mesh(torso_mesh != nullptr ? torso_mesh : getUnitTorso(), chainmail,
                 chainmail_color, nullptr, 1.0F, 1);

  QVector3D chest_center =
      torso.origin + up * (torso_r * 0.12F) + forward * (torso_depth * 0.48F);
  float const plate_width = torso_r * 0.85F;
  float const plate_height = torso_r * 0.65F;
  float const plate_depth = torso_r * 0.18F;

  QMatrix4x4 pectorale = ctx.model;
  pectorale.translate(chest_center);
  QQuaternion chest_rot = QQuaternion::fromDirection(forward, up);
  pectorale.rotate(chest_rot.conjugated());
  pectorale.scale(plate_width, plate_height, plate_depth);
  submitter.mesh(getUnitSphere(), pectorale, steel_color, nullptr, 1.0F, 1);
}

} // namespace Render::GL
