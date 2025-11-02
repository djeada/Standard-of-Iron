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
                 const HumanoidPose &pose,
                 const HumanoidAnimationContext &anim,
                 ISubmitter &out) const override {
    if (resolve_style(ctx).show_armor) {
      auto &registry = EquipmentRegistry::instance();
      auto armor = registry.get(EquipmentCategory::Armor, "carthage_light_armor");
      if (armor) {
        armor->render(ctx, pose.bodyFrames, v.palette, anim, out);
      }
    }
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
