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

  // LIGHT: Linen linothorax - torso and shoulder protection
  const AttachmentFrame &head = frames.head;
  const AttachmentFrame &torso = frames.torso;
  const AttachmentFrame &waist = frames.waist;
  const AttachmentFrame &shoulderL = frames.shoulderL;
  const AttachmentFrame &shoulderR = frames.shoulderR;
  const AttachmentFrame &footL = frames.footL;
  const AttachmentFrame &footR = frames.footR;
  
  if (torso.radius <= 0.0F) return;

  QVector3D linen_color = QVector3D(0.85F, 0.80F, 0.72F);
  float roughness = 0.8F;
  const QVector3D torso_up = torso.up.normalized();
  const QVector3D torso_right = torso.right.normalized();
  const QVector3D torso_forward = torso.forward.normalized();
  const QVector3D waist_up = waist.up.normalized();
  const QVector3D waist_right = waist.right.normalized();
  const QVector3D waist_forward = waist.forward.normalized();

  // 1. CHEST PIECE - Upper torso
  {
    QVector3D center = torso.origin + torso_up * (torso.radius * 0.22F);
    QMatrix4x4 chest = createArmorTransform(ctx, center, torso_up, torso_right, torso_forward,
                                            torso.radius * 1.5F,
                                            torso.radius * 0.95F,
                                            torso.radius * 1.1F);
    submitter.mesh(getUnitTorso(), chest, linen_color, nullptr, roughness);
  }

  // 2. BELLY PIECE - Mid torso
  {
    QVector3D center = torso.origin - torso_up * (torso.radius * 0.22F);
    QMatrix4x4 belly = createArmorTransform(ctx, center, torso_up, torso_right, torso_forward,
                                            torso.radius * 1.4F,
                                            torso.radius * 0.8F,
                                            torso.radius * 1.05F);
    submitter.mesh(getUnitTorso(), belly, linen_color * 0.97F, nullptr, roughness);
  }

  // 3. WAIST PIECE - Lower torso
  {
    float height = waist.radius * 0.75F;
    QVector3D center = waist.origin - waist_up * (height * 0.5F);
    QMatrix4x4 waist_mat = createArmorTransform(ctx, center, waist_up, waist_right, waist_forward,
                                                waist.radius * 1.55F,
                                                height,
                                                waist.radius * 1.05F);
    submitter.mesh(getUnitTorso(), waist_mat, linen_color * 0.95F, nullptr, roughness);
  }

  // 4. LEFT SHOULDER PIECE (layered linen pteruges)
  if (shoulderL.radius > 0.0F) {
  QMatrix4x4 mat = createArmorTransform(
    ctx, shoulderL.origin,
    shoulderL.up.normalized(),
    shoulderL.right.normalized(),
    shoulderL.forward.normalized(),
  shoulderL.radius * 1.05F,
  shoulderL.radius * 0.7F,
  shoulderL.radius * 1.0F);
    submitter.mesh(getUnitSphere(), mat, linen_color, nullptr, roughness);
  }

  // 5. RIGHT SHOULDER PIECE (layered linen pteruges)
  if (shoulderR.radius > 0.0F) {
  QMatrix4x4 mat = createArmorTransform(
    ctx, shoulderR.origin,
    shoulderR.up.normalized(),
    shoulderR.right.normalized(),
    shoulderR.forward.normalized(),
  shoulderR.radius * 1.05F,
  shoulderR.radius * 0.7F,
  shoulderR.radius * 1.0F);
    submitter.mesh(getUnitSphere(), mat, linen_color, nullptr, roughness);
  }

  // 6. LINEN PTERYGES (skirt strips)
  auto submit_pteruges = [&](float side_sign) {
    for (int i = 0; i < 3; ++i) {
      float span = (-0.35F + 0.35F * i);
      QVector3D offset = waist_right * (waist.radius * 0.85F * side_sign)
                       + waist_forward * (waist.radius * 0.45F * span);
      QVector3D top = waist.origin + offset - waist_up * (waist.radius * 0.1F);
      QVector3D bottom = top - waist_up * (waist.radius * 0.95F)
                         + waist_forward * (waist.radius * 0.08F * span);
      float strip_radius = waist.radius * 0.08F;
      submitter.mesh(getUnitCylinder(),
                     cylinderBetween(ctx.model, top, bottom, strip_radius),
                     linen_color * 0.92F, nullptr, roughness);
    }
  };
  submit_pteruges(-1.0F);
  submit_pteruges(1.0F);

  // 7. FRONT/BACK LINEN TABS TO COVER UPPER LEGS
  auto submit_front_tab = [&](const AttachmentFrame &foot, float forward_sign) {
    QVector3D leg_dir = (foot.origin - waist.origin);
    QVector3D center = waist.origin + waist_forward * (waist.radius * 0.85F * forward_sign)
                       + leg_dir * 0.28F;
    QMatrix4x4 tab = createArmorTransform(ctx, center, waist_up, waist_right, waist_forward,
                                          waist.radius * 0.95F,
                                          waist.radius * 0.95F,
                                          waist.radius * 0.4F);
    submitter.mesh(getUnitTorso(), tab, linen_color * 0.9F, nullptr, roughness);
  };
  submit_front_tab(footL, 1.0F);
  submit_front_tab(footR, -1.0F);
}

} // namespace Render::GL
