#include "knight_renderer.h"
#include "../../../../game/core/component.h"
#include "../../../geom/math_utils.h"
#include "../../../geom/transforms.h"
#include "../../../gl/backend.h"
#include "../../../gl/primitives.h"
#include "../../../gl/shader.h"
#include "../../../humanoid/rig.h"
#include "../../../humanoid/style_palette.h"
#include "../../../humanoid_math.h"
#include "../../../humanoid_specs.h"
#include "../../../palette.h"
#include "../../../scene_renderer.h"
#include "../../../submitter.h"
#include "../../registry.h"
#include "../../renderer_constants.h"
#include "knight_style.h"
#include <numbers>
#include <qmatrix4x4.h>
#include <qstringliteral.h>
#include <qvectornd.h>
#include <unordered_map>

#include <QMatrix4x4>
#include <QString>
#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace Render::GL::Carthage {

namespace {

constexpr std::string_view k_knight_default_style_key = "default";
constexpr float k_knight_team_mix_weight = 0.6F;
constexpr float k_knight_style_mix_weight = 0.4F;

auto knight_style_registry()
    -> std::unordered_map<std::string, KnightStyleConfig> & {
  static std::unordered_map<std::string, KnightStyleConfig> styles;
  return styles;
}

void ensure_knight_styles_registered() {
  static const bool registered = []() {
    register_carthage_knight_style();
    return true;
  }();
  (void)registered;
}

} // namespace

void register_knight_style(const std::string &nation_id,
                           const KnightStyleConfig &style) {
  knight_style_registry()[nation_id] = style;
}

using Render::Geom::clamp01;
using Render::Geom::clampf;
using Render::Geom::coneFromTo;
using Render::Geom::cylinderBetween;
using Render::Geom::easeInOutCubic;
using Render::Geom::lerp;
using Render::Geom::nlerp;
using Render::Geom::smoothstep;
using Render::Geom::sphereAt;
using Render::GL::Humanoid::mix_palette_color;
using Render::GL::Humanoid::saturate_color;

struct KnightExtras {
  QVector3D metalColor;
  QVector3D shieldColor;
  QVector3D shieldTrimColor;
  float swordLength = 0.80F;
  float swordWidth = 0.065F;
  float shieldRadius = 0.18F;
  float shieldAspect = 1.0F;

  float guard_half_width = 0.12F;
  float handleRadius = 0.016F;
  float pommelRadius = 0.045F;
  float bladeRicasso = 0.16F;
  float bladeTaperBias = 0.65F;
  bool shieldCrossDecal = false;
  bool hasScabbard = true;
};

class KnightRenderer : public HumanoidRendererBase {
public:
  auto getProportionScaling() const -> QVector3D override {
    return {1.40F, 1.05F, 1.10F};
  }

private:
  mutable std::unordered_map<uint32_t, KnightExtras> m_extrasCache;

public:
  void getVariant(const DrawContext &ctx, uint32_t seed,
                  HumanoidVariant &v) const override {
    QVector3D const team_tint = resolveTeamTint(ctx);
    v.palette = makeHumanoidPalette(team_tint, seed);
    auto const &style = resolve_style(ctx);
    apply_palette_overrides(style, team_tint, v);
  }

  void customizePose(const DrawContext &,
                     const HumanoidAnimationContext &anim_ctx, uint32_t seed,
                     HumanoidPose &pose) const override {
    using HP = HumanProportions;

    const AnimationInputs &anim = anim_ctx.inputs;

    float const arm_height_jitter = (hash_01(seed ^ 0xABCDU) - 0.5F) * 0.03F;
    float const arm_asymmetry = (hash_01(seed ^ 0xDEF0U) - 0.5F) * 0.04F;

    if (anim.is_attacking && anim.isMelee) {
      float const attack_phase =
          std::fmod(anim.time * KNIGHT_INV_ATTACK_CYCLE_TIME, 1.0F);

      QVector3D const rest_pos(0.20F, HP::SHOULDER_Y + 0.05F, 0.15F);
      QVector3D const prepare_pos(0.26F, HP::HEAD_TOP_Y + 0.18F, -0.06F);
      QVector3D const raised_pos(0.25F, HP::HEAD_TOP_Y + 0.22F, 0.02F);
      QVector3D const strike_pos(0.30F, HP::WAIST_Y - 0.05F, 0.50F);
      QVector3D const recover_pos(0.22F, HP::SHOULDER_Y + 0.02F, 0.22F);

      if (attack_phase < 0.18F) {

        float const t = easeInOutCubic(attack_phase / 0.18F);
        pose.hand_r = rest_pos * (1.0F - t) + prepare_pos * t;
        pose.handL =
            QVector3D(-0.21F, HP::SHOULDER_Y - 0.02F - 0.03F * t, 0.15F);
      } else if (attack_phase < 0.32F) {

        float const t = easeInOutCubic((attack_phase - 0.18F) / 0.14F);
        pose.hand_r = prepare_pos * (1.0F - t) + raised_pos * t;
        pose.handL = QVector3D(-0.21F, HP::SHOULDER_Y - 0.05F, 0.17F);
      } else if (attack_phase < 0.52F) {

        float t = (attack_phase - 0.32F) / 0.20F;
        t = t * t * t;
        pose.hand_r = raised_pos * (1.0F - t) + strike_pos * t;
        pose.handL =
            QVector3D(-0.21F, HP::SHOULDER_Y - 0.03F * (1.0F - 0.5F * t),
                      0.17F + 0.20F * t);
      } else if (attack_phase < 0.72F) {

        float const t = easeInOutCubic((attack_phase - 0.52F) / 0.20F);
        pose.hand_r = strike_pos * (1.0F - t) + recover_pos * t;
        pose.handL = QVector3D(-0.20F, HP::SHOULDER_Y - 0.015F * (1.0F - t),
                               lerp(0.37F, 0.20F, t));
      } else {

        float const t = smoothstep(0.72F, 1.0F, attack_phase);
        pose.hand_r = recover_pos * (1.0F - t) + rest_pos * t;
        pose.handL = QVector3D(-0.20F - 0.02F * (1.0F - t),
                               HP::SHOULDER_Y + arm_height_jitter * (1.0F - t),
                               lerp(0.20F, 0.15F, t));
      }
    } else {

      pose.hand_r =
          QVector3D(0.30F + arm_asymmetry,
                    HP::SHOULDER_Y - 0.02F + arm_height_jitter, 0.35F);
      pose.handL = QVector3D(-0.22F - 0.5F * arm_asymmetry,
                             HP::SHOULDER_Y + 0.5F * arm_height_jitter, 0.18F);
    }
  }

  void addAttachments(const DrawContext &ctx, const HumanoidVariant &v,
                      const HumanoidPose &pose,
                      const HumanoidAnimationContext &anim_ctx,
                      ISubmitter &out) const override {
    const AnimationInputs &anim = anim_ctx.inputs;
    uint32_t const seed = reinterpret_cast<uintptr_t>(ctx.entity) & 0xFFFFFFFFU;
    auto const &style = resolve_style(ctx);
    QVector3D const team_tint = resolveTeamTint(ctx);

    KnightExtras extras;
    auto it = m_extrasCache.find(seed);
    if (it != m_extrasCache.end()) {
      extras = it->second;
    } else {
      extras = computeKnightExtras(seed, v);
      apply_extras_overrides(style, team_tint, v, extras);
      m_extrasCache[seed] = extras;

      if (m_extrasCache.size() > MAX_EXTRAS_CACHE_SIZE) {
        m_extrasCache.clear();
      }
    }
    apply_extras_overrides(style, team_tint, v, extras);

    bool const is_attacking = anim.is_attacking && anim.isMelee;
    float attack_phase = 0.0F;
    if (is_attacking) {
      attack_phase = std::fmod(anim.time * KNIGHT_INV_ATTACK_CYCLE_TIME, 1.0F);
    }

    drawSword(ctx, pose, v, extras, is_attacking, attack_phase, out);
    drawShield(ctx, pose, v, extras, out);

    if (!is_attacking && extras.hasScabbard) {
      drawScabbard(ctx, pose, v, extras, out);
    }
  }

  void drawHelmet(const DrawContext &ctx, const HumanoidVariant &v,
                  const HumanoidPose &pose, ISubmitter &out) const override {
    using HP = HumanProportions;

    auto ring = [&](const QVector3D &center, float r, float h,
                    const QVector3D &col) {
      QVector3D const a = center + QVector3D(0, h * 0.5F, 0);
      QVector3D const b = center - QVector3D(0, h * 0.5F, 0);
      out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, a, b, r), col,
               nullptr, 1.0F);
    };

    QVector3D const steel_color =
        v.palette.metal * QVector3D(0.95F, 0.96F, 1.0F);

    float helm_r = pose.headR * 1.15F;
    QVector3D const helm_bot(0, pose.headPos.y() - pose.headR * 0.20F, 0);
    QVector3D const helm_top(0, pose.headPos.y() + pose.headR * 1.40F, 0);

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, helm_bot, helm_top, helm_r),
             steel_color, nullptr, 1.0F);

    QVector3D const cap_top(0, pose.headPos.y() + pose.headR * 1.48F, 0);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, helm_top, cap_top, helm_r * 0.98F),
             steel_color * 1.05F, nullptr, 1.0F);

    ring(QVector3D(0, pose.headPos.y() + pose.headR * 1.25F, 0), helm_r * 1.02F,
         0.015F, steel_color * 1.08F);
    ring(QVector3D(0, pose.headPos.y() + pose.headR * 0.50F, 0), helm_r * 1.02F,
         0.015F, steel_color * 1.08F);
    ring(QVector3D(0, pose.headPos.y() - pose.headR * 0.05F, 0), helm_r * 1.02F,
         0.015F, steel_color * 1.08F);

    float const visor_y = pose.headPos.y() + pose.headR * 0.15F;
    float const visor_z = helm_r * 0.72F;

    QVector3D const visor_hl(-helm_r * 0.35F, visor_y, visor_z);
    QVector3D const visor_hr(helm_r * 0.35F, visor_y, visor_z);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, visor_hl, visor_hr, 0.012F),
             QVector3D(0.1F, 0.1F, 0.1F), nullptr, 1.0F);

    QVector3D const visor_vt(0, visor_y + helm_r * 0.25F, visor_z);
    QVector3D const visor_vb(0, visor_y - helm_r * 0.25F, visor_z);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, visor_vb, visor_vt, 0.012F),
             QVector3D(0.1F, 0.1F, 0.1F), nullptr, 1.0F);

    auto draw_breathing_hole = [&](float x, float y) {
      QVector3D const pos(x, pose.headPos.y() + y, helm_r * 0.70F);
      QMatrix4x4 m = ctx.model;
      m.translate(pos);
      m.scale(0.010F);
      out.mesh(getUnitSphere(), m, QVector3D(0.1F, 0.1F, 0.1F), nullptr, 1.0F);
    };

    for (int i = 0; i < 4; ++i) {
      draw_breathing_hole(helm_r * 0.50F, pose.headR * (0.05F - i * 0.10F));
    }

    for (int i = 0; i < 4; ++i) {
      draw_breathing_hole(-helm_r * 0.50F, pose.headR * (0.05F - i * 0.10F));
    }

    QVector3D const cross_center(0, pose.headPos.y() + pose.headR * 0.60F,
                                 helm_r * 0.75F);
    QVector3D const brass_color = v.palette.metal * QVector3D(1.3F, 1.1F, 0.7F);

    QVector3D const cross_h1 = cross_center + QVector3D(-0.04F, 0, 0);
    QVector3D const cross_h2 = cross_center + QVector3D(0.04F, 0, 0);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, cross_h1, cross_h2, 0.008F),
             brass_color, nullptr, 1.0F);

    QVector3D const cross_v1 = cross_center + QVector3D(0, -0.04F, 0);
    QVector3D const cross_v2 = cross_center + QVector3D(0, 0.04F, 0);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, cross_v1, cross_v2, 0.008F),
             brass_color, nullptr, 1.0F);
  }

  void draw_armorOverlay(const DrawContext &ctx, const HumanoidVariant &v,
                         const HumanoidPose &pose, float y_top_cover,
                         float torso_r, float, float upper_arm_r,
                         const QVector3D &right_axis,
                         ISubmitter &out) const override {
    using HP = HumanProportions;

    auto ring = [&](const QVector3D &center, float r, float h,
                    const QVector3D &col) {
      QVector3D const a = center + QVector3D(0, h * 0.5F, 0);
      QVector3D const b = center - QVector3D(0, h * 0.5F, 0);
      out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, a, b, r), col,
               nullptr, 1.0F);
    };

    QVector3D steel_color = v.palette.metal * QVector3D(0.95F, 0.96F, 1.0F);
    QVector3D const dark_steel = steel_color * 0.85F;
    QVector3D brass_color = v.palette.metal * QVector3D(1.3F, 1.1F, 0.7F);

    QVector3D const bp_top(0, y_top_cover + 0.02F, 0);
    QVector3D const bp_mid(0, (y_top_cover + HP::WAIST_Y) * 0.5F + 0.04F, 0);
    QVector3D const bp_bot(0, HP::WAIST_Y + 0.06F, 0);
    float const r_chest = torso_r * 1.18F;
    float const r_waist = torso_r * 1.14F;

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, bp_top, bp_mid, r_chest), steel_color,
             nullptr, 1.0F);

    QVector3D const bp_mid_low(0, (bp_mid.y() + bp_bot.y()) * 0.5F, 0);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, bp_mid, bp_mid_low, r_chest * 0.98F),
             steel_color * 0.99F, nullptr, 1.0F);

    out.mesh(getUnitCone(), coneFromTo(ctx.model, bp_bot, bp_mid_low, r_waist),
             steel_color * 0.98F, nullptr, 1.0F);

    auto draw_rivet = [&](const QVector3D &pos) {
      QMatrix4x4 m = ctx.model;
      m.translate(pos);
      m.scale(0.012F);
      out.mesh(getUnitSphere(), m, brass_color, nullptr, 1.0F);
    };

    for (int i = 0; i < 8; ++i) {
      float const angle = (i / 8.0F) * 2.0F * std::numbers::pi_v<float>;
      float const x = r_chest * std::sin(angle) * 0.95F;
      float const z = r_chest * std::cos(angle) * 0.95F;
      draw_rivet(QVector3D(x, bp_mid.y() + 0.08F, z));
    }

    auto draw_pauldron = [&](const QVector3D &shoulder,
                             const QVector3D &outward) {
      for (int i = 0; i < 4; ++i) {
        float const seg_y = shoulder.y() + 0.04F - i * 0.045F;
        float const seg_r = upper_arm_r * (2.5F - i * 0.12F);
        QVector3D seg_pos = shoulder + outward * (0.02F + i * 0.008F);
        seg_pos.setY(seg_y);

        out.mesh(getUnitSphere(), sphereAt(ctx.model, seg_pos, seg_r),
                 i == 0 ? steel_color * 1.05F
                        : steel_color * (1.0F - i * 0.03F),
                 nullptr, 1.0F);

        if (i < 3) {
          draw_rivet(seg_pos + QVector3D(0, 0.015F, 0.03F));
        }
      }
    };

    draw_pauldron(pose.shoulderL, -right_axis);
    draw_pauldron(pose.shoulderR, right_axis);

    auto draw_arm_plate = [&](const QVector3D &shoulder,
                              const QVector3D &elbow) {
      QVector3D dir = (elbow - shoulder);
      float const len = dir.length();
      if (len < 1e-5F) {
        return;
      }
      dir /= len;

      for (int i = 0; i < 3; ++i) {
        float const t0 = 0.10F + i * 0.25F;
        float const t1 = t0 + 0.22F;
        QVector3D const a = shoulder + dir * (t0 * len);
        QVector3D const b = shoulder + dir * (t1 * len);
        float const r = upper_arm_r * (1.32F - i * 0.04F);

        out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, a, b, r),
                 steel_color * (0.98F - i * 0.02F), nullptr, 1.0F);

        if (i < 2) {
          draw_rivet(b);
        }
      }
    };

    draw_arm_plate(pose.shoulderL, pose.elbowL);
    draw_arm_plate(pose.shoulderR, pose.elbowR);

    for (int i = 0; i < 4; ++i) {
      float const y0 = HP::WAIST_Y + 0.04F - i * 0.038F;
      float const y1 = y0 - 0.032F;
      float const r0 = r_waist * (1.06F + i * 0.025F);
      out.mesh(
          getUnitCone(),
          coneFromTo(ctx.model, QVector3D(0, y0, 0), QVector3D(0, y1, 0), r0),
          steel_color * (0.96F - i * 0.02F), nullptr, 1.0F);

      if (i < 3) {
        draw_rivet(QVector3D(r0 * 0.90F, y0 - 0.016F, 0));
      }
    }

    QVector3D const gorget_top(0, y_top_cover + 0.025F, 0);
    QVector3D const gorget_bot(0, y_top_cover - 0.012F, 0);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, gorget_bot, gorget_top,
                             HP::NECK_RADIUS * 2.6F),
             steel_color * 1.08F, nullptr, 1.0F);

    ring(gorget_top, HP::NECK_RADIUS * 2.62F, 0.010F, brass_color);
  }

  void drawShoulderDecorations(const DrawContext &ctx, const HumanoidVariant &v,
                               const HumanoidPose &pose, float y_top_cover,
                               float y_neck, const QVector3D &right_axis,
                               ISubmitter &out) const override {
    using HP = HumanProportions;

    QVector3D brass_color = v.palette.metal * QVector3D(1.3F, 1.1F, 0.7F);
    QVector3D const chainmail_color =
        v.palette.metal * QVector3D(0.85F, 0.88F, 0.92F);
    QVector3D mantling_color = v.palette.cloth;

    for (int i = 0; i < 5; ++i) {
      float const y = y_neck - i * 0.022F;
      float const r = HP::NECK_RADIUS * (1.85F + i * 0.08F);
      QVector3D const ring_pos(0, y, 0);
      QVector3D const a = ring_pos + QVector3D(0, 0.010F, 0);
      QVector3D const b = ring_pos - QVector3D(0, 0.010F, 0);
      out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, a, b, r),
               chainmail_color * (1.0F - i * 0.04F), nullptr, 1.0F);
    }

    QVector3D const helm_top(0, HP::HEAD_TOP_Y - HP::HEAD_RADIUS * 0.15F, 0);
    QMatrix4x4 crest_base = ctx.model;
    crest_base.translate(helm_top);
    crest_base.scale(0.025F, 0.015F, 0.025F);
    out.mesh(getUnitSphere(), crest_base, brass_color * 1.2F, nullptr, 1.0F);

    auto draw_stud = [&](const QVector3D &pos) {
      QMatrix4x4 m = ctx.model;
      m.translate(pos);
      m.scale(0.008F);
      out.mesh(getUnitSphere(), m, brass_color * 1.3F, nullptr, 1.0F);
    };

    draw_stud(helm_top + QVector3D(0.020F, 0, 0.020F));
    draw_stud(helm_top + QVector3D(-0.020F, 0, 0.020F));
    draw_stud(helm_top + QVector3D(0.020F, 0, -0.020F));
    draw_stud(helm_top + QVector3D(-0.020F, 0, -0.020F));

    auto draw_mantling = [&](const QVector3D &startPos,
                             const QVector3D &direction) {
      QVector3D current_pos = startPos;
      for (int i = 0; i < 4; ++i) {
        float const seg_len = 0.035F - i * 0.005F;
        float const seg_r = 0.020F - i * 0.003F;
        QVector3D next_pos = current_pos + direction * seg_len;
        next_pos.setY(next_pos.y() - 0.025F);

        out.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, current_pos, next_pos, seg_r),
                 mantling_color * (1.1F - i * 0.06F), nullptr, 1.0F);

        current_pos = next_pos;
      }
    };

    QVector3D const mantling_start(0, HP::CHIN_Y + HP::HEAD_RADIUS * 0.25F, 0);
    draw_mantling(mantling_start + right_axis * HP::HEAD_RADIUS * 0.95F,
                  right_axis * 0.5F + QVector3D(0, -0.1F, -0.3F));
    draw_mantling(mantling_start - right_axis * HP::HEAD_RADIUS * 0.95F,
                  -right_axis * 0.5F + QVector3D(0, -0.1F, -0.3F));

    auto draw_pauldron_rivet = [&](const QVector3D &shoulder,
                                   const QVector3D &outward) {
      for (int i = 0; i < 3; ++i) {
        float const seg_y = shoulder.y() + 0.025F - i * 0.045F;
        QVector3D rivet_pos = shoulder + outward * (0.04F + i * 0.008F);
        rivet_pos.setY(seg_y);

        draw_stud(rivet_pos);
      }
    };

    draw_pauldron_rivet(pose.shoulderL, -right_axis);
    draw_pauldron_rivet(pose.shoulderR, right_axis);

    QVector3D const gorget_top(0, y_top_cover + 0.045F, 0);
    for (int i = 0; i < 6; ++i) {
      float const angle = (i / 6.0F) * 2.0F * std::numbers::pi_v<float>;
      float const x = HP::NECK_RADIUS * 2.58F * std::sin(angle);
      float const z = HP::NECK_RADIUS * 2.58F * std::cos(angle);
      draw_stud(gorget_top + QVector3D(x, 0, z));
    }

    QVector3D const belt_center(0, HP::WAIST_Y + 0.03F,
                                HP::TORSO_BOT_R * 1.15F);
    QMatrix4x4 buckle = ctx.model;
    buckle.translate(belt_center);
    buckle.scale(0.035F, 0.025F, 0.012F);
    out.mesh(getUnitSphere(), buckle, brass_color * 1.25F, nullptr, 1.0F);

    QVector3D const buckle_h1 = belt_center + QVector3D(-0.025F, 0, 0.005F);
    QVector3D const buckle_h2 = belt_center + QVector3D(0.025F, 0, 0.005F);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, buckle_h1, buckle_h2, 0.006F),
             brass_color * 1.4F, nullptr, 1.0F);

    QVector3D const buckle_v1 = belt_center + QVector3D(0, -0.018F, 0.005F);
    QVector3D const buckle_v2 = belt_center + QVector3D(0, 0.018F, 0.005F);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, buckle_v1, buckle_v2, 0.006F),
             brass_color * 1.4F, nullptr, 1.0F);
  }

private:
  static auto computeKnightExtras(uint32_t seed,
                                  const HumanoidVariant &v) -> KnightExtras {
    KnightExtras e;

    e.metalColor = QVector3D(0.72F, 0.73F, 0.78F);

    float const shield_hue = hash_01(seed ^ 0x12345U);
    if (shield_hue < 0.45F) {
      e.shieldColor = v.palette.cloth * 1.10F;
    } else if (shield_hue < 0.90F) {
      e.shieldColor = v.palette.leather * 1.25F;
    } else {

      e.shieldColor = e.metalColor * 0.95F;
    }

    e.swordLength = 0.80F + (hash_01(seed ^ 0xABCDU) - 0.5F) * 0.16F;
    e.swordWidth = 0.060F + (hash_01(seed ^ 0x7777U) - 0.5F) * 0.010F;
    e.shieldRadius = 0.16F + (hash_01(seed ^ 0xDEF0U) - 0.5F) * 0.04F;

    e.guard_half_width = 0.120F + (hash_01(seed ^ 0x3456U) - 0.5F) * 0.020F;
    e.handleRadius = 0.016F + (hash_01(seed ^ 0x88AAU) - 0.5F) * 0.003F;
    e.pommelRadius = 0.045F + (hash_01(seed ^ 0x19C3U) - 0.5F) * 0.006F;

    e.bladeRicasso =
        clampf(0.14F + (hash_01(seed ^ 0xBEEFU) - 0.5F) * 0.04F, 0.10F, 0.20F);
    e.bladeTaperBias = clamp01(0.6F + (hash_01(seed ^ 0xFACEU) - 0.5F) * 0.2F);

    e.shieldCrossDecal = (hash_01(seed ^ 0xA11CU) > 0.55F);
    e.hasScabbard = (hash_01(seed ^ 0x5CABU) > 0.15F);
    e.shieldTrimColor = e.metalColor * 0.95F;
    e.shieldAspect = 1.0F;
    return e;
  }

  static void drawSword(const DrawContext &ctx, const HumanoidPose &pose,
                        const HumanoidVariant &v, const KnightExtras &extras,
                        bool is_attacking, float attack_phase,
                        ISubmitter &out) {
    QVector3D const grip_pos = pose.hand_r;

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
    QVector3D const blade_tip = grip_pos + sword_dir * extras.swordLength;

    out.mesh(
        getUnitCylinder(),
        cylinderBetween(ctx.model, handle_end, blade_base, extras.handleRadius),
        v.palette.leather, nullptr, 1.0F);

    QVector3D const guard_center = blade_base;
    float const gw = extras.guard_half_width;

    QVector3D guard_right =
        QVector3D::crossProduct(QVector3D(0, 1, 0), sword_dir);
    if (guard_right.lengthSquared() < 1e-6F) {
      guard_right = QVector3D::crossProduct(QVector3D(1, 0, 0), sword_dir);
    }
    guard_right.normalize();

    QVector3D const guard_l = guard_center - guard_right * gw;
    QVector3D const guard_r = guard_center + guard_right * gw;

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, guard_l, guard_r, 0.014F),
             extras.metalColor, nullptr, 1.0F);

    QMatrix4x4 gl = ctx.model;
    gl.translate(guard_l);
    gl.scale(0.018F);
    out.mesh(getUnitSphere(), gl, extras.metalColor, nullptr, 1.0F);
    QMatrix4x4 gr = ctx.model;
    gr.translate(guard_r);
    gr.scale(0.018F);
    out.mesh(getUnitSphere(), gr, extras.metalColor, nullptr, 1.0F);

    float const l = extras.swordLength;
    float const base_w = extras.swordWidth;
    float blade_thickness = base_w * 0.15F;

    float const ricasso_len = clampf(extras.bladeRicasso, 0.10F, l * 0.30F);
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

      out.mesh(getUnitCylinder(),
               cylinderBetween(ctx.model, start, end, blade_thickness), color,
               nullptr, 1.0F);

      out.mesh(getUnitCylinder(),
               cylinderBetween(ctx.model, start + right * offset,
                               end + right * offset, blade_thickness * 0.8F),
               color * 0.92F, nullptr, 1.0F);

      out.mesh(getUnitCylinder(),
               cylinderBetween(ctx.model, start - right * offset,
                               end - right * offset, blade_thickness * 0.8F),
               color * 0.92F, nullptr, 1.0F);
    };

    draw_flat_section(blade_base, ricasso_end, base_w, extras.metalColor);

    draw_flat_section(ricasso_end, tip_start, mid_w, extras.metalColor);

    int const tip_segments = 3;
    for (int i = 0; i < tip_segments; ++i) {
      float const t0 = (float)i / tip_segments;
      float const t1 = (float)(i + 1) / tip_segments;
      QVector3D const seg_start =
          tip_start + sword_dir * ((blade_tip - tip_start).length() * t0);
      QVector3D const seg_end =
          tip_start + sword_dir * ((blade_tip - tip_start).length() * t1);
      float const w = lerp(mid_w, tip_w, t1);
      out.mesh(getUnitCylinder(),
               cylinderBetween(ctx.model, seg_start, seg_end, blade_thickness),
               extras.metalColor * (1.0F - i * 0.03F), nullptr, 1.0F);
    }

    QVector3D const fuller_start =
        blade_base + sword_dir * (ricasso_len + 0.02F);
    QVector3D const fuller_end =
        blade_base + sword_dir * (tip_start_dist - 0.06F);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, fuller_start, fuller_end,
                             blade_thickness * 0.6F),
             extras.metalColor * 0.65F, nullptr, 1.0F);

    QVector3D const pommel = handle_end - sword_dir * 0.02F;
    QMatrix4x4 pommel_mat = ctx.model;
    pommel_mat.translate(pommel);
    pommel_mat.scale(extras.pommelRadius);
    out.mesh(getUnitSphere(), pommel_mat, extras.metalColor, nullptr, 1.0F);

    if (is_attacking && attack_phase >= 0.32F && attack_phase < 0.56F) {
      float const t = (attack_phase - 0.32F) / 0.24F;
      float const alpha = clamp01(0.35F * (1.0F - t));
      QVector3D const trail_start = blade_base - sword_dir * 0.05F;
      QVector3D const trail_end = blade_base - sword_dir * (0.28F + 0.15F * t);
      out.mesh(getUnitCone(),
               coneFromTo(ctx.model, trail_end, trail_start, base_w * 0.9F),
               extras.metalColor * 0.9F, nullptr, alpha);
    }
  }

  static void drawShield(const DrawContext &ctx, const HumanoidPose &pose,
                         const HumanoidVariant &v, const KnightExtras &extras,
                         ISubmitter &out) {

    constexpr float k_scale_factor = 2.5F;
    constexpr float k_shield_yaw_degrees = -70.0F;

    QMatrix4x4 rot;
    rot.rotate(k_shield_yaw_degrees, 0.0F, 1.0F, 0.0F);

    const QVector3D n = rot.map(QVector3D(0.0F, 0.0F, 1.0F));
    const QVector3D axis_x = rot.map(QVector3D(1.0F, 0.0F, 0.0F));
    const QVector3D axis_y = rot.map(QVector3D(0.0F, 1.0F, 0.0F));

    float const base_extent = extras.shieldRadius * k_scale_factor;
    float const shield_width = base_extent;
    float const shield_height = base_extent * extras.shieldAspect;
    float const min_extent = std::min(shield_width, shield_height);

    QVector3D shield_center = pose.handL + axis_x * (-shield_width * 0.35F) +
                              axis_y * (-0.05F) + n * (0.06F);

    const float plate_half = 0.0015F;
    const float plate_full = plate_half * 2.0F;

    {
      QMatrix4x4 m = ctx.model;
      m.translate(shield_center + n * plate_half);
      m.rotate(k_shield_yaw_degrees, 0.0F, 1.0F, 0.0F);
      m.scale(shield_width, shield_height, plate_full);
      out.mesh(getUnitCylinder(), m, extras.shieldColor, nullptr, 1.0F);
    }

    {
      QMatrix4x4 m = ctx.model;
      m.translate(shield_center - n * plate_half);
      m.rotate(k_shield_yaw_degrees, 0.0F, 1.0F, 0.0F);
      m.scale(shield_width * 0.985F, shield_height * 0.985F, plate_full);
      out.mesh(getUnitCylinder(), m, v.palette.leather * 0.8F, nullptr, 1.0F);
    }

    auto draw_ring_rotated = [&](float width, float height, float thickness,
                                 const QVector3D &color) {
      constexpr int k_segments = 18;
      for (int i = 0; i < k_segments; ++i) {
        float const a0 =
            (float)i / k_segments * 2.0F * std::numbers::pi_v<float>;
        float const a1 =
            (float)(i + 1) / k_segments * 2.0F * std::numbers::pi_v<float>;

        QVector3D const v0(width * std::cos(a0), height * std::sin(a0), 0.0F);
        QVector3D const v1(width * std::cos(a1), height * std::sin(a1), 0.0F);

        QVector3D const p0 = shield_center + rot.map(v0);
        QVector3D const p1 = shield_center + rot.map(v1);

        out.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, p0, p1, thickness), color, nullptr,
                 1.0F);
      }
    };

    draw_ring_rotated(shield_width, shield_height, min_extent * 0.010F,
                      extras.shieldTrimColor * 0.95F);
    draw_ring_rotated(shield_width * 0.72F, shield_height * 0.72F,
                      min_extent * 0.006F, v.palette.leather * 0.90F);

    {
      QMatrix4x4 m = ctx.model;
      m.translate(shield_center + n * (0.02F * k_scale_factor));
      m.scale(0.045F * k_scale_factor);
      out.mesh(getUnitSphere(), m, extras.metalColor, nullptr, 1.0F);
    }

    {
      QVector3D const grip_a = shield_center - axis_x * 0.035F - n * 0.030F;
      QVector3D const grip_b = shield_center + axis_x * 0.035F - n * 0.030F;
      out.mesh(getUnitCylinder(),
               cylinderBetween(ctx.model, grip_a, grip_b, 0.010F),
               v.palette.leather, nullptr, 1.0F);
    }

    if (extras.shieldCrossDecal) {
      QVector3D const center_front =
          shield_center + n * (plate_full * 0.5F + 0.0015F);
      float const bar_radius = min_extent * 0.10F;

      QVector3D const top = center_front + axis_y * (shield_height * 0.90F);
      QVector3D const bot = center_front - axis_y * (shield_height * 0.90F);
      out.mesh(getUnitCylinder(),
               cylinderBetween(ctx.model, top, bot, bar_radius),
               extras.shieldTrimColor, nullptr, 1.0F);

      QVector3D const left = center_front - axis_x * (shield_width * 0.90F);
      QVector3D const right = center_front + axis_x * (shield_width * 0.90F);
      out.mesh(getUnitCylinder(),
               cylinderBetween(ctx.model, left, right, bar_radius),
               extras.shieldTrimColor, nullptr, 1.0F);
    }
  }

  static void drawScabbard(const DrawContext &ctx, const HumanoidPose &,
                           const HumanoidVariant &v, const KnightExtras &extras,
                           ISubmitter &out) {
    using HP = HumanProportions;

    QVector3D const hip(0.10F, HP::WAIST_Y - 0.04F, -0.02F);
    QVector3D const tip = hip + QVector3D(-0.05F, -0.22F, -0.12F);
    float const sheath_r = extras.swordWidth * 0.85F;

    out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, hip, tip, sheath_r),
             v.palette.leather * 0.9F, nullptr, 1.0F);

    out.mesh(getUnitCone(),
             coneFromTo(ctx.model, tip, tip + QVector3D(-0.02F, -0.02F, -0.02F),
                        sheath_r),
             extras.metalColor, nullptr, 1.0F);

    QVector3D const strap_a = hip + QVector3D(0.00F, 0.03F, 0.00F);
    QVector3D const belt = QVector3D(0.12F, HP::WAIST_Y + 0.01F, 0.02F);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, strap_a, belt, 0.006F),
             v.palette.leather, nullptr, 1.0F);
  }

  auto
  resolve_style(const DrawContext &ctx) const -> const KnightStyleConfig & {
    ensure_knight_styles_registered();
    auto &styles = knight_style_registry();
    std::string nation_id;
    if (ctx.entity != nullptr) {
      if (auto *unit =
              ctx.entity->getComponent<Engine::Core::UnitComponent>()) {
        nation_id = unit->nation_id;
      }
    }
    if (!nation_id.empty()) {
      auto it = styles.find(nation_id);
      if (it != styles.end()) {
        return it->second;
      }
    }
    auto it_default = styles.find(std::string(k_knight_default_style_key));
    if (it_default != styles.end()) {
      return it_default->second;
    }
    static const KnightStyleConfig k_empty{};
    return k_empty;
  }

public:
  auto resolve_shader_key(const DrawContext &ctx) const -> QString {
    const KnightStyleConfig &style = resolve_style(ctx);
    if (!style.shader_id.empty()) {
      return QString::fromStdString(style.shader_id);
    }
    return QStringLiteral("knight");
  }

private:
  void apply_palette_overrides(const KnightStyleConfig &style,
                               const QVector3D &team_tint,
                               HumanoidVariant &variant) const {
    auto apply_color = [&](const std::optional<QVector3D> &override_color,
                           QVector3D &target) {
      target = mix_palette_color(target, override_color, team_tint,
                                 k_knight_team_mix_weight,
                                 k_knight_style_mix_weight);
    };

    apply_color(style.cloth_color, variant.palette.cloth);
    apply_color(style.leather_color, variant.palette.leather);
    apply_color(style.leather_dark_color, variant.palette.leatherDark);
    apply_color(style.metal_color, variant.palette.metal);
  }

  void apply_extras_overrides(const KnightStyleConfig &style,
                              const QVector3D &team_tint,
                              const HumanoidVariant &variant,
                              KnightExtras &extras) const {
    extras.metalColor = saturate_color(variant.palette.metal);
    extras.shieldColor = saturate_color(extras.shieldColor);
    extras.shieldTrimColor = saturate_color(extras.shieldTrimColor);

    auto apply_shield_color =
        [&](const std::optional<QVector3D> &override_color, QVector3D &target) {
          target = mix_palette_color(target, override_color, team_tint,
                                     k_knight_team_mix_weight,
                                     k_knight_style_mix_weight);
        };

    apply_shield_color(style.shield_color, extras.shieldColor);
    apply_shield_color(style.shield_trim_color, extras.shieldTrimColor);

    if (style.shield_radius_scale) {
      extras.shieldRadius =
          std::max(0.10F, extras.shieldRadius * *style.shield_radius_scale);
    }
    if (style.shield_aspect_ratio) {
      extras.shieldAspect = std::max(0.40F, *style.shield_aspect_ratio);
    }
    if (style.has_scabbard) {
      extras.hasScabbard = *style.has_scabbard;
    }
    if (style.shield_cross_decal) {
      extras.shieldCrossDecal = *style.shield_cross_decal;
    }
  }
};

void registerKnightRenderer(Render::GL::EntityRendererRegistry &registry) {
  ensure_knight_styles_registered();
  static KnightRenderer const renderer;
  registry.registerRenderer(
      "troops/carthage/swordsman", [](const DrawContext &ctx, ISubmitter &out) {
        static KnightRenderer const static_renderer;
        Shader *knight_shader = nullptr;
        if (ctx.backend != nullptr) {
          QString shader_key = static_renderer.resolve_shader_key(ctx);
          knight_shader = ctx.backend->shader(shader_key);
          if (knight_shader == nullptr) {
            knight_shader = ctx.backend->shader(QStringLiteral("knight"));
          }
        }
        auto *scene_renderer = dynamic_cast<Renderer *>(&out);
        if ((scene_renderer != nullptr) && (knight_shader != nullptr)) {
          scene_renderer->setCurrentShader(knight_shader);
        }
        static_renderer.render(ctx, out);
        if (scene_renderer != nullptr) {
          scene_renderer->setCurrentShader(nullptr);
        }
      });
  registry.registerRenderer(
      "troops/carthage/knight", [](const DrawContext &ctx, ISubmitter &out) {
        static KnightRenderer const static_renderer;
        Shader *knight_shader = nullptr;
        if (ctx.backend != nullptr) {
          knight_shader = ctx.backend->shader(QStringLiteral("knight"));
        }
        auto *scene_renderer = dynamic_cast<Renderer *>(&out);
        if ((scene_renderer != nullptr) && (knight_shader != nullptr)) {
          scene_renderer->setCurrentShader(knight_shader);
        }
        static_renderer.render(ctx, out);
        if (scene_renderer != nullptr) {
          scene_renderer->setCurrentShader(nullptr);
        }
      });
}

} // namespace Render::GL::Carthage
