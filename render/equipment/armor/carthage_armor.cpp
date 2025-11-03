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
  
  if (torso.radius <= 0.0F) return;

  const float torso_r = torso.radius;
  const QVector3D steel_color = QVector3D(0.50F, 0.52F, 0.55F);
  
  // 1. CHEST PIECE - Wide, covering upper torso
  QVector3D chest_center = torso.origin + torso.up * (torso_r * 0.3F);
  QMatrix4x4 chest_transform = ctx.model;
  chest_transform.translate(chest_center);
  chest_transform.scale(torso_r * 1.35F, torso_r * 0.65F, torso_r * 1.25F);
  submitter.mesh(getUnitSphere(), chest_transform, steel_color, nullptr, 0.4F);
  
  // 2. BELLY PIECE - Tapered, narrower than chest
  QVector3D belly_center = torso.origin - torso.up * (torso_r * 0.1F);
  QMatrix4x4 belly_transform = ctx.model;
  belly_transform.translate(belly_center);
  belly_transform.scale(torso_r * 1.15F, torso_r * 0.55F, torso_r * 1.1F);
  submitter.mesh(getUnitSphere(), belly_transform, steel_color, nullptr, 0.4F);
  
  // 3. MAIL SKIRT - Lower torso to upper thighs
  QVector3D skirt_center = waist.origin - waist.up * (torso_r * 0.4F);
  QMatrix4x4 skirt_transform = ctx.model;
  skirt_transform.translate(skirt_center);
  skirt_transform.scale(torso_r * 1.2F, torso_r * 0.7F, torso_r * 1.15F);
  submitter.mesh(getUnitSphere(), skirt_transform, steel_color, nullptr, 0.4F);
  
  // 4. LEFT SHOULDER GUARD - Covering shoulder joint
  QMatrix4x4 left_shoulder_transform = ctx.model;
  left_shoulder_transform.translate(shoulderL.origin);
  left_shoulder_transform.scale(shoulderL.radius * 1.4F);
  submitter.mesh(getUnitSphere(), left_shoulder_transform, steel_color, nullptr, 0.4F);
  
  // 5. RIGHT SHOULDER GUARD
  QMatrix4x4 right_shoulder_transform = ctx.model;
  right_shoulder_transform.translate(shoulderR.origin);
  right_shoulder_transform.scale(shoulderR.radius * 1.4F);
  submitter.mesh(getUnitSphere(), right_shoulder_transform, steel_color, nullptr, 0.4F);
  
  // 6. LEFT ARM SLEEVE - Upper arm coverage
  QVector3D left_arm_mid = (shoulderL.origin + handL.origin) * 0.5F;
  left_arm_mid += shoulderL.origin * 0.3F; // Bias toward shoulder
  QMatrix4x4 left_arm_transform = ctx.model;
  left_arm_transform.translate(left_arm_mid);
  left_arm_transform.scale(torso_r * 0.5F, torso_r * 0.9F, torso_r * 0.5F);
  submitter.mesh(getUnitSphere(), left_arm_transform, steel_color, nullptr, 0.4F);
  
  // 7. RIGHT ARM SLEEVE
  QVector3D right_arm_mid = (shoulderR.origin + handR.origin) * 0.5F;
  right_arm_mid += shoulderR.origin * 0.3F;
  QMatrix4x4 right_arm_transform = ctx.model;
  right_arm_transform.translate(right_arm_mid);
  right_arm_transform.scale(torso_r * 0.5F, torso_r * 0.9F, torso_r * 0.5F);
  submitter.mesh(getUnitSphere(), right_arm_transform, steel_color, nullptr, 0.4F);
  
  // 8. LEFT THIGH GUARD - Upper leg protection
  QVector3D left_thigh = waist.origin + QVector3D(-torso_r * 0.4F, -torso_r * 1.2F, 0.0F);
  QMatrix4x4 left_thigh_transform = ctx.model;
  left_thigh_transform.translate(left_thigh);
  left_thigh_transform.scale(torso_r * 0.45F, torso_r * 0.8F, torso_r * 0.45F);
  submitter.mesh(getUnitSphere(), left_thigh_transform, steel_color, nullptr, 0.4F);
  
  // 9. RIGHT THIGH GUARD
  QVector3D right_thigh = waist.origin + QVector3D(torso_r * 0.4F, -torso_r * 1.2F, 0.0F);
  QMatrix4x4 right_thigh_transform = ctx.model;
  right_thigh_transform.translate(right_thigh);
  right_thigh_transform.scale(torso_r * 0.45F, torso_r * 0.8F, torso_r * 0.45F);
  submitter.mesh(getUnitSphere(), right_thigh_transform, steel_color, nullptr, 0.4F);
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
  
  if (torso.radius <= 0.0F) return;

  QVector3D linen_color = QVector3D(0.85F, 0.80F, 0.72F);
  float roughness = 0.8F;

  // 1. CHEST PIECE - Upper torso
  {
    QVector3D top = head.origin - head.up * (head.radius * 0.8F);
    QVector3D bottom = torso.origin - torso.up * (torso.radius * 0.4F);
    QVector3D center = (top + bottom) * 0.5F;
    float height = (top - bottom).length();
    
    QMatrix4x4 chest_transform = createArmorTransform(
        ctx, center, torso.up.normalized(), torso.right.normalized(), torso.forward.normalized(),
        torso.radius * 2.4F,  // Shoulder width
        height * 0.55F,
        torso.radius * 1.35F
    );
    
    submitter.mesh(getUnitTorso(), chest_transform, linen_color, nullptr, roughness);
  }

  // 2. BELLY PIECE - Mid torso
  {
    QVector3D top = torso.origin - torso.up * (torso.radius * 0.3F);
    QVector3D bottom = waist.origin + waist.up * (waist.radius * 0.3F);
    QVector3D center = (top + bottom) * 0.5F;
    float height = (top - bottom).length();
    
    QMatrix4x4 belly_transform = createArmorTransform(
        ctx, center, torso.up.normalized(), torso.right.normalized(), torso.forward.normalized(),
        torso.radius * 2.1F,
        height * 0.55F,
        torso.radius * 1.25F
    );
    
    submitter.mesh(getUnitTorso(), belly_transform, linen_color, nullptr, roughness);
  }

  // 3. WAIST PIECE - Lower torso
  {
    QVector3D top = waist.origin;
    QVector3D bottom = waist.origin - waist.up * (waist.radius * 1.0F);
    QVector3D center = (top + bottom) * 0.5F;
    float height = (top - bottom).length();
    
    QMatrix4x4 waist_transform = createArmorTransform(
        ctx, center, waist.up.normalized(), waist.right.normalized(), waist.forward.normalized(),
        waist.radius * 2.2F,
        height * 0.55F,
        waist.radius * 1.2F
    );
    
    submitter.mesh(getUnitTorso(), waist_transform, linen_color, nullptr, roughness);
  }

  // 4. LEFT SHOULDER PIECE (layered linen pteruges)
  if (shoulderL.radius > 0.0F) {
    QMatrix4x4 l_shoulder_transform = createArmorTransform(
        ctx, shoulderL.origin, shoulderL.up.normalized(), 
        shoulderL.right.normalized(), shoulderL.forward.normalized(),
        shoulderL.radius * 1.7F,
        shoulderL.radius * 1.1F,
        shoulderL.radius * 1.5F
    );
    submitter.mesh(getUnitSphere(), l_shoulder_transform, linen_color, nullptr, roughness);
  }

  // 5. RIGHT SHOULDER PIECE (layered linen pteruges)
  if (shoulderR.radius > 0.0F) {
    QMatrix4x4 r_shoulder_transform = createArmorTransform(
        ctx, shoulderR.origin, shoulderR.up.normalized(), 
        shoulderR.right.normalized(), shoulderR.forward.normalized(),
        shoulderR.radius * 1.7F,
        shoulderR.radius * 1.1F,
        shoulderR.radius * 1.5F
    );
    submitter.mesh(getUnitSphere(), r_shoulder_transform, linen_color, nullptr, roughness);
  }

  // 6. PTERYGES (Linen strips hanging from waist) - Left side
  for (int i = 0; i < 3; ++i) {
    float angle = -0.3F + i * 0.15F;
    QVector3D offset = waist.right * std::sin(angle) * waist.radius * 1.2F + 
                       waist.forward * std::cos(angle) * waist.radius * 0.8F;
    QVector3D strip_top = waist.origin + offset - waist.up * (waist.radius * 0.3F);
    QVector3D strip_bottom = strip_top - waist.up * (waist.radius * 1.5F);
    
    submitter.mesh(getUnitCylinder(),
                  cylinderBetween(ctx.model, strip_top, strip_bottom, waist.radius * 0.15F),
                  linen_color * 0.95F, nullptr, roughness);
  }

  // 7. PTERYGES - Right side
  for (int i = 0; i < 3; ++i) {
    float angle = 0.3F - i * 0.15F;
    QVector3D offset = waist.right * std::sin(angle) * waist.radius * 1.2F + 
                       waist.forward * std::cos(angle) * waist.radius * 0.8F;
    QVector3D strip_top = waist.origin + offset - waist.up * (waist.radius * 0.3F);
    QVector3D strip_bottom = strip_top - waist.up * (waist.radius * 1.5F);
    
    submitter.mesh(getUnitCylinder(),
                  cylinderBetween(ctx.model, strip_top, strip_bottom, waist.radius * 0.15F),
                  linen_color * 0.95F, nullptr, roughness);
  }
}

} // namespace Render::GL
