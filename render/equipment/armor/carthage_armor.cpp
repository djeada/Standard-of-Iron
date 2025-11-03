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

  // HEAVY: Full steel chainmail hauberk covering torso, shoulders, upper arms, and upper thighs
  const AttachmentFrame &neck = frames.neck;
  const AttachmentFrame &torso = frames.torso;
  const AttachmentFrame &waist = frames.waist;
  const AttachmentFrame &left_shoulder = frames.left_shoulder;
  const AttachmentFrame &right_shoulder = frames.right_shoulder;
  const AttachmentFrame &left_upper_arm = frames.left_upper_arm;
  const AttachmentFrame &right_upper_arm = frames.right_upper_arm;
  const AttachmentFrame &left_hip = frames.left_hip;
  const AttachmentFrame &right_hip = frames.right_hip;
  
  if (torso.radius <= 0.0F) return;

  QVector3D steel_color = QVector3D(0.50F, 0.52F, 0.55F);
  float roughness = 0.4F;

  // 1. CHEST PIECE - Upper torso from neck to mid-torso
  {
    QVector3D top = neck.origin;
    QVector3D bottom = torso.origin - torso.up * (torso.radius * 0.3F);
    QVector3D center = (top + bottom) * 0.5F;
    float height = (top - bottom).length();
    
    QMatrix4x4 chest_transform = createArmorTransform(
        ctx, center, torso.up.normalized(), torso.right.normalized(), torso.forward.normalized(),
        torso.radius * 2.5F,  // Wide shoulders
        height * 0.55F,       // Vertical coverage
        torso.radius * 1.4F   // Front-to-back depth
    );
    
    submitter.mesh(getUnitTorso(), chest_transform, steel_color, nullptr, roughness);
  }

  // 2. WAIST/BELLY PIECE - Mid-torso to waist
  {
    QVector3D top = torso.origin - torso.up * (torso.radius * 0.2F);
    QVector3D bottom = waist.origin + waist.up * (waist.radius * 0.2F);
    QVector3D center = (top + bottom) * 0.5F;
    float height = (top - bottom).length();
    
    QMatrix4x4 belly_transform = createArmorTransform(
        ctx, center, torso.up.normalized(), torso.right.normalized(), torso.forward.normalized(),
        torso.radius * 2.2F,  // Narrower at waist
        height * 0.55F,
        torso.radius * 1.3F
    );
    
    submitter.mesh(getUnitTorso(), belly_transform, steel_color, nullptr, roughness);
  }

  // 3. LOWER TORSO - Waist to hip (mail skirt)
  {
    QVector3D top = waist.origin;
    QVector3D bottom = waist.origin - waist.up * (waist.radius * 1.5F);
    QVector3D center = (top + bottom) * 0.5F;
    float height = (top - bottom).length();
    
    QMatrix4x4 skirt_transform = createArmorTransform(
        ctx, center, waist.up.normalized(), waist.right.normalized(), waist.forward.normalized(),
        waist.radius * 2.4F,  // Flared for movement
        height * 0.55F,
        waist.radius * 1.3F
    );
    
    submitter.mesh(getUnitTorso(), skirt_transform, steel_color, nullptr, roughness);
  }

  // 4. LEFT SHOULDER GUARD
  if (left_shoulder.radius > 0.0F) {
    QMatrix4x4 l_shoulder_transform = createArmorTransform(
        ctx, left_shoulder.origin, left_shoulder.up.normalized(), 
        left_shoulder.right.normalized(), left_shoulder.forward.normalized(),
        left_shoulder.radius * 1.8F,
        left_shoulder.radius * 1.2F,
        left_shoulder.radius * 1.6F
    );
    submitter.mesh(getUnitSphere(), l_shoulder_transform, steel_color, nullptr, roughness);
  }

  // 5. RIGHT SHOULDER GUARD
  if (right_shoulder.radius > 0.0F) {
    QMatrix4x4 r_shoulder_transform = createArmorTransform(
        ctx, right_shoulder.origin, right_shoulder.up.normalized(), 
        right_shoulder.right.normalized(), right_shoulder.forward.normalized(),
        right_shoulder.radius * 1.8F,
        right_shoulder.radius * 1.2F,
        right_shoulder.radius * 1.6F
    );
    submitter.mesh(getUnitSphere(), r_shoulder_transform, steel_color, nullptr, roughness);
  }

  // 6. LEFT UPPER ARM SLEEVE
  if (left_upper_arm.radius > 0.0F) {
    QVector3D arm_top = left_shoulder.origin - left_shoulder.up * (left_shoulder.radius * 0.5F);
    QVector3D arm_bottom = left_upper_arm.origin - left_upper_arm.up * (left_upper_arm.radius * 1.5F);
    float arm_radius = left_upper_arm.radius * 1.15F;
    
    submitter.mesh(getUnitCylinder(), 
                  cylinderBetween(ctx.model, arm_top, arm_bottom, arm_radius),
                  steel_color, nullptr, roughness);
  }

  // 7. RIGHT UPPER ARM SLEEVE
  if (right_upper_arm.radius > 0.0F) {
    QVector3D arm_top = right_shoulder.origin - right_shoulder.up * (right_shoulder.radius * 0.5F);
    QVector3D arm_bottom = right_upper_arm.origin - right_upper_arm.up * (right_upper_arm.radius * 1.5F);
    float arm_radius = right_upper_arm.radius * 1.15F;
    
    submitter.mesh(getUnitCylinder(), 
                  cylinderBetween(ctx.model, arm_top, arm_bottom, arm_radius),
                  steel_color, nullptr, roughness);
  }

  // 8. LEFT THIGH GUARD (mail extension)
  if (left_hip.radius > 0.0F) {
    QVector3D thigh_top = left_hip.origin;
    QVector3D thigh_bottom = left_hip.origin - left_hip.up * (left_hip.radius * 1.2F);
    float thigh_radius = left_hip.radius * 1.12F;
    
    submitter.mesh(getUnitCylinder(), 
                  cylinderBetween(ctx.model, thigh_top, thigh_bottom, thigh_radius),
                  steel_color, nullptr, roughness);
  }

  // 9. RIGHT THIGH GUARD (mail extension)
  if (right_hip.radius > 0.0F) {
    QVector3D thigh_top = right_hip.origin;
    QVector3D thigh_bottom = right_hip.origin - right_hip.up * (right_hip.radius * 1.2F);
    float thigh_radius = right_hip.radius * 1.12F;
    
    submitter.mesh(getUnitCylinder(), 
                  cylinderBetween(ctx.model, thigh_top, thigh_bottom, thigh_radius),
                  steel_color, nullptr, roughness);
  }
}

void CarthageLightArmorRenderer::render(const DrawContext &ctx,
                                        const BodyFrames &frames,
                                        const HumanoidPalette &palette,
                                        const HumanoidAnimationContext &anim,
                                        ISubmitter &submitter) {
  (void)anim;
  (void)palette;

  // LIGHT: Linen linothorax - torso and shoulder protection only (lighter, more flexible)
  const AttachmentFrame &neck = frames.neck;
  const AttachmentFrame &torso = frames.torso;
  const AttachmentFrame &waist = frames.waist;
  const AttachmentFrame &left_shoulder = frames.left_shoulder;
  const AttachmentFrame &right_shoulder = frames.right_shoulder;
  
  if (torso.radius <= 0.0F) return;

  QVector3D linen_color = QVector3D(0.85F, 0.80F, 0.72F);
  float roughness = 0.8F;

  // 1. CHEST PIECE - Upper torso
  {
    QVector3D top = neck.origin;
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
  if (left_shoulder.radius > 0.0F) {
    QMatrix4x4 l_shoulder_transform = createArmorTransform(
        ctx, left_shoulder.origin, left_shoulder.up.normalized(), 
        left_shoulder.right.normalized(), left_shoulder.forward.normalized(),
        left_shoulder.radius * 1.7F,
        left_shoulder.radius * 1.1F,
        left_shoulder.radius * 1.5F
    );
    submitter.mesh(getUnitSphere(), l_shoulder_transform, linen_color, nullptr, roughness);
  }

  // 5. RIGHT SHOULDER PIECE (layered linen pteruges)
  if (right_shoulder.radius > 0.0F) {
    QMatrix4x4 r_shoulder_transform = createArmorTransform(
        ctx, right_shoulder.origin, right_shoulder.up.normalized(), 
        right_shoulder.right.normalized(), right_shoulder.forward.normalized(),
        right_shoulder.radius * 1.7F,
        right_shoulder.radius * 1.1F,
        right_shoulder.radius * 1.5F
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
