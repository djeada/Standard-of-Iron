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
  
  // Chainmail hauberk that follows torso contours
  QVector3D steel_color = m_config.metal_color;
  
  const QVector3D &origin = torso.origin;
  const QVector3D &right = torso.right;
  const QVector3D &up = torso.up;
  const QVector3D &forward = torso.forward;

  float const shoulder_width = torso_r * 1.15F;
  float const chest_depth_front = torso_r * 1.18F;
  float const chest_depth_back = torso_r * 0.85F;
  float const waist_width = torso_r * 1.05F;

  // Create mail segments from shoulders to hips
  int const num_rows = m_config.detail_level >= 2 ? 16 : 10;
  int const segments_per_row = m_config.detail_level >= 2 ? 24 : 16;
  
  float const y_top = 0.15F; // Shoulder level
  float const y_bottom = -0.25F; // Below waist
  float const row_height = (y_top - y_bottom) / static_cast<float>(num_rows);

  constexpr float pi = std::numbers::pi_v<float>;
  float const ring_thickness = torso_r * 0.015F; // Much thicker rings to be visible

  for (int row = 0; row < num_rows; ++row) {
    float const y_row_top = y_top - static_cast<float>(row) * row_height;
    float const y_row_bottom = y_row_top - row_height;
    
    // Interpolate between chest and waist dimensions
    float const t = static_cast<float>(row) / static_cast<float>(num_rows - 1);
    float const width_scale = shoulder_width * (1.0F - t * 0.12F) + waist_width * t * 0.12F;

    auto getRadius = [&](float angle) -> float {
      float const cos_a = std::cos(angle);
      float depth_front = chest_depth_front * (1.0F - t * 0.15F);
      float depth_back = chest_depth_back * (1.0F - t * 0.08F);
      float depth = (cos_a > 0.0F) ? depth_front : depth_back;
      return width_scale * depth * (std::abs(cos_a) * 0.3F + 0.7F);
    };

    for (int i = 0; i < segments_per_row; ++i) {
      float const angle1 = (static_cast<float>(i) / segments_per_row) * 2.0F * pi;
      float const angle2 = (static_cast<float>(i + 1) / segments_per_row) * 2.0F * pi;

      float const cos1 = std::cos(angle1);
      float const sin1 = std::sin(angle1);
      float const cos2 = std::cos(angle2);
      float const sin2 = std::sin(angle2);

      float const r1 = getRadius(angle1);
      float const r2 = getRadius(angle2);

      QVector3D const p1_top = origin + right * (r1 * sin1) +
                               forward * (r1 * cos1) + up * y_row_top;
      QVector3D const p2_top = origin + right * (r2 * sin2) +
                               forward * (r2 * cos2) + up * y_row_top;
      QVector3D const p1_bot = origin + right * (r1 * sin1) +
                               forward * (r1 * cos1) + up * y_row_bottom;
      QVector3D const p2_bot = origin + right * (r2 * sin2) +
                               forward * (r2 * cos2) + up * y_row_bottom;

      // Vertical ring links
      QVector3D color1 = calculateRingColor(p1_top.x(), p1_top.y(), p1_top.z());
      submitter.mesh(getUnitCylinder(),
                     cylinderBetween(ctx.model, p1_top, p1_bot, ring_thickness),
                     color1, nullptr, 0.75F);

      // Horizontal ring links (every other segment for interlocking pattern)
      if (i % 2 == row % 2) {
        QVector3D color_top = calculateRingColor(p1_top.x(), p1_top.y(), p1_top.z());
        submitter.mesh(getUnitCylinder(),
                       cylinderBetween(ctx.model, p1_top, p2_top, ring_thickness),
                       color_top, nullptr, 0.78F);
      }
    }
  }
  
  // Add ring intersection spheres for proper chainmail look
  if (m_config.detail_level >= 1) {
    int const detail_rows = m_config.detail_level >= 2 ? num_rows : num_rows / 2;
    int const detail_segs = segments_per_row;
    for (int row = 0; row < detail_rows; ++row) {
      float const y_pos = y_top - (static_cast<float>(row) / num_rows) * (y_top - y_bottom);
      float const t = static_cast<float>(row) / static_cast<float>(num_rows - 1);
      float const width_scale = shoulder_width * (1.0F - t * 0.12F) + waist_width * t * 0.12F;
      
      for (int i = 0; i < detail_segs; ++i) {
        float const angle = (static_cast<float>(i) / detail_segs) * 2.0F * pi;
        float const cos_a = std::cos(angle);
        float const sin_a = std::sin(angle);
        
        float depth_front = chest_depth_front * (1.0F - t * 0.15F);
        float depth_back = chest_depth_back * (1.0F - t * 0.08F);
        float depth = (cos_a > 0.0F) ? depth_front : depth_back;
        float r = width_scale * depth * (std::abs(cos_a) * 0.3F + 0.7F);
        
        QVector3D ring_pos = origin + right * (r * sin_a) +
                            forward * (r * cos_a) + up * y_pos;
        
        QMatrix4x4 ring_m = ctx.model;
        ring_m.translate(ring_pos);
        ring_m.scale(ring_thickness * 1.8F); // Visible ring nodes
        
        submitter.mesh(getUnitSphere(), ring_m,
                       calculateRingColor(ring_pos.x(), ring_pos.y(), ring_pos.z()) * 1.15F,
                       nullptr, 0.82F);
      }
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
  
  float const shoulder_radius = 0.08F;
  
  QVector3D left_color = calculateRingColor(left_base.x(), left_base.y(), left_base.z());
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, left_base, left_tip, shoulder_radius),
                 left_color, nullptr, 0.8F);
  
  // Right shoulder pauldron
  QVector3D right_base = shoulder_r.origin;
  QVector3D right_tip = right_base + torso.up * 0.08F + torso.right * 0.05F;
  
  QVector3D right_color = calculateRingColor(right_base.x(), right_base.y(), right_base.z());
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, right_base, right_tip, shoulder_radius),
                 right_color, nullptr, 0.8F);
  
  // Layered shoulder protection (multiple overlapping ring rows)
  if (m_config.detail_level >= 1) {
    for (int layer = 0; layer < 3; ++layer) {
      float layer_offset = static_cast<float>(layer) * 0.025F;
      
      QVector3D left_layer = left_base + torso.up * (-layer_offset);
      QVector3D right_layer = right_base + torso.up * (-layer_offset);
      
      QMatrix4x4 left_m = ctx.model;
      left_m.translate(left_layer);
      left_m.scale(shoulder_radius * 1.3F);
      submitter.mesh(getUnitSphere(), left_m,
                     left_color * (1.0F - layer_offset), nullptr, 0.75F);
      
      QMatrix4x4 right_m = ctx.model;
      right_m.translate(right_layer);
      right_m.scale(shoulder_radius * 1.3F);
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
