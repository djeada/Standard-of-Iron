#include "spear_renderer.h"
#include "../../entity/renderer_constants.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/rig.h"
#include "../../submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <cmath>

namespace Render::GL {

using Render::Geom::coneFromTo;
using Render::Geom::cylinderBetween;

SpearRenderer::SpearRenderer(SpearRenderConfig config)
    : m_config(std::move(config)) {}

void SpearRenderer::render(const DrawContext &ctx, const BodyFrames &frames,
                           const HumanoidPalette &palette,
                           const HumanoidAnimationContext &anim,
                           ISubmitter &submitter) {
  QVector3D const grip_pos = frames.hand_r.origin;

  bool const is_attacking = anim.inputs.is_attacking && anim.inputs.is_melee;
  float attack_phase = 0.0F;
  if (is_attacking) {
    attack_phase =
        std::fmod(anim.inputs.time * SPEARMAN_INV_ATTACK_CYCLE_TIME, 1.0F);
  }

  QVector3D spear_dir = QVector3D(0.05F, 0.55F, 0.85F);
  if (spear_dir.lengthSquared() > 1e-6F) {
    spear_dir.normalize();
  }

  if (anim.inputs.is_in_hold_mode || anim.inputs.is_exiting_hold) {
    float const t = anim.inputs.is_in_hold_mode
                        ? 1.0F
                        : (1.0F - anim.inputs.hold_exit_progress);

    QVector3D braced_dir = QVector3D(0.05F, 0.40F, 0.91F);
    if (braced_dir.lengthSquared() > 1e-6F) {
      braced_dir.normalize();
    }

    spear_dir = spear_dir * (1.0F - t) + braced_dir * t;
    if (spear_dir.lengthSquared() > 1e-6F) {
      spear_dir.normalize();
    }
  } else if (is_attacking) {
    if (attack_phase >= 0.30F && attack_phase < 0.50F) {
      float const t = (attack_phase - 0.30F) / 0.20F;

      QVector3D attack_dir = QVector3D(0.03F, -0.15F, 1.0F);
      if (attack_dir.lengthSquared() > 1e-6F) {
        attack_dir.normalize();
      }

      spear_dir = spear_dir * (1.0F - t) + attack_dir * t;
      if (spear_dir.lengthSquared() > 1e-6F) {
        spear_dir.normalize();
      }
    }
  }

  QVector3D const shaft_base = grip_pos - spear_dir * 0.28F;
  QVector3D shaft_mid = grip_pos + spear_dir * (m_config.spear_length * 0.5F);
  QVector3D const shaft_tip = grip_pos + spear_dir * m_config.spear_length;

  shaft_mid.setY(shaft_mid.y() + 0.02F);

  submitter.mesh(
      getUnitCylinder(),
      cylinderBetween(ctx.model, shaft_base, shaft_mid, m_config.shaft_radius),
      m_config.shaft_color, nullptr, 1.0F);

  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, shaft_mid, shaft_tip,
                                 m_config.shaft_radius * 0.95F),
                 m_config.shaft_color * 0.98F, nullptr, 1.0F);

  QVector3D const spearhead_base = shaft_tip;
  QVector3D const spearhead_tip =
      shaft_tip + spear_dir * m_config.spearhead_length;

  submitter.mesh(getUnitCone(),
                 coneFromTo(ctx.model, spearhead_base, spearhead_tip,
                            m_config.shaft_radius * 1.8F),
                 m_config.spearhead_color, nullptr, 1.0F);

  QVector3D const grip_end = grip_pos + spear_dir * 0.10F;
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, grip_pos, grip_end,
                                 m_config.shaft_radius * 1.5F),
                 palette.leather * 0.92F, nullptr, 1.0F);
}

} // namespace Render::GL
