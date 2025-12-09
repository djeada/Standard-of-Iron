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

namespace {
constexpr QVector3D k_dark_bow_color(0.05F, 0.035F, 0.02F);
}

BowRenderer::BowRenderer(BowRenderConfig config)
    : m_config(std::move(config)) {}

void BowRenderer::render(const DrawContext &ctx, const BodyFrames &frames,
                         const HumanoidPalette &palette,
                         const HumanoidAnimationContext &anim,
                         ISubmitter &submitter) {
  const QVector3D up(0.0F, 1.0F, 0.0F);
  const QVector3D forward(0.0F, 0.0F, 1.0F);

  // Right hand now holds the bow grip; use it as anchor for the bow plane.
  QVector3D const grip = frames.hand_r.origin;

  float const bow_half_height = (m_config.bow_top_y - m_config.bow_bot_y) *
                                0.5F * m_config.bow_height_scale;
  float const bow_mid_y = grip.y();
  float const bow_top_y = bow_mid_y + bow_half_height;
  float const bow_bot_y = bow_mid_y - bow_half_height;

  QVector3D outward = frames.hand_r.right;
  if (outward.lengthSquared() < 1e-6F) {
    outward = QVector3D(-1.0F, 0.0F, 0.0F);
  }
  outward.setY(0.0F);
  if (outward.lengthSquared() < 1e-6F) {
    outward = QVector3D(-1.0F, 0.0F, 0.0F);
  } else {
    outward.normalize();
  }
  // Keep the bow plane close to the grip so the hand actually touches it.
  QVector3D const side = outward * 0.02F;

  float const bow_plane_x = grip.x() + m_config.bow_x + side.x();
  float const bow_plane_z = grip.z() + side.z();

  QVector3D const top_end(bow_plane_x, bow_top_y, bow_plane_z);
  QVector3D const bot_end(bow_plane_x, bow_bot_y, bow_plane_z);

  QVector3D const string_hand = frames.hand_l.origin;
  QVector3D const nock(
      bow_plane_x,
      clampf(string_hand.y(), bow_bot_y + 0.05F, bow_top_y - 0.05F),
      clampf(string_hand.z(), bow_plane_z - 0.30F, bow_plane_z + 0.30F));

  constexpr int k_bowstring_segments = 22;
  auto q_bezier = [](const QVector3D &a, const QVector3D &c, const QVector3D &b,
                     float t) {
    float const u = 1.0F - t;
    return u * u * a + 2.0F * u * t * c + t * t * b;
  };

  float const ctrl_y = bow_mid_y + (0.45F * m_config.bow_curve_factor);
  QVector3D const ctrl(bow_plane_x, ctrl_y,
                       bow_plane_z + m_config.bow_depth * 0.6F *
                                         m_config.bow_curve_factor);

  QVector3D prev = bot_end;
  for (int i = 1; i <= k_bowstring_segments; ++i) {
    float const t = float(i) / float(k_bowstring_segments);
    QVector3D const cur = q_bezier(bot_end, ctrl, top_end, t);
    submitter.mesh(
        getUnitCylinder(),
        cylinderBetween(ctx.model, prev, cur, m_config.bow_rod_radius),
        k_dark_bow_color, nullptr, 1.0F, m_config.material_id);
    prev = cur;
  }

  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, grip - up * 0.05F,
                                 grip + up * 0.05F,
                                 m_config.bow_rod_radius * 1.45F),
                 k_dark_bow_color, nullptr, 1.0F, m_config.material_id);

  submitter.mesh(
      getUnitCylinder(),
      cylinderBetween(ctx.model, top_end, nock, m_config.string_radius),
      m_config.string_color, nullptr, 1.0F, m_config.material_id);
  submitter.mesh(
      getUnitCylinder(),
      cylinderBetween(ctx.model, nock, bot_end, m_config.string_radius),
      m_config.string_color, nullptr, 1.0F, m_config.material_id);

  bool const is_bow_attacking =
      anim.inputs.is_attacking && !anim.inputs.is_melee;
  if (is_bow_attacking) {
    submitter.mesh(
        getUnitCylinder(),
        cylinderBetween(ctx.model, frames.hand_l.origin, nock, 0.0045F),
        m_config.string_color * 0.9F, nullptr, 1.0F, m_config.material_id);
  }

  float attack_phase = 0.0F;
  if (is_bow_attacking) {
    attack_phase =
        std::fmod(anim.inputs.time * ARCHER_INV_ATTACK_CYCLE_TIME, 1.0F);
  }

  constexpr float k_attack_arrow_window_end = 0.52F;
  bool const attack_window_active =
      is_bow_attacking &&
      (attack_phase >= 0.0F && attack_phase < k_attack_arrow_window_end);

  auto arrow_visible = [this, is_bow_attacking,
                        attack_window_active]() -> bool {
    switch (m_config.arrow_visibility) {
    case ArrowVisibility::Hidden:
      return false;
    case ArrowVisibility::AttackCycleOnly:
      return attack_window_active;
    case ArrowVisibility::IdleAndAttackCycle:
      if (!is_bow_attacking) {
        return true;
      }
      return attack_window_active;
    }
    return attack_window_active;
  };

  bool const show_arrow = arrow_visible();

  if (show_arrow) {
    QVector3D const tail = nock - forward * 0.06F;
    QVector3D const tip = tail + forward * 0.90F;

    submitter.mesh(getUnitCylinder(),
                   cylinderBetween(ctx.model, tail, tip, 0.018F), palette.wood,
                   nullptr, 1.0F, m_config.material_id);

    QVector3D const head_base = tip - forward * 0.10F;
    submitter.mesh(getUnitCone(), coneFromTo(ctx.model, head_base, tip, 0.05F),
                   m_config.metal_color, nullptr, 1.0F, m_config.material_id);

    QVector3D const f1b = tail - forward * 0.02F;
    QVector3D const f1a = f1b - forward * 0.06F;
    QVector3D const f2b = tail + forward * 0.02F;
    QVector3D const f2a = f2b + forward * 0.06F;

    submitter.mesh(getUnitCone(), coneFromTo(ctx.model, f1b, f1a, 0.04F),
                   m_config.fletching_color, nullptr, 1.0F,
                   m_config.material_id);
    submitter.mesh(getUnitCone(), coneFromTo(ctx.model, f2a, f2b, 0.04F),
                   m_config.fletching_color, nullptr, 1.0F,
                   m_config.material_id);
  }
}

} // namespace Render::GL
