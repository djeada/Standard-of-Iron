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
  // Carthaginian craftsmen - skilled and lean
  auto get_proportion_scaling() const -> QVector3D override {
    return {0.98F, 1.01F, 0.96F};
  }

  void get_variant(const DrawContext &ctx, uint32_t seed,
                   HumanoidVariant &v) const override {
    QVector3D const team_tint = resolve_team_tint(ctx);
    v.palette = make_humanoid_palette(team_tint, seed);
    auto const &style = resolve_style(ctx);
    apply_palette_overrides(style, team_tint, v);

    // Carthaginian craftsmen often have beards
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

    // Craftsman pose - measuring/working stance
    float const forward = 0.20F + (anim.is_moving ? 0.02F : 0.0F);
    QVector3D const work_hand(-0.12F + asym, HP::WAIST_Y + 0.10F + jitter, forward + 0.04F);
    QVector3D const guide_hand(0.18F - asym * 0.5F, HP::SHOULDER_Y - 0.04F + jitter * 0.5F, 0.15F);

    controller.placeHandAt(true, work_hand);
    controller.placeHandAt(false, guide_hand);
  }

  void add_attachments(const DrawContext &ctx, const HumanoidVariant &v,
                       const HumanoidPose &pose,
                       const HumanoidAnimationContext &anim_ctx,
                       ISubmitter &out) const override {
    uint32_t const seed = reinterpret_cast<uintptr_t>(ctx.entity) & 0xFFFFFFFFU;
    
    // Varied tools for Phoenician craftsmen
    float const tool_roll = hash_01(seed ^ 0x2345U);
    if (tool_roll < 0.30F) {
      draw_adze(ctx, v, pose, seed, out);
    } else if (tool_roll < 0.55F) {
      draw_chisel_mallet(ctx, v, pose, seed, out);
    } else if (tool_roll < 0.80F) {
      draw_saw(ctx, v, pose, seed, out);
    } else {
      draw_measuring_rod(ctx, v, pose, seed, out);
    }
    
    // Some carry a rope coil
    if (hash_01(seed ^ 0x6789U) < 0.35F) {
      draw_rope_coil(ctx, v, pose, out);
    }
  }

  void draw_adze(const DrawContext &ctx, const HumanoidVariant &v,
                 const HumanoidPose &pose, uint32_t seed, ISubmitter &out) const {
    QVector3D const wood = v.palette.wood;
    QVector3D const metal = v.palette.metal * 0.88F;

    QVector3D const hand = pose.hand_l;
    QVector3D const up(0.0F, 1.0F, 0.0F);
    QVector3D const fwd(0.0F, 0.0F, 1.0F);

    float const h_len = 0.30F;
    QVector3D const h_top = hand + up * 0.11F;
    QVector3D const h_bot = h_top - up * h_len;

    out.mesh(get_unit_cylinder(), cylinder_between(ctx.model, h_bot, h_top, 0.015F), wood, nullptr, 1.0F);

    // Adze blade (curved, perpendicular to handle)
    QVector3D const blade_start = h_top + up * 0.02F;
    QVector3D const blade_end = blade_start + fwd * 0.09F - up * 0.03F;
    out.mesh(get_unit_cylinder(), cylinder_between(ctx.model, blade_start, blade_end, 0.018F), metal, nullptr, 1.0F);
  }

  void draw_chisel_mallet(const DrawContext &ctx, const HumanoidVariant &v,
                          const HumanoidPose &pose, uint32_t seed, ISubmitter &out) const {
    QVector3D const wood = v.palette.wood;
    QVector3D const metal = v.palette.metal;

    QVector3D const hand_l = pose.hand_l;
    QVector3D const hand_r = pose.hand_r;
    QVector3D const up(0.0F, 1.0F, 0.0F);
    QVector3D const right(1.0F, 0.0F, 0.0F);

    // Chisel in left hand
    QVector3D const c_top = hand_l + up * 0.08F;
    QVector3D const c_bot = c_top - up * 0.16F;
    out.mesh(get_unit_cylinder(), cylinder_between(ctx.model, c_bot, c_top, 0.010F), metal, nullptr, 1.0F);

    // Small mallet in right hand
    QVector3D const m_top = hand_r + up * 0.08F;
    QVector3D const m_bot = m_top - up * 0.18F;
    out.mesh(get_unit_cylinder(), cylinder_between(ctx.model, m_bot, m_top, 0.012F), wood, nullptr, 1.0F);
    
    QVector3D const head = m_top + up * 0.02F;
    out.mesh(get_unit_cylinder(),
             cylinder_between(ctx.model, head - right * 0.03F, head + right * 0.03F, 0.028F),
             wood * 0.8F, nullptr, 1.0F);
  }

  void draw_saw(const DrawContext &ctx, const HumanoidVariant &v,
                const HumanoidPose &pose, uint32_t seed, ISubmitter &out) const {
    QVector3D const wood = v.palette.wood;
    QVector3D const metal = v.palette.metal * 0.9F;

    QVector3D const hand = pose.hand_l;
    QVector3D const up(0.0F, 1.0F, 0.0F);
    QVector3D const fwd(0.0F, 0.0F, 1.0F);

    // Handle
    QVector3D const h_start = hand;
    QVector3D const h_end = hand + up * 0.08F + fwd * 0.02F;
    out.mesh(get_unit_cylinder(), cylinder_between(ctx.model, h_start, h_end, 0.016F), wood, nullptr, 1.0F);

    // Blade
    QVector3D const b_start = h_end;
    QVector3D const b_end = b_start + fwd * 0.25F - up * 0.02F;
    out.mesh(get_unit_cylinder(), cylinder_between(ctx.model, b_start, b_end, 0.008F), metal, nullptr, 1.0F);
  }

  void draw_measuring_rod(const DrawContext &ctx, const HumanoidVariant &v,
                          const HumanoidPose &pose, uint32_t seed, ISubmitter &out) const {
    QVector3D const wood = v.palette.wood * 1.1F;
    QVector3D const mark_color(0.2F, 0.15F, 0.1F);

    QVector3D const hand = pose.hand_l;
    QVector3D const up(0.0F, 1.0F, 0.0F);

    // Long measuring rod
    QVector3D const rod_top = hand + up * 0.35F;
    QVector3D const rod_bot = hand - up * 0.10F;
    out.mesh(get_unit_cylinder(), cylinder_between(ctx.model, rod_bot, rod_top, 0.012F), wood, nullptr, 1.0F);

    // Measurement marks
    for (int i = 0; i < 4; ++i) {
      float t = 0.2F + float(i) * 0.2F;
      QVector3D pos = rod_bot * (1.0F - t) + rod_top * t;
      out.mesh(get_unit_sphere(), sphere_at(ctx.model, pos, 0.014F), mark_color, nullptr, 1.0F);
    }
  }

  void draw_rope_coil(const DrawContext &ctx, const HumanoidVariant &v,
                      const HumanoidPose &pose, ISubmitter &out) const {
    const BodyFrames &frames = pose.body_frames;
    QVector3D const rope_color(0.55F, 0.45F, 0.32F);

    // Rope coil worn across shoulder
    QVector3D const shoulder = frames.shoulder_r.origin;
    QVector3D const hip = frames.waist.origin - frames.waist.right * 0.06F;
    
    // Draw as series of spheres
    for (int i = 0; i < 6; ++i) {
      float t = float(i) / 5.0F;
      QVector3D pos = shoulder * (1.0F - t) + hip * t;
      pos += QVector3D(0.02F, 0.0F, 0.03F);
      out.mesh(get_unit_sphere(), sphere_at(ctx.model, pos, 0.022F), rope_color, nullptr, 1.0F);
    }
  }

  void draw_helmet(const DrawContext &ctx, const HumanoidVariant &v,
                   const HumanoidPose &pose, ISubmitter &out) const override {
    // Phoenician craftsmen wear headwraps
    draw_headwrap(ctx, v, pose, out);
  }

  void draw_headwrap(const DrawContext &ctx, const HumanoidVariant &v,
                     const HumanoidPose &pose, ISubmitter &out) const {
    const BodyFrames &frames = pose.body_frames;
    QVector3D const wrap_color(0.88F, 0.82F, 0.72F);  // Linen color
    
    QVector3D const head_top = frames.head.origin + frames.head.up * 0.05F;
    QVector3D const head_back = frames.head.origin - frames.head.forward * 0.03F + frames.head.up * 0.02F;
    
    // Wrapped cloth on head
    out.mesh(get_unit_sphere(), sphere_at(ctx.model, head_top, 0.052F), wrap_color, nullptr, 1.0F);
    out.mesh(get_unit_sphere(), sphere_at(ctx.model, head_back, 0.048F), wrap_color * 0.95F, nullptr, 1.0F);
  }

  void draw_armor(const DrawContext &ctx, const HumanoidVariant &v,
                  const HumanoidPose &pose,
                  const HumanoidAnimationContext &anim,
                  ISubmitter &out) const override {
    uint32_t const seed = reinterpret_cast<uintptr_t>(ctx.entity) & 0xFFFFFFFFU;
    draw_craftsman_robes(ctx, v, pose, seed, out);
  }

  void draw_craftsman_robes(const DrawContext &ctx, const HumanoidVariant &v,
                            const HumanoidPose &pose, uint32_t seed, ISubmitter &out) const {
    using HP = HumanProportions;
    const BodyFrames &frames = pose.body_frames;
    const AttachmentFrame &torso = frames.torso;
    const AttachmentFrame &waist = frames.waist;

    if (torso.radius <= 0.0F) return;

    // Varied linen/wool colors
    float const var = hash_01(seed ^ 0xCDEU);
    QVector3D robe_color;
    if (var < 0.35F) {
      robe_color = QVector3D(0.85F, 0.78F, 0.68F); // Light linen
    } else if (var < 0.65F) {
      robe_color = QVector3D(0.72F, 0.65F, 0.55F); // Natural wool
    } else {
      robe_color = QVector3D(0.62F, 0.58F, 0.52F); // Grey-brown
    }
    
    QVector3D const robe_dark = robe_color * 0.88F;
    QVector3D const sash_color(0.55F, 0.25F, 0.18F); // Red-brown sash
    QVector3D const leather = v.palette.leather;

    const QVector3D &origin = torso.origin;
    const QVector3D &right = torso.right;
    const QVector3D &up = torso.up;
    const QVector3D &forward = torso.forward;
    float const tr = torso.radius * 1.06F;
    float const td = (torso.depth > 0.0F) ? torso.depth * 0.90F : torso.radius * 0.78F;

    float const y_sh = origin.y() + 0.035F;
    float const y_w = waist.origin.y();
    float const y_hem = y_w - 0.22F;

    constexpr int segs = 12;
    constexpr float pi = std::numbers::pi_v<float>;

    auto ring = [&](float y, float w, float d, const QVector3D &c, float th) {
      for (int i = 0; i < segs; ++i) {
        float a1 = (float(i) / segs) * 2.0F * pi;
        float a2 = (float(i + 1) / segs) * 2.0F * pi;
        QVector3D p1 = origin + right * (w * std::sin(a1)) + forward * (d * std::cos(a1)) + up * (y - origin.y());
        QVector3D p2 = origin + right * (w * std::sin(a2)) + forward * (d * std::cos(a2)) + up * (y - origin.y());
        out.mesh(get_unit_cylinder(), cylinder_between(ctx.model, p1, p2, th), c, nullptr, 1.0F);
      }
    };

    // Neck
    ring(y_sh + 0.045F, tr * 0.65F, td * 0.58F, robe_dark, 0.020F);
    
    // Shoulders - wide for Phoenician style
    ring(y_sh + 0.03F, tr * 1.15F, td * 1.08F, robe_color, 0.035F);
    ring(y_sh, tr * 1.10F, td * 1.04F, robe_color, 0.032F);

    // Torso
    for (int i = 0; i < 5; ++i) {
      float t = float(i) / 4.0F;
      float y = y_sh - 0.02F - t * (y_sh - y_w - 0.02F);
      QVector3D c = robe_color * (1.0F - t * 0.05F);
      ring(y, tr * (1.06F - t * 0.12F), td * (1.00F - t * 0.10F), c, 0.026F - t * 0.003F);
    }

    // Sash/belt area
    float const sash_y = y_w + 0.005F;
    QVector3D const sash_c = origin + up * (sash_y - origin.y());
    out.mesh(get_unit_cylinder(),
             cylinder_between(ctx.model, sash_c - up * 0.022F, sash_c + up * 0.022F, tr * 0.94F),
             sash_color, nullptr, 1.0F);

    // Longer skirt - Phoenician style
    for (int i = 0; i < 6; ++i) {
      float t = float(i) / 5.0F;
      float y = y_w - 0.02F - t * (y_w - y_hem);
      float flare = 1.0F + t * 0.28F;
      QVector3D c = robe_color * (1.0F - t * 0.06F);
      ring(y, tr * 0.85F * flare, td * 0.80F * flare, c, 0.020F + t * 0.008F);
    }

    // Tool bag on belt
    QVector3D const bag = origin + right * (tr * 0.72F) + up * (sash_y - 0.06F - origin.y()) + forward * td * 0.08F;
    out.mesh(get_unit_cube(),
             [&]() {
               QMatrix4x4 m = ctx.model;
               m.translate(bag);
               m.scale(0.042F, 0.058F, 0.035F);
               return m;
             }(),
             leather * 0.92F, nullptr, 1.0F);

    // Flowing sleeves
    auto sleeve = [&](const QVector3D &sh, const QVector3D &out_dir) {
      QVector3D const back = -forward;
      QVector3D anchor = sh + up * 0.055F + back * 0.012F;
      for (int i = 0; i < 4; ++i) {
        float t = float(i) / 4.0F;
        QVector3D pos = anchor + out_dir * (0.012F + t * 0.022F) + forward * (-0.012F + t * 0.05F) - up * (t * 0.035F);
        float r = HP::UPPER_ARM_R * (1.48F - t * 0.08F);
        out.mesh(get_unit_sphere(), sphere_at(ctx.model, pos, r), robe_color * (1.0F - t * 0.03F), nullptr, 1.0F);
      }
    };
    sleeve(frames.shoulder_l.origin, -right);
    sleeve(frames.shoulder_r.origin, right);
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
    auto f = styles.find(std::string(k_default_style_key));
    if (f != styles.end()) return f->second;
    static const BuilderStyleConfig def{};
    return def;
  }

public:
  auto resolve_shader_key(const DrawContext &ctx) const -> QString {
    const BuilderStyleConfig &s = resolve_style(ctx);
    if (!s.shader_id.empty()) return QString::fromStdString(s.shader_id);
    return QStringLiteral("builder");
  }

private:
  void apply_palette_overrides(const BuilderStyleConfig &style,
                               const QVector3D &team_tint,
                               HumanoidVariant &v) const {
    auto apply = [&](const std::optional<QVector3D> &c, QVector3D &t, float tw, float sw) {
      t = mix_palette_color(t, c, team_tint, tw, sw);
    };
    apply(style.skin_color, v.palette.skin, 0.0F, 1.0F);
    apply(style.cloth_color, v.palette.cloth, 0.0F, 1.0F);
    apply(style.leather_color, v.palette.leather, k_team_mix_weight, k_style_mix_weight);
    apply(style.leather_dark_color, v.palette.leatherDark, k_team_mix_weight, k_style_mix_weight);
    apply(style.metal_color, v.palette.metal, k_team_mix_weight, k_style_mix_weight);
    apply(style.wood_color, v.palette.wood, k_team_mix_weight, k_style_mix_weight);
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
          if (!shader) shader = ctx.backend->shader(QStringLiteral("builder"));
        }
        auto *sr = dynamic_cast<Renderer *>(&out);
        if (sr && shader) sr->set_current_shader(shader);
        r.render(ctx, out);
        if (sr) sr->set_current_shader(nullptr);
      });
}

} // namespace Render::GL::Carthage
