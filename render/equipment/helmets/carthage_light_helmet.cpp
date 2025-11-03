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
  float const head_r = head.radius;
  
  auto headPoint = [&](const QVector3D &normalized) -> QVector3D {
    return HumanoidRendererBase::frameLocalPosition(head, normalized);
  };

  // Main helmet dome - smooth bronze sphere
  QVector3D dome_center = headPoint(QVector3D(0.0F, 0.7F, 0.1F));
  QMatrix4x4 dome_m = ctx.model;
  dome_m.translate(dome_center);
  
  // Scale to create proper helmet shape - slightly elongated
  dome_m.scale(head_r * 1.15F, head_r * 0.85F, head_r * 1.12F);
  
  // Rich polished bronze with subtle variation
  QVector3D bronze_base = m_config.bronze_color;
  submitter.mesh(getUnitSphere(), dome_m, bronze_base, nullptr, 0.92F);
  
  // Helmet rim/edge reinforcement band
  QVector3D rim_pos = headPoint(QVector3D(0.0F, 0.3F, 0.0F));
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, rim_pos, rim_pos + head.up * 0.03F, head_r * 1.18F),
                 bronze_base * 1.1F, nullptr, 0.95F);
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

  // Bronze crest holder - longitudinal ridge
  QVector3D holder_front = headPoint(QVector3D(0.0F, 0.85F, 0.5F));
  QVector3D holder_back = headPoint(QVector3D(0.0F, 0.88F, -0.5F));
  
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, holder_front, holder_back, head_r * 0.08F),
                 m_config.bronze_color * 1.15F, nullptr, 0.95F);
  
  // Vibrant red horsehair plume
  QVector3D crest_color(0.82F, 0.12F, 0.15F); // Bright crimson
  
  // Dense hair strands flowing backward
  int const strands = m_config.detail_level >= 2 ? 12 : 6;
  
  for (int i = 0; i < strands; ++i) {
    float t = static_cast<float>(i) / static_cast<float>(strands - 1);
    
    // Position along crest holder
    QVector3D base = holder_front * (1.0F - t) + holder_back * t;
    base += head.up * (head_r * 0.05F); // Lift above holder
    
    // Flow backward and down with natural curve
    float curve = std::sin(t * 3.14159F) * 0.3F;
    QVector3D tip = base + 
                    head.up * (head_r * (0.25F - t * 0.1F)) +
                    head.forward * (head_r * (-0.4F - curve));
    
    // Slight lateral spread for volume
    float spread = (static_cast<float>(i % 3) - 1.0F) * head_r * 0.08F;
    tip += head.right * spread;
    
    // Color variation for natural look
    QVector3D hair_color = crest_color * (0.9F + (i % 2) * 0.2F);
    
    submitter.mesh(getUnitCylinder(),
                   cylinderBetween(ctx.model, base, tip, head_r * 0.035F),
                   hair_color, nullptr, 0.65F);
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
