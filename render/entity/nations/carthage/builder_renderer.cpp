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
  friend void
  register_builder_renderer(Render::GL::EntityRendererRegistry &registry);

  auto get_proportion_scaling() const -> QVector3D override {
    return {0.98F, 1.01F, 0.96F};
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
    if (style.force_beard || nextRand(beard_seed) < 0.75F) {
      float const style_roll = nextRand(beard_seed);
      if (style_roll < 0.5F) {
        v.facial_hair.style = FacialHairStyle::ShortBeard;
        v.facial_hair.length = 0.7F + nextRand(beard_seed) * 0.3F;
      } else if (style_roll < 0.8F) {
        v.facial_hair.style = FacialHairStyle::FullBeard;
        v.facial_hair.length = 0.8F + nextRand(beard_seed) * 0.4F;
      } else {
        v.facial_hair.style = FacialHairStyle::Goatee;
        v.facial_hair.length = 0.6F + nextRand(beard_seed) * 0.3F;
      }
      v.facial_hair.color = QVector3D(0.15F + nextRand(beard_seed) * 0.1F,
                                      0.12F + nextRand(beard_seed) * 0.08F,
                                      0.10F + nextRand(beard_seed) * 0.06F);
      v.facial_hair.thickness = 0.8F + nextRand(beard_seed) * 0.2F;
    }
  }

  void customize_pose(const DrawContext &,
                      const HumanoidAnimationContext &anim_ctx, uint32_t seed,
                      HumanoidPose &pose) const override {
    using HP = HumanProportions;

    const AnimationInputs &anim = anim_ctx.inputs;
    HumanoidPoseController controller(pose, anim_ctx);

    float const jitter = (hash_01(seed ^ 0xABCDU) - 0.5F) * 0.04F;
    float const asym = (hash_01(seed ^ 0xDEF0U) - 0.5F) * 0.05F;

    if (anim.is_constructing) {

      uint32_t const pose_selector = seed % 100;

      float const phase_offset = float(seed % 100) * 0.0628F;
      float const cycle_speed = 2.0F + float(seed % 50) * 0.02F;
      float const swing_cycle =
          std::fmod(anim.time * cycle_speed + phase_offset, 1.0F);

      if (pose_selector < 40) {

        apply_hammering_pose(controller, swing_cycle, asym, seed);
      } else if (pose_selector < 70) {

        apply_kneeling_work_pose(controller, swing_cycle, asym, seed);
      } else if (pose_selector < 90) {

        apply_sawing_pose(controller, swing_cycle, asym, seed);
      } else {

        apply_lifting_pose(controller, swing_cycle, asym, seed);
      }
      return;
    }

    float const forward = 0.20F + (anim.is_moving ? 0.02F : 0.0F);
    QVector3D const hammer_hand(-0.12F + asym, HP::WAIST_Y + 0.10F + jitter,
                                forward + 0.04F);

    QVector3D const rest_hand(0.22F - asym * 0.5F,
                              HP::WAIST_Y - 0.04F + jitter * 0.5F, 0.10F);

    controller.placeHandAt(true, hammer_hand);
    controller.placeHandAt(false, rest_hand);
  }

  void add_attachments(const DrawContext &ctx, const HumanoidVariant &v,
                       const HumanoidPose &pose,
                       const HumanoidAnimationContext &anim_ctx,
                       ISubmitter &out) const override {
    auto &registry = EquipmentRegistry::instance();

    auto work_apron =
        registry.get(EquipmentCategory::Armor, "work_apron_carthage");
    if (work_apron) {
      work_apron->render(ctx, pose.body_frames, v.palette, anim_ctx, out);
    }

    auto tool_belt =
        registry.get(EquipmentCategory::Armor, "tool_belt_carthage");
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

    draw_headwrap(ctx, v, pose, out);
  }

  void draw_armor(const DrawContext &ctx, const HumanoidVariant &v,
                  const HumanoidPose &pose,
                  const HumanoidAnimationContext &anim,
                  ISubmitter &out) const override {
    uint32_t const seed = reinterpret_cast<uintptr_t>(ctx.entity) & 0xFFFFFFFFU;
    draw_craftsman_robes(ctx, v, pose, seed, out);
  }

private:
  static constexpr uint32_t KNEEL_SEED_OFFSET = 0x5678U;

  void apply_hammering_pose(HumanoidPoseController &controller,
                            float swing_cycle, float asym,
                            uint32_t seed) const {
    using HP = HumanProportions;

    float swing_angle;
    float body_lean;
    float crouch_amount;

    if (swing_cycle < 0.3F) {

      float const t = swing_cycle / 0.3F;
      swing_angle = t * 0.92F;
      body_lean = -t * 0.09F;
      crouch_amount = 0.0F;
    } else if (swing_cycle < 0.5F) {

      float const t = (swing_cycle - 0.3F) / 0.2F;
      swing_angle = 0.92F - t * 1.45F;
      body_lean = -0.09F + t * 0.26F;
      crouch_amount = t * 0.07F;
    } else if (swing_cycle < 0.6F) {

      float const t = (swing_cycle - 0.5F) / 0.1F;
      swing_angle = -0.53F + t * 0.16F;
      body_lean = 0.17F - t * 0.05F;
      crouch_amount = 0.07F - t * 0.02F;
    } else {

      float const t = (swing_cycle - 0.6F) / 0.4F;
      swing_angle = -0.37F + t * 0.37F;
      body_lean = 0.12F * (1.0F - t);
      crouch_amount = 0.05F * (1.0F - t);
    }

    float const torso_y_offset = -crouch_amount;
    float const hammer_y = HP::SHOULDER_Y + 0.10F + swing_angle * 0.20F;
    float const hammer_forward =
        0.18F + std::abs(swing_angle) * 0.15F + body_lean * 0.5F;
    float const hammer_down =
        swing_cycle > 0.4F && swing_cycle < 0.65F ? 0.08F : 0.0F;

    QVector3D const hammer_hand(
        -0.06F + asym, hammer_y - hammer_down + torso_y_offset, hammer_forward);

    float const brace_y =
        HP::WAIST_Y + 0.12F + torso_y_offset - crouch_amount * 0.5F;
    float const brace_forward = 0.15F + body_lean * 0.3F;
    QVector3D const brace_hand(0.14F - asym * 0.5F, brace_y, brace_forward);

    controller.placeHandAt(true, hammer_hand);
    controller.placeHandAt(false, brace_hand);
  }

  void apply_kneeling_work_pose(HumanoidPoseController &controller, float cycle,
                                float asym, uint32_t seed) const {
    using HP = HumanProportions;

    float const kneel_depth =
        0.50F + (hash_01(seed ^ KNEEL_SEED_OFFSET) * 0.12F);
    controller.kneel(kneel_depth);

    float const work_cycle = std::sin(cycle * std::numbers::pi_v<float> * 2.0F);

    float const tool_y = HP::WAIST_Y * 0.32F + work_cycle * 0.07F;
    float const tool_x_offset = 0.06F + work_cycle * 0.05F;
    QVector3D const tool_hand(-tool_x_offset + asym, tool_y, 0.24F);

    float const brace_x = 0.20F - asym * 0.5F;
    QVector3D const brace_hand(brace_x, HP::WAIST_Y * 0.28F, 0.22F);

    controller.placeHandAt(true, tool_hand);
    controller.placeHandAt(false, brace_hand);
  }

  void apply_sawing_pose(HumanoidPoseController &controller, float cycle,
                         float asym, uint32_t seed) const {
    using HP = HumanProportions;

    controller.lean(QVector3D(0.0F, 0.0F, 1.0F), 0.14F);

    float const saw_offset =
        std::sin(cycle * std::numbers::pi_v<float> * 4.0F) * 0.14F;

    float const saw_y = HP::WAIST_Y + 0.18F;
    float const saw_z = 0.22F + saw_offset;

    QVector3D const left_hand(-0.10F + asym, saw_y, saw_z);
    QVector3D const right_hand(0.10F - asym, saw_y + 0.03F, saw_z);

    controller.placeHandAt(true, left_hand);
    controller.placeHandAt(false, right_hand);
  }

  void apply_lifting_pose(HumanoidPoseController &controller, float cycle,
                          float asym, uint32_t seed) const {
    using HP = HumanProportions;

    float lift_height;
    float crouch;

    if (cycle < 0.3F) {

      float const t = cycle / 0.3F;
      lift_height = HP::WAIST_Y * (1.0F - t * 0.5F);
      crouch = t * 0.22F;
    } else if (cycle < 0.6F) {

      float const t = (cycle - 0.3F) / 0.3F;
      lift_height =
          HP::WAIST_Y * 0.5F + t * (HP::SHOULDER_Y - HP::WAIST_Y * 0.5F);
      crouch = 0.22F * (1.0F - t);
    } else if (cycle < 0.8F) {

      lift_height = HP::SHOULDER_Y;
      crouch = 0.0F;
    } else {

      float const t = (cycle - 0.8F) / 0.2F;
      lift_height = HP::SHOULDER_Y * (1.0F - t * 0.35F);
      crouch = 0.0F;
    }

    QVector3D const left_hand(-0.14F + asym, lift_height, 0.18F);
    QVector3D const right_hand(0.14F - asym, lift_height, 0.18F);

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
    QVector3D const wood = v.palette.wood;

    QVector3D const stone_color(0.52F, 0.50F, 0.46F);
    QVector3D const stone_dark(0.42F, 0.40F, 0.36F);

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

    float const h_len = 0.30F;
    QVector3D const handle_offset = anim.is_constructing
                                        ? (forward * 0.11F + up * 0.02F)
                                        : (up * 0.11F + forward * 0.02F);
    QVector3D const h_top = hand + handle_offset;
    QVector3D const h_bot = h_top - handle_axis * h_len;

    out.mesh(get_unit_cylinder(),
             cylinder_between(ctx.model, h_bot, h_top, 0.015F), wood, nullptr,
             1.0F);

    float const head_len = 0.09F;
    float const head_r = 0.028F;
    QVector3D const head_center = h_top + handle_axis * 0.03F;

    out.mesh(
        get_unit_cylinder(),
        cylinder_between(ctx.model, head_center - head_axis * (head_len * 0.5F),
                         head_center + head_axis * (head_len * 0.5F), head_r),
        stone_color, nullptr, 1.0F);

    out.mesh(get_unit_sphere(),
             sphere_at(ctx.model, head_center + head_axis * (head_len * 0.5F),
                       head_r * 1.1F),
             stone_dark, nullptr, 1.0F);

    out.mesh(get_unit_sphere(),
             sphere_at(ctx.model, head_center - head_axis * (head_len * 0.5F),
                       head_r * 0.85F),
             stone_color * 0.92F, nullptr, 1.0F);
  }

  void draw_headwrap(const DrawContext &ctx, const HumanoidVariant &v,
                     const HumanoidPose &pose, ISubmitter &out) const {
    const BodyFrames &frames = pose.body_frames;
    QVector3D const wrap_color(0.88F, 0.82F, 0.72F);

    QVector3D const head_top = frames.head.origin + frames.head.up * 0.05F;
    QVector3D const head_back = frames.head.origin -
                                frames.head.forward * 0.03F +
                                frames.head.up * 0.02F;

    out.mesh(get_unit_sphere(), sphere_at(ctx.model, head_top, 0.052F),
             wrap_color, nullptr, 1.0F);
    out.mesh(get_unit_sphere(), sphere_at(ctx.model, head_back, 0.048F),
             wrap_color * 0.95F, nullptr, 1.0F);
  }

  void draw_craftsman_robes(const DrawContext &ctx, const HumanoidVariant &v,
                            const HumanoidPose &pose, uint32_t seed,
                            ISubmitter &out) const {
    using HP = HumanProportions;
    const BodyFrames &frames = pose.body_frames;
    const AttachmentFrame &torso = frames.torso;
    const AttachmentFrame &waist = frames.waist;

    if (torso.radius <= 0.0F) {
      return;
    }

    float const var = hash_01(seed ^ 0xCDEU);
    QVector3D robe_color;
    if (var < 0.35F) {
      robe_color = QVector3D(0.85F, 0.78F, 0.68F);
    } else if (var < 0.65F) {
      robe_color = QVector3D(0.72F, 0.65F, 0.55F);
    } else {
      robe_color = QVector3D(0.62F, 0.58F, 0.52F);
    }

    QVector3D const robe_dark = robe_color * 0.88F;

    const QVector3D &origin = torso.origin;
    const QVector3D &right = torso.right;
    const QVector3D &up = torso.up;
    const QVector3D &forward = torso.forward;
    float const tr = torso.radius * 1.06F;
    float const td =
        (torso.depth > 0.0F) ? torso.depth * 0.90F : torso.radius * 0.78F;

    float const y_sh = origin.y() + 0.035F;
    float const y_w = waist.origin.y();
    float const y_hem = y_w - 0.22F;

    constexpr int segs = 12;
    constexpr float pi = std::numbers::pi_v<float>;

    auto ring = [&](float y, float w, float d, const QVector3D &c, float th) {
      for (int i = 0; i < segs; ++i) {
        float a1 = (float(i) / segs) * 2.0F * pi;
        float a2 = (float(i + 1) / segs) * 2.0F * pi;
        QVector3D p1 = origin + right * (w * std::sin(a1)) +
                       forward * (d * std::cos(a1)) + up * (y - origin.y());
        QVector3D p2 = origin + right * (w * std::sin(a2)) +
                       forward * (d * std::cos(a2)) + up * (y - origin.y());
        out.mesh(get_unit_cylinder(), cylinder_between(ctx.model, p1, p2, th),
                 c, nullptr, 1.0F);
      }
    };

    ring(y_sh + 0.045F, tr * 0.65F, td * 0.58F, robe_dark, 0.020F);

    ring(y_sh + 0.03F, tr * 1.15F, td * 1.08F, robe_color, 0.035F);
    ring(y_sh, tr * 1.10F, td * 1.04F, robe_color, 0.032F);

    for (int i = 0; i < 5; ++i) {
      float t = float(i) / 4.0F;
      float y = y_sh - 0.02F - t * (y_sh - y_w - 0.02F);
      QVector3D c = robe_color * (1.0F - t * 0.05F);
      ring(y, tr * (1.06F - t * 0.12F), td * (1.00F - t * 0.10F), c,
           0.026F - t * 0.003F);
    }

    for (int i = 0; i < 6; ++i) {
      float t = float(i) / 5.0F;
      float y = y_w - 0.02F - t * (y_w - y_hem);
      float flare = 1.0F + t * 0.28F;
      QVector3D c = robe_color * (1.0F - t * 0.06F);
      ring(y, tr * 0.85F * flare, td * 0.80F * flare, c, 0.020F + t * 0.008F);
    }

    auto sleeve = [&](const QVector3D &sh, const QVector3D &out_dir) {
      QVector3D const back = -forward;
      QVector3D anchor = sh + up * 0.055F + back * 0.012F;
      for (int i = 0; i < 4; ++i) {
        float t = float(i) / 4.0F;
        QVector3D pos = anchor + out_dir * (0.012F + t * 0.022F) +
                        forward * (-0.012F + t * 0.05F) - up * (t * 0.035F);
        float r = HP::UPPER_ARM_R * (1.48F - t * 0.08F);
        out.mesh(get_unit_sphere(), sphere_at(ctx.model, pos, r),
                 robe_color * (1.0F - t * 0.03F), nullptr, 1.0F);
      }
    };
    sleeve(frames.shoulder_l.origin, -right);
    sleeve(frames.shoulder_r.origin, right);

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
      float r = 0.022F - float(i) * 0.002F;
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
    auto f = styles.find(std::string(k_default_style_key));
    if (f != styles.end()) {
      return f->second;
    }
    static const BuilderStyleConfig def{};
    return def;
  }

  auto resolve_shader_key(const DrawContext &ctx) const -> QString {
    const BuilderStyleConfig &s = resolve_style(ctx);
    if (!s.shader_id.empty()) {
      return QString::fromStdString(s.shader_id);
    }
    return QStringLiteral("builder");
  }

  void apply_palette_overrides(const BuilderStyleConfig &style,
                               const QVector3D &team_tint,
                               HumanoidVariant &v) const {
    auto apply = [&](const std::optional<QVector3D> &c, QVector3D &t, float tw,
                     float sw) {
      t = mix_palette_color(t, c, team_tint, tw, sw);
    };
    apply(style.skin_color, v.palette.skin, 0.0F, 1.0F);
    apply(style.cloth_color, v.palette.cloth, 0.0F, 1.0F);
    apply(style.leather_color, v.palette.leather, k_team_mix_weight,
          k_style_mix_weight);
    apply(style.leather_dark_color, v.palette.leatherDark, k_team_mix_weight,
          k_style_mix_weight);
    apply(style.metal_color, v.palette.metal, k_team_mix_weight,
          k_style_mix_weight);
    apply(style.wood_color, v.palette.wood, k_team_mix_weight,
          k_style_mix_weight);
  }
};

void register_builder_renderer(Render::GL::EntityRendererRegistry &registry) {
  ensure_builder_styles_registered();
  static BuilderRenderer const renderer;
  registry.register_renderer(
      "troops/carthage/builder", [](const DrawContext &ctx, ISubmitter &out) {
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

} // namespace Render::GL::Carthage
