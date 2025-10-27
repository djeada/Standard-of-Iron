#include "archer_renderer.h"
#include "../../game/core/component.h"
#include "../../game/core/entity.h"
#include "../geom/math_utils.h"
#include "../geom/transforms.h"
#include "../gl/backend.h"
#include "../gl/primitives.h"
#include "../gl/shader.h"
#include "../humanoid_base.h"
#include "../humanoid_math.h"
#include "../humanoid_specs.h"
#include "../palette.h"
#include "../scene_renderer.h"
#include "../submitter.h"
#include "registry.h"
#include "renderer_constants.h"

#include <QMatrix4x4>
#include <QString>
#include <QVector3D>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <qmatrix4x4.h>
#include <qstringliteral.h>
#include <qvectornd.h>
#include <unordered_map>

namespace Render::GL {

using Render::Geom::clamp01;
using Render::Geom::clampf;
using Render::Geom::coneFromTo;
using Render::Geom::cylinderBetween;
using Render::Geom::sphereAt;

struct ArcherExtras {
  QVector3D stringCol;
  QVector3D fletch;
  QVector3D metalHead;
  float bowRodR = 0.035F;
  float stringR = 0.008F;
  float bowDepth = 0.25F;
  float bowX = 0.0F;
  float bowTopY{};
  float bowBotY{};
};

class ArcherRenderer : public HumanoidRendererBase {
public:
  auto getProportionScaling() const -> QVector3D override {

    return {0.94F, 1.01F, 0.96F};
  }

private:
  mutable std::unordered_map<uint32_t, ArcherExtras> m_extrasCache;

public:
  void getVariant(const DrawContext &ctx, uint32_t seed,
                  HumanoidVariant &v) const override {
    QVector3D const team_tint = resolveTeamTint(ctx);
    v.palette = makeHumanoidPalette(team_tint, seed);
  }

  void customizePose(const DrawContext &ctx, const AnimationInputs &anim,
                     uint32_t seed, HumanoidPose &pose) const override {
    using HP = HumanProportions;

    float const arm_height_jitter = (hash01(seed ^ 0xABCDU) - 0.5F) * 0.03F;
    float const arm_asymmetry = (hash01(seed ^ 0xDEF0U) - 0.5F) * 0.04F;

    float const bowX = 0.0F;

    if (anim.isInHoldMode || anim.isExitingHold) {

      float const t = anim.isInHoldMode ? 1.0F : (1.0F - anim.holdExitProgress);

      float const kneel_depth = 0.45F * t;

      float const pelvis_y = HP::WAIST_Y - kneel_depth;
      pose.pelvisPos.setY(pelvis_y);

      float const stance_narrow = 0.12F;

      float const left_knee_y = HP::GROUND_Y + 0.08F * t;
      float const left_knee_z = -0.05F * t;

      pose.knee_l = QVector3D(-stance_narrow, left_knee_y, left_knee_z);

      pose.footL = QVector3D(-stance_narrow - 0.03F, HP::GROUND_Y,
                             left_knee_z - HP::LOWER_LEG_LEN * 0.95F * t);

      float const right_foot_z = 0.30F * t;
      pose.foot_r = QVector3D(stance_narrow, HP::GROUND_Y + pose.footYOffset,
                              right_foot_z);

      float const right_knee_y = pelvis_y - 0.10F;
      float const right_knee_z = right_foot_z - 0.05F;

      pose.knee_r = QVector3D(stance_narrow, right_knee_y, right_knee_z);

      float const upper_body_drop = kneel_depth;

      pose.shoulderL.setY(HP::SHOULDER_Y - upper_body_drop);
      pose.shoulderR.setY(HP::SHOULDER_Y - upper_body_drop);
      pose.neck_base.setY(HP::NECK_BASE_Y - upper_body_drop);
      pose.headPos.setY((HP::HEAD_TOP_Y + HP::CHIN_Y) * 0.5F - upper_body_drop);

      float const forward_lean = 0.10F * t;
      pose.shoulderL.setZ(pose.shoulderL.z() + forward_lean);
      pose.shoulderR.setZ(pose.shoulderR.z() + forward_lean);
      pose.neck_base.setZ(pose.neck_base.z() + forward_lean * 0.8F);
      pose.headPos.setZ(pose.headPos.z() + forward_lean * 0.7F);

      QVector3D const hold_hand_l(bowX - 0.15F, pose.shoulderL.y() + 0.30F,
                                  0.55F);
      QVector3D const hold_hand_r(bowX + 0.12F, pose.shoulderR.y() + 0.15F,
                                  0.10F);
      QVector3D const normal_hand_l(bowX - 0.05F + arm_asymmetry,
                                    HP::SHOULDER_Y + 0.05F + arm_height_jitter,
                                    0.55F);
      QVector3D const normal_hand_r(
          0.15F - arm_asymmetry * 0.5F,
          HP::SHOULDER_Y + 0.15F + arm_height_jitter * 0.8F, 0.20F);

      pose.handL = normal_hand_l * (1.0F - t) + hold_hand_l * t;
      pose.hand_r = normal_hand_r * (1.0F - t) + hold_hand_r * t;
    } else {
      pose.handL = QVector3D(bowX - 0.05F + arm_asymmetry,
                             HP::SHOULDER_Y + 0.05F + arm_height_jitter, 0.55F);
      pose.hand_r =
          QVector3D(0.15F - arm_asymmetry * 0.5F,
                    HP::SHOULDER_Y + 0.15F + arm_height_jitter * 0.8F, 0.20F);
    }

    if (anim.is_attacking && !anim.isInHoldMode) {
      float const attack_phase =
          std::fmod(anim.time * ARCHER_INV_ATTACK_CYCLE_TIME, 1.0F);

      if (anim.isMelee) {
        QVector3D const rest_pos(0.25F, HP::SHOULDER_Y, 0.10F);
        QVector3D const raised_pos(0.30F, HP::HEAD_TOP_Y + 0.2F, -0.05F);
        QVector3D const strike_pos(0.35F, HP::WAIST_Y, 0.45F);

        if (attack_phase < 0.25F) {
          float t = attack_phase / 0.25F;
          t = t * t;
          pose.hand_r = rest_pos * (1.0F - t) + raised_pos * t;
          pose.handL = QVector3D(-0.15F, HP::SHOULDER_Y - 0.1F * t, 0.20F);
        } else if (attack_phase < 0.35F) {
          pose.hand_r = raised_pos;
          pose.handL = QVector3D(-0.15F, HP::SHOULDER_Y - 0.1F, 0.20F);
        } else if (attack_phase < 0.55F) {
          float t = (attack_phase - 0.35F) / 0.2F;
          t = t * t * t;
          pose.hand_r = raised_pos * (1.0F - t) + strike_pos * t;
          pose.handL =
              QVector3D(-0.15F, HP::SHOULDER_Y - 0.1F * (1.0F - t * 0.5F),
                        0.20F + 0.15F * t);
        } else {
          float t = (attack_phase - 0.55F) / 0.45F;
          t = 1.0F - (1.0F - t) * (1.0F - t);
          pose.hand_r = strike_pos * (1.0F - t) + rest_pos * t;
          pose.handL = QVector3D(-0.15F, HP::SHOULDER_Y - 0.05F * (1.0F - t),
                                 0.35F * (1.0F - t) + 0.20F * t);
        }
      } else {
        QVector3D const aim_pos(0.18F, HP::SHOULDER_Y + 0.18F, 0.35F);
        QVector3D const draw_pos(0.22F, HP::SHOULDER_Y + 0.10F, -0.30F);
        QVector3D const release_pos(0.18F, HP::SHOULDER_Y + 0.20F, 0.10F);

        if (attack_phase < 0.20F) {
          float t = attack_phase / 0.20F;
          t = t * t;
          pose.hand_r = aim_pos * (1.0F - t) + draw_pos * t;
          pose.handL = QVector3D(bowX - 0.05F, HP::SHOULDER_Y + 0.05F, 0.55F);

          float const shoulder_twist = t * 0.08F;
          pose.shoulderR.setY(pose.shoulderR.y() + shoulder_twist);
          pose.shoulderL.setY(pose.shoulderL.y() - shoulder_twist * 0.5F);
        } else if (attack_phase < 0.50F) {
          pose.hand_r = draw_pos;
          pose.handL = QVector3D(bowX - 0.05F, HP::SHOULDER_Y + 0.05F, 0.55F);

          float const shoulder_twist = 0.08F;
          pose.shoulderR.setY(pose.shoulderR.y() + shoulder_twist);
          pose.shoulderL.setY(pose.shoulderL.y() - shoulder_twist * 0.5F);
        } else if (attack_phase < 0.58F) {
          float t = (attack_phase - 0.50F) / 0.08F;
          t = t * t * t;
          pose.hand_r = draw_pos * (1.0F - t) + release_pos * t;
          pose.handL = QVector3D(bowX - 0.05F, HP::SHOULDER_Y + 0.05F, 0.55F);

          float const shoulder_twist = 0.08F * (1.0F - t * 0.6F);
          pose.shoulderR.setY(pose.shoulderR.y() + shoulder_twist);
          pose.shoulderL.setY(pose.shoulderL.y() - shoulder_twist * 0.5F);

          pose.headPos.setZ(pose.headPos.z() - t * 0.04F);
        } else {
          float t = (attack_phase - 0.58F) / 0.42F;
          t = 1.0F - (1.0F - t) * (1.0F - t);
          pose.hand_r = release_pos * (1.0F - t) + aim_pos * t;
          pose.handL = QVector3D(bowX - 0.05F, HP::SHOULDER_Y + 0.05F, 0.55F);

          float const shoulder_twist = 0.08F * 0.4F * (1.0F - t);
          pose.shoulderR.setY(pose.shoulderR.y() + shoulder_twist);
          pose.shoulderL.setY(pose.shoulderL.y() - shoulder_twist * 0.5F);

          pose.headPos.setZ(pose.headPos.z() - 0.04F * (1.0F - t));
        }
      }
    }

    QVector3D right_axis = pose.shoulderR - pose.shoulderL;
    right_axis.setY(0.0F);
    if (right_axis.lengthSquared() < 1e-8F) {
      right_axis = QVector3D(1, 0, 0);
    }
    right_axis.normalize();
    QVector3D const outward_l = -right_axis;
    QVector3D const outward_r = right_axis;

    pose.elbowL = elbowBendTorso(pose.shoulderL, pose.handL, outward_l, 0.45F,
                                 0.15F, -0.08F, +1.0F);
    pose.elbowR = elbowBendTorso(pose.shoulderR, pose.hand_r, outward_r, 0.48F,
                                 0.12F, 0.02F, +1.0F);
  }

  void addAttachments(const DrawContext &ctx, const HumanoidVariant &v,
                      const HumanoidPose &pose, const AnimationInputs &anim,
                      ISubmitter &out) const override {
    using HP = HumanProportions;

    QVector3D team_tint = resolveTeamTint(ctx);
    uint32_t seed = 0U;
    if (ctx.entity != nullptr) {
      auto *unit = ctx.entity->getComponent<Engine::Core::UnitComponent>();
      if (unit != nullptr) {
        seed ^= uint32_t(unit->owner_id * 2654435761U);
      }
      seed ^= uint32_t(reinterpret_cast<uintptr_t>(ctx.entity) & 0xFFFFFFFFU);
    }

    ArcherExtras extras;
    auto it = m_extrasCache.find(seed);
    if (it != m_extrasCache.end()) {
      extras = it->second;
    } else {
      extras.metalHead = Render::Geom::clampVec01(v.palette.metal * 1.15F);
      extras.stringCol = QVector3D(0.30F, 0.30F, 0.32F);
      auto tint = [&](float k) {
        return QVector3D(clamp01(team_tint.x() * k), clamp01(team_tint.y() * k),
                         clamp01(team_tint.z() * k));
      };
      extras.fletch = tint(0.9F);
      extras.bowTopY = HP::SHOULDER_Y + 0.55F;
      extras.bowBotY = HP::WAIST_Y - 0.25F;

      m_extrasCache[seed] = extras;

      if (m_extrasCache.size() > MAX_EXTRAS_CACHE_SIZE) {
        m_extrasCache.clear();
      }
    }

    drawQuiver(ctx, v, pose, extras, seed, out);

    float attack_phase = 0.0F;
    if (anim.is_attacking && !anim.isMelee) {
      attack_phase = std::fmod(anim.time * ARCHER_INV_ATTACK_CYCLE_TIME, 1.0F);
    }
    drawBowAndArrow(ctx, pose, v, extras, anim.is_attacking && !anim.isMelee,
                    attack_phase, out);
  }

  void drawHelmet(const DrawContext &ctx, const HumanoidVariant &v,
                  const HumanoidPose &pose, ISubmitter &out) const override {
    using HP = HumanProportions;

    QVector3D const helmet_color =
        v.palette.metal * QVector3D(1.08F, 0.98F, 0.78F);
    QVector3D const helmet_accent = helmet_color * 1.12F;

    QVector3D const helmet_top(0, pose.headPos.y() + pose.headR * 1.28F, 0);
    QVector3D const helmet_bot(0, pose.headPos.y() + pose.headR * 0.08F, 0);
    float const helmet_r = pose.headR * 1.10F;

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, helmet_bot, helmet_top, helmet_r),
             helmet_color, nullptr, 1.0F);

    QVector3D const apex_pos(0, pose.headPos.y() + pose.headR * 1.48F, 0);
    out.mesh(getUnitCone(),
             coneFromTo(ctx.model, helmet_top, apex_pos, helmet_r * 0.97F),
             helmet_accent, nullptr, 1.0F);

    auto ring = [&](const QVector3D &center, float r, float h,
                    const QVector3D &col) {
      QVector3D const a = center + QVector3D(0, h * 0.5F, 0);
      QVector3D const b = center - QVector3D(0, h * 0.5F, 0);
      out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, a, b, r), col,
               nullptr, 1.0F);
    };

    QVector3D const brow_pos(0, pose.headPos.y() + pose.headR * 0.35F, 0);
    ring(brow_pos, helmet_r * 1.07F, 0.020F, helmet_accent);

    ring(QVector3D(0, pose.headPos.y() + pose.headR * 0.65F, 0),
         helmet_r * 1.03F, 0.015F, helmet_color * 1.05F);
    ring(QVector3D(0, pose.headPos.y() + pose.headR * 0.95F, 0),
         helmet_r * 1.01F, 0.012F, helmet_color * 1.03F);

    float const cheek_w = pose.headR * 0.48F;
    QVector3D const cheek_top(0, pose.headPos.y() + pose.headR * 0.22F, 0);
    QVector3D const cheek_bot(0, pose.headPos.y() - pose.headR * 0.42F, 0);

    QVector3D const cheek_ltop =
        cheek_top + QVector3D(-cheek_w, 0, pose.headR * 0.38F);
    QVector3D const cheek_lbot =
        cheek_bot + QVector3D(-cheek_w * 0.82F, 0, pose.headR * 0.28F);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, cheek_lbot, cheek_ltop, 0.028F),
             helmet_color * 0.96F, nullptr, 1.0F);

    QVector3D const cheek_rtop =
        cheek_top + QVector3D(cheek_w, 0, pose.headR * 0.38F);
    QVector3D const cheek_rbot =
        cheek_bot + QVector3D(cheek_w * 0.82F, 0, pose.headR * 0.28F);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, cheek_rbot, cheek_rtop, 0.028F),
             helmet_color * 0.96F, nullptr, 1.0F);

    QVector3D const neck_guard_top(0, pose.headPos.y() + pose.headR * 0.03F,
                                   -pose.headR * 0.82F);
    QVector3D const neck_guard_bot(0, pose.headPos.y() - pose.headR * 0.32F,
                                   -pose.headR * 0.88F);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, neck_guard_bot, neck_guard_top,
                             helmet_r * 0.88F),
             helmet_color * 0.93F, nullptr, 1.0F);

    QVector3D const crest_base = apex_pos;
    QVector3D const crest_mid = crest_base + QVector3D(0, 0.09F, 0);
    QVector3D const crest_top = crest_mid + QVector3D(0, 0.12F, 0);

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, crest_base, crest_mid, 0.018F),
             helmet_accent, nullptr, 1.0F);

    out.mesh(getUnitCone(), coneFromTo(ctx.model, crest_mid, crest_top, 0.042F),
             QVector3D(0.88F, 0.18F, 0.18F), nullptr, 1.0F);

    out.mesh(getUnitSphere(), sphereAt(ctx.model, crest_top, 0.020F),
             helmet_accent, nullptr, 1.0F);
  }

  void draw_armorOverlay(const DrawContext &ctx, const HumanoidVariant &v,
                         const HumanoidPose &pose, float y_top_cover,
                         float torso_r, float shoulder_half_span,
                         float upper_arm_r, const QVector3D &right_axis,
                         ISubmitter &out) const override {
    using HP = HumanProportions;

    auto ring = [&](const QVector3D &center, float r, float h,
                    const QVector3D &col) {
      QVector3D const a = center + QVector3D(0, h * 0.5F, 0);
      QVector3D const b = center - QVector3D(0, h * 0.5F, 0);
      out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, a, b, r), col,
               nullptr, 1.0F);
    };

    QVector3D mail_color = v.palette.metal * QVector3D(0.85F, 0.87F, 0.92F);
    QVector3D leather_trim = v.palette.leatherDark * 0.90F;

    float const waist_y = pose.pelvisPos.y();

    QVector3D const mail_top(0, y_top_cover + 0.01F, 0);
    QVector3D const mail_mid(0, (y_top_cover + waist_y) * 0.5F, 0);
    QVector3D const mail_bot(0, waist_y + 0.08F, 0);
    float const rTop = torso_r * 1.10F;
    float const rMid = torso_r * 1.08F;

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, mail_top, mail_mid, rTop), mail_color,
             nullptr, 1.0F);

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, mail_mid, mail_bot, rMid),
             mail_color * 0.95F, nullptr, 1.0F);

    for (int i = 0; i < 3; ++i) {
      float const y = mail_top.y() - (i * 0.12F);
      ring(QVector3D(0, y, 0), rTop * (1.01F + i * 0.005F), 0.012F,
           leather_trim);
    }

    auto draw_pauldron = [&](const QVector3D &shoulder,
                             const QVector3D &outward) {
      for (int i = 0; i < 3; ++i) {
        float const segY = shoulder.y() + 0.02F - i * 0.035F;
        float const segR = upper_arm_r * (2.2F - i * 0.15F);
        QVector3D seg_top(shoulder.x(), segY + 0.025F, shoulder.z());
        QVector3D seg_bot(shoulder.x(), segY - 0.010F, shoulder.z());

        seg_top += outward * 0.02F;
        seg_bot += outward * 0.02F;

        out.mesh(getUnitSphere(), sphereAt(ctx.model, seg_top, segR),
                 mail_color * (1.0F - i * 0.05F), nullptr, 1.0F);
      }
    };

    draw_pauldron(pose.shoulderL, -right_axis);
    draw_pauldron(pose.shoulderR, right_axis);

    auto draw_manica = [&](const QVector3D &shoulder, const QVector3D &elbow) {
      QVector3D dir = (elbow - shoulder);
      float const len = dir.length();
      if (len < 1e-5F) {
        return;
      }
      dir /= len;

      for (int i = 0; i < 4; ++i) {
        float const t0 = 0.08F + i * 0.18F;
        float const t1 = t0 + 0.16F;
        QVector3D const a = shoulder + dir * (t0 * len);
        QVector3D const b = shoulder + dir * (t1 * len);
        float const r = upper_arm_r * (1.25F - i * 0.03F);
        out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, a, b, r),
                 mail_color * (0.95F - i * 0.03F), nullptr, 1.0F);
      }
    };

    draw_manica(pose.shoulderL, pose.elbowL);
    draw_manica(pose.shoulderR, pose.elbowR);

    QVector3D const belt_top(0, waist_y + 0.06F, 0);
    QVector3D const belt_bot(0, waist_y - 0.02F, 0);
    float const belt_r = torso_r * 1.12F;
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, belt_top, belt_bot, belt_r),
             leather_trim, nullptr, 1.0F);

    QVector3D const brass_color =
        v.palette.metal * QVector3D(1.2F, 1.0F, 0.65F);
    ring(QVector3D(0, waist_y + 0.02F, 0), belt_r * 1.02F, 0.010F, brass_color);

    auto draw_pteruge = [&](float angle, float yStart, float length) {
      float const rad = torso_r * 1.15F;
      float const x = rad * std::sin(angle);
      float const z = rad * std::cos(angle);
      QVector3D const top(x, yStart, z);
      QVector3D const bot(x * 0.95F, yStart - length, z * 0.95F);
      out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, top, bot, 0.018F),
               leather_trim * 0.85F, nullptr, 1.0F);
    };

    float const shoulder_pteruge_y = y_top_cover - 0.02F;
    for (int i = 0; i < 8; ++i) {
      float const angle = (i / 8.0F) * 2.0F * std::numbers::pi_v<float>;
      draw_pteruge(angle, shoulder_pteruge_y, 0.14F);
    }

    float const waist_pteruge_y = waist_y - 0.04F;
    for (int i = 0; i < 10; ++i) {
      float const angle = (i / 10.0F) * 2.0F * std::numbers::pi_v<float>;
      draw_pteruge(angle, waist_pteruge_y, 0.18F);
    }

    QVector3D const collar_top(0, y_top_cover + 0.018F, 0);
    QVector3D const collar_bot(0, y_top_cover - 0.008F, 0);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, collar_top, collar_bot,
                             HP::NECK_RADIUS * 1.8F),
             mail_color * 1.05F, nullptr, 1.0F);
  }

  void drawShoulderDecorations(const DrawContext &ctx, const HumanoidVariant &v,
                               const HumanoidPose &pose, float y_top_cover,
                               float y_neck, const QVector3D &right_axis,
                               ISubmitter &out) const override {
    using HP = HumanProportions;

    QVector3D brass_color = v.palette.metal * QVector3D(1.2F, 1.0F, 0.65F);

    auto draw_phalera = [&](const QVector3D &pos) {
      QMatrix4x4 m = ctx.model;
      m.translate(pos);
      m.scale(0.025F);
      out.mesh(getUnitSphere(), m, brass_color, nullptr, 1.0F);
    };

    draw_phalera(pose.shoulderL + QVector3D(0, 0.05F, 0.02F));

    draw_phalera(pose.shoulderR + QVector3D(0, 0.05F, 0.02F));

    QVector3D const clasp_pos(0, y_neck + 0.02F, 0.08F);
    QMatrix4x4 clasp_m = ctx.model;
    clasp_m.translate(clasp_pos);
    clasp_m.scale(0.020F);
    out.mesh(getUnitSphere(), clasp_m, brass_color * 1.1F, nullptr, 1.0F);

    QVector3D const cape_top = clasp_pos + QVector3D(0, -0.02F, -0.05F);
    QVector3D const cape_bot = clasp_pos + QVector3D(0, -0.25F, -0.15F);
    QVector3D const red_fabric = v.palette.cloth * QVector3D(1.2F, 0.3F, 0.3F);

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, cape_top, cape_bot, 0.025F),
             red_fabric * 0.85F, nullptr, 1.0F);
  }

private:
  static void drawQuiver(const DrawContext &ctx, const HumanoidVariant &v,
                         const HumanoidPose &pose, const ArcherExtras &extras,
                         uint32_t seed, ISubmitter &out) {
    using HP = HumanProportions;

    QVector3D const spine_mid = (pose.shoulderL + pose.shoulderR) * 0.5F;
    QVector3D const quiver_offset(-0.08F, 0.10F, -0.25F);
    QVector3D const qTop = spine_mid + quiver_offset;
    QVector3D const q_base = qTop + QVector3D(-0.02F, -0.30F, 0.03F);

    float const quiver_r = HP::HEAD_RADIUS * 0.45F;
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, q_base, qTop, quiver_r),
             v.palette.leather, nullptr, 1.0F);

    float const j = (hash01(seed) - 0.5F) * 0.04F;
    float const k = (hash01(seed ^ 0x9E3779B9U) - 0.5F) * 0.04F;

    QVector3D const a1 = qTop + QVector3D(0.00F + j, 0.08F, 0.00F + k);
    out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, qTop, a1, 0.010F),
             v.palette.wood, nullptr, 1.0F);
    out.mesh(getUnitCone(),
             coneFromTo(ctx.model, a1, a1 + QVector3D(0, 0.05F, 0), 0.025F),
             extras.fletch, nullptr, 1.0F);

    QVector3D const a2 = qTop + QVector3D(0.02F - j, 0.07F, 0.02F - k);
    out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, qTop, a2, 0.010F),
             v.palette.wood, nullptr, 1.0F);
    out.mesh(getUnitCone(),
             coneFromTo(ctx.model, a2, a2 + QVector3D(0, 0.05F, 0), 0.025F),
             extras.fletch, nullptr, 1.0F);
  }

  static void drawBowAndArrow(const DrawContext &ctx, const HumanoidPose &pose,
                              const HumanoidVariant &v,
                              const ArcherExtras &extras, bool is_attacking,
                              float attack_phase, ISubmitter &out) {
    const QVector3D up(0.0F, 1.0F, 0.0F);
    const QVector3D forward(0.0F, 0.0F, 1.0F);

    QVector3D const grip = pose.handL;

    float const bow_plane_z = 0.45F;
    QVector3D const top_end(extras.bowX, extras.bowTopY, bow_plane_z);
    QVector3D const bot_end(extras.bowX, extras.bowBotY, bow_plane_z);

    QVector3D const nock(
        extras.bowX,
        clampf(pose.hand_r.y(), extras.bowBotY + 0.05F, extras.bowTopY - 0.05F),
        clampf(pose.hand_r.z(), bow_plane_z - 0.30F, bow_plane_z + 0.30F));

    const int segs = 22;
    auto q_bezier = [](const QVector3D &a, const QVector3D &c,
                       const QVector3D &b, float t) {
      float const u = 1.0F - t;
      return u * u * a + 2.0F * u * t * c + t * t * b;
    };

    float const bow_mid_y = (top_end.y() + bot_end.y()) * 0.5F;

    float const ctrl_y = bow_mid_y + 0.45F;

    QVector3D const ctrl(extras.bowX, ctrl_y,
                         bow_plane_z + extras.bowDepth * 0.6F);

    QVector3D prev = bot_end;
    for (int i = 1; i <= segs; ++i) {
      float const t = float(i) / float(segs);
      QVector3D const cur = q_bezier(bot_end, ctrl, top_end, t);
      out.mesh(getUnitCylinder(),
               cylinderBetween(ctx.model, prev, cur, extras.bowRodR),
               v.palette.wood, nullptr, 1.0F);
      prev = cur;
    }
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, grip - up * 0.05F, grip + up * 0.05F,
                             extras.bowRodR * 1.45F),
             v.palette.wood, nullptr, 1.0F);

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, top_end, nock, extras.stringR),
             extras.stringCol, nullptr, 1.0F);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, nock, bot_end, extras.stringR),
             extras.stringCol, nullptr, 1.0F);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, pose.hand_r, nock, 0.0045F),
             extras.stringCol * 0.9F, nullptr, 1.0F);

    bool const show_arrow =
        !is_attacking ||
        (is_attacking && attack_phase >= 0.0F && attack_phase < 0.52F);

    if (show_arrow) {
      QVector3D const tail = nock - forward * 0.06F;
      QVector3D const tip = tail + forward * 0.90F;
      out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, tail, tip, 0.018F),
               v.palette.wood, nullptr, 1.0F);
      QVector3D const head_base = tip - forward * 0.10F;
      out.mesh(getUnitCone(), coneFromTo(ctx.model, head_base, tip, 0.05F),
               extras.metalHead, nullptr, 1.0F);
      QVector3D f1b = tail - forward * 0.02F;
      QVector3D f1a = f1b - forward * 0.06F;
      QVector3D f2b = tail + forward * 0.02F;
      QVector3D f2a = f2b + forward * 0.06F;
      out.mesh(getUnitCone(), coneFromTo(ctx.model, f1b, f1a, 0.04F),
               extras.fletch, nullptr, 1.0F);
      out.mesh(getUnitCone(), coneFromTo(ctx.model, f2a, f2b, 0.04F),
               extras.fletch, nullptr, 1.0F);
    }
  }
};

void registerArcherRenderer(Render::GL::EntityRendererRegistry &registry) {
  static ArcherRenderer const renderer;
  registry.registerRenderer(
      "archer", [](const DrawContext &ctx, ISubmitter &out) {
        static ArcherRenderer const static_renderer;
        Shader *archer_shader = nullptr;
        if (ctx.backend != nullptr) {
          archer_shader = ctx.backend->shader(QStringLiteral("archer"));
        }
        auto *scene_renderer = dynamic_cast<Renderer *>(&out);
        if ((scene_renderer != nullptr) && (archer_shader != nullptr)) {
          scene_renderer->setCurrentShader(archer_shader);
        }
        static_renderer.render(ctx, out);
        if (scene_renderer != nullptr) {
          scene_renderer->setCurrentShader(nullptr);
        }
      });
}

} // namespace Render::GL
