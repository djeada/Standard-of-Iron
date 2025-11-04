#include "carthage_armor.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_math.h"
#include "../../humanoid/humanoid_specs.h"
#include "../../humanoid/rig.h"
#include "../../humanoid/style_palette.h"
#include "../../submitter.h"
#include "tunic_renderer.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <numbers>

namespace Render::GL {

using Render::Geom::cylinderBetween;
using Render::Geom::sphereAt;
using Render::GL::Humanoid::saturate_color;

// Helper to create armor piece aligned with frame
static QMatrix4x4 createArmorTransform(const DrawContext &ctx,
                                       const QVector3D &center,
                                       const QVector3D &up,
                                       const QVector3D &right,
                                       const QVector3D &forward,
                                       float width, float height, float depth) {
  QMatrix4x4 transform = ctx.model;
  transform.translate(center);
  
  QMatrix4x4 orientation;
  orientation(0, 0) = right.x(); orientation(0, 1) = up.x(); orientation(0, 2) = forward.x();
  orientation(1, 0) = right.y(); orientation(1, 1) = up.y(); orientation(1, 2) = forward.y();
  orientation(2, 0) = right.z(); orientation(2, 1) = up.z(); orientation(2, 2) = forward.z();
  
  transform = transform * orientation;
  transform.scale(width, height, depth);
  
  return transform;
}

void CarthageHeavyArmorRenderer::render(const DrawContext &ctx,
                                        const BodyFrames &frames,
                                        const HumanoidPalette &palette,
                                        const HumanoidAnimationContext &anim,
                                        ISubmitter &submitter) {
  (void)anim;
  (void)palette;

  // HEAVY: Full steel chainmail hauberk covering torso, shoulders, arms, thighs
  const AttachmentFrame &head = frames.head;
  const AttachmentFrame &torso = frames.torso;
  const AttachmentFrame &waist = frames.waist;
  const AttachmentFrame &shoulderL = frames.shoulderL;
  const AttachmentFrame &shoulderR = frames.shoulderR;
  const AttachmentFrame &handL = frames.handL;
  const AttachmentFrame &handR = frames.handR;
  const AttachmentFrame &footL = frames.footL;
  const AttachmentFrame &footR = frames.footR;

  if (torso.radius <= 0.0F) return;

  const float torso_r = torso.radius;
  const QVector3D steel_color = QVector3D(0.52F, 0.54F, 0.58F);

  const QVector3D up = torso.up.normalized();
  const QVector3D right = torso.right.normalized();
  const QVector3D forward = torso.forward.normalized();

  // 1. CHEST AND BACK HAUBERK
  {
    float height = torso_r * 0.95F;
    QVector3D center = torso.origin + up * (torso_r * 0.28F);
    QMatrix4x4 chest = createArmorTransform(ctx, center, up, right, forward,
                                            torso_r * 1.55F,
                                            height,
                                            torso_r * 1.25F);
    submitter.mesh(getUnitTorso(), chest, steel_color, nullptr, 0.32F);
  }

  // 2. MIDSECTION
  {
    float height = torso_r * 0.75F;
    QVector3D center = torso.origin - up * (torso_r * 0.2F);
    QMatrix4x4 mid = createArmorTransform(ctx, center, up, right, forward,
                                          torso_r * 1.45F,
                                          height,
                                          torso_r * 1.1F);
    submitter.mesh(getUnitTorso(), mid, steel_color * 0.98F, nullptr, 0.34F);
  }

  // 3. HAUBERK SKIRT (covers hips to upper thighs)
  {
    QVector3D waist_up = waist.up.normalized();
    QVector3D waist_right = waist.right.normalized();
    QVector3D waist_forward = waist.forward.normalized();

    float height = waist.radius * 1.0F;
    QVector3D center = waist.origin - waist_up * (height * 0.45F);
    QMatrix4x4 skirt = createArmorTransform(ctx, center, waist_up, waist_right,
                                            waist_forward,
                                            waist.radius * 1.65F,
                                            height,
                                            waist.radius * 1.25F);
    submitter.mesh(getUnitTorso(), skirt, steel_color * 0.97F, nullptr, 0.36F);
  }

  // 4. MAIL COLLAR AROUND NECK BASE
  {
    QVector3D collar_center = head.origin - head.up.normalized() * (head.radius * 0.5F);
    QMatrix4x4 collar = createArmorTransform(ctx, collar_center, up, right, forward,
                                             torso_r * 1.8F,
                                             torso_r * 0.35F,
                                             torso_r * 1.5F);
    submitter.mesh(getUnitSphere(), collar, steel_color * 1.02F, nullptr, 0.30F);
  }

  // 5. PAULDRONS (shoulder mail pads)
  auto submit_pauldron = [&](const AttachmentFrame &shoulder) {
    if (shoulder.radius <= 0.0F) return;
    QMatrix4x4 mat = createArmorTransform(ctx, shoulder.origin,
                                          shoulder.up.normalized(),
                                          shoulder.right.normalized(),
                                          shoulder.forward.normalized(),
                                          shoulder.radius * 1.1F,
                                          shoulder.radius * 0.75F,
                                          shoulder.radius * 1.1F);
    submitter.mesh(getUnitSphere(), mat, steel_color, nullptr, 0.33F);
  };
  submit_pauldron(shoulderL);
  submit_pauldron(shoulderR);

  // 6. SLEEVES (chainmail tubes down to wrist)
  auto submit_sleeve = [&](const AttachmentFrame &shoulder,
                           const AttachmentFrame &hand) {
    if (shoulder.radius <= 0.0F) return;
    QVector3D start = shoulder.origin - shoulder.up.normalized() * (shoulder.radius * 0.2F);
    QVector3D end = hand.origin + hand.up.normalized() * (hand.radius * 0.25F);
  float radius = std::max(shoulder.radius, torso_r * 0.3F) * 0.4F;
    submitter.mesh(getUnitCylinder(),
                   cylinderBetween(ctx.model, start, end, radius),
                   steel_color * 0.97F, nullptr, 0.35F);
  };
  submit_sleeve(shoulderL, handL);
  submit_sleeve(shoulderR, handR);

  // 7. THIGH PROTECTION (mail down to knees)
  auto submit_thigh = [&](const AttachmentFrame &foot, float side_sign) {
    QVector3D leg_dir = foot.origin - waist.origin;
    QVector3D start = waist.origin + leg_dir * 0.18F + right * (torso_r * 0.45F * side_sign);
    QVector3D end = waist.origin + leg_dir * 0.55F + right * (torso_r * 0.35F * side_sign);
  float radius = torso_r * 0.32F;
    submitter.mesh(getUnitCylinder(),
                   cylinderBetween(ctx.model, start, end, radius),
                   steel_color * 0.94F, nullptr, 0.37F);
  };
  submit_thigh(footL, -1.0F);
  submit_thigh(footR, 1.0F);
}

void CarthageLightArmorRenderer::render(const DrawContext &ctx,
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

  QVector3D linen_color = QVector3D(0.85F, 0.80F, 0.72F);

  const QVector3D up = torso.up.normalized();
  const QVector3D right = torso.right.normalized();
  const QVector3D forward = torso.forward.normalized();

  QVector3D top = torso.origin + up * (torso.radius * 0.45F);
  if (head.radius > 0.0F) {
    top = head.origin - head.up.normalized() * (head.radius * 0.55F);
  }

  QVector3D bottom = torso.origin - up * (torso.radius * 0.35F);
  if (waist.radius > 0.0F) {
    bottom = waist.origin - waist.up.normalized() * (waist.radius * 0.35F);
  }

  QVector3D center = (top + bottom) * 0.5F;
  float height = (top - bottom).length();

  float torso_radius = torso.radius;
  float width = torso_radius * 1.28F;
  float depth = torso_radius * 1.06F;

  QMatrix4x4 transform = createArmorTransform(ctx, center, up, right, forward,
                                              width, height * 0.5F, depth);

  submitter.mesh(getUnitTorso(), transform, linen_color, nullptr, 0.8F);
}

} // namespace Render::GL
