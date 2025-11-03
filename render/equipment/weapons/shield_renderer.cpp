#include "shield_renderer.h"
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

ShieldRenderer::ShieldRenderer(ShieldRenderConfig config)
    : m_config(std::move(config)) {}

void ShieldRenderer::render(const DrawContext &ctx, const BodyFrames &frames,
                            const HumanoidPalette &palette,
                            const HumanoidAnimationContext &,
                            ISubmitter &submitter) {
  constexpr float k_scale_factor = 2.5F;
  constexpr float k_shield_yaw_degrees = -70.0F;

  QMatrix4x4 rot;
  rot.rotate(k_shield_yaw_degrees, 0.0F, 1.0F, 0.0F);

  const QVector3D n = rot.map(QVector3D(0.0F, 0.0F, 1.0F));
  const QVector3D axis_x = rot.map(QVector3D(1.0F, 0.0F, 0.0F));
  const QVector3D axis_y = rot.map(QVector3D(0.0F, 1.0F, 0.0F));

  float const base_extent = m_config.shield_radius * k_scale_factor;
  float const shield_width = base_extent;
  float const shield_height = base_extent * m_config.shield_aspect;
  float const min_extent = std::min(shield_width, shield_height);

  QVector3D shield_center = frames.handL.origin +
                            axis_x * (-shield_width * 0.35F) +
                            axis_y * (-0.05F) + n * (0.06F);

  const float plate_half = 0.0015F;
  const float plate_full = plate_half * 2.0F;

  {
    QMatrix4x4 m = ctx.model;
    m.translate(shield_center + n * plate_half);
    m.rotate(k_shield_yaw_degrees, 0.0F, 1.0F, 0.0F);
    m.scale(shield_width, shield_height, plate_full);
    submitter.mesh(getUnitCylinder(), m, m_config.shield_color, nullptr, 1.0F);
  }

  {
    QMatrix4x4 m = ctx.model;
    m.translate(shield_center - n * plate_half);
    m.rotate(k_shield_yaw_degrees, 0.0F, 1.0F, 0.0F);
    m.scale(shield_width * 0.985F, shield_height * 0.985F, plate_full);
    submitter.mesh(getUnitCylinder(), m, palette.leather * 0.8F, nullptr, 1.0F);
  }

  auto draw_ring_rotated = [&](float width, float height, float thickness,
                               const QVector3D &color) {
    constexpr int k_segments = 18;
    for (int i = 0; i < k_segments; ++i) {
      float const a0 = (float)i / k_segments * 2.0F * std::numbers::pi_v<float>;
      float const a1 =
          (float)(i + 1) / k_segments * 2.0F * std::numbers::pi_v<float>;

      QVector3D const v0(width * std::cos(a0), height * std::sin(a0), 0.0F);
      QVector3D const v1(width * std::cos(a1), height * std::sin(a1), 0.0F);

      QVector3D const p0 = shield_center + rot.map(v0);
      QVector3D const p1 = shield_center + rot.map(v1);

      submitter.mesh(getUnitCylinder(),
                     cylinderBetween(ctx.model, p0, p1, thickness), color,
                     nullptr, 1.0F);
    }
  };

  draw_ring_rotated(shield_width, shield_height, min_extent * 0.010F,
                    m_config.trim_color * 0.95F);
  draw_ring_rotated(shield_width * 0.72F, shield_height * 0.72F,
                    min_extent * 0.006F, palette.leather * 0.90F);

  {
    QMatrix4x4 m = ctx.model;
    m.translate(shield_center + n * (0.02F * k_scale_factor));
    m.scale(0.045F * k_scale_factor);
    submitter.mesh(getUnitSphere(), m, m_config.metal_color, nullptr, 1.0F);
  }

  {
    QVector3D const grip_a = shield_center - axis_x * 0.035F - n * 0.030F;
    QVector3D const grip_b = shield_center + axis_x * 0.035F - n * 0.030F;
    submitter.mesh(getUnitCylinder(),
                   cylinderBetween(ctx.model, grip_a, grip_b, 0.010F),
                   palette.leather, nullptr, 1.0F);
  }

  if (m_config.has_cross_decal) {
    QVector3D const center_front =
        shield_center + n * (plate_full * 0.5F + 0.0015F);
    float const bar_radius = min_extent * 0.10F;

    QVector3D const top = center_front + axis_y * (shield_height * 0.90F);
    QVector3D const bot = center_front - axis_y * (shield_height * 0.90F);
    submitter.mesh(getUnitCylinder(),
                   cylinderBetween(ctx.model, top, bot, bar_radius),
                   m_config.trim_color, nullptr, 1.0F);

    QVector3D const left = center_front - axis_x * (shield_width * 0.90F);
    QVector3D const right = center_front + axis_x * (shield_width * 0.90F);
    submitter.mesh(getUnitCylinder(),
                   cylinderBetween(ctx.model, left, right, bar_radius),
                   m_config.trim_color, nullptr, 1.0F);
  }
}

} // namespace Render::GL
