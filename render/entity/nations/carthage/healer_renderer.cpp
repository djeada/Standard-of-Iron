#include "healer_renderer.h"
#include "../../../../game/core/component.h"
#include "../../../../game/core/entity.h"
#include "../../../../game/systems/nation_id.h"
#include "../../../creature/archetype_registry.h"
#include "../../../creature/pipeline/unit_visual_spec.h"
#include "../../../equipment/armor/armor_light_carthage.h"
#include "../../../equipment/equipment_submit.h"
#include "../../../geom/math_utils.h"
#include "../../../geom/transforms.h"
#include "../../../gl/backend.h"
#include "../../../gl/primitives.h"
#include "../../../gl/render_constants.h"
#include "../../../gl/shader.h"
#include "../../../humanoid/humanoid_math.h"
#include "../../../humanoid/humanoid_renderer_base.h"
#include "../../../humanoid/humanoid_spec.h"
#include "../../../humanoid/humanoid_specs.h"
#include "../../../humanoid/pose_controller.h"
#include "../../../humanoid/skeleton.h"
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
#include <array>
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

auto style_registry() -> std::unordered_map<std::string, HealerStyleConfig> & {
  static std::unordered_map<std::string, HealerStyleConfig> styles;
  return styles;
}

void ensure_healer_styles_registered() {
  static const bool registered = []() {
    register_carthage_healer_style();
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
using Render::Geom::clamp_f;
using Render::GL::Humanoid::mix_palette_color;
using Render::GL::Humanoid::saturate_color;

class HealerRenderer : public HumanoidRendererBase {
public:
  auto get_proportion_scaling() const -> QVector3D override {

    return {0.88F, 0.99F, 0.90F};
  }

  auto visual_spec() const
      -> const Render::Creature::Pipeline::UnitVisualSpec & override {
    using namespace Render::Creature::Pipeline;

    ensure_healer_styles_registered();

    static const UnitVisualSpec spec = []() {
      static const auto k_chest_bone =
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Chest);
      static const auto k_armor_base_role_byte =
          static_cast<std::uint8_t>(Render::Humanoid::kHumanoidRoleCount + 1U);
      static const Render::Creature::StaticAttachmentSpec k_armor_spec =
          Render::GL::armor_light_carthage_make_static_attachment(
              k_chest_bone, k_armor_base_role_byte);
      static const std::array<Render::Creature::StaticAttachmentSpec, 1>
          k_attachments{k_armor_spec};
      static const auto extra_roles_fn =
          +[](const void *variant_void, QVector3D *out,
              std::uint32_t base_count,
              std::size_t max_count) -> std::uint32_t {
        if (variant_void == nullptr || max_count <= base_count) {
          return base_count;
        }
        const auto &v = *static_cast<const HumanoidVariant *>(variant_void);
        return base_count +
               Render::GL::armor_light_carthage_fill_role_colors(
                   v.palette, out + base_count, max_count - base_count);
      };
      static const auto k_archetype =
          Render::Creature::ArchetypeRegistry::instance()
              .register_unit_archetype(
                  "troops/carthage/healer", CreatureKind::Humanoid,
                  std::span<const Render::Creature::StaticAttachmentSpec>(
                      k_attachments.data(), k_attachments.size()),
                  extra_roles_fn);

      UnitVisualSpec s{};
      s.kind = CreatureKind::Humanoid;
      s.debug_name = "troops/carthage/healer";
      s.scaling = ProportionScaling{0.88F, 0.99F, 0.90F};
      s.owned_legacy_slots = LegacySlotMask::AllHumanoid;
      s.archetype_id = k_archetype;
      return s;
    }();
    return spec;
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
      wants_beard = (beard_roll < 0.85F);
    }

    if (wants_beard) {
      float const style_roll = nextRand(beard_seed);

      if (style_roll < 0.45F) {
        v.facial_hair.style = FacialHairStyle::ShortBeard;
        v.facial_hair.length = 0.8F + nextRand(beard_seed) * 0.4F;
      } else if (style_roll < 0.75F) {
        v.facial_hair.style = FacialHairStyle::FullBeard;
        v.facial_hair.length = 0.9F + nextRand(beard_seed) * 0.5F;
      } else if (style_roll < 0.90F) {
        v.facial_hair.style = FacialHairStyle::Goatee;
        v.facial_hair.length = 0.7F + nextRand(beard_seed) * 0.4F;
      } else {
        v.facial_hair.style = FacialHairStyle::MustacheAndBeard;
        v.facial_hair.length = 1.0F + nextRand(beard_seed) * 0.4F;
      }

      float const color_roll = nextRand(beard_seed);
      if (color_roll < 0.55F) {

        v.facial_hair.color = QVector3D(0.12F + nextRand(beard_seed) * 0.08F,
                                        0.10F + nextRand(beard_seed) * 0.06F,
                                        0.08F + nextRand(beard_seed) * 0.05F);
      } else if (color_roll < 0.80F) {

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
    float const arm_asymmetry = (hash_01(seed ^ 0xDEF0U) - 0.5F) * 0.04F;

    if (anim.is_healing) {

      float const healing_time = anim.time * 2.2F;
      float const sway_phase = std::sin(healing_time);
      float const sway_phase_offset = std::sin(healing_time + 0.7F);

      float const high_arm_height =
          HP::SHOULDER_Y + 0.18F + 0.04F * sway_phase + arm_height_jitter;
      float const extended_arm_height = HP::SHOULDER_Y + 0.05F +
                                        0.02F * sway_phase_offset +
                                        arm_height_jitter;

      float const circle_x = 0.03F * std::sin(healing_time * 0.8F);
      float const circle_z = 0.02F * std::cos(healing_time * 0.8F);

      QVector3D const heal_hand_l(-0.08F + arm_asymmetry + circle_x,
                                  high_arm_height, 0.28F + circle_z);
      QVector3D const heal_hand_r(0.15F - arm_asymmetry * 0.5F,
                                  extended_arm_height,
                                  0.40F + 0.03F * sway_phase_offset);

      controller.place_hand_at(true, heal_hand_l);
      controller.place_hand_at(false, heal_hand_r);

      float target_dir_x = 0.0F;
      float target_dir_z = 1.0F;
      float const target_dist =
          std::sqrt(anim.healing_target_dx * anim.healing_target_dx +
                    anim.healing_target_dz * anim.healing_target_dz);
      if (target_dist > 0.01F) {
        target_dir_x = anim.healing_target_dx / target_dist;
        target_dir_z = anim.healing_target_dz / target_dist;
      }

      QVector3D const look_dir(target_dir_x, 0.0F, target_dir_z);
      QVector3D const head_focus =
          pose.head_pos +
          QVector3D(look_dir.x() * 0.18F, 0.0F, look_dir.z() * 0.45F);
      controller.look_at(head_focus);

    } else {

      QVector3D const idle_hand_l(-0.10F + arm_asymmetry,
                                  HP::SHOULDER_Y + 0.10F + arm_height_jitter,
                                  0.45F);
      QVector3D const idle_hand_r(
          0.10F - arm_asymmetry * 0.5F,
          HP::SHOULDER_Y + 0.10F + arm_height_jitter * 0.8F, 0.45F);

      controller.place_hand_at(true, idle_hand_l);
      controller.place_hand_at(false, idle_hand_r);
    }
  }

  void draw_healer_robes(const DrawContext &ctx, const HumanoidVariant &v,
                         const HumanoidPose &pose,
                         EquipmentBatch &batch) const {
    using HP = HumanProportions;
    const BodyFrames &frames = pose.body_frames;
    const AttachmentFrame &torso = frames.torso;
    const AttachmentFrame &waist = frames.waist;

    if (torso.radius <= 0.0F) {
      return;
    }

    QVector3D const team_tint = resolve_team_tint(ctx);
    QVector3D const robe_cream(0.46F, 0.46F, 0.48F);
    QVector3D const robe_light(0.42F, 0.42F, 0.44F);
    QVector3D const robe_tan(0.38F, 0.38F, 0.40F);
    QVector3D const purple_tyrian(0.05F, 0.05F, 0.05F);
    QVector3D const purple_dark(0.05F, 0.05F, 0.05F);
    QVector3D const bronze_color(0.78F, 0.58F, 0.32F);

    const QVector3D &origin = torso.origin;
    const QVector3D &right = torso.right;
    const QVector3D &up = torso.up;
    const QVector3D &forward = torso.forward;

    constexpr int k_mat_tunic = 1;
    constexpr int k_mat_purple_trim = 2;
    constexpr int k_mat_tools = 4;
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

        batch.meshes.push_back({get_unit_cylinder(), nullptr,
                                cylinder_between(ctx.model, p1, p2, thickness),
                                color, nullptr, 1.0F, materialId});
      }
    };

    drawRobeRing(y_shoulder - 0.00F, torso_r * 1.22F, torso_depth * 1.12F,
                 robe_cream, 0.036F, k_mat_tunic);
    drawRobeRing(y_shoulder - 0.05F, torso_r * 1.30F, torso_depth * 1.18F,
                 robe_cream, 0.038F, k_mat_tunic);

    drawRobeRing(y_shoulder - 0.09F, torso_r * 1.12F, torso_depth * 1.00F,
                 robe_cream, 0.032F, k_mat_tunic);

    float const torso_fill_top = y_shoulder - 0.12F;
    float const torso_fill_bot = y_waist + 0.04F;
    constexpr int torso_fill_layers = 8;
    for (int i = 0; i < torso_fill_layers; ++i) {
      float const t =
          static_cast<float>(i) / static_cast<float>(torso_fill_layers - 1);
      float const y = torso_fill_top + (torso_fill_bot - torso_fill_top) * t;
      float const width = torso_r * (1.08F - t * 0.22F);
      float const depth = torso_depth * (1.00F - t * 0.18F);
      float const thickness = 0.030F - t * 0.010F;
      QVector3D const c =
          (t < 0.35F) ? robe_cream : robe_light * (1.0F - (t - 0.35F) * 0.3F);
      drawRobeRing(y, width, depth, c, thickness, k_mat_tunic);
    }

    float const skirt_flare = 1.40F;
    constexpr int skirt_layers = 9;
    for (int layer = 0; layer < skirt_layers; ++layer) {
      float const t =
          static_cast<float>(layer) / static_cast<float>(skirt_layers - 1);
      float const y = y_waist - t * 0.32F;
      float const flare = 1.0F + t * (skirt_flare - 1.0F);
      QVector3D const skirt_color = robe_cream * (1.0F - t * 0.08F);
      drawRobeRing(y, torso_r * 0.90F * flare, torso_depth * 0.84F * flare,
                   skirt_color, 0.022F + t * 0.012F, k_mat_tunic);
    }

    float const sash_y = y_waist + 0.01F;
    QVector3D const sash_top = origin + up * (sash_y + 0.028F - origin.y());
    QVector3D const sash_bot = origin + up * (sash_y - 0.028F - origin.y());
    batch.meshes.push_back(
        {get_unit_cylinder(), nullptr,
         cylinder_between(ctx.model, sash_bot, sash_top, torso_r * 0.99F),
         purple_tyrian, nullptr, 1.0F, k_mat_purple_trim});

    batch.meshes.push_back(
        {get_unit_cylinder(), nullptr,
         cylinder_between(ctx.model, sash_top, sash_top - up * 0.006F,
                          torso_r * 1.02F),
         team_tint, nullptr, 1.0F, k_mat_tools});
    batch.meshes.push_back({get_unit_cylinder(), nullptr,
                            cylinder_between(ctx.model, sash_bot + up * 0.006F,
                                             sash_bot, torso_r * 1.02F),
                            team_tint, nullptr, 1.0F, k_mat_tools});

    QVector3D const sash_hang_start =
        origin + right * (torso_r * 0.3F) + up * (sash_y - origin.y());
    QVector3D const sash_hang_end =
        sash_hang_start - up * 0.12F + forward * 0.02F;
    batch.meshes.push_back(
        {get_unit_cylinder(), nullptr,
         cylinder_between(ctx.model, sash_hang_start, sash_hang_end, 0.018F),
         purple_dark, nullptr, 1.0F, k_mat_purple_trim});

    batch.meshes.push_back(
        {get_unit_sphere(), nullptr,
         sphere_at(ctx.model, sash_hang_end - up * 0.01F, 0.015F), bronze_color,
         nullptr, 1.0F, k_mat_tools});

    float const neck_y = y_shoulder + 0.04F;
    QVector3D const neck_center = origin + up * (neck_y - origin.y());

    batch.meshes.push_back(
        {get_unit_cylinder(), nullptr,
         cylinder_between(ctx.model, neck_center - up * 0.012F,
                          neck_center + up * 0.012F, HP::NECK_RADIUS * 1.7F),
         robe_tan, nullptr, 1.0F, k_mat_tunic});

    batch.meshes.push_back(
        {get_unit_cylinder(), nullptr,
         cylinder_between(ctx.model, neck_center + up * 0.010F,
                          neck_center + up * 0.018F, HP::NECK_RADIUS * 2.0F),
         purple_tyrian * 0.9F, nullptr, 1.0F, k_mat_purple_trim});

    auto drawFlowingSleeve = [&](const QVector3D &shoulder_pos,
                                 const QVector3D &outward) {
      QVector3D const backward = -forward;
      QVector3D const anchor = shoulder_pos + up * 0.070F + backward * 0.020F;
      for (int i = 0; i < 5; ++i) {
        float const t = static_cast<float>(i) / 5.0F;
        QVector3D const sleeve_pos = anchor + outward * (0.014F + t * 0.030F) +
                                     forward * (-0.020F + t * 0.065F) -
                                     up * (t * 0.05F);
        float const sleeve_r = HP::UPPER_ARM_R * (1.55F - t * 0.08F);
        QVector3D const sleeve_color = robe_cream * (1.0F - t * 0.04F);
        batch.meshes.push_back({get_unit_sphere(), nullptr,
                                sphere_at(ctx.model, sleeve_pos, sleeve_r),
                                sleeve_color, nullptr, 1.0F, k_mat_tunic});
      }

      QVector3D const cuff_pos =
          anchor + outward * 0.055F + forward * 0.040F - up * 0.05F;
      batch.meshes.push_back(
          {get_unit_sphere(), nullptr,
           sphere_at(ctx.model, cuff_pos, HP::UPPER_ARM_R * 1.15F),
           purple_tyrian * 0.85F, nullptr, 1.0F, k_mat_purple_trim});
    };
    drawFlowingSleeve(frames.shoulder_l.origin, -right);
    drawFlowingSleeve(frames.shoulder_r.origin, right);

    QVector3D const pendant_pos = origin + forward * (torso_depth * 0.6F) +
                                  up * (y_shoulder - 0.06F - origin.y());
    batch.meshes.push_back({get_unit_sphere(), nullptr,
                            sphere_at(ctx.model, pendant_pos, 0.022F),
                            bronze_color, nullptr, 1.0F, k_mat_tools});

    batch.meshes.push_back(
        {get_unit_cylinder(), nullptr,
         cylinder_between(ctx.model,
                          neck_center + forward * (torso_depth * 0.3F),
                          pendant_pos + up * 0.01F, 0.006F),
         bronze_color * 0.85F, nullptr, 1.0F, k_mat_tools});
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
        nation_id = Game::Systems::nation_id_to_string(unit->nation_id);
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

private:
  void apply_palette_overrides(const HealerStyleConfig &style,
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
    apply_color(style.leather_dark_color, variant.palette.leather_dark,
                k_team_mix_weight, k_style_mix_weight);
    apply_color(style.metal_color, variant.palette.metal, k_team_mix_weight,
                k_style_mix_weight);
    apply_color(style.wood_color, variant.palette.wood, k_team_mix_weight,
                k_style_mix_weight);
  }
};

void register_healer_renderer(Render::GL::EntityRendererRegistry &registry) {
  ensure_healer_styles_registered();
  static HealerRenderer const renderer;
  registry.register_renderer("troops/carthage/healer",
                             [](const DrawContext &ctx, ISubmitter &out) {
                               static HealerRenderer const static_renderer;
                               static_renderer.render(ctx, out);
                             });
}

} // namespace Render::GL::Carthage
