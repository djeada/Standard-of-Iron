#include "spearman_renderer.h"
#include "../../../../game/core/component.h"
#include "../../../../game/systems/nation_id.h"
#include "../../../equipment/equipment_registry.h"
#include "../../../geom/math_utils.h"
#include "../../../geom/transforms.h"
#include "../../../gl/backend.h"
#include "../../../gl/primitives.h"
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
#include "spearman_style.h"

#include <QMatrix4x4>
#include <QString>
#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <qstringliteral.h>
#include <qvectornd.h>
#include <string>
#include <string_view>
#include <unordered_map>

namespace Render::GL::Roman {

namespace {

constexpr std::string_view k_spearman_default_style_key = "default";
constexpr float k_spearman_team_mix_weight = 0.6F;
constexpr float k_spearman_style_mix_weight = 0.4F;

auto spearman_style_registry()
    -> std::unordered_map<std::string, SpearmanStyleConfig> & {
  static std::unordered_map<std::string, SpearmanStyleConfig> styles;
  return styles;
}

void ensure_spearman_styles_registered() {
  static const bool registered = []() {
    register_roman_spearman_style();
    return true;
  }();
  (void)registered;
}

} // namespace

void register_spearman_style(const std::string &nation_id,
                             const SpearmanStyleConfig &style) {
  spearman_style_registry()[nation_id] = style;
}

using Render::Geom::clamp01;
using Render::Geom::coneFromTo;
using Render::Geom::cylinderBetween;
using Render::Geom::easeInOutCubic;
using Render::Geom::lerp;
using Render::Geom::smoothstep;
using Render::Geom::sphereAt;
using Render::GL::Humanoid::mix_palette_color;
using Render::GL::Humanoid::saturate_color;

struct SpearmanExtras {
  QVector3D spearShaftColor;
  QVector3D spearheadColor;
  float spearLength = 1.20F;
  float spearShaftRadius = 0.020F;
  float spearheadLength = 0.18F;
};

class SpearmanRenderer : public HumanoidRendererBase {
public:
  auto getProportionScaling() const -> QVector3D override {
    return {1.10F, 1.02F, 1.05F};
  }

private:
  mutable std::unordered_map<uint32_t, SpearmanExtras> m_extrasCache;

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

    if (anim.isInHoldMode || anim.isExitingHold) {
      float const t = anim.isInHoldMode ? 1.0F : (1.0F - anim.holdExitProgress);

      float const kneel_depth = 0.35F * t;
      float const pelvis_y = HP::WAIST_Y - kneel_depth;
      pose.pelvisPos.setY(pelvis_y);

      float const stance_narrow = 0.10F;

      float const left_knee_y = HP::GROUND_Y + 0.06F * t;
      float const left_knee_z = -0.08F * t;
      pose.knee_l = QVector3D(-stance_narrow, left_knee_y, left_knee_z);
      pose.footL = QVector3D(-stance_narrow - 0.02F, HP::GROUND_Y,
                             left_knee_z - HP::LOWER_LEG_LEN * 0.90F * t);

      float const right_knee_y =
          HP::WAIST_Y * 0.45F * (1.0F - t) + HP::WAIST_Y * 0.30F * t;
      pose.knee_r = QVector3D(stance_narrow + 0.05F, right_knee_y, 0.15F * t);
      pose.foot_r = QVector3D(stance_narrow + 0.08F, HP::GROUND_Y, 0.25F * t);

      float const upper_body_drop = kneel_depth;
      pose.shoulderL.setY(HP::SHOULDER_Y - upper_body_drop);
      pose.shoulderR.setY(HP::SHOULDER_Y - upper_body_drop);
      pose.neck_base.setY(HP::NECK_BASE_Y - upper_body_drop);

      float const lowered_chin_y = HP::CHIN_Y - upper_body_drop;

      pose.headPos.setY(lowered_chin_y + pose.headR);

      float const forward_lean = 0.08F * t;
      pose.shoulderL.setZ(pose.shoulderL.z() + forward_lean);
      pose.shoulderR.setZ(pose.shoulderR.z() + forward_lean);
      pose.neck_base.setZ(pose.neck_base.z() + forward_lean * 0.8F);
      pose.headPos.setZ(pose.headPos.z() + forward_lean * 0.7F);

      float const lowered_shoulder_y = HP::SHOULDER_Y - upper_body_drop;

      pose.hand_r =
          QVector3D(0.18F * (1.0F - t) + 0.22F * t,
                    lowered_shoulder_y * (1.0F - t) + (pelvis_y + 0.05F) * t,
                    0.15F * (1.0F - t) + 0.20F * t);

      pose.handL = QVector3D(0.0F,
                             lowered_shoulder_y * (1.0F - t) +
                                 (lowered_shoulder_y - 0.10F) * t,
                             0.30F * (1.0F - t) + 0.55F * t);

      QVector3D const shoulder_to_hand_r = pose.hand_r - pose.shoulderR;
      float const arm_length_r = shoulder_to_hand_r.length();
      QVector3D const arm_dir_r = shoulder_to_hand_r.normalized();
      pose.elbowR = pose.shoulderR + arm_dir_r * (arm_length_r * 0.5F) +
                    QVector3D(0.08F, -0.15F, -0.05F);

      QVector3D const shoulder_to_hand_l = pose.handL - pose.shoulderL;
      float const arm_length_l = shoulder_to_hand_l.length();
      QVector3D const arm_dir_l = shoulder_to_hand_l.normalized();
      pose.elbowL = pose.shoulderL + arm_dir_l * (arm_length_l * 0.5F) +
                    QVector3D(-0.08F, -0.12F, 0.05F);

    } else if (anim.is_attacking && anim.isMelee && !anim.isInHoldMode) {
      float const attack_phase =
          std::fmod(anim.time * SPEARMAN_INV_ATTACK_CYCLE_TIME, 1.0F);

      QVector3D const guard_pos(0.28F, HP::SHOULDER_Y + 0.05F, 0.25F);
      QVector3D const prepare_pos(0.35F, HP::SHOULDER_Y + 0.08F, 0.05F);
      QVector3D const thrust_pos(0.32F, HP::SHOULDER_Y + 0.10F, 0.90F);
      QVector3D const recover_pos(0.28F, HP::SHOULDER_Y + 0.06F, 0.40F);

      if (attack_phase < 0.20F) {

        float const t = easeInOutCubic(attack_phase / 0.20F);
        pose.hand_r = guard_pos * (1.0F - t) + prepare_pos * t;

        pose.handL = QVector3D(-0.10F, HP::SHOULDER_Y - 0.05F,
                               0.20F * (1.0F - t) + 0.08F * t);
      } else if (attack_phase < 0.30F) {

        pose.hand_r = prepare_pos;
        pose.handL = QVector3D(-0.10F, HP::SHOULDER_Y - 0.05F, 0.08F);
      } else if (attack_phase < 0.50F) {

        float t = (attack_phase - 0.30F) / 0.20F;
        t = t * t * t;
        pose.hand_r = prepare_pos * (1.0F - t) + thrust_pos * t;

        pose.handL =
            QVector3D(-0.10F + 0.05F * t, HP::SHOULDER_Y - 0.05F + 0.03F * t,
                      0.08F + 0.45F * t);
      } else if (attack_phase < 0.70F) {

        float const t = easeInOutCubic((attack_phase - 0.50F) / 0.20F);
        pose.hand_r = thrust_pos * (1.0F - t) + recover_pos * t;
        pose.handL = QVector3D(-0.05F * (1.0F - t) - 0.10F * t,
                               HP::SHOULDER_Y - 0.02F * (1.0F - t) - 0.06F * t,
                               lerp(0.53F, 0.35F, t));
      } else {

        float const t = smoothstep(0.70F, 1.0F, attack_phase);
        pose.hand_r = recover_pos * (1.0F - t) + guard_pos * t;
        pose.handL = QVector3D(-0.10F - 0.02F * (1.0F - t),
                               HP::SHOULDER_Y - 0.06F + 0.01F * t +
                                   arm_height_jitter * (1.0F - t),
                               lerp(0.35F, 0.25F, t));
      }
    } else {
      pose.hand_r =
          QVector3D(0.28F + arm_asymmetry,
                    HP::SHOULDER_Y - 0.02F + arm_height_jitter, 0.30F);

      pose.handL =
          QVector3D(-0.08F - 0.5F * arm_asymmetry,
                    HP::SHOULDER_Y - 0.08F + 0.5F * arm_height_jitter, 0.45F);

      QVector3D const shoulder_to_hand = pose.hand_r - pose.shoulderR;
      float const arm_length = shoulder_to_hand.length();
      QVector3D const arm_dir = shoulder_to_hand.normalized();

      pose.elbowR = pose.shoulderR + arm_dir * (arm_length * 0.5F) +
                    QVector3D(0.06F, -0.12F, -0.04F);
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

    SpearmanExtras extras;
    auto it = m_extrasCache.find(seed);
    if (it != m_extrasCache.end()) {
      extras = it->second;
    } else {
      extras = computeSpearmanExtras(seed, v);
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
      attack_phase =
          std::fmod(anim.time * SPEARMAN_INV_ATTACK_CYCLE_TIME, 1.0F);
    }

    drawSpear(ctx, pose, v, extras, anim, is_attacking, attack_phase, out);
  }

  void drawHelmet(const DrawContext &ctx, const HumanoidVariant &v,
                  const HumanoidPose &pose, ISubmitter &out) const override {

    auto &registry = EquipmentRegistry::instance();
    auto helmet = registry.get(EquipmentCategory::Helmet, "roman_heavy");
    if (helmet) {
      HumanoidAnimationContext anim_ctx{};
      helmet->render(ctx, pose.bodyFrames, v.palette, anim_ctx, out);
    }
  }

  void drawArmor(const DrawContext &ctx, const HumanoidVariant &v,
                 const HumanoidPose &pose,
                 const HumanoidAnimationContext &anim,
                 ISubmitter &out) const override {
    auto &registry = EquipmentRegistry::instance();
    auto armor = registry.get(EquipmentCategory::Armor, "heavy_tunic");
    if (armor) {
      armor->render(ctx, pose.bodyFrames, v.palette, anim, out);
    }
  }

  void draw_armorOverlay(const DrawContext &ctx, const HumanoidVariant &v,
                         const HumanoidPose &pose, float y_top_cover,
                         float torso_r, float, float upper_arm_r,
                         const QVector3D &right_axis,
                         ISubmitter &out) const override {
    using HP = HumanProportions;

    const QVector3D iron_color = v.palette.metal * IRON_TINT;
    const QVector3D leather_color = v.palette.leather * 0.95F;

    QVector3D const chest_top(0, y_top_cover + 0.02F, 0);
    QVector3D const chest_bot(0, HP::WAIST_Y + 0.08F, 0);
    float const r_chest = torso_r * 1.14F;

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, chest_top, chest_bot, r_chest),
             iron_color, nullptr, 1.0F);

    auto draw_pauldron = [&](const QVector3D &shoulder,
                             const QVector3D &outward) {
      for (int i = 0; i < 3; ++i) {
        float const seg_y = shoulder.y() + 0.03F - i * 0.040F;
        float const seg_r = upper_arm_r * (2.2F - i * 0.10F);
        QVector3D seg_pos = shoulder + outward * (0.015F + i * 0.006F);
        seg_pos.setY(seg_y);

        out.mesh(getUnitSphere(), sphereAt(ctx.model, seg_pos, seg_r),
                 i == 0 ? iron_color * 1.04F : iron_color * (1.0F - i * 0.02F),
                 nullptr, 1.0F);
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

      for (int i = 0; i < 2; ++i) {
        float const t0 = 0.12F + i * 0.28F;
        float const t1 = t0 + 0.24F;
        QVector3D const a = shoulder + dir * (t0 * len);
        QVector3D const b = shoulder + dir * (t1 * len);
        float const r = upper_arm_r * (1.26F - i * 0.03F);

        out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, a, b, r),
                 iron_color * (0.96F - i * 0.02F), nullptr, 1.0F);
      }
    };

    draw_arm_plate(pose.shoulderL, pose.elbowL);
    draw_arm_plate(pose.shoulderR, pose.elbowR);

    for (int i = 0; i < 3; ++i) {
      float const y = HP::WAIST_Y + 0.06F - i * 0.035F;
      float const r = torso_r * (1.12F + i * 0.020F);
      QVector3D const strip_top(0, y, 0);
      QVector3D const strip_bot(0, y - 0.030F, 0);

      out.mesh(getUnitCone(), coneFromTo(ctx.model, strip_top, strip_bot, r),
               leather_color * (0.98F - i * 0.02F), nullptr, 1.0F);
    }
  }

  void drawShoulderDecorations(const DrawContext &ctx, const HumanoidVariant &v,
                               const HumanoidPose &pose, float y_top_cover,
                               float y_neck, const QVector3D &right_axis,
                               ISubmitter &out) const override {}

private:
  static auto computeSpearmanExtras(uint32_t seed, const HumanoidVariant &v)
      -> SpearmanExtras {
    SpearmanExtras e;

    e.spearShaftColor = v.palette.leather * QVector3D(0.85F, 0.75F, 0.65F);
    e.spearheadColor = QVector3D(0.75F, 0.76F, 0.80F);

    e.spearLength = 1.15F + (hash_01(seed ^ 0xABCDU) - 0.5F) * 0.10F;
    e.spearShaftRadius = 0.018F + (hash_01(seed ^ 0x7777U) - 0.5F) * 0.003F;
    e.spearheadLength = 0.16F + (hash_01(seed ^ 0xBEEFU) - 0.5F) * 0.04F;

    return e;
  }

  static void drawSpear(const DrawContext &ctx, const HumanoidPose &pose,
                        const HumanoidVariant &v, const SpearmanExtras &extras,
                        const AnimationInputs &anim, bool is_attacking,
                        float attack_phase, ISubmitter &out) {
    QVector3D const grip_pos = pose.hand_r;

    QVector3D spear_dir = QVector3D(0.05F, 0.55F, 0.85F);
    if (spear_dir.lengthSquared() > 1e-6F) {
      spear_dir.normalize();
    }

    if (anim.isInHoldMode || anim.isExitingHold) {
      float const t = anim.isInHoldMode ? 1.0F : (1.0F - anim.holdExitProgress);

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
    QVector3D shaft_mid = grip_pos + spear_dir * (extras.spearLength * 0.5F);
    QVector3D const shaft_tip = grip_pos + spear_dir * extras.spearLength;

    shaft_mid.setY(shaft_mid.y() + 0.02F);

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, shaft_base, shaft_mid,
                             extras.spearShaftRadius),
             extras.spearShaftColor, nullptr, 1.0F);

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, shaft_mid, shaft_tip,
                             extras.spearShaftRadius * 0.95F),
             extras.spearShaftColor * 0.98F, nullptr, 1.0F);

    QVector3D const spearhead_base = shaft_tip;
    QVector3D const spearhead_tip =
        shaft_tip + spear_dir * extras.spearheadLength;

    out.mesh(getUnitCone(),
             coneFromTo(ctx.model, spearhead_base, spearhead_tip,
                        extras.spearShaftRadius * 1.8F),
             extras.spearheadColor, nullptr, 1.0F);

    QVector3D const grip_end = grip_pos + spear_dir * 0.10F;
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, grip_pos, grip_end,
                             extras.spearShaftRadius * 1.5F),
             v.palette.leather * 0.92F, nullptr, 1.0F);
  }

  auto
  resolve_style(const DrawContext &ctx) const -> const SpearmanStyleConfig & {
    ensure_spearman_styles_registered();
    auto &styles = spearman_style_registry();
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
    auto it_default = styles.find(std::string(k_spearman_default_style_key));
    if (it_default != styles.end()) {
      return it_default->second;
    }
    static const SpearmanStyleConfig k_empty{};
    return k_empty;
  }

public:
  auto resolve_shader_key(const DrawContext &ctx) const -> QString {
    const SpearmanStyleConfig &style = resolve_style(ctx);
    if (!style.shader_id.empty()) {
      return QString::fromStdString(style.shader_id);
    }
    return QStringLiteral("spearman");
  }

private:
  void apply_palette_overrides(const SpearmanStyleConfig &style,
                               const QVector3D &team_tint,
                               HumanoidVariant &variant) const {
    auto apply_color = [&](const std::optional<QVector3D> &override_color,
                           QVector3D &target) {
      target = mix_palette_color(target, override_color, team_tint,
                                 k_spearman_team_mix_weight,
                                 k_spearman_style_mix_weight);
    };

    apply_color(style.cloth_color, variant.palette.cloth);
    apply_color(style.leather_color, variant.palette.leather);
    apply_color(style.leather_dark_color, variant.palette.leatherDark);
    apply_color(style.metal_color, variant.palette.metal);
  }

  void apply_extras_overrides(const SpearmanStyleConfig &style,
                              const QVector3D &team_tint,
                              [[maybe_unused]] const HumanoidVariant &variant,
                              SpearmanExtras &extras) const {
    extras.spearShaftColor = saturate_color(extras.spearShaftColor);
    extras.spearheadColor = saturate_color(extras.spearheadColor);

    auto apply_color = [&](const std::optional<QVector3D> &override_color,
                           QVector3D &target) {
      target = mix_palette_color(target, override_color, team_tint,
                                 k_spearman_team_mix_weight,
                                 k_spearman_style_mix_weight);
    };

    apply_color(style.spear_shaft_color, extras.spearShaftColor);
    apply_color(style.spearhead_color, extras.spearheadColor);

    if (style.spear_length_scale) {
      extras.spearLength =
          std::max(0.80F, extras.spearLength * *style.spear_length_scale);
    }
  }
};

void registerSpearmanRenderer(Render::GL::EntityRendererRegistry &registry) {
  ensure_spearman_styles_registered();
  static SpearmanRenderer const renderer;
  registry.registerRenderer(
      "troops/roman/spearman", [](const DrawContext &ctx, ISubmitter &out) {
        static SpearmanRenderer const static_renderer;
        Shader *spearman_shader = nullptr;
        if (ctx.backend != nullptr) {
          QString shader_key = static_renderer.resolve_shader_key(ctx);
          spearman_shader = ctx.backend->shader(shader_key);
          if (spearman_shader == nullptr) {
            spearman_shader = ctx.backend->shader(QStringLiteral("spearman"));
          }
        }
        auto *scene_renderer = dynamic_cast<Renderer *>(&out);
        if ((scene_renderer != nullptr) && (spearman_shader != nullptr)) {
          scene_renderer->setCurrentShader(spearman_shader);
        }
        static_renderer.render(ctx, out);
        if (scene_renderer != nullptr) {
          scene_renderer->setCurrentShader(nullptr);
        }
      });
}

} // namespace Render::GL::Roman
