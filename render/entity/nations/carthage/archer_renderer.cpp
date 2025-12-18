#include "archer_renderer.h"
#include "../../../../game/core/component.h"
#include "../../../../game/core/entity.h"
#include "../../../../game/systems/nation_id.h"
#include "../../../equipment/armor/cloak_renderer.h"
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
#include "../../../humanoid/pose_controller.h"
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
#include <algorithm>
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

namespace Render::GL::Carthage {

namespace {

constexpr std::string_view k_default_style_key = "default";
constexpr std::string_view k_attachment_headwrap = "carthage_headwrap";

constexpr float k_kneel_depth_multiplier = 1.125F;
constexpr float k_lean_amount_multiplier = 0.83F;

auto style_registry() -> std::unordered_map<std::string, ArcherStyleConfig> & {
  static std::unordered_map<std::string, ArcherStyleConfig> styles;
  return styles;
}

void ensure_archer_styles_registered() {
  static const bool registered = []() {
    register_carthage_archer_style();
    return true;
  }();
  (void)registered;
}

constexpr float k_team_mix_weight = 0.65F;
constexpr float k_style_mix_weight = 0.35F;

} 

void register_archer_style(const std::string &nation_id,
                           const ArcherStyleConfig &style) {
  style_registry()[nation_id] = style;
}

using Render::Geom::clamp01;
using Render::Geom::clampf;
using Render::Geom::cone_from_to;
using Render::Geom::cylinder_between;
using Render::Geom::sphere_at;
using Render::GL::Humanoid::mix_palette_color;
using Render::GL::Humanoid::saturate_color;

class ArcherRenderer : public HumanoidRendererBase {
public:
  auto get_proportion_scaling() const -> QVector3D override {

    return {1.03F, 1.08F, 0.98F};
  }

  void adjust_variation(const DrawContext &, uint32_t,
                        VariationParams &variation) const override {
    variation.height_scale *= 1.06F;
    variation.bulk_scale *= 0.90F;
    variation.stance_width *= 0.90F;
    variation.arm_swing_amp *= 0.92F;
  }

  void get_variant(const DrawContext &ctx, uint32_t seed,
                   HumanoidVariant &v) const override {
    QVector3D const team_tint = resolve_team_tint(ctx);
    v.palette = make_humanoid_palette(team_tint, seed);
    auto const &style = resolve_style(ctx);
    apply_palette_overrides(style, team_tint, v);

    auto nextRand = [](uint32_t &s) -> float {
      s = s * 1664525U + 1013904223U;
      return float(s & 0x7FFFFFU) / float(0x7FFFFFU);
    };

    uint32_t beard_seed = seed ^ 0xBEAD01U;

    v.facial_hair.style = FacialHairStyle::None;

    v.muscularity = 0.95F + nextRand(beard_seed) * 0.25F;
    v.scarring = nextRand(beard_seed) * 0.30F;
    v.weathering = 0.40F + nextRand(beard_seed) * 0.40F;
  }

  void customize_pose(const DrawContext &,
                      const HumanoidAnimationContext &anim_ctx, uint32_t seed,
                      HumanoidPose &pose) const override {
    using HP = HumanProportions;

    const AnimationInputs &anim = anim_ctx.inputs;
    HumanoidPoseController controller(pose, anim_ctx);

    float const arm_height_jitter = (hash_01(seed ^ 0xABCDU) - 0.5F) * 0.03F;
    float const arm_asymmetry = (hash_01(seed ^ 0xDEF0U) - 0.5F) * 0.04F;

    float const bow_x = 0.0F;

    if (anim.is_in_hold_mode || anim.is_exiting_hold) {
      float const t =
          anim.is_in_hold_mode ? 1.0F : (1.0F - anim.hold_exit_progress);

      controller.kneel(t * k_kneel_depth_multiplier);
      controller.lean(QVector3D(0.0F, 0.0F, 1.0F),
                      t * k_lean_amount_multiplier);

      QVector3D const hold_hand_r(
          bow_x + 0.03F, controller.get_shoulder_y(false) + 0.30F, 0.55F);
      QVector3D const hold_hand_l(
          bow_x - 0.02F, controller.get_shoulder_y(true) + 0.12F, 0.55F);
      QVector3D const normal_hand_r(bow_x + 0.03F - arm_asymmetry,
                                    HP::SHOULDER_Y + 0.05F + arm_height_jitter,
                                    0.55F);
      QVector3D const normal_hand_l(
          bow_x - 0.02F + arm_asymmetry * 0.5F,
          HP::SHOULDER_Y + 0.12F + arm_height_jitter * 0.8F, 0.50F);

      QVector3D const blended_hand_r =
          normal_hand_r * (1.0F - t) + hold_hand_r * t;
      QVector3D const blended_hand_l =
          normal_hand_l * (1.0F - t) + hold_hand_l * t;

      controller.placeHandAt(false, blended_hand_r);
      controller.placeHandAt(true, blended_hand_l);
    } else {
      QVector3D const idle_hand_r(bow_x + 0.03F - arm_asymmetry,
                                  HP::SHOULDER_Y + 0.05F + arm_height_jitter,
                                  0.55F);
      QVector3D const idle_hand_l(
          bow_x - 0.05F + arm_asymmetry * 0.5F,
          HP::SHOULDER_Y + 0.14F + arm_height_jitter * 0.8F, 0.48F);

      controller.placeHandAt(false, idle_hand_r);
      controller.placeHandAt(true, idle_hand_l);
    }

    if (anim.is_attacking && !anim.is_in_hold_mode) {
      float const attack_phase =
          std::fmod(anim_ctx.attack_phase * ARCHER_INV_ATTACK_CYCLE_TIME, 1.0F);

      if (anim.is_melee) {
        controller.meleeStrike(attack_phase);
      } else {
        controller.aim_bow(attack_phase);
      }
    }
  }

  void add_attachments(const DrawContext &ctx, const HumanoidVariant &v,
                       const HumanoidPose &pose,
                       const HumanoidAnimationContext &anim_ctx,
                       ISubmitter &out) const override {
    using HP = HumanProportions;

    auto const &style = resolve_style(ctx);
    const AnimationInputs &anim = anim_ctx.inputs;
    QVector3D team_tint = resolve_team_tint(ctx);
    uint32_t seed = 0U;
    if (ctx.entity != nullptr) {
      auto *unit = ctx.entity->get_component<Engine::Core::UnitComponent>();
      if (unit != nullptr) {
        seed ^= uint32_t(unit->owner_id * 2654435761U);
      }
      seed ^= uint32_t(reinterpret_cast<uintptr_t>(ctx.entity) & 0xFFFFFFFFU);
    }

    auto tint = [&](float k) {
      return QVector3D(clamp01(team_tint.x() * k), clamp01(team_tint.y() * k),
                       clamp01(team_tint.z() * k));
    };
    QVector3D const fletch = tint(0.9F);

    auto &registry = EquipmentRegistry::instance();

    if (style.show_cape) {
      auto cloak = registry.get(EquipmentCategory::Armor, "cloak_carthage");
      if (cloak) {
        CloakConfig cloak_config;
        if (style.cape_color) {
          cloak_config.primary_color = *style.cape_color;
        } else {
          cloak_config.primary_color = QVector3D(0.14F, 0.38F, 0.54F);
        }
        cloak_config.trim_color = v.palette.metal;

        auto *cloak_renderer = dynamic_cast<CloakRenderer *>(cloak.get());
        if (cloak_renderer) {
          cloak_renderer->set_config(cloak_config);
        }

        cloak->render(ctx, pose.body_frames, v.palette, anim_ctx, out);
      }
    }

    auto quiver = registry.get(EquipmentCategory::Weapon, "quiver");
    if (quiver) {

      QuiverRenderConfig quiver_config;
      quiver_config.fletching_color = fletch;
      quiver_config.quiver_radius = HP::HEAD_RADIUS * 0.45F;

      auto *quiver_renderer = dynamic_cast<QuiverRenderer *>(quiver.get());
      if (quiver_renderer) {
        quiver_renderer->set_config(quiver_config);
      }

      quiver->render(ctx, pose.body_frames, v.palette, anim_ctx, out);
    }

    auto bow = registry.get(EquipmentCategory::Weapon, "bow_carthage");
    if (bow) {

      BowRenderConfig bow_config;
      bow_config.string_color = QVector3D(0.30F, 0.30F, 0.32F);
      bow_config.metal_color =
          Render::Geom::clampVec01(v.palette.metal * 1.15F);
      bow_config.fletching_color = fletch;
      bow_config.bow_top_y = HP::SHOULDER_Y + 0.55F;
      bow_config.bow_bot_y = HP::WAIST_Y - 0.25F;
      bow_config.bow_x = 0.0F;
      bow_config.arrow_visibility = ArrowVisibility::IdleAndAttackCycle;

      if (style.bow_string_color) {
        bow_config.string_color = saturate_color(*style.bow_string_color);
      }
      if (style.fletching_color) {
        bow_config.fletching_color = saturate_color(*style.fletching_color);
      }

      auto *bow_renderer = dynamic_cast<BowRenderer *>(bow.get());
      if (bow_renderer) {
        bow_renderer->set_config(bow_config);
      }

      bow->render(ctx, pose.body_frames, v.palette, anim_ctx, out);
    }
  }

  void draw_helmet(const DrawContext &ctx, const HumanoidVariant &v,
                   const HumanoidPose &pose, ISubmitter &out) const override {
    using HP = HumanProportions;

    auto const &style = resolve_style(ctx);
    auto &registry = EquipmentRegistry::instance();
    HumanoidAnimationContext anim_ctx{};

    if (!style.show_helmet) {
      if (style.attachment_profile == std::string(k_attachment_headwrap)) {

        auto headwrap = registry.get(EquipmentCategory::Helmet, "headwrap");
        if (headwrap) {
          headwrap->render(ctx, pose.body_frames, v.palette, anim_ctx, out);
        }
      }
      return;
    }

    auto helmet = registry.get(EquipmentCategory::Helmet, "carthage_light");
    if (helmet) {
      helmet->render(ctx, pose.body_frames, v.palette, anim_ctx, out);
    }
  }

  void draw_armor(const DrawContext &ctx, const HumanoidVariant &v,
                  const HumanoidPose &pose,
                  const HumanoidAnimationContext &anim,
                  ISubmitter &out) const override {
    if (resolve_style(ctx).show_armor) {
      auto &registry = EquipmentRegistry::instance();
      auto const &style = resolve_style(ctx);

      std::string armor_key =
          style.armor_id.empty() ? "armor_light_carthage" : style.armor_id;

      auto armor = registry.get(EquipmentCategory::Armor, armor_key);
      if (armor) {
        armor->render(ctx, pose.body_frames, v.palette, anim, out);
      }
    }
  }

private:
  auto
  resolve_style(const DrawContext &ctx) const -> const ArcherStyleConfig & {
    ensure_archer_styles_registered();
    auto &styles = style_registry();
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
};

void register_archer_renderer(Render::GL::EntityRendererRegistry &registry) {
  ensure_archer_styles_registered();
  static ArcherRenderer const renderer;
  registry.register_renderer(
      "troops/carthage/archer", [](const DrawContext &ctx, ISubmitter &out) {
        static ArcherRenderer const static_renderer;
        Shader *archer_shader = nullptr;
        if (ctx.backend != nullptr) {
          QString shader_key = static_renderer.resolve_shader_key(ctx);
          archer_shader = ctx.backend->shader(shader_key);
          if ((archer_shader == nullptr) &&
              shader_key == QStringLiteral("archer_carthage")) {
            archer_shader = ctx.backend->get_or_load_shader(
                shader_key,
                QStringLiteral(":/assets/shaders/archer_carthage.vert"),
                QStringLiteral(":/assets/shaders/archer_carthage.frag"));
          }
          if (archer_shader == nullptr) {
            archer_shader = ctx.backend->shader(QStringLiteral("archer"));
          }
        }
        auto *scene_renderer = dynamic_cast<Renderer *>(&out);
        if ((scene_renderer != nullptr) && (archer_shader != nullptr)) {
          scene_renderer->set_current_shader(archer_shader);
        }
        static_renderer.render(ctx, out);
        if (scene_renderer != nullptr) {
          scene_renderer->set_current_shader(nullptr);
        }
      });
}
} 
