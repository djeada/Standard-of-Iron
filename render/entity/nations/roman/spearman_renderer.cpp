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
#include "../../../humanoid/pose_controller.h"
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

constexpr float k_kneel_depth_multiplier = 0.875F;
constexpr float k_lean_amount_multiplier = 0.67F;

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
    HumanoidPoseController controller(pose, anim_ctx);

    float const arm_height_jitter = (hash_01(seed ^ 0xABCDU) - 0.5F) * 0.03F;
    float const arm_asymmetry = (hash_01(seed ^ 0xDEF0U) - 0.5F) * 0.04F;

    if (anim.isInHoldMode || anim.isExitingHold) {
      float const t = anim.isInHoldMode ? 1.0F : (1.0F - anim.holdExitProgress);

      controller.kneel(t * k_kneel_depth_multiplier);
      controller.lean(QVector3D(0.0F, 0.0F, 1.0F), t * k_lean_amount_multiplier);

      float const lowered_shoulder_y = pose.shoulderL.y();
      float const pelvis_y = pose.pelvisPos.y();

      QVector3D const hand_r_pos(
          0.18F * (1.0F - t) + 0.22F * t,
          lowered_shoulder_y * (1.0F - t) + (pelvis_y + 0.05F) * t,
          0.15F * (1.0F - t) + 0.20F * t);

      QVector3D const hand_l_pos(
          0.0F, lowered_shoulder_y * (1.0F - t) + (lowered_shoulder_y - 0.10F) * t,
          0.30F * (1.0F - t) + 0.55F * t);

      controller.placeHandAt(false, hand_r_pos);
      controller.placeHandAt(true, hand_l_pos);

    } else if (anim.is_attacking && anim.isMelee && !anim.isInHoldMode) {
      float const attack_phase =
          std::fmod(anim.time * SPEARMAN_INV_ATTACK_CYCLE_TIME, 1.0F);

      QVector3D const guard_pos(0.28F, HP::SHOULDER_Y + 0.05F, 0.25F);
      QVector3D const prepare_pos(0.35F, HP::SHOULDER_Y + 0.08F, 0.05F);
      QVector3D const thrust_pos(0.32F, HP::SHOULDER_Y + 0.10F, 0.90F);
      QVector3D const recover_pos(0.28F, HP::SHOULDER_Y + 0.06F, 0.40F);

      QVector3D hand_r_target;
      QVector3D hand_l_target;

      if (attack_phase < 0.20F) {
        float const t = easeInOutCubic(attack_phase / 0.20F);
        hand_r_target = guard_pos * (1.0F - t) + prepare_pos * t;
        hand_l_target = QVector3D(-0.10F, HP::SHOULDER_Y - 0.05F,
                                  0.20F * (1.0F - t) + 0.08F * t);
      } else if (attack_phase < 0.30F) {
        hand_r_target = prepare_pos;
        hand_l_target = QVector3D(-0.10F, HP::SHOULDER_Y - 0.05F, 0.08F);
      } else if (attack_phase < 0.50F) {
        float t = (attack_phase - 0.30F) / 0.20F;
        t = t * t * t;
        hand_r_target = prepare_pos * (1.0F - t) + thrust_pos * t;
        hand_l_target =
            QVector3D(-0.10F + 0.05F * t, HP::SHOULDER_Y - 0.05F + 0.03F * t,
                      0.08F + 0.45F * t);
      } else if (attack_phase < 0.70F) {
        float const t = easeInOutCubic((attack_phase - 0.50F) / 0.20F);
        hand_r_target = thrust_pos * (1.0F - t) + recover_pos * t;
        hand_l_target = QVector3D(-0.05F * (1.0F - t) - 0.10F * t,
                                  HP::SHOULDER_Y - 0.02F * (1.0F - t) - 0.06F * t,
                                  lerp(0.53F, 0.35F, t));
      } else {
        float const t = smoothstep(0.70F, 1.0F, attack_phase);
        hand_r_target = recover_pos * (1.0F - t) + guard_pos * t;
        hand_l_target = QVector3D(-0.10F - 0.02F * (1.0F - t),
                                  HP::SHOULDER_Y - 0.06F + 0.01F * t +
                                      arm_height_jitter * (1.0F - t),
                                  lerp(0.35F, 0.25F, t));
      }

      controller.placeHandAt(false, hand_r_target);
      controller.placeHandAt(true, hand_l_target);
    } else {
      QVector3D const idle_hand_r(0.28F + arm_asymmetry,
                                  HP::SHOULDER_Y - 0.02F + arm_height_jitter,
                                  0.30F);
      QVector3D const idle_hand_l(-0.08F - 0.5F * arm_asymmetry,
                                  HP::SHOULDER_Y - 0.08F + 0.5F * arm_height_jitter,
                                  0.45F);

      controller.placeHandAt(false, idle_hand_r);
      controller.placeHandAt(true, idle_hand_l);
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
    auto armor = registry.get(EquipmentCategory::Armor, "roman_heavy_armor");
    if (armor) {
      armor->render(ctx, pose.bodyFrames, v.palette, anim, out);
    }
  }

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
