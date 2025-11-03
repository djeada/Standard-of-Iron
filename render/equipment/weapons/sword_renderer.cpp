#include "sword_renderer.h"
#include "../../entity/renderer_constants.h"
#include "../../geom/math_utils.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_math.h"
#include "../../humanoid/rig.h"
#include "../../humanoid/style_palette.h"
#include "../../submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <cmath>

namespace Render::GL {

using Render::Geom::clamp01;
using Render::Geom::clampf;
using Render::Geom::coneFromTo;
using Render::Geom::cylinderBetween;
using Render::Geom::easeInOutCubic;
using Render::Geom::lerp;
using Render::Geom::nlerp;
using Render::Geom::smoothstep;

SwordRenderer::SwordRenderer(SwordRenderConfig config)
    : m_config(std::move(config)) {}

void SwordRenderer::render(const DrawContext &ctx, const BodyFrames &frames,
                           const HumanoidPalette &palette,
                           const HumanoidAnimationContext &anim,
                           ISubmitter &submitter) {
  QVector3D const grip_pos = frames.handR.origin;

  bool const is_attacking = anim.inputs.is_attacking && anim.inputs.isMelee;
  float attack_phase = 0.0F;
  if (is_attacking) {
    attack_phase =
        std::fmod(anim.inputs.time * KNIGHT_INV_ATTACK_CYCLE_TIME, 1.0F);
  }

  constexpr float k_sword_yaw_deg = 25.0F;
  QMatrix4x4 yaw_m;
  yaw_m.rotate(k_sword_yaw_deg, 0.0F, 1.0F, 0.0F);

  QVector3D upish = yaw_m.map(QVector3D(0.05F, 1.0F, 0.15F));
  QVector3D midish = yaw_m.map(QVector3D(0.08F, 0.20F, 1.0F));
  QVector3D downish = yaw_m.map(QVector3D(0.10F, -1.0F, 0.25F));
  if (upish.lengthSquared() > 1e-6F) {
    upish.normalize();
  }
  if (midish.lengthSquared() > 1e-6F) {
    midish.normalize();
  }
  if (downish.lengthSquared() > 1e-6F) {
    downish.normalize();
  }

  QVector3D sword_dir = upish;

  if (is_attacking) {
    if (attack_phase < 0.18F) {
      float const t = easeInOutCubic(attack_phase / 0.18F);
      sword_dir = nlerp(upish, upish, t);
    } else if (attack_phase < 0.32F) {
      float const t = easeInOutCubic((attack_phase - 0.18F) / 0.14F);
      sword_dir = nlerp(upish, midish, t * 0.35F);
    } else if (attack_phase < 0.52F) {
      float t = (attack_phase - 0.32F) / 0.20F;
      t = t * t * t;
      if (t < 0.5F) {
        float const u = t / 0.5F;
        sword_dir = nlerp(upish, midish, u);
      } else {
        float const u = (t - 0.5F) / 0.5F;
        sword_dir = nlerp(midish, downish, u);
      }
    } else if (attack_phase < 0.72F) {
      float const t = easeInOutCubic((attack_phase - 0.52F) / 0.20F);
      sword_dir = nlerp(downish, midish, t);
    } else {
      float const t = smoothstep(0.72F, 1.0F, attack_phase);
      sword_dir = nlerp(midish, upish, t);
    }
  }

  QVector3D const handle_end = grip_pos - sword_dir * 0.10F;
  QVector3D const blade_base = grip_pos;
  QVector3D const blade_tip = grip_pos + sword_dir * m_config.sword_length;

  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, handle_end, blade_base,
                                 m_config.handle_radius),
                 palette.leather, nullptr, 1.0F);

  QVector3D const guard_center = blade_base;
  float const gw = m_config.guard_half_width;

  QVector3D guard_right =
      QVector3D::crossProduct(QVector3D(0, 1, 0), sword_dir);
  if (guard_right.lengthSquared() < 1e-6F) {
    guard_right = QVector3D::crossProduct(QVector3D(1, 0, 0), sword_dir);
  }
  guard_right.normalize();

  QVector3D const guard_l = guard_center - guard_right * gw;
  QVector3D const guard_r = guard_center + guard_right * gw;

  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, guard_l, guard_r, 0.014F),
                 m_config.metal_color, nullptr, 1.0F);

  QMatrix4x4 gl = ctx.model;
  gl.translate(guard_l);
  gl.scale(0.018F);
  submitter.mesh(getUnitSphere(), gl, m_config.metal_color, nullptr, 1.0F);
  QMatrix4x4 gr = ctx.model;
  gr.translate(guard_r);
  gr.scale(0.018F);
  submitter.mesh(getUnitSphere(), gr, m_config.metal_color, nullptr, 1.0F);

  float const l = m_config.sword_length;
  float const base_w = m_config.sword_width;
  float blade_thickness = base_w * 0.15F;

  float const ricasso_len = clampf(m_config.blade_ricasso, 0.10F, l * 0.30F);
  QVector3D const ricasso_end = blade_base + sword_dir * ricasso_len;

  float const mid_w = base_w * 0.95F;
  float const tip_w = base_w * 0.28F;
  float const tip_start_dist = lerp(ricasso_len, l, 0.70F);
  QVector3D const tip_start = blade_base + sword_dir * tip_start_dist;

  auto draw_flat_section = [&](const QVector3D &start, const QVector3D &end,
                               float width, const QVector3D &color) {
    QVector3D right = QVector3D::crossProduct(sword_dir, QVector3D(0, 1, 0));
    if (right.lengthSquared() < 0.001F) {
      right = QVector3D::crossProduct(sword_dir, QVector3D(1, 0, 0));
    }
    right.normalize();

    float const offset = width * 0.33F;

    submitter.mesh(getUnitCylinder(),
                   cylinderBetween(ctx.model, start, end, blade_thickness),
                   color, nullptr, 1.0F);

    submitter.mesh(getUnitCylinder(),
                   cylinderBetween(ctx.model, start + right * offset,
                                   end + right * offset,
                                   blade_thickness * 0.8F),
                   color * 0.92F, nullptr, 1.0F);

    submitter.mesh(getUnitCylinder(),
                   cylinderBetween(ctx.model, start - right * offset,
                                   end - right * offset,
                                   blade_thickness * 0.8F),
                   color * 0.92F, nullptr, 1.0F);
  };

  draw_flat_section(blade_base, ricasso_end, base_w, m_config.metal_color);

  draw_flat_section(ricasso_end, tip_start, mid_w, m_config.metal_color);

  int const tip_segments = 3;
  for (int i = 0; i < tip_segments; ++i) {
    float const t0 = (float)i / tip_segments;
    float const t1 = (float)(i + 1) / tip_segments;
    QVector3D const seg_start =
        tip_start + sword_dir * ((blade_tip - tip_start).length() * t0);
    QVector3D const seg_end =
        tip_start + sword_dir * ((blade_tip - tip_start).length() * t1);
    float const w = lerp(mid_w, tip_w, t1);
    submitter.mesh(
        getUnitCylinder(),
        cylinderBetween(ctx.model, seg_start, seg_end, blade_thickness),
        m_config.metal_color * (1.0F - i * 0.03F), nullptr, 1.0F);
  }

  QVector3D const fuller_start = blade_base + sword_dir * (ricasso_len + 0.02F);
  QVector3D const fuller_end =
      blade_base + sword_dir * (tip_start_dist - 0.06F);
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, fuller_start, fuller_end,
                                 blade_thickness * 0.6F),
                 m_config.metal_color * 0.65F, nullptr, 1.0F);

  QVector3D const pommel = handle_end - sword_dir * 0.02F;
  QMatrix4x4 pommel_mat = ctx.model;
  pommel_mat.translate(pommel);
  pommel_mat.scale(m_config.pommel_radius);
  submitter.mesh(getUnitSphere(), pommel_mat, m_config.metal_color, nullptr,
                 1.0F);

  if (is_attacking && attack_phase >= 0.32F && attack_phase < 0.56F) {
    float const t = (attack_phase - 0.32F) / 0.24F;
    float const alpha = clamp01(0.35F * (1.0F - t));
    QVector3D const trail_start = blade_base - sword_dir * 0.05F;
    QVector3D const trail_end = blade_base - sword_dir * (0.28F + 0.15F * t);
    submitter.mesh(getUnitCone(),
                   coneFromTo(ctx.model, trail_end, trail_start, base_w * 0.9F),
                   m_config.metal_color * 0.9F, nullptr, alpha);
  }
}

} // namespace Render::GL
