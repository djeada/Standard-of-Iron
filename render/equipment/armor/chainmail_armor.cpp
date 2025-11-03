#include "chainmail_armor.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_math.h"
#include "../../humanoid/rig.h"
#include "../../submitter.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <cmath>
#include <numbers>

namespace Render::GL {

using Render::Geom::cylinderBetween;

auto ChainmailArmorRenderer::calculateRingColor(float x, float y,
                                                 float z) const -> QVector3D {
  // Procedural rust/weathering based on position
  float rust_noise = std::sin(x * 127.3F) * std::cos(y * 97.1F) * 
                     std::sin(z * 83.7F);
  rust_noise = (rust_noise + 1.0F) * 0.5F; // Normalize to [0,1]
  
  // More rust in lower areas (gravity effect)
  float gravity_rust = std::clamp(1.0F - y * 0.8F, 0.0F, 1.0F);
  float total_rust = (rust_noise * 0.6F + gravity_rust * 0.4F) * m_config.rust_amount;
  
  return m_config.metal_color * (1.0F - total_rust) + 
         m_config.rust_tint * total_rust;
}

void ChainmailArmorRenderer::render(const DrawContext &ctx,
                                    const BodyFrames &frames,
                                    const HumanoidPalette &palette,
                                    const HumanoidAnimationContext &anim,
                                    ISubmitter &submitter) {
  (void)palette;
  (void)anim;

  renderTorsoMail(ctx, frames, submitter);
  
  if (m_config.has_shoulder_guards) {
    renderShoulderGuards(ctx, frames, submitter);
  }
  
  if (m_config.has_arm_coverage && m_config.coverage > 0.5F) {
    renderArmMail(ctx, frames, submitter);
  }
}

void ChainmailArmorRenderer::renderTorsoMail(const DrawContext &ctx,
                                             const BodyFrames &frames,
                                             ISubmitter &submitter) {
  const AttachmentFrame &torso = frames.torso;
  const AttachmentFrame &waist = frames.waist;
  
  if (torso.radius <= 0.0F) {
    return;
  }

  float const torso_r = torso.radius;
  float const coverage_height = m_config.coverage;
  
  // Main chainmail hauberk body - layered construction
  int const vertical_segments = m_config.detail_level >= 2 ? 16 : 8;
  
  for (int seg = 0; seg < vertical_segments; ++seg) {
    float t0 = static_cast<float>(seg) / static_cast<float>(vertical_segments);
    float t1 = static_cast<float>(seg + 1) / static_cast<float>(vertical_segments);
    
    // Interpolate between torso and waist
    QVector3D pos0 = torso.origin * (1.0F - t0) + waist.origin * t0;
    QVector3D pos1 = torso.origin * (1.0F - t1) + waist.origin * t1;
    
    float r0 = torso_r * (1.0F + t0 * 0.15F); // Slight flare at waist
    float r1 = torso_r * (1.0F + t1 * 0.15F);
    
    QVector3D ring_color = calculateRingColor(pos0.x(), pos0.y(), pos0.z());
    
    // If high detail, render actual ring structure
    if (m_config.detail_level >= 2) {
      renderRingDetails(ctx, pos0, r0, (pos1 - pos0).length(),
                        torso.up, torso.right, submitter);
    } else {
      // Lower detail: solid segments with chainmail texture hint
      submitter.mesh(getUnitCylinder(),
                     cylinderBetween(ctx.model, pos0, pos1, (r0 + r1) * 0.5F * 1.02F),
                     ring_color, nullptr, 0.75F);
    }
  }
  
  // Bottom edge rings (hanging mail at waist)
  if (coverage_height > 0.7F) {
    int const edge_rings = m_config.detail_level >= 1 ? 16 : 8;
    
    for (int i = 0; i < edge_rings; ++i) {
      float angle = (static_cast<float>(i) / static_cast<float>(edge_rings)) *
                    2.0F * std::numbers::pi_v<float>;
      
      float x = std::cos(angle);
      float z = std::sin(angle);
      
      QVector3D ring_pos = waist.origin + 
                           waist.right * (x * torso_r * 1.15F) +
                           waist.forward * (z * torso_r * 1.15F) +
                           waist.up * (-0.05F);
      
      QMatrix4x4 ring_m = ctx.model;
      ring_m.translate(ring_pos);
      ring_m.scale(m_config.ring_size * 1.5F);
      
      QVector3D edge_color = calculateRingColor(ring_pos.x(), ring_pos.y(), ring_pos.z());
      submitter.mesh(getUnitSphere(), ring_m, edge_color, nullptr, 0.8F);
    }
  }
}

void ChainmailArmorRenderer::renderShoulderGuards(const DrawContext &ctx,
                                                  const BodyFrames &frames,
                                                  ISubmitter &submitter) {
  const AttachmentFrame &shoulder_l = frames.shoulderL;
  const AttachmentFrame &shoulder_r = frames.shoulderR;
  const AttachmentFrame &torso = frames.torso;
  
  // Left shoulder pauldron
  QVector3D left_base = shoulder_l.origin;
  QVector3D left_tip = left_base + torso.up * 0.08F + torso.right * (-0.05F);
  
  float const shoulder_r = 0.08F;
  
  QVector3D left_color = calculateRingColor(left_base.x(), left_base.y(), left_base.z());
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, left_base, left_tip, shoulder_r),
                 left_color, nullptr, 0.8F);
  
  // Right shoulder pauldron
  QVector3D right_base = shoulder_r.origin;
  QVector3D right_tip = right_base + torso.up * 0.08F + torso.right * 0.05F;
  
  QVector3D right_color = calculateRingColor(right_base.x(), right_base.y(), right_base.z());
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, right_base, right_tip, shoulder_r),
                 right_color, nullptr, 0.8F);
  
  // Layered shoulder protection (multiple overlapping ring rows)
  if (m_config.detail_level >= 1) {
    for (int layer = 0; layer < 3; ++layer) {
      float layer_offset = static_cast<float>(layer) * 0.025F;
      
      QVector3D left_layer = left_base + torso.up * (-layer_offset);
      QVector3D right_layer = right_base + torso.up * (-layer_offset);
      
      QMatrix4x4 left_m = ctx.model;
      left_m.translate(left_layer);
      left_m.scale(shoulder_r * 1.3F);
      submitter.mesh(getUnitSphere(), left_m,
                     left_color * (1.0F - layer_offset), nullptr, 0.75F);
      
      QMatrix4x4 right_m = ctx.model;
      right_m.translate(right_layer);
      right_m.scale(shoulder_r * 1.3F);
      submitter.mesh(getUnitSphere(), right_m,
                     right_color * (1.0F - layer_offset), nullptr, 0.75F);
    }
  }
}

void ChainmailArmorRenderer::renderArmMail(const DrawContext &ctx,
                                           const BodyFrames &frames,
                                           ISubmitter &submitter) {
  // Arm coverage (sleeves) extending from shoulders to elbows
  const AttachmentFrame &shoulder_l = frames.shoulderL;
  const AttachmentFrame &shoulder_r = frames.shoulderR;
  const AttachmentFrame &hand_l = frames.handL;
  const AttachmentFrame &hand_r = frames.handR;
  
  // Left arm mail sleeve
  QVector3D left_shoulder = shoulder_l.origin;
  QVector3D left_elbow = (left_shoulder + hand_l.origin) * 0.5F; // Approximate elbow
  
  int const arm_segments = m_config.detail_level >= 2 ? 6 : 3;
  
  for (int i = 0; i < arm_segments; ++i) {
    float t0 = static_cast<float>(i) / static_cast<float>(arm_segments);
    float t1 = static_cast<float>(i + 1) / static_cast<float>(arm_segments);
    
    QVector3D pos0 = left_shoulder * (1.0F - t0) + left_elbow * t0;
    QVector3D pos1 = left_shoulder * (1.0F - t1) + left_elbow * t1;
    
    float radius = 0.05F * (1.0F - t0 * 0.2F); // Taper toward elbow
    
    QVector3D color = calculateRingColor(pos0.x(), pos0.y(), pos0.z());
    submitter.mesh(getUnitCylinder(),
                   cylinderBetween(ctx.model, pos0, pos1, radius),
                   color, nullptr, 0.75F);
  }
  
  // Right arm mail sleeve
  QVector3D right_shoulder = shoulder_r.origin;
  QVector3D right_elbow = (right_shoulder + hand_r.origin) * 0.5F;
  
  for (int i = 0; i < arm_segments; ++i) {
    float t0 = static_cast<float>(i) / static_cast<float>(arm_segments);
    float t1 = static_cast<float>(i + 1) / static_cast<float>(arm_segments);
    
    QVector3D pos0 = right_shoulder * (1.0F - t0) + right_elbow * t0;
    QVector3D pos1 = right_shoulder * (1.0F - t1) + right_elbow * t1;
    
    float radius = 0.05F * (1.0F - t0 * 0.2F);
    
    QVector3D color = calculateRingColor(pos0.x(), pos0.y(), pos0.z());
    submitter.mesh(getUnitCylinder(),
                   cylinderBetween(ctx.model, pos0, pos1, radius),
                   color, nullptr, 0.75F);
  }
}

void ChainmailArmorRenderer::renderRingDetails(
    const DrawContext &ctx, const QVector3D &center, float radius,
    float height, const QVector3D &up, const QVector3D &right,
    ISubmitter &submitter) {
  // Render individual interlocking rings for high detail mode
  int const rings_around = 24;
  int const rings_vertical = 4;
  
  for (int row = 0; row < rings_vertical; ++row) {
    float y = (static_cast<float>(row) / static_cast<float>(rings_vertical)) * height;
    
    // Offset alternating rows for interlocking pattern
    float row_offset = (row % 2) * 0.5F;
    
    for (int col = 0; col < rings_around; ++col) {
      float angle = ((static_cast<float>(col) + row_offset) / 
                     static_cast<float>(rings_around)) * 
                    2.0F * std::numbers::pi_v<float>;
      
      float x = std::cos(angle) * radius;
      float z = std::sin(angle) * radius;
      
      QVector3D ring_pos = center + up * y + right * x + 
                           QVector3D::crossProduct(up, right).normalized() * z;
      
      QMatrix4x4 ring_m = ctx.model;
      ring_m.translate(ring_pos);
      ring_m.scale(m_config.ring_size);
      
      QVector3D color = calculateRingColor(ring_pos.x(), ring_pos.y(), ring_pos.z());
      submitter.mesh(getUnitSphere(), ring_m, color, nullptr, 0.85F);
    }
  }
}

} // namespace Render::GL
