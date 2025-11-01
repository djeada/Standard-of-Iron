#include "archer_renderer.h"
#include "../../../../game/core/component.h"
#include "../../../../game/core/entity.h"
#include "../../../../game/systems/nation_id.h"
#include "../../../geom/math_utils.h"
#include "../../../geom/transforms.h"
#include "../../../gl/backend.h"
#include "../../../gl/primitives.h"
#include "../../../gl/render_constants.h"
#include "../../../gl/shader.h"
#include "../../../humanoid/humanoid_math.h"
#include "../../../humanoid/humanoid_specs.h"
#include "../../../humanoid/rig.h"
#include "../../../humanoid/style_palette.h"
#include "../../../palette.h"
#include "../../../scene_renderer.h"
#include "../../../submitter.h"
#include "../../registry.h"
#include "../../renderer_constants.h"
#include "archer_style.h"

#include <QMatrix4x4>
#include <QString>
#include <QVector3D>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>
#include <qmatrix4x4.h>
#include <qstringliteral.h>
#include <qvectornd.h>
#include <string>
#include <string_view>
#include <unordered_map>

namespace Render::GL::Kingdom {

namespace {

constexpr std::string_view k_default_style_key = "default";
constexpr std::string_view k_attachment_headwrap = "carthage_headwrap";

auto style_registry() -> std::unordered_map<std::string, ArcherStyleConfig> & {
  static std::unordered_map<std::string, ArcherStyleConfig> styles;
  return styles;
}

void ensure_archer_styles_registered() {
  static const bool registered = []() {
    register_kingdom_archer_style();
    return true;
  }();
  (void)registered;
}

constexpr float k_team_mix_weight = 0.65F;
constexpr float k_style_mix_weight = 0.35F;

} // namespace

void register_archer_style(const std::string &nation_id,
                           const ArcherStyleConfig &style) {
  style_registry()[nation_id] = style;
}

using Render::Geom::clamp01;
using Render::Geom::clampf;
using Render::Geom::coneFromTo;
using Render::Geom::cylinderBetween;
using Render::Geom::sphereAt;
using Render::GL::Humanoid::mix_palette_color;
using Render::GL::Humanoid::saturate_color;

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

    float const bow_x = 0.0F;

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

      QVector3D const hold_hand_l(bow_x - 0.15F, pose.shoulderL.y() + 0.30F,
                                  0.55F);
      QVector3D const hold_hand_r(bow_x + 0.12F, pose.shoulderR.y() + 0.15F,
                                  0.10F);
      QVector3D const normal_hand_l(bow_x - 0.05F + arm_asymmetry,
                                    HP::SHOULDER_Y + 0.05F + arm_height_jitter,
                                    0.55F);
      QVector3D const normal_hand_r(
          0.15F - arm_asymmetry * 0.5F,
          HP::SHOULDER_Y + 0.15F + arm_height_jitter * 0.8F, 0.20F);

      pose.handL = normal_hand_l * (1.0F - t) + hold_hand_l * t;
      pose.hand_r = normal_hand_r * (1.0F - t) + hold_hand_r * t;
    } else {
      pose.handL = QVector3D(bow_x - 0.05F + arm_asymmetry,
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
          pose.handL = QVector3D(bow_x - 0.05F, HP::SHOULDER_Y + 0.05F, 0.55F);

          float const shoulder_twist = t * 0.08F;
          pose.shoulderR.setY(pose.shoulderR.y() + shoulder_twist);
          pose.shoulderL.setY(pose.shoulderL.y() - shoulder_twist * 0.5F);
        } else if (attack_phase < 0.50F) {
          pose.hand_r = draw_pos;
          pose.handL = QVector3D(bow_x - 0.05F, HP::SHOULDER_Y + 0.05F, 0.55F);

          float const shoulder_twist = 0.08F;
          pose.shoulderR.setY(pose.shoulderR.y() + shoulder_twist);
          pose.shoulderL.setY(pose.shoulderL.y() - shoulder_twist * 0.5F);
        } else if (attack_phase < 0.58F) {
          float t = (attack_phase - 0.50F) / 0.08F;
          t = t * t * t;
          pose.hand_r = draw_pos * (1.0F - t) + release_pos * t;
          pose.handL = QVector3D(bow_x - 0.05F, HP::SHOULDER_Y + 0.05F, 0.55F);

          float const shoulder_twist = 0.08F * (1.0F - t * 0.6F);
          pose.shoulderR.setY(pose.shoulderR.y() + shoulder_twist);
          pose.shoulderL.setY(pose.shoulderL.y() - shoulder_twist * 0.5F);

          pose.headPos.setZ(pose.headPos.z() - t * 0.04F);
        } else {
          float t = (attack_phase - 0.58F) / 0.42F;
          t = 1.0F - (1.0F - t) * (1.0F - t);
          pose.hand_r = release_pos * (1.0F - t) + aim_pos * t;
          pose.handL = QVector3D(bow_x - 0.05F, HP::SHOULDER_Y + 0.05F, 0.55F);

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
                      const HumanoidPose &pose,
                      const HumanoidAnimationContext &anim_ctx,
                      ISubmitter &out) const override {
    using HP = HumanProportions;

    auto const &style = resolve_style(ctx);
    const AnimationInputs &anim = anim_ctx.inputs;
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

      apply_extras_overrides(style, extras);
      m_extrasCache[seed] = extras;

      if (m_extrasCache.size() > MAX_EXTRAS_CACHE_SIZE) {
        m_extrasCache.clear();
      }
    }
    apply_extras_overrides(style, extras);

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

    auto const &style = resolve_style(ctx);
    if (!style.show_helmet) {
      if (style.attachment_profile == std::string(k_attachment_headwrap)) {
        draw_headwrap(ctx, v, pose, out);
      }
      return;
    }

    const AttachmentFrame &head = pose.bodyFrames.head;
    float const head_r = head.radius;
    if (head_r <= 0.0F) {
      return;
    }

    QVector3D const steel_color =
        v.palette.metal * QVector3D(0.88F, 0.90F, 0.95F);
    QVector3D const steel_dark = steel_color * 0.82F;

    auto headPoint = [&](const QVector3D &normalized) -> QVector3D {
      return frameLocalPosition(head, normalized);
    };

    float const bowl_scale = 1.06F;
    QVector3D const bowl_top = headPoint(QVector3D(0.0F, 1.10F, 0.0F));
    QVector3D const bowl_bot = headPoint(QVector3D(0.0F, 0.15F, 0.0F));
    float const bowl_r = head_r * bowl_scale;

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, bowl_bot, bowl_top, bowl_r),
             steel_color, nullptr, 1.0F);

    QMatrix4x4 cap_m = ctx.model;
    cap_m.translate(bowl_top);
    cap_m.scale(bowl_r * 0.92F, head_r * 0.28F, bowl_r * 0.92F);
    out.mesh(getUnitSphere(), cap_m, steel_color * 1.05F, nullptr, 1.0F);

    QVector3D const brim_top = headPoint(QVector3D(0.0F, 0.18F, 0.0F));
    QVector3D const brim_bot = headPoint(QVector3D(0.0F, 0.08F, 0.0F));
    float const brim_r = head_r * 1.42F;

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, brim_bot, brim_top, brim_r), steel_dark,
             nullptr, 1.0F);

    auto ring = [&](float y_offset, float radius_scale, const QVector3D &col) {
      QVector3D const center = headPoint(QVector3D(0.0F, y_offset, 0.0F));
      float const height = head_r * 0.010F;
      QVector3D const a = center + head.up * (height * 0.5F);
      QVector3D const b = center - head.up * (height * 0.5F);
      out.mesh(getUnitCylinder(),
               cylinderBetween(ctx.model, a, b, head_r * radius_scale), col,
               nullptr, 1.0F);
    };

    ring(0.13F, 1.42F * 1.01F / head_r, steel_color);

    QMatrix4x4 rivet_m = ctx.model;
    rivet_m.translate(headPoint(QVector3D(0.0F, 1.15F, 0.0F)));
    rivet_m.scale(0.015F);
    out.mesh(getUnitSphere(), rivet_m, steel_color * 1.15F, nullptr, 1.0F);
  }

  void draw_armorOverlay(const DrawContext &ctx, const HumanoidVariant &v,
                         const HumanoidPose &pose, float y_top_cover,
                         float torso_r, float, float upper_arm_r,
                         const QVector3D &right_axis,
                         ISubmitter &out) const override {
    using HP = HumanProportions;

    if (!resolve_style(ctx).show_armor) {
      return;
    }

    QVector3D const gambeson_base =
        v.palette.cloth * QVector3D(0.92F, 0.88F, 0.75F);
    QVector3D const gambeson_dark = gambeson_base * 0.85F;
    QVector3D const leather_trim = v.palette.leatherDark * 0.88F;
    QVector3D const green_tunic =
        v.palette.cloth * QVector3D(0.45F, 0.75F, 0.52F);

    float const waist_y = pose.pelvisPos.y();

    QVector3D const gambeson_top(0, y_top_cover, 0);
    QVector3D const gambeson_mid(0, (y_top_cover + waist_y) * 0.55F, 0);
    QVector3D const gambeson_bot(0, waist_y + 0.05F, 0);
    float const r_top = torso_r * 1.14F;
    float const r_mid = torso_r * 1.16F;

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, gambeson_top, gambeson_mid, r_top),
             gambeson_base, nullptr, 1.0F);

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, gambeson_mid, gambeson_bot, r_mid),
             gambeson_dark, nullptr, 1.0F);

    auto stitch_ring = [&](float y, float r, const QVector3D &col) {
      QVector3D const a = QVector3D(0, y + 0.005F, 0);
      QVector3D const b = QVector3D(0, y - 0.005F, 0);
      out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, a, b, r), col,
               nullptr, 1.0F);
    };

    for (int i = 0; i < 6; ++i) {
      float const y = gambeson_top.y() - (i * 0.08F);
      if (y > waist_y) {
        stitch_ring(y, r_top * (1.005F + i * 0.002F), gambeson_base * 0.78F);
      }
    }

    auto draw_padded_sleeve = [&](const QVector3D &shoulder,
                                  const QVector3D &elbow,
                                  const QVector3D &outward) {
      for (int i = 0; i < 2; ++i) {
        float const seg_y = shoulder.y() - i * 0.04F;
        float const seg_r = upper_arm_r * (1.55F - i * 0.08F);
        QVector3D seg_top(shoulder.x(), seg_y + 0.022F, shoulder.z());
        QVector3D seg_bot(shoulder.x(), seg_y - 0.018F, shoulder.z());

        seg_top += outward * 0.015F;
        seg_bot += outward * 0.015F;

        out.mesh(getUnitSphere(), sphereAt(ctx.model, seg_top, seg_r),
                 gambeson_base * (1.0F - i * 0.06F), nullptr, 1.0F);
      }

      QVector3D dir = (elbow - shoulder);
      float const len = dir.length();
      if (len < 1e-5F) {
        return;
      }
      dir /= len;

      for (int i = 0; i < 3; ++i) {
        float const t0 = 0.10F + i * 0.20F;
        float const t1 = t0 + 0.18F;
        QVector3D const a = shoulder + dir * (t0 * len);
        QVector3D const b = shoulder + dir * (t1 * len);
        float const r = upper_arm_r * (1.28F - i * 0.04F);
        out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, a, b, r),
                 gambeson_base * (0.96F - i * 0.04F), nullptr, 1.0F);
      }
    };

    draw_padded_sleeve(pose.shoulderL, pose.elbowL, -right_axis);
    draw_padded_sleeve(pose.shoulderR, pose.elbowR, right_axis);

    QVector3D const belt_top(0, waist_y + 0.04F, 0);
    QVector3D const belt_bot(0, waist_y - 0.03F, 0);
    float const belt_r = torso_r * 1.18F;

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, belt_top, belt_bot, belt_r),
             leather_trim, nullptr, 1.0F);

    QVector3D const iron_color =
        v.palette.metal * QVector3D(0.60F, 0.62F, 0.68F);
    QMatrix4x4 buckle_m = ctx.model;
    buckle_m.translate(QVector3D(0, waist_y, torso_r * 1.22F));
    buckle_m.scale(0.032F, 0.020F, 0.010F);
    out.mesh(getUnitCylinder(), buckle_m, iron_color, nullptr, 1.0F);

    QVector3D const tunic_top = gambeson_bot;
    QVector3D const tunic_bot(0, waist_y - 0.02F, 0);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, tunic_bot, tunic_top, r_mid * 1.02F),
             green_tunic, nullptr, 1.0F);
  }

  void drawShoulderDecorations(const DrawContext &ctx, const HumanoidVariant &v,
                               const HumanoidPose &pose, float, float y_neck,
                               const QVector3D &,
                               ISubmitter &out) const override {
    using HP = HumanProportions;

    auto const &style = resolve_style(ctx);

    if (!style.show_shoulder_decor && !style.show_cape) {
      return;
    }

    if (style.show_shoulder_decor) {
      QVector3D const hood_color =
          v.palette.cloth * QVector3D(0.75F, 0.68F, 0.58F);

      QVector3D const collar_top(0, y_neck + 0.01F, 0);
      QVector3D const collar_bot(0, y_neck - 0.03F, 0);
      out.mesh(getUnitCylinder(),
               cylinderBetween(ctx.model, collar_bot, collar_top,
                               HP::NECK_RADIUS * 1.35F),
               hood_color, nullptr, 1.0F);
    }

    if (style.show_cape) {
      QVector3D cloak_color = v.palette.cloth * QVector3D(0.48F, 0.62F, 0.52F);
      if (style.cape_color) {
        cloak_color = saturate_color(*style.cape_color);
      }

      QVector3D const toggle_pos(0, y_neck, 0.06F);
      QMatrix4x4 toggle_m = ctx.model;
      toggle_m.translate(toggle_pos);
      toggle_m.scale(0.012F, 0.025F, 0.012F);
      out.mesh(getUnitCylinder(), toggle_m, v.palette.wood * 0.75F, nullptr,
               1.0F);

      QVector3D const cloak_top = toggle_pos + QVector3D(0, -0.01F, -0.04F);
      QVector3D const cloak_bot = cloak_top + QVector3D(0, -0.22F, -0.12F);

      out.mesh(getUnitCylinder(),
               cylinderBetween(ctx.model, cloak_top, cloak_bot, 0.022F),
               cloak_color * 0.88F, nullptr, 1.0F);
    }
  }

private:
  mutable std::unordered_map<uint32_t, ArcherExtras> m_extrasCache;

  auto
  resolve_style(const DrawContext &ctx) const -> const ArcherStyleConfig & {
    ensure_archer_styles_registered();
    auto &styles = style_registry();
    std::string nation_id;
    if (ctx.entity != nullptr) {
      if (auto *unit =
              ctx.entity->getComponent<Engine::Core::UnitComponent>()) {
        nation_id = Game::Systems::nationIDToString(unit->nation_id);
      }
    }
    if (!nation_id.empty()) {
      auto it = styles.find(nation_id);
      if (it != styles.end()) {
        return it->second;
      }
    }
    auto fallback = styles.find(std::string(k_default_style_key));
    if (fallback != styles.end()) {
      return fallback->second;
    }
    static const ArcherStyleConfig default_style{};
    return default_style;
  }

public:
  auto resolve_shader_key(const DrawContext &ctx) const -> QString {
    const ArcherStyleConfig &style = resolve_style(ctx);
    if (!style.shader_id.empty()) {
      return QString::fromStdString(style.shader_id);
    }
    return QStringLiteral("archer");
  }

private:
  void apply_palette_overrides(const ArcherStyleConfig &style,
                               const QVector3D &team_tint,
                               HumanoidVariant &variant) const {
    auto apply_color = [&](const std::optional<QVector3D> &override_color,
                           QVector3D &target) {
      target = mix_palette_color(target, override_color, team_tint,
                                 k_team_mix_weight, k_style_mix_weight);
    };

    apply_color(style.cloth_color, variant.palette.cloth);
    apply_color(style.leather_color, variant.palette.leather);
    apply_color(style.leather_dark_color, variant.palette.leatherDark);
    apply_color(style.metal_color, variant.palette.metal);
    apply_color(style.wood_color, variant.palette.wood);
  }

  void apply_extras_overrides(const ArcherStyleConfig &style,
                              ArcherExtras &extras) const {
    if (style.fletching_color) {
      extras.fletch = saturate_color(*style.fletching_color);
    }
    if (style.bow_string_color) {
      extras.stringCol = saturate_color(*style.bow_string_color);
    }
  }

  void draw_headwrap(const DrawContext &ctx, const HumanoidVariant &v,
                     const HumanoidPose &pose, ISubmitter &out) const {
    QVector3D const cloth_color =
        saturate_color(v.palette.cloth * QVector3D(0.9F, 1.05F, 1.05F));
    const AttachmentFrame &head = pose.bodyFrames.head;
    float const head_r = head.radius;
    if (head_r <= 0.0F) {
      return;
    }

    auto headPoint = [&](const QVector3D &normalized) -> QVector3D {
      return frameLocalPosition(head, normalized);
    };

    QVector3D const band_top = headPoint(QVector3D(0.0F, 0.70F, 0.0F));
    QVector3D const band_bot = headPoint(QVector3D(0.0F, 0.30F, 0.0F));
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, band_bot, band_top, head_r * 1.08F),
             cloth_color, nullptr, 1.0F);

    QVector3D const knot_center = headPoint(QVector3D(0.10F, 0.60F, 0.72F));
    QMatrix4x4 knot_m = ctx.model;
    knot_m.translate(knot_center);
    knot_m.scale(head_r * 0.32F);
    out.mesh(getUnitSphere(), knot_m, cloth_color * 1.05F, nullptr, 1.0F);

    QVector3D const tail_top = knot_center + head.right * (-0.08F) +
                               head.up * (-0.05F) + head.forward * (-0.06F);
    QVector3D const tail_bot = tail_top + head.right * 0.02F +
                               head.up * (-0.28F) + head.forward * (-0.08F);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, tail_top, tail_bot, head_r * 0.28F),
             cloth_color * QVector3D(0.92F, 0.98F, 1.05F), nullptr, 1.0F);
  }

  static void drawQuiver(const DrawContext &ctx, const HumanoidVariant &v,
                         const HumanoidPose &pose, const ArcherExtras &extras,
                         uint32_t seed, ISubmitter &out) {
    using HP = HumanProportions;

    QVector3D const spine_mid = (pose.shoulderL + pose.shoulderR) * 0.5F;
    QVector3D const quiver_offset(-0.08F, 0.10F, -0.25F);
    QVector3D const q_top = spine_mid + quiver_offset;
    QVector3D const q_base = q_top + QVector3D(-0.02F, -0.30F, 0.03F);

    float const quiver_r = HP::HEAD_RADIUS * 0.45F;
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, q_base, q_top, quiver_r),
             v.palette.leather, nullptr, 1.0F);

    float const j = (hash_01(seed) - 0.5F) * 0.04F;
    float const k =
        (hash_01(seed ^ HashXorShift::k_golden_ratio) - 0.5F) * 0.04F;

    QVector3D const a1 = q_top + QVector3D(0.00F + j, 0.08F, 0.00F + k);
    out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, q_top, a1, 0.010F),
             v.palette.wood, nullptr, 1.0F);
    out.mesh(getUnitCone(),
             coneFromTo(ctx.model, a1, a1 + QVector3D(0, 0.05F, 0), 0.025F),
             extras.fletch, nullptr, 1.0F);

    QVector3D const a2 = q_top + QVector3D(0.02F - j, 0.07F, 0.02F - k);
    out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, q_top, a2, 0.010F),
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

    constexpr int k_bowstring_segments = 22;
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
    for (int i = 1; i <= k_bowstring_segments; ++i) {
      float const t = float(i) / float(k_bowstring_segments);
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
      QVector3D const f1b = tail - forward * 0.02F;
      QVector3D const f1a = f1b - forward * 0.06F;
      QVector3D const f2b = tail + forward * 0.02F;
      QVector3D const f2a = f2b + forward * 0.06F;
      out.mesh(getUnitCone(), coneFromTo(ctx.model, f1b, f1a, 0.04F),
               extras.fletch, nullptr, 1.0F);
      out.mesh(getUnitCone(), coneFromTo(ctx.model, f2a, f2b, 0.04F),
               extras.fletch, nullptr, 1.0F);
    }
  }
};

void registerArcherRenderer(Render::GL::EntityRendererRegistry &registry) {
  ensure_archer_styles_registered();
  static ArcherRenderer const renderer;
  registry.registerRenderer(
      "troops/kingdom/archer", [](const DrawContext &ctx, ISubmitter &out) {
        static ArcherRenderer const static_renderer;
        Shader *archer_shader = nullptr;
        if (ctx.backend != nullptr) {
          QString shader_key = static_renderer.resolve_shader_key(ctx);
          archer_shader = ctx.backend->shader(shader_key);
          if (archer_shader == nullptr) {
            archer_shader = ctx.backend->shader(QStringLiteral("archer"));
          }
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

} // namespace Render::GL::Kingdom
