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

    return {1.15F, 1.02F, 0.75F};
  }

  void getVariant(const DrawContext &ctx, uint32_t seed,
                  HumanoidVariant &v) const override {
    QVector3D const team_tint = resolveTeamTint(ctx);
    v.palette = makeHumanoidPalette(team_tint, seed);
    auto const &style = resolve_style(ctx);
    apply_palette_overrides(style, team_tint, v);

    auto nextRand = [](uint32_t &s) -> float {
      s = s * 1664525U + 1013904223U;
      return float(s & 0x7FFFFFU) / float(0x7FFFFFU);
    };

    uint32_t beard_seed = seed ^ 0xBEAD01U;

    float const beard_chance = nextRand(beard_seed);
    bool const wants_beard = style.force_beard || (beard_chance < 0.85F);

    if (wants_beard) {

      float const style_roll = nextRand(beard_seed);

      if (style_roll < 0.50F) {

        v.facialHair.style = FacialHairStyle::FullBeard;
        v.facialHair.length = 0.9F + nextRand(beard_seed) * 0.6F;
      } else if (style_roll < 0.75F) {

        v.facialHair.style = FacialHairStyle::LongBeard;
        v.facialHair.length = 1.2F + nextRand(beard_seed) * 0.8F;
      } else if (style_roll < 0.90F) {

        v.facialHair.style = FacialHairStyle::ShortBeard;
        v.facialHair.length = 0.8F + nextRand(beard_seed) * 0.4F;
      } else {

        v.facialHair.style = FacialHairStyle::Goatee;
        v.facialHair.length = 0.9F + nextRand(beard_seed) * 0.5F;
      }

      float const color_roll = nextRand(beard_seed);
      if (color_roll < 0.60F) {

        v.facialHair.color = QVector3D(0.18F + nextRand(beard_seed) * 0.10F,
                                       0.14F + nextRand(beard_seed) * 0.08F,
                                       0.10F + nextRand(beard_seed) * 0.06F);
      } else if (color_roll < 0.85F) {

        v.facialHair.color = QVector3D(0.30F + nextRand(beard_seed) * 0.12F,
                                       0.24F + nextRand(beard_seed) * 0.10F,
                                       0.16F + nextRand(beard_seed) * 0.08F);
      } else {

        v.facialHair.color = QVector3D(0.35F + nextRand(beard_seed) * 0.10F,
                                       0.20F + nextRand(beard_seed) * 0.08F,
                                       0.12F + nextRand(beard_seed) * 0.06F);
      }

      v.facialHair.thickness = 0.85F + nextRand(beard_seed) * 0.35F;
      v.facialHair.coverage = 0.75F + nextRand(beard_seed) * 0.25F;

      if (nextRand(beard_seed) < 0.10F) {
        v.facialHair.greyness = 0.15F + nextRand(beard_seed) * 0.35F;
      } else {
        v.facialHair.greyness = 0.0F;
      }
    } else {

      v.facialHair.style = FacialHairStyle::None;
    }

    v.muscularity = 0.95F + nextRand(beard_seed) * 0.25F;
    v.scarring = nextRand(beard_seed) * 0.30F;
    v.weathering = 0.40F + nextRand(beard_seed) * 0.40F;
  }

  void customizePose(const DrawContext &,
                     const HumanoidAnimationContext &anim_ctx, uint32_t seed,
                     HumanoidPose &pose) const override {
    using HP = HumanProportions;

    const AnimationInputs &anim = anim_ctx.inputs;
    HumanoidPoseController controller(pose, anim_ctx);

    float const arm_height_jitter = (hash_01(seed ^ 0xABCDU) - 0.5F) * 0.03F;
    float const arm_asymmetry = (hash_01(seed ^ 0xDEF0U) - 0.5F) * 0.04F;

    float const bow_x = 0.0F;

    if (anim.isInHoldMode || anim.isExitingHold) {
      float const t = anim.isInHoldMode ? 1.0F : (1.0F - anim.holdExitProgress);

      controller.kneel(t * k_kneel_depth_multiplier);
      controller.lean(QVector3D(0.0F, 0.0F, 1.0F), t * k_lean_amount_multiplier);

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

      QVector3D const blended_hand_l =
          normal_hand_l * (1.0F - t) + hold_hand_l * t;
      QVector3D const blended_hand_r =
          normal_hand_r * (1.0F - t) + hold_hand_r * t;

      controller.placeHandAt(true, blended_hand_l);
      controller.placeHandAt(false, blended_hand_r);
    } else {
      QVector3D const idle_hand_l(bow_x - 0.05F + arm_asymmetry,
                                  HP::SHOULDER_Y + 0.05F + arm_height_jitter,
                                  0.55F);
      QVector3D const idle_hand_r(
          0.15F - arm_asymmetry * 0.5F,
          HP::SHOULDER_Y + 0.15F + arm_height_jitter * 0.8F, 0.20F);

      controller.placeHandAt(true, idle_hand_l);
      controller.placeHandAt(false, idle_hand_r);
    }

    if (anim.is_attacking && !anim.isInHoldMode) {
      float const attack_phase =
          std::fmod(anim.time * ARCHER_INV_ATTACK_CYCLE_TIME, 1.0F);

      if (anim.isMelee) {
        QVector3D const rest_pos(0.25F, HP::SHOULDER_Y, 0.10F);
        QVector3D const raised_pos(0.30F, HP::HEAD_TOP_Y + 0.2F, -0.05F);
        QVector3D const strike_pos(0.35F, HP::WAIST_Y, 0.45F);

        QVector3D hand_r_target;
        QVector3D hand_l_target;

        if (attack_phase < 0.25F) {
          float t = attack_phase / 0.25F;
          t = t * t;
          hand_r_target = rest_pos * (1.0F - t) + raised_pos * t;
          hand_l_target = QVector3D(-0.15F, HP::SHOULDER_Y - 0.1F * t, 0.20F);
        } else if (attack_phase < 0.35F) {
          hand_r_target = raised_pos;
          hand_l_target = QVector3D(-0.15F, HP::SHOULDER_Y - 0.1F, 0.20F);
        } else if (attack_phase < 0.55F) {
          float t = (attack_phase - 0.35F) / 0.2F;
          t = t * t * t;
          hand_r_target = raised_pos * (1.0F - t) + strike_pos * t;
          hand_l_target =
              QVector3D(-0.15F, HP::SHOULDER_Y - 0.1F * (1.0F - t * 0.5F),
                        0.20F + 0.15F * t);
        } else {
          float t = (attack_phase - 0.55F) / 0.45F;
          t = 1.0F - (1.0F - t) * (1.0F - t);
          hand_r_target = strike_pos * (1.0F - t) + rest_pos * t;
          hand_l_target = QVector3D(-0.15F, HP::SHOULDER_Y - 0.05F * (1.0F - t),
                                    0.35F * (1.0F - t) + 0.20F * t);
        }

        controller.placeHandAt(true, hand_l_target);
        controller.placeHandAt(false, hand_r_target);
      } else {
        QVector3D const aim_pos(0.18F, HP::SHOULDER_Y + 0.18F, 0.35F);
        QVector3D const draw_pos(0.22F, HP::SHOULDER_Y + 0.10F, -0.30F);
        QVector3D const release_pos(0.18F, HP::SHOULDER_Y + 0.20F, 0.10F);

        QVector3D hand_r_target;
        float shoulder_twist = 0.0F;
        float head_recoil = 0.0F;

        if (attack_phase < 0.20F) {
          float t = attack_phase / 0.20F;
          t = t * t;
          hand_r_target = aim_pos * (1.0F - t) + draw_pos * t;
          shoulder_twist = t * 0.08F;
        } else if (attack_phase < 0.50F) {
          hand_r_target = draw_pos;
          shoulder_twist = 0.08F;
        } else if (attack_phase < 0.58F) {
          float t = (attack_phase - 0.50F) / 0.08F;
          t = t * t * t;
          hand_r_target = draw_pos * (1.0F - t) + release_pos * t;
          shoulder_twist = 0.08F * (1.0F - t * 0.6F);
          head_recoil = t * 0.04F;
        } else {
          float t = (attack_phase - 0.58F) / 0.42F;
          t = 1.0F - (1.0F - t) * (1.0F - t);
          hand_r_target = release_pos * (1.0F - t) + aim_pos * t;
          shoulder_twist = 0.08F * 0.4F * (1.0F - t);
          head_recoil = 0.04F * (1.0F - t);
        }

        QVector3D const hand_l_target(bow_x - 0.05F, HP::SHOULDER_Y + 0.05F,
                                      0.55F);

        controller.placeHandAt(true, hand_l_target);
        controller.placeHandAt(false, hand_r_target);

        pose.shoulderR.setY(pose.shoulderR.y() + shoulder_twist);
        pose.shoulderL.setY(pose.shoulderL.y() - shoulder_twist * 0.5F);
        pose.headPos.setZ(pose.headPos.z() - head_recoil);
      }
    }
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

    auto tint = [&](float k) {
      return QVector3D(clamp01(team_tint.x() * k), clamp01(team_tint.y() * k),
                       clamp01(team_tint.z() * k));
    };
    QVector3D const fletch = tint(0.9F);

    auto &registry = EquipmentRegistry::instance();
    auto quiver = registry.get(EquipmentCategory::Weapon, "quiver");
    if (quiver) {

      QuiverRenderConfig quiver_config;
      quiver_config.fletching_color = fletch;
      quiver_config.quiver_radius = HP::HEAD_RADIUS * 0.45F;

      auto *quiver_renderer = dynamic_cast<QuiverRenderer *>(quiver.get());
      if (quiver_renderer) {
        quiver_renderer->setConfig(quiver_config);
      }

      quiver->render(ctx, pose.bodyFrames, v.palette, anim_ctx, out);
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

      if (style.bow_string_color) {
        bow_config.string_color = saturate_color(*style.bow_string_color);
      }
      if (style.fletching_color) {
        bow_config.fletching_color = saturate_color(*style.fletching_color);
      }

      auto *bow_renderer = dynamic_cast<BowRenderer *>(bow.get());
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
    auto &registry = EquipmentRegistry::instance();
    HumanoidAnimationContext anim_ctx{};

    if (!style.show_helmet) {
      if (style.attachment_profile == std::string(k_attachment_headwrap)) {

        auto headwrap = registry.get(EquipmentCategory::Helmet, "headwrap");
        if (headwrap) {
          headwrap->render(ctx, pose.bodyFrames, v.palette, anim_ctx, out);
        }
      }
      return;
    }

    auto helmet = registry.get(EquipmentCategory::Helmet, "montefortino");
    if (helmet) {
      helmet->render(ctx, pose.bodyFrames, v.palette, anim_ctx, out);
    }
  }

  void drawArmor(const DrawContext &ctx, const HumanoidVariant &v,
                 const HumanoidPose &pose, const HumanoidAnimationContext &anim,
                 ISubmitter &out) const override {
    if (resolve_style(ctx).show_armor) {
      auto &registry = EquipmentRegistry::instance();
      auto armor =
          registry.get(EquipmentCategory::Armor, "carthage_light_armor");
      if (armor) {
        armor->render(ctx, pose.bodyFrames, v.palette, anim, out);
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
};

void registerArcherRenderer(Render::GL::EntityRendererRegistry &registry) {
  ensure_archer_styles_registered();
  static ArcherRenderer const renderer;
  registry.registerRenderer(
      "troops/carthage/archer", [](const DrawContext &ctx, ISubmitter &out) {
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
} // namespace Render::GL::Carthage
