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

namespace Render::GL::Roman {

namespace {

constexpr std::string_view k_default_style_key = "default";

auto style_registry() -> std::unordered_map<std::string, BuilderStyleConfig> & {
  static std::unordered_map<std::string, BuilderStyleConfig> styles;
  return styles;
}

void ensure_builder_styles_registered() {
  static const bool registered = []() {
    register_roman_builder_style();
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
    return {0.92F, 1.02F, 0.94F};
  }

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
    float const arm_asymmetry = (hash_01(seed ^ 0xDEF0U) - 0.5F) * 0.06F;

    float const forward_offset = 0.18F + (anim.is_moving ? 0.04F : 0.0F);
    float const hand_height = HP::WAIST_Y + 0.06F + arm_height_jitter;

    QVector3D const idle_hand_l(-0.18F + arm_asymmetry, hand_height,
                                forward_offset);
    QVector3D const idle_hand_r(0.14F - arm_asymmetry * 0.6F,
                                hand_height + 0.02F, forward_offset * 0.85F);

    controller.placeHandAt(true, idle_hand_l);
    controller.placeHandAt(false, idle_hand_r);
  }

  void add_attachments(const DrawContext &ctx, const HumanoidVariant &v,
                       const HumanoidPose &pose,
                       const HumanoidAnimationContext &anim_ctx,
                       ISubmitter &out) const override {
    draw_hammer(ctx, v, pose, out);
  }

  void draw_hammer(const DrawContext &ctx, const HumanoidVariant &v,
                   const HumanoidPose &pose, ISubmitter &out) const {
    QVector3D const wood_color = v.palette.wood;
    QVector3D const metal_color = v.palette.metal;

    QVector3D const hand_r = pose.hand_r;
    QVector3D const up(0.0F, 1.0F, 0.0F);
    QVector3D const forward(0.0F, 0.0F, 1.0F);

    float const handle_length = 0.25F;
    float const handle_radius = 0.012F;
    QVector3D const handle_top = hand_r + up * 0.08F;
    QVector3D const handle_bottom = handle_top - up * handle_length;

    out.mesh(
        get_unit_cylinder(),
        cylinder_between(ctx.model, handle_bottom, handle_top, handle_radius),
        wood_color, nullptr, 1.0F);

    float const head_width = 0.06F;
    float const head_height = 0.04F;
    QVector3D const head_center = handle_top + up * 0.02F;
    QVector3D const head_left = head_center - forward * (head_width * 0.5F);
    QVector3D const head_right = head_center + forward * (head_width * 0.5F);

    out.mesh(
        get_unit_cylinder(),
        cylinder_between(ctx.model, head_left, head_right, head_height * 0.5F),
        metal_color, nullptr, 1.0F);
  }

  void draw_helmet(const DrawContext &ctx, const HumanoidVariant &v,
                   const HumanoidPose &pose, ISubmitter &out) const override {
    if (!resolve_style(ctx).show_helmet) {
      return;
    }
    auto &registry = EquipmentRegistry::instance();
    auto helmet = registry.get(EquipmentCategory::Helmet, "roman_light");
    if (helmet) {
      HumanoidAnimationContext anim_ctx{};
      helmet->render(ctx, pose.body_frames, v.palette, anim_ctx, out);
    }
  }

  void draw_armor(const DrawContext &ctx, const HumanoidVariant &v,
                  const HumanoidPose &pose,
                  const HumanoidAnimationContext &anim,
                  ISubmitter &out) const override {
    draw_builder_tunic(ctx, v, pose, out);
  }

  void draw_builder_tunic(const DrawContext &ctx, const HumanoidVariant &v,
                          const HumanoidPose &pose, ISubmitter &out) const {
    using HP = HumanProportions;
    const BodyFrames &frames = pose.body_frames;
    const AttachmentFrame &torso = frames.torso;
    const AttachmentFrame &waist = frames.waist;
    const BuilderStyleConfig &style = resolve_style(ctx);

    if (torso.radius <= 0.0F) {
      return;
    }

    QVector3D const tunic_brown(0.72F, 0.58F, 0.42F);
    QVector3D const tunic_tan(0.68F, 0.54F, 0.38F);
    QVector3D const apron_color =
        style.apron_color.value_or(QVector3D(0.45F, 0.38F, 0.28F));
    QVector3D const leather_brown = v.palette.leather;
    QVector3D const metal_bronze = v.palette.metal;

    const QVector3D &origin = torso.origin;
    const QVector3D &right = torso.right;
    const QVector3D &up = torso.up;
    const QVector3D &forward = torso.forward;
    float const torso_r = torso.radius * 1.05F;
    float const torso_depth =
        (torso.depth > 0.0F) ? torso.depth * 0.95F : torso.radius * 0.82F;

    float const y_shoulder = origin.y() + 0.030F;
    float const y_waist = waist.origin.y();
    float const y_tunic_bottom = y_waist - 0.20F;

    constexpr int segments = 14;
    constexpr float pi = std::numbers::pi_v<float>;

    auto drawFabricRing = [&](float y_pos, float width, float depth,
                              const QVector3D &color, float thickness) {
      for (int i = 0; i < segments; ++i) {
        float const angle1 = (static_cast<float>(i) / segments) * 2.0F * pi;
        float const angle2 = (static_cast<float>(i + 1) / segments) * 2.0F * pi;

        float const sin1 = std::sin(angle1);
        float const cos1 = std::cos(angle1);
        float const sin2 = std::sin(angle2);
        float const cos2 = std::cos(angle2);

        QVector3D const p1 = origin + right * (width * sin1) +
                             forward * (depth * cos1) +
                             up * (y_pos - origin.y());
        QVector3D const p2 = origin + right * (width * sin2) +
                             forward * (depth * cos2) +
                             up * (y_pos - origin.y());

        out.mesh(get_unit_cylinder(),
                 cylinder_between(ctx.model, p1, p2, thickness), color, nullptr,
                 1.0F);
      }
    };

    drawFabricRing(y_shoulder + 0.04F, torso_r * 0.72F, torso_depth * 0.64F,
                   tunic_tan, 0.024F);

    drawFabricRing(y_shoulder + 0.05F, torso_r * 1.12F, torso_depth * 1.06F,
                   tunic_brown, 0.034F);
    drawFabricRing(y_shoulder, torso_r * 1.06F, torso_depth * 1.00F,
                   tunic_brown, 0.032F);

    for (int i = 0; i < 6; ++i) {
      float const t = static_cast<float>(i) / 5.0F;
      float const y = y_shoulder - 0.02F - t * (y_shoulder - y_waist - 0.04F);
      float const width = torso_r * (1.04F - t * 0.16F);
      float const depth = torso_depth * (0.98F - t * 0.12F);
      QVector3D const c = tunic_brown * (1.0F - t * 0.08F);
      drawFabricRing(y, width, depth, c, 0.028F - t * 0.006F);
    }

    constexpr int skirt_layers = 6;
    for (int layer = 0; layer < skirt_layers; ++layer) {
      float const t =
          static_cast<float>(layer) / static_cast<float>(skirt_layers - 1);
      float const y = y_waist - 0.02F - t * (y_waist - y_tunic_bottom);
      float const flare = 1.0F + t * 0.25F;
      float const width = torso_r * 0.82F * flare;
      float const depth = torso_depth * 0.78F * flare;
      QVector3D const layer_color = tunic_brown * (1.0F - t * 0.10F);
      drawFabricRing(y, width, depth, layer_color, 0.020F + t * 0.008F);
    }

    QVector3D const apron_top = origin + forward * (torso_depth * 0.85F) +
                                up * (y_waist + 0.02F - origin.y());
    QVector3D const apron_bottom = origin + forward * (torso_depth * 0.90F) +
                                   up * (y_tunic_bottom - 0.02F - origin.y());
    float const apron_width = torso_r * 0.7F;

    out.mesh(
        get_unit_cube(),
        [&]() {
          QMatrix4x4 m = ctx.model;
          QVector3D const center = (apron_top + apron_bottom) * 0.5F;
          m.translate(center);
          m.scale(apron_width, (apron_top.y() - apron_bottom.y()) * 0.5F,
                  0.015F);
          return m;
        }(),
        apron_color, nullptr, 1.0F);

    if (style.show_tool_belt) {
      float const belt_y = y_waist + 0.01F;
      QVector3D const belt_center = origin + up * (belt_y - origin.y());
      out.mesh(get_unit_cylinder(),
               cylinder_between(ctx.model, belt_center - up * 0.015F,
                                belt_center + up * 0.015F, torso_r * 0.88F),
               leather_brown, nullptr, 1.0F);

      QVector3D const pouch_pos = origin + right * (torso_r * 0.75F) +
                                  up * (belt_y - 0.06F - origin.y()) +
                                  forward * (torso_depth * 0.12F);
      out.mesh(
          get_unit_cube(),
          [&]() {
            QMatrix4x4 m = ctx.model;
            m.translate(pouch_pos);
            m.scale(0.04F, 0.05F, 0.03F);
            return m;
          }(),
          leather_brown * 0.9F, nullptr, 1.0F);

      QVector3D const buckle_pos =
          origin + forward * (torso_depth * 0.85F) + up * (belt_y - origin.y());
      out.mesh(get_unit_sphere(), sphere_at(ctx.model, buckle_pos, 0.018F),
               metal_bronze, nullptr, 1.0F);
    }

    auto drawSleeve = [&](const QVector3D &shoulder_pos,
                          const QVector3D &outward,
                          const QVector3D &elbow_pos) {
      out.mesh(get_unit_sphere(),
               sphere_at(ctx.model, shoulder_pos + outward * 0.01F,
                         HP::UPPER_ARM_R * 1.5F),
               tunic_brown, nullptr, 1.0F);

      for (int i = 0; i < 4; ++i) {
        float const t = static_cast<float>(i) / 4.0F;
        QVector3D const sleeve_pos = shoulder_pos * (1.0F - t) + elbow_pos * t +
                                     outward * (0.01F - t * 0.005F);
        float const sleeve_r = HP::UPPER_ARM_R * (1.45F - t * 0.30F);
        QVector3D const sleeve_color = tunic_brown * (1.0F - t * 0.05F);
        out.mesh(get_unit_sphere(), sphere_at(ctx.model, sleeve_pos, sleeve_r),
                 sleeve_color, nullptr, 1.0F);
      }
    };
    drawSleeve(frames.shoulder_l.origin, -right, pose.elbow_l);
    drawSleeve(frames.shoulder_r.origin, right, pose.elbow_r);
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

void register_builder_renderer(Render::GL::EntityRendererRegistry &registry) {
  ensure_builder_styles_registered();
  static BuilderRenderer const renderer;
  registry.register_renderer(
      "troops/roman/builder", [](const DrawContext &ctx, ISubmitter &out) {
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

} // namespace Render::GL::Roman
