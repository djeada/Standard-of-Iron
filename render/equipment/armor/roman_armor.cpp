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
  if (torso.radius <= 0.0F) {
    return;
  }

  using HP = HumanProportions;

  // Lorica Segmentata - polished steel with cool blue-grey tint
  QVector3D const steel_color =
      saturate_color(palette.metal * QVector3D(0.88F, 0.92F, 1.08F));

  // Main torso armor - single mesh, all detail in shaders
  QMatrix4x4 torso_transform = ctx.model;
  torso_transform.translate(torso.origin);
  torso_transform.rotate(
      QQuaternion::fromDirection(torso.forward, torso.up).conjugated());
  torso_transform.scale(torso.radius * 1.12F, torso.radius * 1.08F,
                        torso.radius * 1.08F);

  submitter.mesh(getUnitTorso(), torso_transform, steel_color, nullptr, 1.0F);

  // Shoulder guards (pteruges) - minimal geometry, 2 per side
  auto renderShoulderGuard = [&](const QVector3D &shoulder_pos,
                                 const QVector3D &outward) {
    QVector3D const shoulder_color = steel_color * 0.96F;
    
    // Upper shoulder plate
    QVector3D upper_pos = shoulder_pos + outward * 0.04F;
    submitter.mesh(getUnitSphere(),
                   sphereAt(ctx.model, upper_pos, HP::UPPER_ARM_R * 1.85F),
                   shoulder_color, nullptr, 1.0F);
    
    // Lower shoulder plate
    QVector3D lower_pos = upper_pos - torso.up * 0.05F + outward * 0.02F;
    submitter.mesh(getUnitSphere(),
                   sphereAt(ctx.model, lower_pos, HP::UPPER_ARM_R * 1.65F),
                   shoulder_color * 0.94F, nullptr, 1.0F);
  };

  renderShoulderGuard(frames.shoulder_l.origin, -torso.right);
  renderShoulderGuard(frames.shoulder_r.origin, torso.right);

  // Belt - single cylinder with leather color
  const AttachmentFrame &waist = frames.waist;
  auto safeDir = [](const QVector3D &axis, const QVector3D &fallback) {
    if (axis.lengthSquared() > 1e-6F) {
      return axis.normalized();
    }
    QVector3D fb = fallback;
    if (fb.lengthSquared() < 1e-6F) {
      fb = QVector3D(0.0F, 1.0F, 0.0F);
    }
    return fb.normalized();
  };

  QVector3D const leather_color =
      saturate_color(palette.leather * QVector3D(0.6F, 0.4F, 0.3F));
  QVector3D const waist_center =
      (waist.radius > 0.0F) ? waist.origin
                            : QVector3D(torso.origin.x(), HP::WAIST_Y,
                                        torso.origin.z());
  QVector3D const waist_up = safeDir(waist.up, torso.up);
  float const belt_height =
      (waist.radius > 0.0F ? waist.radius : torso.radius) * 0.22F;
  QVector3D const belt_top = waist_center + waist_up * (0.5F * belt_height);
  QVector3D const belt_bot = waist_center - waist_up * (0.5F * belt_height);
  float const belt_radius =
      (waist.radius > 0.0F ? waist.radius : torso.radius * 0.95F) * 1.08F;

  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, belt_bot, belt_top, belt_radius),
                 leather_color, nullptr, 1.0F);
}

void RomanLightArmorRenderer::render(const DrawContext &ctx,
                                     const BodyFrames &frames,
                                     const HumanoidPalette &palette,
                                     const HumanoidAnimationContext &anim,
                                     ISubmitter &submitter) {
  (void)anim;

  const AttachmentFrame &torso = frames.torso;
  if (torso.radius <= 0.0F) {
    return;
  }

  using HP = HumanProportions;

  // Chainmail (lorica hamata) - steel with slight silver tint
  QVector3D const steel_color =
      saturate_color(palette.metal * QVector3D(0.90F, 0.95F, 1.05F));

  // Main torso armor - single mesh, chainmail detail in shaders
  QMatrix4x4 torso_transform = ctx.model;
  torso_transform.translate(torso.origin);
  torso_transform.rotate(
      QQuaternion::fromDirection(torso.forward, torso.up).conjugated());
  torso_transform.scale(torso.radius * 1.06F, torso.radius * 1.04F,
                        torso.radius * 1.04F);

  submitter.mesh(getUnitTorso(), torso_transform, steel_color, nullptr, 1.0F);

  // Pectorale (chest plate) - optional reinforcement over chainmail
  QVector3D const chest_center =
      torso.origin + torso.forward * (torso.radius * 0.92F) +
      torso.up * (HP::CHEST_Y + 0.08F - torso.origin.y());
  QMatrix4x4 pectorale_transform = ctx.model;
  pectorale_transform.translate(chest_center);
  pectorale_transform.rotate(
      QQuaternion::fromDirection(torso.forward, torso.up).conjugated());
  pectorale_transform.scale(torso.radius * 0.45F, torso.radius * 0.35F,
                            torso.radius * 0.12F);

  QVector3D const plate_color = steel_color * 1.10F; // Brighter for contrast
  submitter.mesh(getUnitSphere(), pectorale_transform, plate_color, nullptr,
                 1.0F);

  // Belt - leather cingulum
  const AttachmentFrame &waist = frames.waist;
  auto safeDir = [](const QVector3D &axis, const QVector3D &fallback) {
    if (axis.lengthSquared() > 1e-6F) {
      return axis.normalized();
    }
    QVector3D fb = fallback;
    if (fb.lengthSquared() < 1e-6F) {
      fb = QVector3D(0.0F, 1.0F, 0.0F);
    }
    return fb.normalized();
  };

  QVector3D const leather_color =
      saturate_color(palette.leather * QVector3D(0.55F, 0.38F, 0.28F));
  QVector3D const waist_center =
      (waist.radius > 0.0F)
          ? waist.origin
          : QVector3D(torso.origin.x(), HP::WAIST_Y + 0.02F,
                      torso.origin.z());
  QVector3D const waist_up = safeDir(waist.up, torso.up);
  float const belt_height =
      (waist.radius > 0.0F ? waist.radius : torso.radius) * 0.16F;
  QVector3D const belt_top = waist_center + waist_up * (0.5F * belt_height);
  QVector3D const belt_bot = waist_center - waist_up * (0.5F * belt_height);
  float const belt_r =
      (waist.radius > 0.0F ? waist.radius : torso.radius) * 1.00F;

  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, belt_top, belt_bot, belt_r),
                 leather_color, nullptr, 1.0F);
}

} // namespace Render::GL
