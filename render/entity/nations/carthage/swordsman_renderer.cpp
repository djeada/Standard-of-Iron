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

namespace Render::GL::Carthage {

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
    register_carthage_swordsman_style();
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
using Render::Geom::clamp_f;
using Render::Geom::cone_from_to;
using Render::Geom::cylinder_between;
using Render::Geom::ease_in_out_cubic;
using Render::Geom::lerp;
using Render::Geom::nlerp;
using Render::Geom::smoothstep;
using Render::Geom::sphere_at;
using Render::GL::Humanoid::mix_palette_color;
using Render::GL::Humanoid::saturate_color;

struct KnightExtras {
  QVector3D metal_color;
  QVector3D shield_color;
  QVector3D shield_trim_color;
  float sword_length = 0.80F;
  float swordWidth = 0.065F;
  float shieldRadius = 0.18F;
  float shield_aspect = 1.0F;

  float guard_half_width = 0.12F;
  float handleRadius = 0.016F;
  float pommel_radius = 0.045F;
  float blade_ricasso = 0.16F;
  float blade_taper_bias = 0.65F;
  bool shieldCrossDecal = false;
  bool has_scabbard = true;
};

class KnightRenderer : public HumanoidRendererBase {
public:
  KnightRenderer() { cache_equipment(); }

  static constexpr float kLimbWidthScale = 0.90F;
  static constexpr float kTorsoWidthScale = 0.75F;
  static constexpr float kHeightScale = 1.03F;
  static constexpr float kDepthScale = 0.46F;

  auto get_proportion_scaling() const -> QVector3D override {

    return {kLimbWidthScale, kHeightScale, kDepthScale};
  }

  auto get_torso_scale() const -> float override { return kTorsoWidthScale; }

private:
  mutable std::unordered_map<uint32_t, KnightExtras> m_extrasCache;

public:
  void get_variant(const DrawContext &ctx, uint32_t seed,
                   HumanoidVariant &v) const override {
    QVector3D const team_tint = resolve_team_tint(ctx);
    v.palette = make_humanoid_palette(team_tint, seed);
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
          std::fmod(anim_ctx.attack_phase * KNIGHT_INV_ATTACK_CYCLE_TIME, 1.0F);
      controller.sword_slash_variant(attack_phase, anim.attack_variant);
    } else {
      QVector3D const idle_hand_r(0.30F + arm_asymmetry,
                                  HP::SHOULDER_Y - 0.02F + arm_height_jitter,
                                  0.35F);
      QVector3D const idle_hand_l(-0.22F - 0.5F * arm_asymmetry,
                                  HP::SHOULDER_Y + 0.5F * arm_height_jitter,
                                  0.18F);

      controller.place_hand_at(false, idle_hand_r);
      controller.place_hand_at(true, idle_hand_l);
    }
  }

  void add_attachments(const DrawContext &ctx, const HumanoidVariant &v,
                       const HumanoidPose &pose,
                       const HumanoidAnimationContext &anim_ctx,
                       ISubmitter &out) const override {
    const AnimationInputs &anim = anim_ctx.inputs;
    uint32_t const seed = reinterpret_cast<uintptr_t>(ctx.entity) & 0xFFFFFFFFU;
    auto const &style = resolve_style(ctx);
    QVector3D const team_tint = resolve_team_tint(ctx);

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

    if (m_cached_sword) {
      SwordRenderConfig sword_config;
      sword_config.metal_color = extras.metal_color;
      sword_config.sword_length = extras.sword_length;
      sword_config.sword_width = extras.swordWidth;
      sword_config.guard_half_width = extras.guard_half_width;
      sword_config.handle_radius = extras.handleRadius;
      sword_config.pommel_radius = extras.pommel_radius;
      sword_config.blade_ricasso = extras.blade_ricasso;
      sword_config.blade_taper_bias = extras.blade_taper_bias;
      sword_config.has_scabbard = extras.has_scabbard;

      auto *sword_renderer =
          dynamic_cast<SwordRenderer *>(m_cached_sword.get());
      if (sword_renderer) {
        sword_renderer->set_config(sword_config);
      }
      m_cached_sword->render(ctx, pose.body_frames, v.palette, anim_ctx, out);
    }

    if (m_cached_shield) {
      m_cached_shield->render(ctx, pose.body_frames, v.palette, anim_ctx, out);
    }

    if (!is_attacking && extras.has_scabbard) {
      drawScabbard(ctx, pose, v, extras, out);
    }
  }

  void draw_helmet(const DrawContext &ctx, const HumanoidVariant &v,
                   const HumanoidPose &pose, ISubmitter &out) const override {

    if (m_cached_helmet) {
      HumanoidAnimationContext anim_ctx{};
      m_cached_helmet->render(ctx, pose.body_frames, v.palette, anim_ctx, out);
    }
  }

  void draw_armor(const DrawContext &ctx, const HumanoidVariant &v,
                  const HumanoidPose &pose,
                  const HumanoidAnimationContext &anim,
                  ISubmitter &out) const override {
    if (m_cached_armor) {
      m_cached_armor->render(ctx, pose.body_frames, v.palette, anim, out);
    }

    if (m_cached_shoulder_cover) {
      m_cached_shoulder_cover->render(ctx, pose.body_frames, v.palette, anim,
                                      out);
    }
  }

private:
  void cache_equipment() {
    auto &registry = EquipmentRegistry::instance();
    m_cached_sword = registry.get(EquipmentCategory::Weapon, "sword_carthage");
    m_cached_shield =
        registry.get(EquipmentCategory::Weapon, "shield_carthage");
    m_cached_helmet = registry.get(EquipmentCategory::Helmet, "carthage_heavy");
    m_cached_armor =
        registry.get(EquipmentCategory::Armor, "armor_heavy_carthage");
    m_cached_shoulder_cover =
        registry.get(EquipmentCategory::Armor, "carthage_shoulder_cover");
  }

  mutable std::shared_ptr<IEquipmentRenderer> m_cached_sword;
  mutable std::shared_ptr<IEquipmentRenderer> m_cached_shield;
  mutable std::shared_ptr<IEquipmentRenderer> m_cached_helmet;
  mutable std::shared_ptr<IEquipmentRenderer> m_cached_armor;
  mutable std::shared_ptr<IEquipmentRenderer> m_cached_shoulder_cover;

  static auto computeKnightExtras(uint32_t seed,
                                  const HumanoidVariant &v) -> KnightExtras {
    KnightExtras e;

    e.metal_color = QVector3D(0.72F, 0.73F, 0.78F);

    float const shield_hue = hash_01(seed ^ 0x12345U);
    if (shield_hue < 0.45F) {
      e.shield_color = v.palette.cloth * 1.10F;
    } else if (shield_hue < 0.90F) {
      e.shield_color = v.palette.leather * 1.25F;
    } else {

      e.shield_color = e.metal_color * 0.95F;
    }

    e.sword_length = 0.80F + (hash_01(seed ^ 0xABCDU) - 0.5F) * 0.16F;
    e.swordWidth = 0.060F + (hash_01(seed ^ 0x7777U) - 0.5F) * 0.010F;
    e.shieldRadius = 0.16F + (hash_01(seed ^ 0xDEF0U) - 0.5F) * 0.04F;

    e.guard_half_width = 0.120F + (hash_01(seed ^ 0x3456U) - 0.5F) * 0.020F;
    e.handleRadius = 0.016F + (hash_01(seed ^ 0x88AAU) - 0.5F) * 0.003F;
    e.pommel_radius = 0.045F + (hash_01(seed ^ 0x19C3U) - 0.5F) * 0.006F;

    e.blade_ricasso =
        clamp_f(0.14F + (hash_01(seed ^ 0xBEEFU) - 0.5F) * 0.04F, 0.10F, 0.20F);
    e.blade_taper_bias =
        clamp01(0.6F + (hash_01(seed ^ 0xFACEU) - 0.5F) * 0.2F);

    e.shieldCrossDecal = (hash_01(seed ^ 0xA11CU) > 0.55F);
    e.has_scabbard = (hash_01(seed ^ 0x5CABU) > 0.15F);
    e.shield_trim_color = e.metal_color * 0.95F;
    e.shield_aspect = 1.0F;
    return e;
  }

  static void drawScabbard(const DrawContext &ctx, const HumanoidPose &,
                           const HumanoidVariant &v, const KnightExtras &extras,
                           ISubmitter &out) {
    using HP = HumanProportions;

    QVector3D const hip(0.10F, HP::WAIST_Y - 0.04F, -0.02F);
    QVector3D const tip = hip + QVector3D(-0.05F, -0.22F, -0.12F);
    float const sheath_r = extras.swordWidth * 0.85F;

    out.mesh(get_unit_cylinder(),
             cylinder_between(ctx.model, hip, tip, sheath_r),
             v.palette.leather * 0.9F, nullptr, 1.0F);

    out.mesh(get_unit_cone(),
             cone_from_to(ctx.model, tip,
                          tip + QVector3D(-0.02F, -0.02F, -0.02F), sheath_r),
             extras.metal_color, nullptr, 1.0F);
  }

  auto
  resolve_style(const DrawContext &ctx) const -> const KnightStyleConfig & {
    ensure_swordsman_styles_registered();
    auto &styles = swordsman_style_registry();
    std::string nation_id;
    if (ctx.entity != nullptr) {
      if (auto *unit =
              ctx.entity->get_component<Engine::Core::UnitComponent>()) {
        nation_id = Game::Systems::nation_id_to_string(unit->nation_id);
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
    apply_color(style.leather_dark_color, variant.palette.leather_dark);
    apply_color(style.metal_color, variant.palette.metal);
  }

  void apply_extras_overrides(const KnightStyleConfig &style,
                              const QVector3D &team_tint,
                              const HumanoidVariant &variant,
                              KnightExtras &extras) const {
    extras.metal_color = saturate_color(variant.palette.metal);
    extras.shield_color = saturate_color(extras.shield_color);
    extras.shield_trim_color = saturate_color(extras.shield_trim_color);

    auto apply_shield_color =
        [&](const std::optional<QVector3D> &override_color, QVector3D &target) {
          target = mix_palette_color(target, override_color, team_tint,
                                     k_swordsman_team_mix_weight,
                                     k_swordsman_style_mix_weight);
        };

    apply_shield_color(style.shield_color, extras.shield_color);
    apply_shield_color(style.shield_trim_color, extras.shield_trim_color);

    if (style.shield_radius_scale) {
      extras.shieldRadius =
          std::max(0.10F, extras.shieldRadius * *style.shield_radius_scale);
    }
    if (style.shield_aspect_ratio) {
      extras.shield_aspect = std::max(0.40F, *style.shield_aspect_ratio);
    }
    if (style.has_scabbard) {
      extras.has_scabbard = *style.has_scabbard;
    }
    if (style.shield_cross_decal) {
      extras.shieldCrossDecal = *style.shield_cross_decal;
    }
  }
};

void register_knight_renderer(Render::GL::EntityRendererRegistry &registry) {
  ensure_swordsman_styles_registered();
  static KnightRenderer const renderer;
  registry.register_renderer(
      "troops/carthage/swordsman", [](const DrawContext &ctx, ISubmitter &out) {
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
          scene_renderer->set_current_shader(swordsman_shader);
        }
        static_renderer.render(ctx, out);
        if (scene_renderer != nullptr) {
          scene_renderer->set_current_shader(nullptr);
        }
      });
}

} // namespace Render::GL::Carthage
