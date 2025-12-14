#include "healer_renderer.h"
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
#include "healer_style.h"

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

auto style_registry() -> std::unordered_map<std::string, HealerStyleConfig> & {
  static std::unordered_map<std::string, HealerStyleConfig> styles;
  return styles;
}

void ensure_healer_styles_registered() {
  static const bool registered = []() {
    register_roman_healer_style();
    return true;
  }();
  (void)registered;
}

constexpr float k_team_mix_weight = 0.65F;
constexpr float k_style_mix_weight = 0.35F;

} // namespace

void register_healer_style(const std::string &nation_id,
                           const HealerStyleConfig &style) {
  style_registry()[nation_id] = style;
}

using Render::Geom::clamp01;
using Render::Geom::clampf;
using Render::GL::Humanoid::mix_palette_color;
using Render::GL::Humanoid::saturate_color;

class HealerRenderer : public HumanoidRendererBase {
public:
  auto get_proportion_scaling() const -> QVector3D override {

    return {0.86F, 0.99F, 0.90F};
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

    if (anim.is_healing) {

      float const healing_time = anim.time * 2.5F;
      float const sway_phase = std::sin(healing_time);
      float const sway_phase_offset = std::sin(healing_time + 0.5F);

      float const base_arm_height = HP::SHOULDER_Y - 0.02F + arm_height_jitter;
      float const sway_height = 0.03F * sway_phase;

      float const target_dist =
          std::sqrt(anim.healing_target_dx * anim.healing_target_dx +
                    anim.healing_target_dz * anim.healing_target_dz);
      float target_dir_x = 0.0F;
      float target_dir_z = 1.0F;
      if (target_dist > 0.01F) {
        target_dir_x = anim.healing_target_dx / target_dist;
        target_dir_z = anim.healing_target_dz / target_dist;
      }

      float const arm_spread = 0.18F + 0.02F * sway_phase_offset;
      float const forward_reach = 0.22F + 0.03F * std::sin(healing_time * 0.7F);

      QVector3D const heal_hand_l(-arm_spread + arm_asymmetry * 0.3F,
                                  base_arm_height + sway_height, forward_reach);
      QVector3D const heal_hand_r(arm_spread - arm_asymmetry * 0.3F,
                                  base_arm_height + sway_height + 0.01F,
                                  forward_reach * 0.95F);

      controller.placeHandAt(true, heal_hand_l);
      controller.placeHandAt(false, heal_hand_r);

      QVector3D const look_dir(target_dir_x, 0.0F, target_dir_z);
      QVector3D const head_focus =
          pose.head_pos +
          QVector3D(look_dir.x() * 0.18F, 0.0F, look_dir.z() * 0.45F);
      controller.look_at(head_focus);
      controller.lean(look_dir, 0.18F);

    } else {

      float const forward_offset = 0.16F + (anim.is_moving ? 0.05F : 0.0F);
      float const hand_height = HP::WAIST_Y + 0.04F + arm_height_jitter;
      QVector3D const idle_hand_l(-0.16F + arm_asymmetry, hand_height,
                                  forward_offset);
      QVector3D const idle_hand_r(0.12F - arm_asymmetry * 0.6F,
                                  hand_height + 0.01F, forward_offset * 0.9F);

      controller.placeHandAt(true, idle_hand_l);
      controller.placeHandAt(false, idle_hand_r);
    }
  }

  void add_attachments(const DrawContext &ctx, const HumanoidVariant &v,
                       const HumanoidPose &pose,
                       const HumanoidAnimationContext &anim_ctx,
                       ISubmitter &out) const override {}

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

    draw_healer_tunic(ctx, v, pose, out);

    if (resolve_style(ctx).show_armor) {
      auto &registry = EquipmentRegistry::instance();
      auto armor = registry.get(EquipmentCategory::Armor, "roman_light_armor");
      if (armor) {
        armor->render(ctx, pose.body_frames, v.palette, anim, out);
      }
    }
  }

  void draw_healer_tunic(const DrawContext &ctx, const HumanoidVariant &v,
                         const HumanoidPose &pose, ISubmitter &out) const {
    using HP = HumanProportions;
    const BodyFrames &frames = pose.body_frames;
    const AttachmentFrame &torso = frames.torso;
    const AttachmentFrame &waist = frames.waist;
    const AttachmentFrame &back = frames.back;
    const HealerStyleConfig &style = resolve_style(ctx);

    if (torso.radius <= 0.0F) {
      return;
    }

    QVector3D const tunic_white(0.96F, 0.95F, 0.92F);
    QVector3D const tunic_offwhite(0.93F, 0.91F, 0.86F);
    QVector3D const tunic_cream(0.89F, 0.86F, 0.80F);
    QVector3D const sash_red =
        style.cape_color.value_or(QVector3D(0.72F, 0.18F, 0.15F));
    QVector3D const trim_gold =
        saturate_color(v.palette.metal * 0.92F + QVector3D(0.05F, 0.04F, 0.0F));
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

    float const y_robe_bottom = y_waist - 0.38F;

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

        float const r1 = std::sqrt((width * width * sin1 * sin1) +
                                   (depth * depth * cos1 * cos1));
        float const r2 = std::sqrt((width * width * sin2 * sin2) +
                                   (depth * depth * cos2 * cos2));

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

    auto drawTorsoSection = [&](float y_top, float y_bot, float width_top,
                                float width_bot, float depth_top,
                                float depth_bot, const QVector3D &color) {
      QVector3D const top_pos = origin + up * (y_top - origin.y());
      QVector3D const bot_pos = origin + up * (y_bot - origin.y());
      float const avg_r = (width_top + width_bot) * 0.5F;
      out.mesh(get_unit_cylinder(),
               cylinder_between(ctx.model, bot_pos, top_pos, avg_r), color,
               nullptr, 1.0F);
    };

    float const neck_y = y_shoulder + 0.04F;
    drawFabricRing(neck_y, torso_r * 0.72F, torso_depth * 0.64F, tunic_cream,
                   0.024F);

    drawFabricRing(y_shoulder + 0.05F, torso_r * 1.16F, torso_depth * 1.10F,
                   tunic_white, 0.036F);
    drawFabricRing(y_shoulder + 0.010F, torso_r * 1.10F, torso_depth * 1.04F,
                   tunic_white, 0.034F);

    drawTorsoSection(y_shoulder + 0.02F, y_shoulder - 0.10F, torso_r * 1.08F,
                     torso_r * 1.02F, torso_depth * 1.06F, torso_depth * 1.00F,
                     tunic_white);

    drawTorsoSection(y_shoulder - 0.10F, y_shoulder - 0.20F, torso_r * 1.02F,
                     torso_r * 0.92F, torso_depth * 1.00F, torso_depth * 0.88F,
                     tunic_offwhite);
    drawFabricRing(y_shoulder - 0.14F, torso_r * 0.98F, torso_depth * 0.92F,
                   tunic_offwhite, 0.030F);

    drawTorsoSection(y_shoulder - 0.20F, y_waist + 0.02F, torso_r * 0.90F,
                     torso_r * 0.82F, torso_depth * 0.86F, torso_depth * 0.78F,
                     tunic_offwhite);

    float const sash_y = y_waist + 0.010F;
    QVector3D const sash_center = origin + up * (sash_y - origin.y());

    out.mesh(get_unit_cylinder(),
             cylinder_between(ctx.model, sash_center - up * 0.022F,
                              sash_center + up * 0.022F, torso_r * 0.86F),
             sash_red, nullptr, 1.0F);

    out.mesh(get_unit_cylinder(),
             cylinder_between(ctx.model, sash_center + up * 0.020F,
                              sash_center + up * 0.026F, torso_r * 0.88F),
             trim_gold, nullptr, 1.0F);
    out.mesh(get_unit_cylinder(),
             cylinder_between(ctx.model, sash_center - up * 0.026F,
                              sash_center - up * 0.020F, torso_r * 0.88F),
             trim_gold, nullptr, 1.0F);

    if (style.show_cape) {
      float const cape_bottom_y =
          std::max(y_robe_bottom + 0.08F, y_waist - 0.20F);
      QVector3D const cape_color =
          saturate_color(sash_red * 0.95F + v.palette.cloth * 0.15F);

      QVector3D const left_top =
          frames.shoulder_l.origin + back.forward * 0.03F + up * 0.015F;
      QVector3D const right_top =
          frames.shoulder_r.origin + back.forward * 0.03F + up * 0.015F;

      QVector3D const left_bottom =
          left_top + up * (cape_bottom_y - left_top.y()) + back.forward * 0.05F;
      QVector3D const right_bottom = right_top +
                                     up * (cape_bottom_y - right_top.y()) +
                                     back.forward * 0.05F;

      out.mesh(get_unit_cylinder(),
               cylinder_between(ctx.model, left_top, right_top, 0.020F),
               cape_color, nullptr, 1.0F);
      out.mesh(get_unit_cylinder(),
               cylinder_between(ctx.model, left_top, left_bottom, 0.028F),
               cape_color, nullptr, 1.0F);
      out.mesh(get_unit_cylinder(),
               cylinder_between(ctx.model, right_top, right_bottom, 0.028F),
               cape_color, nullptr, 1.0F);
      out.mesh(get_unit_cylinder(),
               cylinder_between(ctx.model, left_bottom, right_bottom, 0.022F),
               cape_color * 0.94F, nullptr, 1.0F);

      QVector3D const cape_trim_top =
          (left_top + right_top) * 0.5F + back.forward * 0.01F;
      out.mesh(get_unit_sphere(),
               sphere_at(ctx.model, cape_trim_top, torso_r * 0.16F),
               trim_gold * 0.9F, nullptr, 1.0F);
    }

    QVector3D const emblem_center = origin + forward * (torso_depth * 0.90F) +
                                    up * ((y_shoulder - origin.y()) - 0.06F);
    float const cross_half = torso_r * 0.36F;
    float const cross_thickness = torso_r * 0.18F;
    QVector3D const cross_color = saturate_color(sash_red * 1.05F);
    out.mesh(get_unit_cylinder(),
             cylinder_between(ctx.model, emblem_center - right * cross_half,
                              emblem_center + right * cross_half,
                              cross_thickness),
             cross_color, nullptr, 1.0F);
    out.mesh(get_unit_cylinder(),
             cylinder_between(
                 ctx.model, emblem_center - up * (cross_half * 1.1F),
                 emblem_center + up * (cross_half * 1.1F), cross_thickness),
             cross_color, nullptr, 1.0F);

    float const robe_length = y_waist - y_robe_bottom;
    constexpr int skirt_layers = 10;
    for (int layer = 0; layer < skirt_layers; ++layer) {
      float const t =
          static_cast<float>(layer) / static_cast<float>(skirt_layers - 1);
      float const y = y_waist - 0.02F - t * robe_length;

      float const flare = 1.0F + t * 0.45F;
      float const width = torso_r * 0.88F * flare;
      float const depth = torso_depth * 0.82F * flare;

      QVector3D const layer_color =
          tunic_white * (1.0F - t * 0.12F) + tunic_cream * (t * 0.12F);

      float const thickness = 0.018F + t * 0.014F;
      drawFabricRing(y, width, depth, layer_color, thickness);
    }

    float const hem_y = y_robe_bottom + 0.01F;
    drawFabricRing(hem_y, torso_r * 0.88F * 1.45F, torso_depth * 0.82F * 1.45F,
                   tunic_cream * 0.92F, 0.035F);

    drawFabricRing(hem_y - 0.012F, torso_r * 0.90F * 1.45F,
                   torso_depth * 0.84F * 1.45F, trim_gold * 0.85F, 0.015F);

    auto drawSleeve = [&](const QVector3D &shoulder_pos,
                          const QVector3D &outward,
                          const QVector3D &elbow_pos) {
      out.mesh(get_unit_sphere(),
               sphere_at(ctx.model, shoulder_pos + outward * 0.01F,
                         HP::UPPER_ARM_R * 1.6F),
               tunic_white, nullptr, 1.0F);

      for (int i = 0; i < 5; ++i) {
        float const t = static_cast<float>(i) / 5.0F;
        QVector3D const sleeve_pos = shoulder_pos * (1.0F - t) + elbow_pos * t +
                                     outward * (0.01F - t * 0.005F);
        float const sleeve_r = HP::UPPER_ARM_R * (1.55F - t * 0.35F);
        QVector3D const sleeve_color = tunic_white * (1.0F - t * 0.06F);
        out.mesh(get_unit_sphere(), sphere_at(ctx.model, sleeve_pos, sleeve_r),
                 sleeve_color, nullptr, 1.0F);
      }

      QVector3D const cuff_pos = elbow_pos + outward * 0.005F;
      out.mesh(get_unit_sphere(),
               sphere_at(ctx.model, cuff_pos, HP::UPPER_ARM_R * 1.25F),
               tunic_cream * 0.95F, nullptr, 1.0F);
    };
    drawSleeve(frames.shoulder_l.origin, -right, pose.elbow_l);
    drawSleeve(frames.shoulder_r.origin, right, pose.elbow_r);

    QVector3D const satchel_pos = origin + right * (torso_r * 0.75F) +
                                  up * (y_waist - 0.08F - origin.y()) +
                                  forward * (torso_depth * 0.15F);

    out.mesh(
        get_unit_cube(),
        [&]() {
          QMatrix4x4 m = ctx.model;
          m.translate(satchel_pos);
          m.scale(0.045F, 0.06F, 0.035F);
          return m;
        }(),
        leather_brown, nullptr, 1.0F);

    out.mesh(
        get_unit_cube(),
        [&]() {
          QMatrix4x4 m = ctx.model;
          m.translate(satchel_pos + up * 0.035F + forward * 0.01F);
          m.scale(0.048F, 0.015F, 0.038F);
          return m;
        }(),
        leather_brown * 0.85F, nullptr, 1.0F);

    QVector3D const clasp_pos = origin + right * (torso_r * 0.4F) +
                                up * (y_shoulder - origin.y()) +
                                forward * (torso_depth * 0.3F);
    out.mesh(get_unit_sphere(), sphere_at(ctx.model, clasp_pos, 0.022F),
             metal_bronze, nullptr, 1.0F);
  }

private:
  auto
  resolve_style(const DrawContext &ctx) const -> const HealerStyleConfig & {
    ensure_healer_styles_registered();
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
    static const HealerStyleConfig default_style{};
    return default_style;
  }

public:
  auto resolve_shader_key(const DrawContext &ctx) const -> QString {
    const HealerStyleConfig &style = resolve_style(ctx);
    if (!style.shader_id.empty()) {
      return QString::fromStdString(style.shader_id);
    }
    return QStringLiteral("healer");
  }

private:
  void apply_palette_overrides(const HealerStyleConfig &style,
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

void register_healer_renderer(Render::GL::EntityRendererRegistry &registry) {
  ensure_healer_styles_registered();
  static HealerRenderer const renderer;
  registry.register_renderer(
      "troops/roman/healer", [](const DrawContext &ctx, ISubmitter &out) {
        static HealerRenderer const static_renderer;
        Shader *healer_shader = nullptr;
        if (ctx.backend != nullptr) {
          QString shader_key = static_renderer.resolve_shader_key(ctx);
          healer_shader = ctx.backend->shader(shader_key);
          if (healer_shader == nullptr) {
            healer_shader = ctx.backend->shader(QStringLiteral("healer"));
          }
        }
        auto *scene_renderer = dynamic_cast<Renderer *>(&out);
        if ((scene_renderer != nullptr) && (healer_shader != nullptr)) {
          scene_renderer->set_current_shader(healer_shader);
        }
        static_renderer.render(ctx, out);
        if (scene_renderer != nullptr) {
          scene_renderer->set_current_shader(nullptr);
        }
      });
}

} // namespace Render::GL::Roman
