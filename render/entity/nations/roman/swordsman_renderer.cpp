#include "swordsman_renderer.h"
#include "../../../../game/core/component.h"
#include "../../../../game/systems/nation_id.h"
#include "../../../equipment/equipment_registry.h"
#include "../../../equipment/weapons/shield_renderer.h"
#include "../../../equipment/weapons/sword_renderer.h"
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
#include "swordsman_style.h"
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

namespace Render::GL::Roman {

namespace {

constexpr std::string_view k_swordsman_default_style_key = "default";
constexpr float k_swordsman_team_mix_weight = 0.6F;
constexpr float k_swordsman_style_mix_weight = 0.4F;

auto swordsman_style_registry()
    -> std::unordered_map<std::string, KnightStyleConfig> & {
  static std::unordered_map<std::string, KnightStyleConfig> styles;
  return styles;
}

void ensure_swordsman_styles_registered() {
  static const bool registered = []() {
    register_roman_swordsman_style();
    return true;
  }();
  (void)registered;
}

} // namespace

void register_swordsman_style(const std::string &nation_id,
                              const KnightStyleConfig &style) {
  swordsman_style_registry()[nation_id] = style;
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
  static constexpr float kShoulderWidth = 1.02F;
  static constexpr float kTorsoScale = 0.94F;
  static constexpr float kArmScale = 0.88F;

  auto get_proportion_scaling() const -> QVector3D override {

    return {kShoulderWidth, kTorsoScale, kArmScale};
  }

private:
  mutable std::unordered_map<uint32_t, KnightExtras> m_extrasCache;

public:
  void get_variant(const DrawContext &ctx, uint32_t seed,
                   HumanoidVariant &v) const override {
    QVector3D const team_tint = resolveTeamTint(ctx);
    v.palette = makeHumanoidPalette(team_tint, seed);
    auto const &style = resolve_style(ctx);
    apply_palette_overrides(style, team_tint, v);
  }

  void customize_pose(const DrawContext &,
                      const HumanoidAnimationContext &anim_ctx, uint32_t seed,
                      HumanoidPose &pose) const override {
    using HP = HumanProportions;

    const AnimationInputs &anim = anim_ctx.inputs;
    HumanoidPoseController controller(pose, anim_ctx);

    float const arm_height_jitter = (hash_01(seed ^ 0xABCDU) - 0.5F) * 0.03F;
    float const arm_asymmetry = (hash_01(seed ^ 0xDEF0U) - 0.5F) * 0.04F;

    if (anim.is_attacking && anim.is_melee) {
      float const attack_phase =
          std::fmod(anim.time * KNIGHT_INV_ATTACK_CYCLE_TIME, 1.0F);
      controller.swordSlash(attack_phase);
    } else {
      QVector3D const idle_hand_r(0.30F + arm_asymmetry,
                                  HP::SHOULDER_Y - 0.02F + arm_height_jitter,
                                  0.35F);
      QVector3D const idle_hand_l(-0.22F - 0.5F * arm_asymmetry,
                                  HP::SHOULDER_Y + 0.5F * arm_height_jitter,
                                  0.18F);

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

    bool const is_attacking = anim.is_attacking && anim.is_melee;

    auto &registry = EquipmentRegistry::instance();

    auto sword = registry.get(EquipmentCategory::Weapon, "sword_roman");
    if (sword) {
      SwordRenderConfig sword_config;
      sword_config.metal_color = extras.metalColor;
      sword_config.sword_length = extras.swordLength;
      sword_config.sword_width = extras.swordWidth;
      sword_config.guard_half_width = extras.guard_half_width;
      sword_config.handle_radius = extras.handleRadius;
      sword_config.pommel_radius = extras.pommelRadius;
      sword_config.blade_ricasso = extras.bladeRicasso;
      sword_config.blade_taper_bias = extras.bladeTaperBias;
      sword_config.has_scabbard = extras.hasScabbard;

      auto *sword_renderer = dynamic_cast<SwordRenderer *>(sword.get());
      if (sword_renderer) {
        sword_renderer->setConfig(sword_config);
      }
      sword->render(ctx, pose.body_frames, v.palette, anim_ctx, out);
    }

    auto shield = registry.get(EquipmentCategory::Weapon, "shield_roman");
    if (shield) {
      shield->render(ctx, pose.body_frames, v.palette, anim_ctx, out);
    }

    if (!is_attacking && extras.hasScabbard) {
      drawScabbard(ctx, pose, v, extras, out);
    }
  }

  void draw_helmet(const DrawContext &ctx, const HumanoidVariant &v,
                   const HumanoidPose &pose, ISubmitter &out) const override {

    auto &registry = EquipmentRegistry::instance();
    auto helmet = registry.get(EquipmentCategory::Helmet, "roman_heavy");
    if (helmet) {
      HumanoidAnimationContext anim_ctx{};
      helmet->render(ctx, pose.body_frames, v.palette, anim_ctx, out);
    }
  }

  void draw_armor(const DrawContext &ctx, const HumanoidVariant &v,
                  const HumanoidPose &pose,
                  const HumanoidAnimationContext &anim,
                  ISubmitter &out) const override {
    auto &registry = EquipmentRegistry::instance();
    auto armor = registry.get(EquipmentCategory::Armor, "roman_heavy_armor");
    if (armor) {
      armor->render(ctx, pose.body_frames, v.palette, anim, out);
    }

    auto shoulder_cover =
        registry.get(EquipmentCategory::Armor, "roman_shoulder_cover");
    if (shoulder_cover) {
      shoulder_cover->render(ctx, pose.body_frames, v.palette, anim, out);
    }
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
  }

  auto
  resolve_style(const DrawContext &ctx) const -> const KnightStyleConfig & {
    ensure_swordsman_styles_registered();
    auto &styles = swordsman_style_registry();
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
    auto it_default = styles.find(std::string(k_swordsman_default_style_key));
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
    return QStringLiteral("swordsman");
  }

private:
  void apply_palette_overrides(const KnightStyleConfig &style,
                               const QVector3D &team_tint,
                               HumanoidVariant &variant) const {
    auto apply_color = [&](const std::optional<QVector3D> &override_color,
                           QVector3D &target) {
      target = mix_palette_color(target, override_color, team_tint,
                                 k_swordsman_team_mix_weight,
                                 k_swordsman_style_mix_weight);
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
                                     k_swordsman_team_mix_weight,
                                     k_swordsman_style_mix_weight);
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
  ensure_swordsman_styles_registered();
  static KnightRenderer const renderer;
  registry.register_renderer(
      "troops/roman/swordsman", [](const DrawContext &ctx, ISubmitter &out) {
        static KnightRenderer const static_renderer;
        Shader *swordsman_shader = nullptr;
        if (ctx.backend != nullptr) {
          QString shader_key = static_renderer.resolve_shader_key(ctx);
          swordsman_shader = ctx.backend->shader(shader_key);
          if (swordsman_shader == nullptr) {
            swordsman_shader = ctx.backend->shader(QStringLiteral("swordsman"));
          }
        }
        auto *scene_renderer = dynamic_cast<Renderer *>(&out);
        if ((scene_renderer != nullptr) && (swordsman_shader != nullptr)) {
          scene_renderer->setCurrentShader(swordsman_shader);
        }
        static_renderer.render(ctx, out);
        if (scene_renderer != nullptr) {
          scene_renderer->setCurrentShader(nullptr);
        }
      });
}

} // namespace Render::GL::Roman
