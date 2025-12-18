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
  friend void
  register_builder_renderer(Render::GL::EntityRendererRegistry &registry);

  auto get_proportion_scaling() const -> QVector3D override {
    return {1.05F, 0.98F, 1.02F};
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

    float const arm_jitter = (hash_01(seed ^ 0xABCDU) - 0.5F) * 0.04F;
    float const asymmetry = (hash_01(seed ^ 0xDEF0U) - 0.5F) * 0.05F;

    if (anim.is_constructing) {

      uint32_t const pose_selector = seed % 100;

      float const phase_offset = float(seed % 100) * 0.0628F;
      float const cycle_speed = 2.0F + float(seed % 50) * 0.02F;
      float const swing_cycle =
          std::fmod(anim.time * cycle_speed + phase_offset, 1.0F);

      if (pose_selector < 40) {

        apply_hammering_pose(controller, swing_cycle, asymmetry, seed);
      } else if (pose_selector < 70) {

        apply_kneeling_work_pose(controller, swing_cycle, asymmetry, seed);
      } else if (pose_selector < 90) {

        apply_sawing_pose(controller, swing_cycle, asymmetry, seed);
      } else {

        apply_lifting_pose(controller, swing_cycle, asymmetry, seed);
      }
      return;
    }

    float const hammer_hand_forward = 0.22F + (anim.is_moving ? 0.03F : 0.0F);
    float const hammer_hand_height = HP::WAIST_Y + 0.08F + arm_jitter;

    QVector3D const hammer_hand(-0.10F + asymmetry, hammer_hand_height + 0.04F,
                                hammer_hand_forward);

    QVector3D const rest_hand(0.24F - asymmetry * 0.5F,
                              HP::WAIST_Y - 0.02F + arm_jitter * 0.5F, 0.08F);

    controller.placeHandAt(true, hammer_hand);
    controller.placeHandAt(false, rest_hand);
  }

  void add_attachments(const DrawContext &ctx, const HumanoidVariant &v,
                       const HumanoidPose &pose,
                       const HumanoidAnimationContext &anim_ctx,
                       ISubmitter &out) const override {
    auto &registry = EquipmentRegistry::instance();

    auto work_apron =
        registry.get(EquipmentCategory::Armor, "work_apron_roman");
    if (work_apron) {
      work_apron->render(ctx, pose.body_frames, v.palette, anim_ctx, out);
    }

    auto tool_belt = registry.get(EquipmentCategory::Armor, "tool_belt_roman");
    if (tool_belt) {
      tool_belt->render(ctx, pose.body_frames, v.palette, anim_ctx, out);
    }

    auto arm_guards = registry.get(EquipmentCategory::Armor, "arm_guards");
    if (arm_guards) {
      arm_guards->render(ctx, pose.body_frames, v.palette, anim_ctx, out);
    }

    draw_stone_hammer(ctx, v, pose, anim_ctx, out);
  }

  void draw_helmet(const DrawContext &ctx, const HumanoidVariant &v,
                   const HumanoidPose &pose, ISubmitter &out) const override {

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
    uint32_t const seed = reinterpret_cast<uintptr_t>(ctx.entity) & 0xFFFFFFFFU;
    draw_work_tunic(ctx, v, pose, seed, out);
  }

private:
  static constexpr uint32_t KNEEL_SEED_OFFSET = 0x1234U;

  void apply_hammering_pose(HumanoidPoseController &controller,
                            float swing_cycle, float asymmetry,
                            uint32_t seed) const {
    using HP = HumanProportions;

    float swing_angle;
    float body_lean;
    float crouch_amount;

    if (swing_cycle < 0.3F) {

      float const t = swing_cycle / 0.3F;
      swing_angle = t * 0.95F;
      body_lean = -t * 0.10F;
      crouch_amount = 0.0F;
    } else if (swing_cycle < 0.5F) {

      float const t = (swing_cycle - 0.3F) / 0.2F;
      swing_angle = 0.95F - t * 1.5F;
      body_lean = -0.10F + t * 0.28F;
      crouch_amount = t * 0.08F;
    } else if (swing_cycle < 0.6F) {

      float const t = (swing_cycle - 0.5F) / 0.1F;
      swing_angle = -0.55F + t * 0.18F;
      body_lean = 0.18F - t * 0.06F;
      crouch_amount = 0.08F - t * 0.02F;
    } else {

      float const t = (swing_cycle - 0.6F) / 0.4F;
      swing_angle = -0.37F + t * 0.37F;
      body_lean = 0.12F * (1.0F - t);
      crouch_amount = 0.06F * (1.0F - t);
    }

    float const torso_y_offset = -crouch_amount;
    float const hammer_y = HP::SHOULDER_Y + 0.10F + swing_angle * 0.22F;
    float const hammer_forward =
        0.18F + std::abs(swing_angle) * 0.16F + body_lean * 0.5F;
    float const hammer_down =
        swing_cycle > 0.4F && swing_cycle < 0.65F ? 0.10F : 0.0F;

    QVector3D const hammer_hand(-0.06F + asymmetry,
                                hammer_y - hammer_down + torso_y_offset,
                                hammer_forward);

    float const brace_y =
        HP::WAIST_Y + 0.12F + torso_y_offset - crouch_amount * 0.5F;
    float const brace_forward = 0.15F + body_lean * 0.3F;
    QVector3D const brace_hand(0.14F - asymmetry * 0.5F, brace_y,
                               brace_forward);

    controller.placeHandAt(true, hammer_hand);
    controller.placeHandAt(false, brace_hand);
  }

  void apply_kneeling_work_pose(HumanoidPoseController &controller, float cycle,
                                float asymmetry, uint32_t seed) const {
    using HP = HumanProportions;

    float const kneel_depth =
        0.45F + (hash_01(seed ^ KNEEL_SEED_OFFSET) * 0.15F);
    controller.kneel(kneel_depth);

    float const work_cycle = std::sin(cycle * std::numbers::pi_v<float> * 2.0F);

    float const tool_y = HP::WAIST_Y * 0.3F + work_cycle * 0.08F;
    float const tool_x_offset = 0.05F + work_cycle * 0.04F;
    QVector3D const tool_hand(-tool_x_offset + asymmetry, tool_y, 0.25F);

    float const brace_x = 0.18F - asymmetry * 0.5F;
    QVector3D const brace_hand(brace_x, HP::WAIST_Y * 0.25F, 0.20F);

    controller.placeHandAt(true, tool_hand);
    controller.placeHandAt(false, brace_hand);
  }

  void apply_sawing_pose(HumanoidPoseController &controller, float cycle,
                         float asymmetry, uint32_t seed) const {
    using HP = HumanProportions;

    controller.lean(QVector3D(0.0F, 0.0F, 1.0F), 0.12F);

    float const saw_offset =
        std::sin(cycle * std::numbers::pi_v<float> * 4.0F) * 0.12F;

    float const saw_y = HP::WAIST_Y + 0.15F;
    float const saw_z = 0.20F + saw_offset;

    QVector3D const left_hand(-0.08F + asymmetry, saw_y, saw_z);
    QVector3D const right_hand(0.08F - asymmetry, saw_y + 0.02F, saw_z);

    controller.placeHandAt(true, left_hand);
    controller.placeHandAt(false, right_hand);
  }

  void apply_lifting_pose(HumanoidPoseController &controller, float cycle,
                          float asymmetry, uint32_t seed) const {
    using HP = HumanProportions;

    float lift_height;
    float crouch;

    if (cycle < 0.3F) {

      float const t = cycle / 0.3F;
      lift_height = HP::WAIST_Y * (1.0F - t * 0.5F);
      crouch = t * 0.20F;
    } else if (cycle < 0.6F) {

      float const t = (cycle - 0.3F) / 0.3F;
      lift_height =
          HP::WAIST_Y * 0.5F + t * (HP::SHOULDER_Y - HP::WAIST_Y * 0.5F);
      crouch = 0.20F * (1.0F - t);
    } else if (cycle < 0.8F) {

      lift_height = HP::SHOULDER_Y;
      crouch = 0.0F;
    } else {

      float const t = (cycle - 0.8F) / 0.2F;
      lift_height = HP::SHOULDER_Y * (1.0F - t * 0.3F);
      crouch = 0.0F;
    }

    QVector3D const left_hand(-0.12F + asymmetry, lift_height, 0.15F);
    QVector3D const right_hand(0.12F - asymmetry, lift_height, 0.15F);

    controller.placeHandAt(true, left_hand);
    controller.placeHandAt(false, right_hand);

    if (crouch > 0.0F) {
      controller.kneel(crouch);
    }
  }

  void draw_stone_hammer(const DrawContext &ctx, const HumanoidVariant &v,
                         const HumanoidPose &pose,
                         const HumanoidAnimationContext &anim_ctx,
                         ISubmitter &out) const {
    QVector3D const wood_color = v.palette.wood;

    QVector3D const stone_color(0.55F, 0.52F, 0.48F);
    QVector3D const stone_dark(0.45F, 0.42F, 0.38F);

    QVector3D const hand = pose.hand_l;
    QVector3D const up(0.0F, 1.0F, 0.0F);
    QVector3D const forward(0.0F, 0.0F, 1.0F);
    QVector3D const right(1.0F, 0.0F, 0.0F);

    const AnimationInputs &anim = anim_ctx.inputs;
    QVector3D handle_axis;
    QVector3D head_axis;

    if (anim.is_constructing) {

      handle_axis = forward;
      head_axis = up;
    } else {

      handle_axis = up;
      head_axis = right;
    }

    float const handle_len = 0.32F;
    float const handle_r = 0.016F;
    QVector3D const handle_offset = anim.is_constructing
                                        ? (forward * 0.12F + up * 0.02F)
                                        : (up * 0.12F + forward * 0.02F);
    QVector3D const handle_top = hand + handle_offset;
    QVector3D const handle_bot = handle_top - handle_axis * handle_len;

    out.mesh(get_unit_cylinder(),
             cylinder_between(ctx.model, handle_bot, handle_top, handle_r),
             wood_color, nullptr, 1.0F);

    float const head_len = 0.10F;
    float const head_r = 0.030F;
    QVector3D const head_center = handle_top + handle_axis * 0.035F;

    out.mesh(
        get_unit_cylinder(),
        cylinder_between(ctx.model, head_center - head_axis * (head_len * 0.5F),
                         head_center + head_axis * (head_len * 0.5F), head_r),
        stone_color, nullptr, 1.0F);

    out.mesh(get_unit_sphere(),
             sphere_at(ctx.model, head_center + head_axis * (head_len * 0.5F),
                       head_r * 1.15F),
             stone_dark, nullptr, 1.0F);

    out.mesh(get_unit_sphere(),
             sphere_at(ctx.model, head_center - head_axis * (head_len * 0.5F),
                       head_r * 0.9F),
             stone_color * 0.95F, nullptr, 1.0F);
  }

  void draw_work_tunic(const DrawContext &ctx, const HumanoidVariant &v,
                       const HumanoidPose &pose, uint32_t seed,
                       ISubmitter &out) const {
    using HP = HumanProportions;
    const BodyFrames &frames = pose.body_frames;
    const AttachmentFrame &torso = frames.torso;
    const AttachmentFrame &waist = frames.waist;

    if (torso.radius <= 0.0F) {
      return;
    }

    float const color_var = hash_01(seed ^ 0xABCU);
    QVector3D tunic_base;
    if (color_var < 0.4F) {
      tunic_base = QVector3D(0.65F, 0.52F, 0.38F);
    } else if (color_var < 0.7F) {
      tunic_base = QVector3D(0.58F, 0.48F, 0.35F);
    } else {
      tunic_base = QVector3D(0.72F, 0.62F, 0.48F);
    }

    QVector3D const tunic_dark = tunic_base * 0.85F;

    const QVector3D &origin = torso.origin;
    const QVector3D &right = torso.right;
    const QVector3D &up = torso.up;
    const QVector3D &forward = torso.forward;
    float const torso_r = torso.radius * 1.08F;
    float const torso_d =
        (torso.depth > 0.0F) ? torso.depth * 0.92F : torso.radius * 0.80F;

    float const y_shoulder = origin.y() + 0.032F;
    float const y_waist = waist.origin.y();
    float const y_hem = y_waist - 0.16F;

    constexpr int segs = 12;
    constexpr float pi = std::numbers::pi_v<float>;

    auto drawRing = [&](float y, float w, float d, const QVector3D &col,
                        float th) {
      for (int i = 0; i < segs; ++i) {
        float a1 = (float(i) / segs) * 2.0F * pi;
        float a2 = (float(i + 1) / segs) * 2.0F * pi;
        QVector3D p1 = origin + right * (w * std::sin(a1)) +
                       forward * (d * std::cos(a1)) + up * (y - origin.y());
        QVector3D p2 = origin + right * (w * std::sin(a2)) +
                       forward * (d * std::cos(a2)) + up * (y - origin.y());
        out.mesh(get_unit_cylinder(), cylinder_between(ctx.model, p1, p2, th),
                 col, nullptr, 1.0F);
      }
    };

    drawRing(y_shoulder + 0.04F, torso_r * 0.68F, torso_d * 0.60F, tunic_dark,
             0.022F);

    drawRing(y_shoulder + 0.02F, torso_r * 1.08F, torso_d * 1.02F, tunic_base,
             0.032F);

    for (int i = 0; i < 5; ++i) {
      float t = float(i) / 4.0F;
      float y = y_shoulder - 0.01F - t * (y_shoulder - y_waist - 0.03F);
      float w = torso_r * (1.04F - t * 0.14F);
      float d = torso_d * (0.98F - t * 0.10F);
      QVector3D col = tunic_base * (1.0F - t * 0.06F);
      drawRing(y, w, d, col, 0.026F - t * 0.004F);
    }

    for (int i = 0; i < 4; ++i) {
      float t = float(i) / 3.0F;
      float y = y_waist - 0.01F - t * (y_waist - y_hem);
      float flare = 1.0F + t * 0.18F;
      QVector3D col = tunic_base * (1.0F - t * 0.08F);
      drawRing(y, torso_r * 0.80F * flare, torso_d * 0.76F * flare, col,
               0.018F + t * 0.006F);
    }

    auto drawSleeve = [&](const QVector3D &shoulder, const QVector3D &out_dir,
                          const QVector3D &elbow) {
      for (int i = 0; i < 3; ++i) {
        float t = float(i) / 3.0F;
        QVector3D pos =
            shoulder * (1.0F - t) + elbow * t * 0.6F + out_dir * 0.008F;
        float r = HP::UPPER_ARM_R * (1.40F - t * 0.25F);
        out.mesh(get_unit_sphere(), sphere_at(ctx.model, pos, r),
                 tunic_base * (1.0F - t * 0.04F), nullptr, 1.0F);
      }
    };
    drawSleeve(frames.shoulder_l.origin, -right, pose.elbow_l);
    drawSleeve(frames.shoulder_r.origin, right, pose.elbow_r);

    draw_extended_forearm(ctx, v, pose, out);
  }

  void draw_extended_forearm(const DrawContext &ctx, const HumanoidVariant &v,
                             const HumanoidPose &pose, ISubmitter &out) const {

    QVector3D const skin_color = v.palette.skin;

    QVector3D const elbow_r = pose.elbow_r;
    QVector3D const hand_r = pose.hand_r;

    for (int i = 0; i < 4; ++i) {
      float t = 0.25F + float(i) * 0.20F;
      QVector3D pos = elbow_r * (1.0F - t) + hand_r * t;
      float r = 0.024F - float(i) * 0.002F;
      out.mesh(get_unit_sphere(), sphere_at(ctx.model, pos, r), skin_color,
               nullptr, 1.0F);
    }
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

  auto resolve_shader_key(const DrawContext &ctx) const -> QString {
    const BuilderStyleConfig &style = resolve_style(ctx);
    if (!style.shader_id.empty()) {
      return QString::fromStdString(style.shader_id);
    }
    return QStringLiteral("builder");
  }

  void apply_palette_overrides(const BuilderStyleConfig &style,
                               const QVector3D &team_tint,
                               HumanoidVariant &variant) const {
    auto apply = [&](const std::optional<QVector3D> &c, QVector3D &t) {
      t = mix_palette_color(t, c, team_tint, k_team_mix_weight,
                            k_style_mix_weight);
    };
    apply(style.cloth_color, variant.palette.cloth);
    apply(style.leather_color, variant.palette.leather);
    apply(style.leather_dark_color, variant.palette.leatherDark);
    apply(style.metal_color, variant.palette.metal);
    apply(style.wood_color, variant.palette.wood);
  }
};

void register_builder_renderer(Render::GL::EntityRendererRegistry &registry) {
  ensure_builder_styles_registered();
  static BuilderRenderer const renderer;
  registry.register_renderer(
      "troops/roman/builder", [](const DrawContext &ctx, ISubmitter &out) {
        static BuilderRenderer const r;
        Shader *shader = nullptr;
        if (ctx.backend != nullptr) {
          QString key = r.resolve_shader_key(ctx);
          shader = ctx.backend->shader(key);
          if (!shader) {
            shader = ctx.backend->shader(QStringLiteral("builder"));
          }
        }
        auto *sr = dynamic_cast<Renderer *>(&out);
        if (sr && shader) {
          sr->set_current_shader(shader);
        }
        r.render(ctx, out);
        if (sr) {
          sr->set_current_shader(nullptr);
        }
      });
}

} // namespace Render::GL::Roman
