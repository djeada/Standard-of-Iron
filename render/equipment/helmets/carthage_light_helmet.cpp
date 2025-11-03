#include "carthage_light_helmet.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_math.h"
#include "../../humanoid/rig.h"
#include "../../submitter.h"
#include <QMatrix4x4>
#include <QQuaternion>
#include <QVector3D>
#include <cmath>
#include <numbers>

namespace Render::GL {

using Render::Geom::cylinderBetween;
using Render::Geom::sphereAt;

void CarthageLightHelmetRenderer::render(
    const DrawContext &ctx, const BodyFrames &frames,
    const HumanoidPalette &palette, const HumanoidAnimationContext &anim,
    ISubmitter &submitter) {
  (void)anim;
  (void)palette;

  const AttachmentFrame &head = frames.head;
  if (head.radius <= 0.0F) {
    return;
  }

  renderBowl(ctx, head, submitter);
  renderBrim(ctx, head, submitter);
  renderCheekGuards(ctx, head, submitter);
  
  if (m_config.has_nasal_guard) {
    renderNasalGuard(ctx, head, submitter);
  }
  
  if (m_config.has_crest) {
    renderCrest(ctx, head, submitter);
  }
  
  if (m_config.detail_level >= 2) {
    renderRivets(ctx, head, submitter);
  }
}

void CarthageLightHelmetRenderer::renderBowl(const DrawContext &ctx,
                                             const AttachmentFrame &head,
                                             ISubmitter &submitter) {
  // Main helmet bowl - hemispherical bronze cap
  float const head_r = head.radius;
  float const bowl_height = m_config.helmet_height;
  
  auto headPoint = [&](const QVector3D &normalized) -> QVector3D {
    return HumanoidRendererBase::frameLocalPosition(head, normalized);
  };

  // Create graduated helmet bowl with multiple segments for realism
  int const segments = m_config.detail_level >= 2 ? 16 : 8;
  
  for (int i = 0; i < segments; ++i) {
    float t0 = static_cast<float>(i) / static_cast<float>(segments);
    float t1 = static_cast<float>(i + 1) / static_cast<float>(segments);
    
    // Spherical cap profile
    float angle0 = t0 * std::numbers::pi_v<float> * 0.5F;
    float angle1 = t1 * std::numbers::pi_v<float> * 0.5F;
    
    float y0 = std::cos(angle0) * bowl_height;
    float y1 = std::cos(angle1) * bowl_height;
    float r0 = std::sin(angle0) * head_r * 1.08F;
    float r1 = std::sin(angle1) * head_r * 1.08F;
    
    QVector3D bottom = headPoint(QVector3D(0.0F, 0.85F - t0 * 0.3F, 0.0F));
    QVector3D top = headPoint(QVector3D(0.0F, 0.85F - t1 * 0.3F, 0.0F));
    
    // Vary bronze color slightly per segment for hammered metal look
    float variation = 1.0F + (std::sin(static_cast<float>(i) * 1.7F) * 0.08F);
    QVector3D segment_color = m_config.bronze_color * variation;
    
    submitter.mesh(getUnitCylinder(),
                   cylinderBetween(ctx.model, bottom, top, (r0 + r1) * 0.5F),
                   segment_color, nullptr, 0.85F);
  }
  
  // Top dome cap
  QVector3D apex = headPoint(QVector3D(0.0F, 1.0F, 0.0F));
  QMatrix4x4 apex_m = ctx.model;
  apex_m.translate(apex);
  apex_m.scale(head_r * 0.25F);
  submitter.mesh(getUnitSphere(), apex_m, 
                 m_config.bronze_color * 1.15F, nullptr, 0.9F);
}

void CarthageLightHelmetRenderer::renderBrim(const DrawContext &ctx,
                                             const AttachmentFrame &head,
                                             ISubmitter &submitter) {
  float const head_r = head.radius;
  float const brim_width = m_config.brim_width;
  
  auto headPoint = [&](const QVector3D &normalized) -> QVector3D {
    return HumanoidRendererBase::frameLocalPosition(head, normalized);
  };

  // Front brim protection
  QVector3D brim_base = headPoint(QVector3D(0.0F, 0.35F, 0.65F));
  QVector3D brim_tip = brim_base + head.forward * brim_width;
  
  QMatrix4x4 brim_m = ctx.model;
  brim_m.translate((brim_base + brim_tip) * 0.5F);
  
  // Orient brim forward
  QVector3D brim_vec = brim_tip - brim_base;
  float brim_len = brim_vec.length();
  if (brim_len > 0.001F) {
    brim_vec.normalize();
    QVector3D right = QVector3D::crossProduct(brim_vec, head.up).normalized();
    QVector3D up = QVector3D::crossProduct(right, brim_vec);
    
    QMatrix4x4 rotation;
    rotation.setColumn(0, QVector4D(right, 0.0F));
    rotation.setColumn(1, QVector4D(up, 0.0F));
    rotation.setColumn(2, QVector4D(brim_vec, 0.0F));
    rotation.setColumn(3, QVector4D(0.0F, 0.0F, 0.0F, 1.0F));
    
    brim_m = brim_m * rotation;
  }
  
  brim_m.scale(head_r * 1.1F, head_r * 0.15F, brim_len * 0.5F);
  submitter.mesh(getUnitSphere(), brim_m, 
                 m_config.bronze_color * 0.92F, nullptr, 0.85F);
}

void CarthageLightHelmetRenderer::renderCheekGuards(
    const DrawContext &ctx, const AttachmentFrame &head,
    ISubmitter &submitter) {
  float const head_r = head.radius;
  float const guard_len = m_config.cheek_guard_length;
  
  auto headPoint = [&](const QVector3D &normalized) -> QVector3D {
    return HumanoidRendererBase::frameLocalPosition(head, normalized);
  };

  // Left cheek guard
  QVector3D left_top = headPoint(QVector3D(-0.75F, 0.45F, 0.35F));
  QVector3D left_bot = left_top + head.up * (-guard_len) + 
                       head.forward * 0.02F;
  
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, left_top, left_bot, head_r * 0.42F),
                 m_config.bronze_color * 0.88F, nullptr, 0.8F);
  
  // Right cheek guard
  QVector3D right_top = headPoint(QVector3D(0.75F, 0.45F, 0.35F));
  QVector3D right_bot = right_top + head.up * (-guard_len) + 
                        head.forward * 0.02F;
  
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, right_top, right_bot, head_r * 0.42F),
                 m_config.bronze_color * 0.88F, nullptr, 0.8F);
  
  // Cheek guard attachment rivets
  if (m_config.detail_level >= 1) {
    for (auto pos : {left_top, right_top}) {
      QMatrix4x4 rivet_m = ctx.model;
      rivet_m.translate(pos);
      rivet_m.scale(head_r * 0.08F);
      submitter.mesh(getUnitSphere(), rivet_m,
                     m_config.bronze_color * 1.3F, nullptr, 1.0F);
    }
  }
}

void CarthageLightHelmetRenderer::renderNasalGuard(
    const DrawContext &ctx, const AttachmentFrame &head,
    ISubmitter &submitter) {
  float const head_r = head.radius;
  
  auto headPoint = [&](const QVector3D &normalized) -> QVector3D {
    return HumanoidRendererBase::frameLocalPosition(head, normalized);
  };

  // Vertical nose guard strip
  QVector3D nasal_top = headPoint(QVector3D(0.0F, 0.55F, 0.85F));
  QVector3D nasal_bot = headPoint(QVector3D(0.0F, 0.0F, 0.92F));
  
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, nasal_bot, nasal_top, head_r * 0.12F),
                 m_config.bronze_color * 0.95F, nullptr, 0.9F);
}

void CarthageLightHelmetRenderer::renderCrest(const DrawContext &ctx,
                                              const AttachmentFrame &head,
                                              ISubmitter &submitter) {
  float const head_r = head.radius;
  
  auto headPoint = [&](const QVector3D &normalized) -> QVector3D {
    return HumanoidRendererBase::frameLocalPosition(head, normalized);
  };

  // Horsehair crest plume (red/purple dyed)
  QVector3D crest_color(0.65F, 0.15F, 0.18F); // Dark red
  
  // Crest base mount
  QVector3D crest_base = headPoint(QVector3D(0.0F, 0.95F, -0.15F));
  QMatrix4x4 base_m = ctx.model;
  base_m.translate(crest_base);
  base_m.scale(head_r * 0.18F, head_r * 0.08F, head_r * 0.3F);
  submitter.mesh(getUnitSphere(), base_m,
                 m_config.bronze_color * 1.1F, nullptr, 0.95F);
  
  // Horsehair plume strands (multiple cylinders)
  int const hair_strands = m_config.detail_level >= 2 ? 7 : 4;
  
  for (int i = 0; i < hair_strands; ++i) {
    float offset = (static_cast<float>(i) - static_cast<float>(hair_strands) * 0.5F) 
                   / static_cast<float>(hair_strands);
    
    QVector3D strand_base = crest_base + head.forward * (offset * head_r * 0.15F);
    QVector3D strand_tip = strand_base + head.up * (head_r * 0.35F) + 
                           head.forward * (head_r * -0.12F);
    
    float wave = std::sin(offset * 3.14159F) * 0.02F;
    strand_tip += head.right * (wave * head_r);
    
    QVector3D strand_color = crest_color * (1.0F + offset * 0.15F);
    
    submitter.mesh(getUnitCylinder(),
                   cylinderBetween(ctx.model, strand_base, strand_tip, head_r * 0.045F),
                   strand_color, nullptr, 0.7F);
  }
}

void CarthageLightHelmetRenderer::renderRivets(const DrawContext &ctx,
                                               const AttachmentFrame &head,
                                               ISubmitter &submitter) {
  float const head_r = head.radius;
  
  auto headPoint = [&](const QVector3D &normalized) -> QVector3D {
    return HumanoidRendererBase::frameLocalPosition(head, normalized);
  };

  // Decorative bronze rivets around helmet bowl
  QVector3D rivet_color = m_config.bronze_color * 1.25F;
  
  int const rivet_count = 12;
  for (int i = 0; i < rivet_count; ++i) {
    float angle = (static_cast<float>(i) / static_cast<float>(rivet_count)) 
                  * 2.0F * std::numbers::pi_v<float>;
    
    float x = std::cos(angle) * 0.85F;
    float z = std::sin(angle) * 0.85F;
    
    QVector3D rivet_pos = headPoint(QVector3D(x, 0.55F, z));
    
    QMatrix4x4 rivet_m = ctx.model;
    rivet_m.translate(rivet_pos);
    rivet_m.scale(head_r * 0.06F);
    
    submitter.mesh(getUnitSphere(), rivet_m, rivet_color, nullptr, 1.0F);
  }
}

} // namespace Render::GL
