#include "builder_renderer.h"
#include "../../../../game/core/component.h"
#include "../../../../game/core/entity.h"
#include "../../../../game/systems/nation_id.h"
#include "../../../equipment/equipment_registry.h"
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
#include "builder_style.h"

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

using Render::Geom::cylinder_between;
using Render::Geom::sphere_at;

namespace Render::GL::Carthage {

namespace {

constexpr std::string_view k_default_style_key = "default";

auto style_registry() -> std::unordered_map<std::string, BuilderStyleConfig> & {
  static std::unordered_map<std::string, BuilderStyleConfig> styles;
  return styles;
}

void ensure_builder_styles_registered() {
  static const bool registered = []() {
    register_carthage_builder_style();
    return true;
  }();
  (void)registered;
}

constexpr float k_team_mix_weight = 0.65F;
constexpr float k_style_mix_weight = 0.35F;

} // namespace

void register_builder_style(const std::string &nation_id,
                            const BuilderStyleConfig &style) {
  style_registry()[nation_id] = style;
}

using Render::Geom::clamp01;
using Render::Geom::clampf;
using Render::GL::Humanoid::mix_palette_color;
using Render::GL::Humanoid::saturate_color;

class BuilderRenderer : public HumanoidRendererBase {
public:
  auto get_proportion_scaling() const -> QVector3D override {
    return {0.94F, 1.02F, 0.92F};
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

    uint32_t beard_seed = seed ^ 0x0EA101U;
    bool wants_beard = style.force_beard;
    if (!wants_beard) {
      float const beard_roll = nextRand(beard_seed);
      wants_beard = (beard_roll < 0.80F);
    }

    if (wants_beard) {
      float const style_roll = nextRand(beard_seed);

      if (style_roll < 0.50F) {
        v.facial_hair.style = FacialHairStyle::ShortBeard;
        v.facial_hair.length = 0.8F + nextRand(beard_seed) * 0.4F;
      } else if (style_roll < 0.80F) {
        v.facial_hair.style = FacialHairStyle::FullBeard;
        v.facial_hair.length = 0.9F + nextRand(beard_seed) * 0.5F;
      } else {
        v.facial_hair.style = FacialHairStyle::Goatee;
        v.facial_hair.length = 0.7F + nextRand(beard_seed) * 0.4F;
      }

      float const color_roll = nextRand(beard_seed);
      if (color_roll < 0.60F) {
        v.facial_hair.color = QVector3D(0.12F + nextRand(beard_seed) * 0.08F,
                                        0.10F + nextRand(beard_seed) * 0.06F,
                                        0.08F + nextRand(beard_seed) * 0.05F);
      } else if (color_roll < 0.85F) {
        v.facial_hair.color = QVector3D(0.22F + nextRand(beard_seed) * 0.10F,
                                        0.17F + nextRand(beard_seed) * 0.08F,
                                        0.12F + nextRand(beard_seed) * 0.06F);
      } else {
        v.facial_hair.color = QVector3D(0.35F + nextRand(beard_seed) * 0.15F,
                                        0.32F + nextRand(beard_seed) * 0.12F,
                                        0.30F + nextRand(beard_seed) * 0.10F);
        v.facial_hair.greyness = 0.3F + nextRand(beard_seed) * 0.4F;
      }

      v.facial_hair.thickness = 0.85F + nextRand(beard_seed) * 0.25F;
      v.facial_hair.coverage = 0.80F + nextRand(beard_seed) * 0.20F;
    }
  }

  void customize_pose(const DrawContext &,
                      const HumanoidAnimationContext &anim_ctx, uint32_t seed,
                      HumanoidPose &pose) const override {
    using HP = HumanProportions;

    const AnimationInputs &anim = anim_ctx.inputs;
    HumanoidPoseController controller(pose, anim_ctx);

    float const arm_height_jitter = (hash_01(seed ^ 0xABCDU) - 0.5F) * 0.03F;
    float const arm_asymmetry = (hash_01(seed ^ 0xDEF0U) - 0.5F) * 0.05F;

    float const forward_offset = 0.20F + (anim.is_moving ? 0.03F : 0.0F);
    float const hand_height = HP::WAIST_Y + 0.08F + arm_height_jitter;

    QVector3D const idle_hand_l(-0.16F + arm_asymmetry, hand_height,
                                forward_offset);
    QVector3D const idle_hand_r(0.12F - arm_asymmetry * 0.5F,
                                hand_height + 0.01F, forward_offset * 0.9F);

    controller.placeHandAt(true, idle_hand_l);
    controller.placeHandAt(false, idle_hand_r);
  }

  void add_attachments(const DrawContext &ctx, const HumanoidVariant &v,
                       const HumanoidPose &pose,
                       const HumanoidAnimationContext &anim_ctx,
                       ISubmitter &out) const override {
    draw_mallet(ctx, v, pose, out);
  }

  void draw_mallet(const DrawContext &ctx, const HumanoidVariant &v,
                   const HumanoidPose &pose, ISubmitter &out) const {
    QVector3D const wood_color = v.palette.wood;
    QVector3D const wood_dark = v.palette.wood * 0.75F;

    QVector3D const hand_r = pose.hand_r;
    QVector3D const up(0.0F, 1.0F, 0.0F);

    float const handle_length = 0.22F;
    float const handle_radius = 0.014F;
    QVector3D const handle_top = hand_r + up * 0.06F;
    QVector3D const handle_bottom = handle_top - up * handle_length;

    out.mesh(
        get_unit_cylinder(),
        cylinder_between(ctx.model, handle_bottom, handle_top, handle_radius),
        wood_color, nullptr, 1.0F);

    float const head_radius = 0.035F;
    QVector3D const head_center = handle_top + up * 0.035F;

    out.mesh(get_unit_sphere(), sphere_at(ctx.model, head_center, head_radius),
             wood_dark, nullptr, 1.0F);
  }

  void draw_helmet(const DrawContext &ctx, const HumanoidVariant &v,
                   const HumanoidPose &pose, ISubmitter &out) const override {
    if (!resolve_style(ctx).show_helmet) {
      return;
    }
    auto &registry = EquipmentRegistry::instance();
    auto helmet = registry.get(EquipmentCategory::Helmet, "carthage_light");
    if (helmet) {
      HumanoidAnimationContext anim_ctx{};
      helmet->render(ctx, pose.body_frames, v.palette, anim_ctx, out);
    }
  }

  void draw_armor(const DrawContext &ctx, const HumanoidVariant &v,
                  const HumanoidPose &pose,
                  const HumanoidAnimationContext &anim,
                  ISubmitter &out) const override {
    draw_builder_robes(ctx, v, pose, out);
  }

  void draw_builder_robes(const DrawContext &ctx, const HumanoidVariant &v,
                          const HumanoidPose &pose, ISubmitter &out) const {
    using HP = HumanProportions;
    const BodyFrames &frames = pose.body_frames;
    const AttachmentFrame &torso = frames.torso;
    const AttachmentFrame &waist = frames.waist;

    if (torso.radius <= 0.0F) {
      return;
    }

    QVector3D const team_tint = resolve_team_tint(ctx);
    QVector3D const robe_tan(0.68F, 0.54F, 0.38F);
    QVector3D const robe_light(0.72F, 0.58F, 0.42F);
    QVector3D const apron_color =
        resolve_style(ctx).apron_color.value_or(QVector3D(0.42F, 0.35F, 0.25F));
    QVector3D const bronze_color = v.palette.metal;
    QVector3D const leather_color = v.palette.leather;

    const QVector3D &origin = torso.origin;
    const QVector3D &right = torso.right;
    const QVector3D &up = torso.up;
    const QVector3D &forward = torso.forward;

    constexpr int k_mat_tunic = 1;
    constexpr int k_mat_leather = 2;
    constexpr int k_mat_tools = 3;
    float const torso_r = torso.radius * 1.02F;
    float const torso_depth =
        (torso.depth > 0.0F) ? torso.depth * 0.88F : torso.radius * 0.82F;

    float const y_shoulder = origin.y() + 0.040F;
    float const y_waist = waist.origin.y();

    constexpr int segments = 12;
    constexpr float pi = std::numbers::pi_v<float>;

    auto drawRobeRing = [&](float y_pos, float width, float depth,
                            const QVector3D &color, float thickness,
                            int materialId) {
      for (int i = 0; i < segments; ++i) {
        float const angle1 = (static_cast<float>(i) / segments) * 2.0F * pi;
        float const angle2 = (static_cast<float>(i + 1) / segments) * 2.0F * pi;

        float const sin1 = std::sin(angle1);
        float const cos1 = std::cos(angle1);
        float const sin2 = std::sin(angle2);
        float const cos2 = std::cos(angle2);

        float const r1 =
            (std::abs(cos1) * depth + (1.0F - std::abs(cos1)) * width);
        float const r2 =
            (std::abs(cos2) * depth + (1.0F - std::abs(cos2)) * width);

        QVector3D const p1 = origin + right * (r1 * sin1) +
                             forward * (r1 * cos1) + up * (y_pos - origin.y());
        QVector3D const p2 = origin + right * (r2 * sin2) +
                             forward * (r2 * cos2) + up * (y_pos - origin.y());

        out.mesh(get_unit_cylinder(),
                 cylinder_between(ctx.model, p1, p2, thickness), color, nullptr,
                 1.0F, materialId);
      }
    };

    drawRobeRing(y_shoulder - 0.00F, torso_r * 1.18F, torso_depth * 1.08F,
                 robe_light, 0.034F, k_mat_tunic);
    drawRobeRing(y_shoulder - 0.04F, torso_r * 1.24F, torso_depth * 1.12F,
                 robe_light, 0.036F, k_mat_tunic);

    float const torso_fill_top = y_shoulder - 0.08F;
    float const torso_fill_bot = y_waist + 0.04F;
    constexpr int torso_fill_layers = 7;
    for (int i = 0; i < torso_fill_layers; ++i) {
      float const t =
          static_cast<float>(i) / static_cast<float>(torso_fill_layers - 1);
      float const y = torso_fill_top + (torso_fill_bot - torso_fill_top) * t;
      float const width = torso_r * (1.06F - t * 0.18F);
      float const depth = torso_depth * (0.98F - t * 0.14F);
      float const thickness = 0.028F - t * 0.008F;
      QVector3D const c =
          (t < 0.4F) ? robe_light : robe_tan * (1.0F - (t - 0.4F) * 0.25F);
      drawRobeRing(y, width, depth, c, thickness, k_mat_tunic);
    }

    constexpr int skirt_layers = 7;
    for (int layer = 0; layer < skirt_layers; ++layer) {
      float const t =
          static_cast<float>(layer) / static_cast<float>(skirt_layers - 1);
      float const y = y_waist - t * 0.22F;
      float const flare = 1.0F + t * 0.30F;
      QVector3D const skirt_color = robe_tan * (1.0F - t * 0.06F);
      drawRobeRing(y, torso_r * 0.88F * flare, torso_depth * 0.82F * flare,
                   skirt_color, 0.020F + t * 0.010F, k_mat_tunic);
    }

    float const belt_y = y_waist + 0.01F;
    QVector3D const belt_top = origin + up * (belt_y + 0.020F - origin.y());
    QVector3D const belt_bot = origin + up * (belt_y - 0.020F - origin.y());
    out.mesh(get_unit_cylinder(),
             cylinder_between(ctx.model, belt_bot, belt_top, torso_r * 0.96F),
             leather_color, nullptr, 1.0F, k_mat_leather);

    QVector3D const apron_top = origin + forward * (torso_depth * 0.82F) +
                                up * (y_waist + 0.01F - origin.y());
    QVector3D const apron_bottom = origin + forward * (torso_depth * 0.88F) +
                                   up * (y_waist - 0.18F - origin.y());
    float const apron_width = torso_r * 0.65F;

    out.mesh(
        get_unit_cube(),
        [&]() {
          QMatrix4x4 m = ctx.model;
          QVector3D const center = (apron_top + apron_bottom) * 0.5F;
          m.translate(center);
          m.scale(apron_width, (apron_top.y() - apron_bottom.y()) * 0.5F,
                  0.012F);
          return m;
        }(),
        apron_color, nullptr, 1.0F, k_mat_leather);

    QVector3D const pouch_pos = origin + right * (torso_r * 0.70F) +
                                up * (belt_y - 0.05F - origin.y()) +
                                forward * (torso_depth * 0.10F);
    out.mesh(
        get_unit_cube(),
        [&]() {
          QMatrix4x4 m = ctx.model;
          m.translate(pouch_pos);
          m.scale(0.035F, 0.045F, 0.025F);
          return m;
        }(),
        leather_color * 0.85F, nullptr, 1.0F, k_mat_leather);

    QVector3D const buckle_pos =
        origin + forward * (torso_depth * 0.82F) + up * (belt_y - origin.y());
    out.mesh(get_unit_sphere(), sphere_at(ctx.model, buckle_pos, 0.016F),
             bronze_color, nullptr, 1.0F, k_mat_tools);

    auto drawFlowingSleeve = [&](const QVector3D &shoulder_pos,
                                 const QVector3D &outward) {
      QVector3D const backward = -forward;
      QVector3D const anchor = shoulder_pos + up * 0.060F + backward * 0.015F;
      for (int i = 0; i < 5; ++i) {
        float const t = static_cast<float>(i) / 5.0F;
        QVector3D const sleeve_pos = anchor + outward * (0.012F + t * 0.025F) +
                                     forward * (-0.015F + t * 0.055F) -
                                     up * (t * 0.04F);
        float const sleeve_r = HP::UPPER_ARM_R * (1.50F - t * 0.06F);
        QVector3D const sleeve_color = robe_light * (1.0F - t * 0.03F);
        out.mesh(get_unit_sphere(), sphere_at(ctx.model, sleeve_pos, sleeve_r),
                 sleeve_color, nullptr, 1.0F, k_mat_tunic);
      }
    };
    drawFlowingSleeve(frames.shoulder_l.origin, -right);
    drawFlowingSleeve(frames.shoulder_r.origin, right);
  }

private:
  auto
  resolve_style(const DrawContext &ctx) const -> const BuilderStyleConfig & {
    ensure_builder_styles_registered();
    auto &styles = style_registry();
    std::string nation_id;
    if (ctx.entity != nullptr) {
      if (auto *unit =
              ctx.entity->get_component<Engine::Core::UnitComponent>()) {
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
    static const BuilderStyleConfig default_style{};
    return default_style;
  }

public:
  auto resolve_shader_key(const DrawContext &ctx) const -> QString {
    const BuilderStyleConfig &style = resolve_style(ctx);
    if (!style.shader_id.empty()) {
      return QString::fromStdString(style.shader_id);
    }
    return QStringLiteral("builder");
  }

private:
  void apply_palette_overrides(const BuilderStyleConfig &style,
                               const QVector3D &team_tint,
                               HumanoidVariant &variant) const {
    auto apply_color = [&](const std::optional<QVector3D> &override_color,
                           QVector3D &target, float team_weight,
                           float style_weight) {
      target = mix_palette_color(target, override_color, team_tint, team_weight,
                                 style_weight);
    };

    constexpr float k_skin_team_mix_weight = 0.0F;
    constexpr float k_skin_style_mix_weight = 1.0F;

    constexpr float k_cloth_team_mix_weight = 0.0F;
    constexpr float k_cloth_style_mix_weight = 1.0F;

    apply_color(style.skin_color, variant.palette.skin, k_skin_team_mix_weight,
                k_skin_style_mix_weight);
    apply_color(style.cloth_color, variant.palette.cloth,
                k_cloth_team_mix_weight, k_cloth_style_mix_weight);
    apply_color(style.leather_color, variant.palette.leather, k_team_mix_weight,
                k_style_mix_weight);
    apply_color(style.leather_dark_color, variant.palette.leatherDark,
                k_team_mix_weight, k_style_mix_weight);
    apply_color(style.metal_color, variant.palette.metal, k_team_mix_weight,
                k_style_mix_weight);
    apply_color(style.wood_color, variant.palette.wood, k_team_mix_weight,
                k_style_mix_weight);
  }
};

void register_builder_renderer(Render::GL::EntityRendererRegistry &registry) {
  ensure_builder_styles_registered();
  static BuilderRenderer const renderer;
  registry.register_renderer(
      "troops/carthage/builder", [](const DrawContext &ctx, ISubmitter &out) {
        static BuilderRenderer const static_renderer;
        Shader *builder_shader = nullptr;
        if (ctx.backend != nullptr) {
          QString shader_key = static_renderer.resolve_shader_key(ctx);
          builder_shader = ctx.backend->shader(shader_key);
          if (builder_shader == nullptr) {
            builder_shader = ctx.backend->shader(QStringLiteral("builder"));
          }
        }
        auto *scene_renderer = dynamic_cast<Renderer *>(&out);
        if ((scene_renderer != nullptr) && (builder_shader != nullptr)) {
          scene_renderer->set_current_shader(builder_shader);
        }
        static_renderer.render(ctx, out);
        if (scene_renderer != nullptr) {
          scene_renderer->set_current_shader(nullptr);
        }
      });
}

} // namespace Render::GL::Carthage
