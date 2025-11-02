#include "archer_renderer.h"
#include "../../../../game/core/component.h"
#include "../../../../game/core/entity.h"
#include "../../../../game/systems/nation_id.h"
#include "../../../equipment/equipment_registry.h"
#include "../../../equipment/weapons/bow_renderer.h"
#include "../../../equipment/weapons/quiver_renderer.h"
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

namespace Render::GL::Roman {

namespace {

constexpr std::string_view k_default_style_key = "default";
constexpr std::string_view k_attachment_headwrap = "carthage_headwrap";

auto style_registry() -> std::unordered_map<std::string, ArcherStyleConfig> & {
  static std::unordered_map<std::string, ArcherStyleConfig> styles;
  return styles;
}

void ensure_archer_styles_registered() {
  static const bool registered = []() {
    register_roman_archer_style();
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
    QVector3D team_tint = resolveTeamTint(ctx);
    
    // Get fletching color for quiver renderer
    auto tint = [&](float k) {
      return QVector3D(clamp01(team_tint.x() * k), clamp01(team_tint.y() * k),
                       clamp01(team_tint.z() * k));
    };
    QVector3D const fletch = tint(0.9F);

    // Render quiver using equipment registry
    auto &registry = EquipmentRegistry::instance();
    auto quiver = registry.get(EquipmentCategory::Weapon, "quiver");
    if (quiver) {
      // Configure quiver with appropriate colors
      QuiverRenderConfig quiver_config;
      quiver_config.fletching_color = fletch;
      quiver_config.quiver_radius = HP::HEAD_RADIUS * 0.45F;
      
      // Safe downcast - we know "quiver" is a QuiverRenderer
      auto *quiver_renderer = dynamic_cast<QuiverRenderer*>(quiver.get());
      if (quiver_renderer) {
        quiver_renderer->setConfig(quiver_config);
      }
      
      quiver->render(ctx, pose.bodyFrames, v.palette, anim_ctx, out);
    }

    // Render bow using equipment registry
    auto bow = registry.get(EquipmentCategory::Weapon, "bow_roman");
    if (bow) {
      // Configure Roman simple self bow (moderate curve, standard size)
      BowRenderConfig bow_config;
      bow_config.string_color = QVector3D(0.30F, 0.30F, 0.32F);
      bow_config.metal_color = Render::Geom::clampVec01(v.palette.metal * 1.15F);
      bow_config.fletching_color = fletch;
      bow_config.bow_top_y = HP::SHOULDER_Y + 0.55F;   // Standard height
      bow_config.bow_bot_y = HP::WAIST_Y - 0.25F;
      bow_config.bow_x = 0.0F;
      bow_config.bow_depth = 0.22F;          // Moderate curve
      bow_config.bow_curve_factor = 1.0F;    // Standard curve
      bow_config.bow_height_scale = 1.0F;    // Standard size
      
      // Apply style overrides if available
      if (style.bow_string_color) {
        bow_config.string_color = saturate_color(*style.bow_string_color);
      }
      if (style.fletching_color) {
        bow_config.fletching_color = saturate_color(*style.fletching_color);
      }
      
      // Safe downcast - we know "bow" is a BowRenderer
      auto *bow_renderer = dynamic_cast<BowRenderer*>(bow.get());
      if (bow_renderer) {
        bow_renderer->setConfig(bow_config);
      }
      
      bow->render(ctx, pose.bodyFrames, v.palette, anim_ctx, out);
    }
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

    auto headPoint = [&](const QVector3D &normalized) -> QVector3D {
      return frameLocalPosition(head, normalized);
    };

    QVector3D const helmet_color =
        v.palette.metal * QVector3D(1.08F, 0.98F, 0.78F);
    QVector3D const helmet_accent = helmet_color * 1.12F;

    QVector3D const helmet_top = headPoint(QVector3D(0.0F, 1.28F, 0.0F));
    QVector3D const helmet_bot = headPoint(QVector3D(0.0F, 0.08F, 0.0F));
    float const helmet_r = head_r * 1.10F;

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, helmet_bot, helmet_top, helmet_r),
             helmet_color, nullptr, 1.0F);

    QVector3D const apex_pos = headPoint(QVector3D(0.0F, 1.48F, 0.0F));
    out.mesh(getUnitCone(),
             coneFromTo(ctx.model, helmet_top, apex_pos, helmet_r * 0.97F),
             helmet_accent, nullptr, 1.0F);

    auto ring = [&](float y_offset, float r_scale, float h, const QVector3D &col) {
      QVector3D const center = headPoint(QVector3D(0.0F, y_offset, 0.0F));
      QVector3D const a = center + head.up * (h * 0.5F);
      QVector3D const b = center - head.up * (h * 0.5F);
      out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, a, b, helmet_r * r_scale), col,
               nullptr, 1.0F);
    };

    ring(0.35F, 1.07F, 0.020F, helmet_accent);
    ring(0.65F, 1.03F, 0.015F, helmet_color * 1.05F);
    ring(0.95F, 1.01F, 0.012F, helmet_color * 1.03F);

    float const cheek_w = head_r * 0.48F;
    QVector3D const cheek_top = headPoint(QVector3D(0.0F, 0.22F, 0.0F));
    QVector3D const cheek_bot = headPoint(QVector3D(0.0F, -0.42F, 0.0F));

    QVector3D const cheek_ltop = cheek_top + head.right * (-cheek_w / head_r) + head.forward * 0.38F;
    QVector3D const cheek_lbot = cheek_bot + head.right * (-cheek_w * 0.82F / head_r) + head.forward * 0.28F;
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, cheek_lbot, cheek_ltop, 0.028F),
             helmet_color * 0.96F, nullptr, 1.0F);

    QVector3D const cheek_rtop = cheek_top + head.right * (cheek_w / head_r) + head.forward * 0.38F;
    QVector3D const cheek_rbot = cheek_bot + head.right * (cheek_w * 0.82F / head_r) + head.forward * 0.28F;
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, cheek_rbot, cheek_rtop, 0.028F),
             helmet_color * 0.96F, nullptr, 1.0F);

    QVector3D const neck_guard_top = headPoint(QVector3D(0.0F, 0.03F, -0.82F));
    QVector3D const neck_guard_bot = headPoint(QVector3D(0.0F, -0.32F, -0.88F));
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, neck_guard_bot, neck_guard_top,
                             helmet_r * 0.88F),
             helmet_color * 0.93F, nullptr, 1.0F);

    QVector3D const crest_base = apex_pos;
    QVector3D const crest_mid = crest_base + head.up * 0.09F;
    QVector3D const crest_top = crest_mid + head.up * 0.12F;

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
                         float torso_r, float, float upper_arm_r,
                         const QVector3D &right_axis,
                         ISubmitter &out) const override {
    using HP = HumanProportions;

    if (!resolve_style(ctx).show_armor) {
      return;
    }

    auto ring = [&](const QVector3D &center, float r, float h,
                    const QVector3D &col) {
      QVector3D const a = center + QVector3D(0, h * 0.5F, 0);
      QVector3D const b = center - QVector3D(0, h * 0.5F, 0);
      out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, a, b, r), col,
               nullptr, 1.0F);
    };

    uint32_t seed = 0U;
    if (ctx.entity != nullptr) {
      seed = reinterpret_cast<uintptr_t>(ctx.entity) & 0xFFFFFFFFU;
    }

    bool const use_scale_armor = (hash_01(seed ^ 0x9876U) > 0.50F);

    QVector3D mail_color = v.palette.metal * QVector3D(0.85F, 0.87F, 0.92F);
    QVector3D scale_color = v.palette.metal * QVector3D(0.95F, 0.80F, 0.55F);
    QVector3D leather_trim = v.palette.leatherDark * 0.90F;
    QVector3D red_tunic = QVector3D(0.72F, 0.18F, 0.15F);

    float const waist_y = pose.pelvisPos.y();

    QVector3D const armor_top(0, y_top_cover + 0.01F, 0);
    QVector3D const armor_mid(0, (y_top_cover + waist_y) * 0.5F, 0);
    QVector3D const armor_bot(0, waist_y + 0.08F, 0);
    float const r_top = torso_r * 1.12F;
    float const r_mid = torso_r * 1.10F;

    QVector3D armor_color = use_scale_armor ? scale_color : mail_color;

    if (use_scale_armor) {

      out.mesh(getUnitCylinder(),
               cylinderBetween(ctx.model, armor_top, armor_mid, r_top),
               scale_color, nullptr, 1.0F);

      out.mesh(getUnitCylinder(),
               cylinderBetween(ctx.model, armor_mid, armor_bot, r_mid),
               scale_color * 0.92F, nullptr, 1.0F);

      for (int i = 0; i < 8; ++i) {
        float const y = armor_top.y() - (i * 0.06F);
        if (y > armor_bot.y()) {
          ring(QVector3D(0, y, 0), r_top * (1.00F + i * 0.002F), 0.008F,
               scale_color * (1.05F - i * 0.03F));
        }
      }

      ring(QVector3D(0, armor_top.y() - 0.01F, 0), r_top * 1.02F, 0.012F,
           leather_trim);

    } else {

      out.mesh(getUnitCylinder(),
               cylinderBetween(ctx.model, armor_top, armor_mid, r_top),
               mail_color, nullptr, 1.0F);

      out.mesh(getUnitCylinder(),
               cylinderBetween(ctx.model, armor_mid, armor_bot, r_mid),
               mail_color * 0.95F, nullptr, 1.0F);

      for (int i = 0; i < 3; ++i) {
        float const y = armor_top.y() - (i * 0.12F);
        ring(QVector3D(0, y, 0), r_top * (1.01F + i * 0.005F), 0.012F,
             leather_trim);
      }
    }

    auto draw_pauldron = [&](const QVector3D &shoulder,
                             const QVector3D &outward) {
      for (int i = 0; i < 3; ++i) {
        float const seg_y = shoulder.y() + 0.02F - i * 0.035F;
        float const seg_r = upper_arm_r * (2.2F - i * 0.15F);
        QVector3D seg_top(shoulder.x(), seg_y + 0.025F, shoulder.z());
        QVector3D seg_bot(shoulder.x(), seg_y - 0.010F, shoulder.z());

        seg_top += outward * 0.02F;
        seg_bot += outward * 0.02F;

        out.mesh(getUnitSphere(), sphereAt(ctx.model, seg_top, seg_r),
                 armor_color * (1.0F - i * 0.05F), nullptr, 1.0F);
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
                 armor_color * (0.95F - i * 0.03F), nullptr, 1.0F);
      }
    };

    draw_manica(pose.shoulderL, pose.elbowL);
    draw_manica(pose.shoulderR, pose.elbowR);

    QVector3D const belt_top(0, waist_y + 0.06F, 0);
    QVector3D const belt_bot(0, waist_y - 0.02F, 0);
    float const belt_r = torso_r * 1.14F;
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, belt_top, belt_bot, belt_r),
             leather_trim, nullptr, 1.0F);

    QVector3D const brass_color =
        v.palette.metal * QVector3D(1.2F, 1.0F, 0.65F);
    ring(QVector3D(0, waist_y + 0.02F, 0), belt_r * 1.02F, 0.010F, brass_color);

    auto draw_pteruge = [&](float angle, float yStart, float length) {
      float const rad = torso_r * 1.17F;
      float const x = rad * std::sin(angle);
      float const z = rad * std::cos(angle);
      QVector3D const top(x, yStart, z);
      QVector3D const bot(x * 0.95F, yStart - length, z * 0.95F);
      out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, top, bot, 0.018F),
               leather_trim * 0.85F, nullptr, 1.0F);
    };

    float const shoulder_pteruge_y = y_top_cover - 0.02F;
    constexpr int k_shoulder_pteruge_count = 8;
    constexpr float k_shoulder_pteruge_divisor = 8.0F;
    for (int i = 0; i < k_shoulder_pteruge_count; ++i) {
      float const angle =
          (i / k_shoulder_pteruge_divisor) * 2.0F * std::numbers::pi_v<float>;
      draw_pteruge(angle, shoulder_pteruge_y, 0.14F);
    }

    float const waist_pteruge_y = waist_y - 0.04F;
    constexpr int k_waist_pteruge_count = 10;
    constexpr float k_waist_pteruge_divisor = 10.0F;
    for (int i = 0; i < k_waist_pteruge_count; ++i) {
      float const angle =
          (i / k_waist_pteruge_divisor) * 2.0F * std::numbers::pi_v<float>;
      draw_pteruge(angle, waist_pteruge_y, 0.18F);
    }

    QVector3D const collar_top(0, y_top_cover + 0.018F, 0);
    QVector3D const collar_bot(0, y_top_cover - 0.008F, 0);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, collar_top, collar_bot,
                             HP::NECK_RADIUS * 1.8F),
             armor_color * 1.05F, nullptr, 1.0F);

    QVector3D const tunic_peek(0, armor_bot.y() - 0.01F, 0);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, tunic_peek, armor_bot, r_mid * 1.01F),
             red_tunic, nullptr, 1.0F);
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

    QVector3D brass_color = v.palette.metal * QVector3D(1.2F, 1.0F, 0.65F);

    auto draw_phalera = [&](const QVector3D &pos) {
      QMatrix4x4 m = ctx.model;
      m.translate(pos);
      m.scale(0.025F);
      out.mesh(getUnitSphere(), m, brass_color, nullptr, 1.0F);
    };

    if (style.show_shoulder_decor) {
      draw_phalera(pose.shoulderL + QVector3D(0, 0.05F, 0.02F));
      draw_phalera(pose.shoulderR + QVector3D(0, 0.05F, 0.02F));
    }

    if (!style.show_cape) {
      return;
    }

    QVector3D const clasp_pos(0, y_neck + 0.02F, 0.08F);
    QMatrix4x4 clasp_m = ctx.model;
    clasp_m.translate(clasp_pos);
    clasp_m.scale(0.020F);
    out.mesh(getUnitSphere(), clasp_m, brass_color * 1.1F, nullptr, 1.0F);

    QVector3D const cape_top = clasp_pos + QVector3D(0, -0.02F, -0.05F);
    QVector3D const cape_bot = clasp_pos + QVector3D(0, -0.25F, -0.15F);
    QVector3D cape_fabric = v.palette.cloth * QVector3D(1.2F, 0.3F, 0.3F);
    if (style.cape_color) {
      cape_fabric = saturate_color(*style.cape_color);
    }

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, cape_top, cape_bot, 0.025F),
             cape_fabric * 0.85F, nullptr, 1.0F);
  }

private:
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

  void draw_headwrap(const DrawContext &ctx, const HumanoidVariant &v,
                     const HumanoidPose &pose, ISubmitter &out) const {
    const AttachmentFrame &head = pose.bodyFrames.head;
    float const head_r = head.radius;
    if (head_r <= 0.0F) {
      return;
    }

    auto headPoint = [&](const QVector3D &normalized) -> QVector3D {
      return frameLocalPosition(head, normalized);
    };

    QVector3D const cloth_color =
        saturate_color(v.palette.cloth * QVector3D(0.9F, 1.05F, 1.05F));

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

    QVector3D const tail_top = knot_center + head.right * (-0.08F) + head.up * (-0.05F) + head.forward * (-0.06F);
    QVector3D const tail_bot = tail_top + head.right * 0.02F + head.up * (-0.28F) + head.forward * (-0.08F);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, tail_top, tail_bot, head_r * 0.28F),
             cloth_color * QVector3D(0.92F, 0.98F, 1.05F), nullptr, 1.0F);
  }
};

void registerArcherRenderer(Render::GL::EntityRendererRegistry &registry) {
  ensure_archer_styles_registered();
  static ArcherRenderer const renderer;
  registry.registerRenderer(
      "troops/roman/archer", [](const DrawContext &ctx, ISubmitter &out) {
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

} // namespace Render::GL::Roman
