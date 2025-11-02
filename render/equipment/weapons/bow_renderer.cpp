#include "bow_renderer.h"
#include "../../entity/registry.h"
#include "../../entity/renderer_constants.h"
#include "../../geom/math_utils.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_math.h"
#include "../../humanoid/humanoid_specs.h"
#include "../../humanoid/rig.h"
#include "../../submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>
#include <cmath>

namespace Render::GL {

using Render::Geom::clampf;
using Render::Geom::coneFromTo;
using Render::Geom::cylinderBetween;

BowRenderer::BowRenderer(BowRenderConfig config)
    : m_config(std::move(config)) {}

void BowRenderer::render(const DrawContext &ctx, const BodyFrames &frames,
                         const HumanoidPalette &palette,
                         const HumanoidAnimationContext &anim,
                         ISubmitter &submitter) {
  const QVector3D up(0.0F, 1.0F, 0.0F);
  const QVector3D forward(0.0F, 0.0F, 1.0F);

  QVector3D const grip = frames.handL.origin;

  float const bow_plane_z = 0.45F;
  QVector3D const top_end(m_config.bow_x, m_config.bow_top_y, bow_plane_z);
  QVector3D const bot_end(m_config.bow_x, m_config.bow_bot_y, bow_plane_z);

  QVector3D const right_hand = frames.handR.origin;
  QVector3D const nock(
      m_config.bow_x,
      clampf(right_hand.y(), m_config.bow_bot_y + 0.05F,
             m_config.bow_top_y - 0.05F),
      clampf(right_hand.z(), bow_plane_z - 0.30F, bow_plane_z + 0.30F));

  constexpr int k_bowstring_segments = 22;
  auto q_bezier = [](const QVector3D &a, const QVector3D &c, const QVector3D &b,
                     float t) {
    float const u = 1.0F - t;
    return u * u * a + 2.0F * u * t * c + t * t * b;
  };

  float const bow_mid_y = (top_end.y() + bot_end.y()) * 0.5F;
  float const ctrl_y = bow_mid_y + (0.45F * m_config.bow_curve_factor);
  QVector3D const ctrl(m_config.bow_x, ctrl_y,
                       bow_plane_z + m_config.bow_depth * 0.6F *
                                         m_config.bow_curve_factor);

  QVector3D prev = bot_end;
  for (int i = 1; i <= k_bowstring_segments; ++i) {
    float const t = float(i) / float(k_bowstring_segments);
    QVector3D const cur = q_bezier(bot_end, ctrl, top_end, t);
    submitter.mesh(
        getUnitCylinder(),
        cylinderBetween(ctx.model, prev, cur, m_config.bow_rod_radius),
        palette.wood, nullptr, 1.0F);
    prev = cur;
  }

  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, grip - up * 0.05F,
                                 grip + up * 0.05F,
                                 m_config.bow_rod_radius * 1.45F),
                 palette.wood, nullptr, 1.0F);

  submitter.mesh(
      getUnitCylinder(),
      cylinderBetween(ctx.model, top_end, nock, m_config.string_radius),
      m_config.string_color, nullptr, 1.0F);
  submitter.mesh(
      getUnitCylinder(),
      cylinderBetween(ctx.model, nock, bot_end, m_config.string_radius),
      m_config.string_color, nullptr, 1.0F);

  bool const is_bow_attacking =
      anim.inputs.is_attacking && !anim.inputs.isMelee;
  if (is_bow_attacking) {
    submitter.mesh(
        getUnitCylinder(),
        cylinderBetween(ctx.model, frames.handR.origin, nock, 0.0045F),
        m_config.string_color * 0.9F, nullptr, 1.0F);
  }

  float attack_phase = 0.0F;
  if (is_bow_attacking) {
    attack_phase =
        std::fmod(anim.inputs.time * ARCHER_INV_ATTACK_CYCLE_TIME, 1.0F);
  }

  bool const show_arrow =
      !is_bow_attacking ||
      (is_bow_attacking && attack_phase >= 0.0F && attack_phase < 0.52F);

  if (show_arrow) {
    QVector3D const tail = nock - forward * 0.06F;
    QVector3D const tip = tail + forward * 0.90F;

    submitter.mesh(getUnitCylinder(),
                   cylinderBetween(ctx.model, tail, tip, 0.018F), palette.wood,
                   nullptr, 1.0F);

    QVector3D const head_base = tip - forward * 0.10F;
    submitter.mesh(getUnitCone(), coneFromTo(ctx.model, head_base, tip, 0.05F),
                   m_config.metal_color, nullptr, 1.0F);

    QVector3D const f1b = tail - forward * 0.02F;
    QVector3D const f1a = f1b - forward * 0.06F;
    QVector3D const f2b = tail + forward * 0.02F;
    QVector3D const f2a = f2b + forward * 0.06F;

    submitter.mesh(getUnitCone(), coneFromTo(ctx.model, f1b, f1a, 0.04F),
                   m_config.fletching_color, nullptr, 1.0F);
    submitter.mesh(getUnitCone(), coneFromTo(ctx.model, f2a, f2b, 0.04F),
                   m_config.fletching_color, nullptr, 1.0F);
  }
}

} // namespace Render::GL
