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

// Builder tool types for variety
enum class BuilderTool { Hammer, Pickaxe, Chisel, Trowel, PlumbBob };

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
  // Builders are sturdy workers
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

    // Working pose - one hand forward holding tool, other at side
    float const tool_hand_forward = 0.22F + (anim.is_moving ? 0.03F : 0.0F);
    float const tool_hand_height = HP::WAIST_Y + 0.08F + arm_jitter;

    QVector3D const tool_hand(-0.10F + asymmetry, tool_hand_height + 0.04F,
                              tool_hand_forward);
    QVector3D const rest_hand(0.20F - asymmetry * 0.5F,
                              HP::WAIST_Y + 0.02F + arm_jitter * 0.5F,
                              0.12F);

    controller.placeHandAt(true, tool_hand);  // Left hand holds tool
    controller.placeHandAt(false, rest_hand); // Right hand at rest
  }

  void add_attachments(const DrawContext &ctx, const HumanoidVariant &v,
                       const HumanoidPose &pose,
                       const HumanoidAnimationContext &anim_ctx,
                       ISubmitter &out) const override {
    uint32_t const seed = reinterpret_cast<uintptr_t>(ctx.entity) & 0xFFFFFFFFU;
    
    // Each builder gets a different tool based on seed
    float const tool_roll = hash_01(seed ^ 0x1234U);
    if (tool_roll < 0.35F) {
      draw_hammer(ctx, v, pose, seed, out);
    } else if (tool_roll < 0.55F) {
      draw_pickaxe(ctx, v, pose, seed, out);
    } else if (tool_roll < 0.75F) {
      draw_trowel(ctx, v, pose, seed, out);
    } else {
      draw_mallet(ctx, v, pose, seed, out);
    }
    
    // Some builders carry a plumb bob
    if (hash_01(seed ^ 0x5678U) < 0.3F) {
      draw_plumb_bob(ctx, v, pose, out);
    }
  }

  void draw_hammer(const DrawContext &ctx, const HumanoidVariant &v,
                   const HumanoidPose &pose, uint32_t seed, ISubmitter &out) const {
    QVector3D const wood_color = v.palette.wood;
    QVector3D const metal_color = v.palette.metal;

    QVector3D const hand = pose.hand_l;
    QVector3D const up(0.0F, 1.0F, 0.0F);
    QVector3D const forward(0.0F, 0.0F, 1.0F);
    QVector3D const right(1.0F, 0.0F, 0.0F);

    float const handle_len = 0.28F + hash_01(seed ^ 0x111U) * 0.04F;
    float const handle_r = 0.014F;
    QVector3D const handle_top = hand + up * 0.10F + forward * 0.02F;
    QVector3D const handle_bot = handle_top - up * handle_len;

    out.mesh(get_unit_cylinder(),
             cylinder_between(ctx.model, handle_bot, handle_top, handle_r),
             wood_color, nullptr, 1.0F);

    // Hammer head - cross shape
    float const head_len = 0.08F;
    float const head_r = 0.022F;
    QVector3D const head_center = handle_top + up * 0.025F;
    
    out.mesh(get_unit_cylinder(),
             cylinder_between(ctx.model, head_center - right * (head_len * 0.5F),
                              head_center + right * (head_len * 0.5F), head_r),
             metal_color, nullptr, 1.0F);
    
    // Hammer face (flat end)
    out.mesh(get_unit_sphere(),
             sphere_at(ctx.model, head_center + right * (head_len * 0.5F), head_r * 1.2F),
             metal_color * 0.9F, nullptr, 1.0F);
  }

  void draw_pickaxe(const DrawContext &ctx, const HumanoidVariant &v,
                    const HumanoidPose &pose, uint32_t seed, ISubmitter &out) const {
    QVector3D const wood_color = v.palette.wood;
    QVector3D const metal_color = v.palette.metal * 0.85F;

    QVector3D const hand = pose.hand_l;
    QVector3D const up(0.0F, 1.0F, 0.0F);
    QVector3D const forward(0.0F, 0.0F, 1.0F);

    float const handle_len = 0.35F;
    float const handle_r = 0.016F;
    QVector3D const handle_top = hand + up * 0.12F;
    QVector3D const handle_bot = handle_top - up * handle_len;

    out.mesh(get_unit_cylinder(),
             cylinder_between(ctx.model, handle_bot, handle_top, handle_r),
             wood_color, nullptr, 1.0F);

    // Pickaxe head - pointed both ends
    QVector3D const head_center = handle_top + up * 0.02F;
    float const head_len = 0.12F;
    
    // Front pick
    out.mesh(get_unit_cylinder(),
             cylinder_between(ctx.model, head_center, 
                              head_center + forward * head_len - up * 0.02F, 0.012F),
             metal_color, nullptr, 1.0F);
    // Back chisel
    out.mesh(get_unit_cylinder(),
             cylinder_between(ctx.model, head_center, 
                              head_center - forward * (head_len * 0.7F), 0.018F),
             metal_color, nullptr, 1.0F);
  }

  void draw_trowel(const DrawContext &ctx, const HumanoidVariant &v,
                   const HumanoidPose &pose, uint32_t seed, ISubmitter &out) const {
    QVector3D const wood_color = v.palette.wood;
    QVector3D const metal_color = v.palette.metal;

    QVector3D const hand = pose.hand_l;
    QVector3D const up(0.0F, 1.0F, 0.0F);
    QVector3D const forward(0.0F, 0.0F, 1.0F);

    float const handle_len = 0.12F;
    QVector3D const handle_start = hand + up * 0.04F;
    QVector3D const handle_end = handle_start + up * handle_len;

    out.mesh(get_unit_cylinder(),
             cylinder_between(ctx.model, handle_start, handle_end, 0.015F),
             wood_color, nullptr, 1.0F);

    // Trowel blade (flat triangle shape approximated)
    QVector3D const blade_start = handle_end + forward * 0.01F;
    QVector3D const blade_end = blade_start + forward * 0.10F - up * 0.02F;
    
    out.mesh(get_unit_cylinder(),
             cylinder_between(ctx.model, blade_start, blade_end, 0.025F),
             metal_color, nullptr, 1.0F);
  }

  void draw_mallet(const DrawContext &ctx, const HumanoidVariant &v,
                   const HumanoidPose &pose, uint32_t seed, ISubmitter &out) const {
    QVector3D const wood_color = v.palette.wood;
    QVector3D const wood_dark = v.palette.wood * 0.75F;

    QVector3D const hand = pose.hand_l;
    QVector3D const up(0.0F, 1.0F, 0.0F);
    QVector3D const right(1.0F, 0.0F, 0.0F);

    float const handle_len = 0.26F;
    QVector3D const handle_top = hand + up * 0.10F;
    QVector3D const handle_bot = handle_top - up * handle_len;

    out.mesh(get_unit_cylinder(),
             cylinder_between(ctx.model, handle_bot, handle_top, 0.013F),
             wood_color, nullptr, 1.0F);

    // Wooden mallet head (cylinder)
    QVector3D const head_center = handle_top + up * 0.03F;
    float const head_len = 0.08F;
    float const head_r = 0.035F;
    
    out.mesh(get_unit_cylinder(),
             cylinder_between(ctx.model, head_center - right * (head_len * 0.5F),
                              head_center + right * (head_len * 0.5F), head_r),
             wood_dark, nullptr, 1.0F);
  }

  void draw_plumb_bob(const DrawContext &ctx, const HumanoidVariant &v,
                      const HumanoidPose &pose, ISubmitter &out) const {
    const BodyFrames &frames = pose.body_frames;
    QVector3D const metal_color = v.palette.metal;
    QVector3D const string_color(0.6F, 0.55F, 0.45F);

    // Plumb bob hangs from belt
    QVector3D const belt_pos = frames.waist.origin + frames.waist.right * 0.08F;
    QVector3D const bob_pos = belt_pos - QVector3D(0.0F, 0.15F, 0.0F);

    // String
    out.mesh(get_unit_cylinder(),
             cylinder_between(ctx.model, belt_pos, bob_pos, 0.004F),
             string_color, nullptr, 1.0F);
    
    // Bob (cone shape approximated with sphere)
    out.mesh(get_unit_sphere(), sphere_at(ctx.model, bob_pos, 0.018F),
             metal_color * 0.9F, nullptr, 1.0F);
  }

  void draw_helmet(const DrawContext &ctx, const HumanoidVariant &v,
                   const HumanoidPose &pose, ISubmitter &out) const override {
    // Builders wear a simple leather cap, not military helmet
    draw_leather_cap(ctx, v, pose, out);
  }

  void draw_leather_cap(const DrawContext &ctx, const HumanoidVariant &v,
                        const HumanoidPose &pose, ISubmitter &out) const {
    const BodyFrames &frames = pose.body_frames;
    QVector3D const leather_color = v.palette.leather * 1.1F;
    
    QVector3D const head_top = frames.head.origin + frames.head.up * 0.06F;
    float const cap_r = 0.055F;
    
    // Simple rounded cap
    out.mesh(get_unit_sphere(), sphere_at(ctx.model, head_top, cap_r),
             leather_color, nullptr, 1.0F);
  }

  void draw_armor(const DrawContext &ctx, const HumanoidVariant &v,
                  const HumanoidPose &pose,
                  const HumanoidAnimationContext &anim,
                  ISubmitter &out) const override {
    uint32_t const seed = reinterpret_cast<uintptr_t>(ctx.entity) & 0xFFFFFFFFU;
    draw_work_tunic(ctx, v, pose, seed, out);
  }

  void draw_work_tunic(const DrawContext &ctx, const HumanoidVariant &v,
                       const HumanoidPose &pose, uint32_t seed, ISubmitter &out) const {
    using HP = HumanProportions;
    const BodyFrames &frames = pose.body_frames;
    const AttachmentFrame &torso = frames.torso;
    const AttachmentFrame &waist = frames.waist;
    const BuilderStyleConfig &style = resolve_style(ctx);

    if (torso.radius <= 0.0F) return;

    // Varied tunic colors for different workers
    float const color_var = hash_01(seed ^ 0xABCU);
    QVector3D tunic_base;
    if (color_var < 0.4F) {
      tunic_base = QVector3D(0.65F, 0.52F, 0.38F); // Brown
    } else if (color_var < 0.7F) {
      tunic_base = QVector3D(0.58F, 0.48F, 0.35F); // Darker brown
    } else {
      tunic_base = QVector3D(0.72F, 0.62F, 0.48F); // Tan
    }
    
    QVector3D const tunic_dark = tunic_base * 0.85F;
    QVector3D const leather_color = v.palette.leather;
    QVector3D const metal_color = v.palette.metal;

    const QVector3D &origin = torso.origin;
    const QVector3D &right = torso.right;
    const QVector3D &up = torso.up;
    const QVector3D &forward = torso.forward;
    float const torso_r = torso.radius * 1.08F;
    float const torso_d = (torso.depth > 0.0F) ? torso.depth * 0.92F : torso.radius * 0.80F;

    float const y_shoulder = origin.y() + 0.032F;
    float const y_waist = waist.origin.y();
    float const y_hem = y_waist - 0.16F;

    constexpr int segs = 12;
    constexpr float pi = std::numbers::pi_v<float>;

    auto drawRing = [&](float y, float w, float d, const QVector3D &col, float th) {
      for (int i = 0; i < segs; ++i) {
        float a1 = (float(i) / segs) * 2.0F * pi;
        float a2 = (float(i + 1) / segs) * 2.0F * pi;
        QVector3D p1 = origin + right * (w * std::sin(a1)) + forward * (d * std::cos(a1)) + up * (y - origin.y());
        QVector3D p2 = origin + right * (w * std::sin(a2)) + forward * (d * std::cos(a2)) + up * (y - origin.y());
        out.mesh(get_unit_cylinder(), cylinder_between(ctx.model, p1, p2, th), col, nullptr, 1.0F);
      }
    };

    // Neck opening
    drawRing(y_shoulder + 0.04F, torso_r * 0.68F, torso_d * 0.60F, tunic_dark, 0.022F);
    
    // Shoulder area
    drawRing(y_shoulder + 0.02F, torso_r * 1.08F, torso_d * 1.02F, tunic_base, 0.032F);
    
    // Torso
    for (int i = 0; i < 5; ++i) {
      float t = float(i) / 4.0F;
      float y = y_shoulder - 0.01F - t * (y_shoulder - y_waist - 0.03F);
      float w = torso_r * (1.04F - t * 0.14F);
      float d = torso_d * (0.98F - t * 0.10F);
      QVector3D col = tunic_base * (1.0F - t * 0.06F);
      drawRing(y, w, d, col, 0.026F - t * 0.004F);
    }

    // Short skirt (workers wear shorter tunics)
    for (int i = 0; i < 4; ++i) {
      float t = float(i) / 3.0F;
      float y = y_waist - 0.01F - t * (y_waist - y_hem);
      float flare = 1.0F + t * 0.18F;
      QVector3D col = tunic_base * (1.0F - t * 0.08F);
      drawRing(y, torso_r * 0.80F * flare, torso_d * 0.76F * flare, col, 0.018F + t * 0.006F);
    }

    // Leather work belt
    float const belt_y = y_waist + 0.008F;
    QVector3D const belt_c = origin + up * (belt_y - origin.y());
    out.mesh(get_unit_cylinder(),
             cylinder_between(ctx.model, belt_c - up * 0.018F, belt_c + up * 0.018F, torso_r * 0.92F),
             leather_color, nullptr, 1.0F);

    // Belt buckle
    out.mesh(get_unit_sphere(),
             sphere_at(ctx.model, belt_c + forward * (torso_d * 0.88F), 0.020F),
             metal_color, nullptr, 1.0F);

    // Tool pouch on belt (right side)
    QVector3D const pouch = origin + right * (torso_r * 0.78F) + up * (belt_y - 0.05F - origin.y()) + forward * (torso_d * 0.10F);
    out.mesh(get_unit_cube(),
             [&]() {
               QMatrix4x4 m = ctx.model;
               m.translate(pouch);
               m.scale(0.045F, 0.055F, 0.032F);
               return m;
             }(),
             leather_color * 0.88F, nullptr, 1.0F);

    // Simple short sleeves
    auto drawSleeve = [&](const QVector3D &shoulder, const QVector3D &out_dir, const QVector3D &elbow) {
      for (int i = 0; i < 3; ++i) {
        float t = float(i) / 3.0F;
        QVector3D pos = shoulder * (1.0F - t) + elbow * t * 0.6F + out_dir * 0.008F;
        float r = HP::UPPER_ARM_R * (1.40F - t * 0.25F);
        out.mesh(get_unit_sphere(), sphere_at(ctx.model, pos, r), tunic_base * (1.0F - t * 0.04F), nullptr, 1.0F);
      }
    };
    drawSleeve(frames.shoulder_l.origin, -right, pose.elbow_l);
    drawSleeve(frames.shoulder_r.origin, right, pose.elbow_r);

    // Arm wraps/bracers for protection
    draw_arm_wraps(ctx, v, pose, out);
  }

  void draw_arm_wraps(const DrawContext &ctx, const HumanoidVariant &v,
                      const HumanoidPose &pose, ISubmitter &out) const {
    QVector3D const wrap_color = v.palette.leather * 0.95F;
    
    // Forearm wraps
    auto drawWrap = [&](const QVector3D &elbow, const QVector3D &hand) {
      for (int i = 0; i < 3; ++i) {
        float t = 0.3F + float(i) * 0.2F;
        QVector3D pos = elbow * (1.0F - t) + hand * t;
        out.mesh(get_unit_sphere(), sphere_at(ctx.model, pos, 0.022F), wrap_color, nullptr, 1.0F);
      }
    };
    drawWrap(pose.elbow_l, pose.hand_l);
    drawWrap(pose.elbow_r, pose.hand_r);
  }

private:
  auto resolve_style(const DrawContext &ctx) const -> const BuilderStyleConfig & {
    ensure_builder_styles_registered();
    auto &styles = style_registry();
    std::string nation_id;
    if (ctx.entity != nullptr) {
      if (auto *unit = ctx.entity->get_component<Engine::Core::UnitComponent>()) {
        nation_id = Game::Systems::nationIDToString(unit->nation_id);
      }
    }
    if (!nation_id.empty()) {
      auto it = styles.find(nation_id);
      if (it != styles.end()) return it->second;
    }
    auto fallback = styles.find(std::string(k_default_style_key));
    if (fallback != styles.end()) return fallback->second;
    static const BuilderStyleConfig default_style{};
    return default_style;
  }

public:
  auto resolve_shader_key(const DrawContext &ctx) const -> QString {
    const BuilderStyleConfig &style = resolve_style(ctx);
    if (!style.shader_id.empty()) return QString::fromStdString(style.shader_id);
    return QStringLiteral("builder");
  }

private:
  void apply_palette_overrides(const BuilderStyleConfig &style,
                               const QVector3D &team_tint,
                               HumanoidVariant &variant) const {
    auto apply = [&](const std::optional<QVector3D> &c, QVector3D &t) {
      t = mix_palette_color(t, c, team_tint, k_team_mix_weight, k_style_mix_weight);
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
          if (!shader) shader = ctx.backend->shader(QStringLiteral("builder"));
        }
        auto *sr = dynamic_cast<Renderer *>(&out);
        if (sr && shader) sr->set_current_shader(shader);
        r.render(ctx, out);
        if (sr) sr->set_current_shader(nullptr);
      });
}

} // namespace Render::GL::Roman
