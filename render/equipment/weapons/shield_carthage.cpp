#include "shield_carthage.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/rig.h"
#include "../../submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <numbers>

namespace Render::GL {

using Render::Geom::cylinderBetween;
using Render::Geom::sphereAt;

CarthageShieldRenderer::CarthageShieldRenderer() {
  ShieldRenderConfig config;
  config.shield_color = {0.20F, 0.46F, 0.62F};      // Carthage blue
  config.trim_color = {0.76F, 0.68F, 0.42F};        // Carthage gold trim
  config.metal_color = {0.70F, 0.68F, 0.52F};       // Carthage metal
  config.shield_radius = 0.18F * 0.9F;              // Slightly smaller
  config.shield_aspect = 1.0F;                      // Circular
  config.has_cross_decal = false;                   // No cross for Carthage

  setConfig(config);
}

void CarthageShieldRenderer::render(const DrawContext &ctx,
                                    const BodyFrames &frames,
                                    const HumanoidPalette &palette,
                                    const HumanoidAnimationContext &,
                                    ISubmitter &submitter) {
  constexpr float k_shield_yaw_degrees = -70.0F;

  QMatrix4x4 rot;
  rot.rotate(k_shield_yaw_degrees, 0.0F, 1.0F, 0.0F);

  const QVector3D n = rot.map(QVector3D(0.0F, 0.0F, 1.0F));
  const QVector3D axis_x = rot.map(QVector3D(1.0F, 0.0F, 0.0F));
  const QVector3D axis_y = rot.map(QVector3D(0.0F, 1.0F, 0.0F));

  ShieldRenderConfig config;
  const_cast<CarthageShieldRenderer*>(this)->setConfig(config); // Get config
  float const shield_radius = 0.18F * 0.9F * 2.5F;

  QVector3D shield_center = frames.handL.origin +
                            axis_x * (-shield_radius * 0.35F) +
                            axis_y * (-0.05F) + n * (0.06F);

  QVector3D const shield_color{0.20F, 0.46F, 0.62F};
  QVector3D const trim_color{0.76F, 0.68F, 0.42F};
  QVector3D const metal_color{0.70F, 0.68F, 0.52F};

  // Draw circular dome segments
  constexpr int radial_segments = 16;
  constexpr int ring_segments = 6;
  
  for (int ring = 0; ring < ring_segments; ++ring) {
    float const ring_t = static_cast<float>(ring) / static_cast<float>(ring_segments);
    float const next_ring_t = static_cast<float>(ring + 1) / static_cast<float>(ring_segments);
    
    float const r = shield_radius * ring_t;
    float const next_r = shield_radius * next_ring_t;
    float const dome_height = shield_radius * 0.15F * (1.0F - ring_t * ring_t);
    float const next_dome_height = shield_radius * 0.15F * (1.0F - next_ring_t * next_ring_t);
    
    for (int i = 0; i < radial_segments; ++i) {
      float const angle = (static_cast<float>(i) / static_cast<float>(radial_segments)) * 2.0F * std::numbers::pi_v<float>;
      
      QVector3D pos = shield_center +
                      axis_x * (r * std::cos(angle)) +
                      axis_y * (r * std::sin(angle)) +
                      n * dome_height;
      
      QMatrix4x4 m = ctx.model;
      m.translate(pos);
      m.scale(0.02F, 0.02F, 0.015F);
      
      submitter.mesh(getUnitSphere(), m, shield_color, nullptr, 1.0F);
    }
  }

  // Draw outer rim (gold trim)
  constexpr int rim_segments = 24;
  for (int i = 0; i < rim_segments; ++i) {
    float const a0 = (static_cast<float>(i) / static_cast<float>(rim_segments)) * 2.0F * std::numbers::pi_v<float>;
    float const a1 = (static_cast<float>(i + 1) / static_cast<float>(rim_segments)) * 2.0F * std::numbers::pi_v<float>;

    QVector3D const p0 = shield_center + axis_x * (shield_radius * std::cos(a0)) +
                         axis_y * (shield_radius * std::sin(a0));
    QVector3D const p1 = shield_center + axis_x * (shield_radius * std::cos(a1)) +
                         axis_y * (shield_radius * std::sin(a1));

    submitter.mesh(getUnitCylinder(),
                   cylinderBetween(ctx.model, p0, p1, 0.012F),
                   trim_color, nullptr, 1.0F);
  }

  // Draw boss (stone/metal center)
  float const boss_radius = shield_radius * 0.25F;
  submitter.mesh(getUnitSphere(),
                 sphereAt(ctx.model, shield_center + n * 0.08F, boss_radius),
                 metal_color, nullptr, 1.0F);

  // Draw grip
  QVector3D const grip_a = shield_center - axis_x * 0.035F - n * 0.030F;
  QVector3D const grip_b = shield_center + axis_x * 0.035F - n * 0.030F;
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, grip_a, grip_b, 0.010F),
                 palette.leather, nullptr, 1.0F);
}

} // namespace Render::GL
