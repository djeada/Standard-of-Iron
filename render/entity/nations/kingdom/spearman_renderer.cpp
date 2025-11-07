#include "spearman_renderer.h"
#include "../../../../game/core/component.h"
#include "../../../../game/systems/nation_id.h"
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

#include "../../../equipment/equipment_registry.h"
#include "../../../equipment/weapons/spear_renderer.h"
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

namespace Render::GL::Kingdom {

namespace {

constexpr std::string_view k_spearman_default_style_key = "default";
constexpr float k_spearman_team_mix_weight = 0.6F;
constexpr float k_spearman_style_mix_weight = 0.4F;

constexpr float k_kneel_depth_multiplier = 0.875F;
constexpr float k_lean_amount_multiplier = 0.67F;

struct SpearmanShaderResourcePaths {
  QString vertex;
  QString fragment;
};

auto lookup_spearman_shader_resources(const QString &shader_key)
    -> std::optional<SpearmanShaderResourcePaths> {
  if (shader_key == QStringLiteral("spearman_carthage")) {
    return SpearmanShaderResourcePaths{
        QStringLiteral(":/assets/shaders/spearman_carthage.vert"),
        QStringLiteral(":/assets/shaders/spearman_carthage.frag")};
  }
  if (shader_key == QStringLiteral("spearman_kingdom_of_iron")) {
    return SpearmanShaderResourcePaths{
        QStringLiteral(":/assets/shaders/spearman_kingdom_of_iron.vert"),
        QStringLiteral(":/assets/shaders/spearman_kingdom_of_iron.frag")};
  }
  if (shader_key == QStringLiteral("spearman_roman_republic")) {
    return SpearmanShaderResourcePaths{
        QStringLiteral(":/assets/shaders/spearman_roman_republic.vert"),
        QStringLiteral(":/assets/shaders/spearman_roman_republic.frag")};
  }
  return std::nullopt;
}

auto spearman_style_registry()
    -> std::unordered_map<std::string, SpearmanStyleConfig> & {
  static std::unordered_map<std::string, SpearmanStyleConfig> styles;
  return styles;
}

void ensure_spearman_styles_registered() {
  static const bool registered = []() {
    register_kingdom_spearman_style();
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
  auto get_proportion_scaling() const -> QVector3D override {
    return {1.10F, 1.02F, 1.05F};
  }

private:
  mutable std::unordered_map<uint32_t, SpearmanExtras> m_extrasCache;

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

    if (anim.isInHoldMode || anim.isExitingHold) {
      float const t = anim.isInHoldMode ? 1.0F : (1.0F - anim.holdExitProgress);

      controller.kneel(t * k_kneel_depth_multiplier);
      controller.lean(QVector3D(0.0F, 0.0F, 1.0F),
                      t * k_lean_amount_multiplier);

      float const lowered_shoulder_y = pose.shoulderL.y();
      float const pelvis_y = pose.pelvisPos.y();

      QVector3D const hand_r_pos(0.18F * (1.0F - t) + 0.22F * t,
                                 lowered_shoulder_y * (1.0F - t) +
                                     (pelvis_y + 0.05F) * t,
                                 0.15F * (1.0F - t) + 0.20F * t);

      QVector3D const hand_l_pos(0.0F,
                                 lowered_shoulder_y * (1.0F - t) +
                                     (lowered_shoulder_y - 0.10F) * t,
                                 0.30F * (1.0F - t) + 0.55F * t);

      controller.placeHandAt(false, hand_r_pos);
      controller.placeHandAt(true, hand_l_pos);

    } else if (anim.is_attacking && anim.isMelee && !anim.isInHoldMode) {
      float const attack_phase =
          std::fmod(anim.time * SPEARMAN_INV_ATTACK_CYCLE_TIME, 1.0F);
      controller.spearThrust(attack_phase);
    } else {
      QVector3D const idle_hand_r(0.28F + arm_asymmetry,
                                  HP::SHOULDER_Y - 0.02F + arm_height_jitter,
                                  0.30F);
      QVector3D const idle_hand_l(
          -0.08F - 0.5F * arm_asymmetry,
          HP::SHOULDER_Y - 0.08F + 0.5F * arm_height_jitter, 0.45F);

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

    auto &registry = EquipmentRegistry::instance();

    auto spear = registry.get(EquipmentCategory::Weapon, "spear");
    if (spear) {
      SpearRenderConfig spear_config;
      spear_config.shaft_color = extras.spearShaftColor;
      spear_config.spearhead_color = extras.spearheadColor;
      spear_config.spear_length = extras.spearLength;
      spear_config.shaft_radius = extras.spearShaftRadius;
      spear_config.spearhead_length = extras.spearheadLength;

      auto *spear_renderer = dynamic_cast<SpearRenderer *>(spear.get());
      if (spear_renderer) {
        spear_renderer->setConfig(spear_config);
      }
      spear->render(ctx, pose.bodyFrames, v.palette, anim_ctx, out);
    }
  }

  void draw_helmet(const DrawContext &ctx, const HumanoidVariant &v,
                   const HumanoidPose &pose, ISubmitter &out) const override {

    auto &registry = EquipmentRegistry::instance();
    auto helmet = registry.get(EquipmentCategory::Helmet, "kingdom_heavy");
    if (helmet) {
      HumanoidAnimationContext anim_ctx{};
      helmet->render(ctx, pose.bodyFrames, v.palette, anim_ctx, out);
    }
  }

  void draw_armor(const DrawContext &ctx, const HumanoidVariant &v,
                  const HumanoidPose &pose,
                  const HumanoidAnimationContext &anim,
                  ISubmitter &out) const override {
    auto &registry = EquipmentRegistry::instance();
    auto armor = registry.get(EquipmentCategory::Armor, "kingdom_heavy_armor");
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
  registry.register_renderer(
      "troops/kingdom/spearman", [](const DrawContext &ctx, ISubmitter &out) {
        static SpearmanRenderer const static_renderer;
        Shader *spearman_shader = nullptr;
        auto acquireShader = [&](const QString &shader_key) -> Shader * {
          if (ctx.backend == nullptr || shader_key.isEmpty()) {
            return nullptr;
          }
          Shader *shader = ctx.backend->shader(shader_key);
          if (shader != nullptr) {
            return shader;
          }
          if (auto resources = lookup_spearman_shader_resources(shader_key)) {
            shader = ctx.backend->getOrLoadShader(shader_key, resources->vertex,
                                                  resources->fragment);
          }
          return shader;
        };
        if (ctx.backend != nullptr) {
          QString shader_key = static_renderer.resolve_shader_key(ctx);
          spearman_shader = acquireShader(shader_key);
          if (spearman_shader == nullptr) {
            spearman_shader = acquireShader(QStringLiteral("spearman"));
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

} // namespace Render::GL::Kingdom
