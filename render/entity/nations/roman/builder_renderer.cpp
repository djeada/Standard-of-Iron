#include "builder_renderer.h"
#include "../../../../game/core/component.h"
#include "../../../../game/core/entity.h"
#include "../../../../game/systems/nation_id.h"
#include "../../../creature/archetype_registry.h"
#include "../../../creature/pipeline/unit_visual_spec.h"
#include "../../../equipment/armor/arm_guards_renderer.h"
#include "../../../equipment/armor/tool_belt_renderer.h"
#include "../../../equipment/armor/torso_local_archetype_utils.h"
#include "../../../equipment/armor/work_apron_renderer.h"
#include "../../../equipment/attachment_builder.h"
#include "../../../equipment/generated_equipment.h"
#include "../../../equipment/helmets/roman_light_helmet.h"
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
constexpr std::uint32_t kBuilderWorkTunicRoleCount = 2;
constexpr std::uint32_t kBuilderHammerRoleCount = 3;

enum BuilderWorkTunicPaletteSlot : std::uint8_t {
  k_builder_tunic_base_slot = 0U,
  k_builder_tunic_dark_slot = 1U,
};

enum BuilderHammerPaletteSlot : std::uint8_t {
  k_builder_hammer_wood_slot = 0U,
  k_builder_hammer_stone_slot = 1U,
  k_builder_hammer_stone_dark_slot = 2U,
};

auto builder_work_tunic_fill_role_colors(const HumanoidPalette &palette,
                                         QVector3D *out,
                                         std::size_t max) -> std::uint32_t {
  if (max < kBuilderWorkTunicRoleCount) {
    return 0U;
  }
  out[0] = palette.cloth;
  out[1] = palette.cloth * 0.85F;
  return kBuilderWorkTunicRoleCount;
}

auto builder_hammer_fill_role_colors(const HumanoidPalette &palette,
                                     QVector3D *out,
                                     std::size_t max) -> std::uint32_t {
  if (max < kBuilderHammerRoleCount) {
    return 0U;
  }
  out[0] = palette.wood;
  out[1] = QVector3D(0.55F, 0.52F, 0.48F);
  out[2] = QVector3D(0.45F, 0.42F, 0.38F);
  return kBuilderHammerRoleCount;
}

auto builder_work_tunic_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    using GP = Render::GL::GeneratedEquipmentPrimitive;
    std::vector<GP> primitives;
    const auto &bind_frames = Render::Humanoid::humanoid_bind_body_frames();
    const AttachmentFrame &torso = bind_frames.torso;
    const AttachmentFrame &waist = bind_frames.waist;
    const TorsoLocalFrame torso_local =
        make_torso_local_frame(QMatrix4x4{}, torso);

    float const torso_r = torso.radius * 1.08F;
    float const torso_d =
        (torso.depth > 0.0F) ? torso.depth * 0.92F : torso.radius * 0.80F;
    float const y_shoulder = 0.032F;
    float const y_waist = torso_local.point(waist.origin).y();
    float const y_hem = y_waist - 0.16F;

    constexpr int segs = 12;
    constexpr float pi = std::numbers::pi_v<float>;
    auto add_ring = [&](float y, float w, float d, std::uint8_t slot,
                        float thickness) {
      for (int i = 0; i < segs; ++i) {
        float const a1 = (static_cast<float>(i) / segs) * 2.0F * pi;
        float const a2 = (static_cast<float>(i + 1) / segs) * 2.0F * pi;
        primitives.push_back(generated_cylinder(
            QVector3D(w * std::sin(a1), y, d * std::cos(a1)),
            QVector3D(w * std::sin(a2), y, d * std::cos(a2)), thickness, slot));
      }
    };

    add_ring(y_shoulder + 0.04F, torso_r * 0.68F, torso_d * 0.60F,
             k_builder_tunic_dark_slot, 0.022F);
    add_ring(y_shoulder + 0.02F, torso_r * 1.08F, torso_d * 1.02F,
             k_builder_tunic_base_slot, 0.032F);

    for (int i = 0; i < 5; ++i) {
      float const t = static_cast<float>(i) / 4.0F;
      float const y = y_shoulder - 0.01F - t * (y_shoulder - y_waist - 0.03F);
      float const w = torso_r * (1.04F - t * 0.14F);
      float const d = torso_d * (0.98F - t * 0.10F);
      add_ring(y, w, d, k_builder_tunic_base_slot, 0.026F - t * 0.004F);
    }

    for (int i = 0; i < 4; ++i) {
      float const t = static_cast<float>(i) / 3.0F;
      float const y = y_waist - 0.01F - t * (y_waist - y_hem);
      float const flare = 1.0F + t * 0.18F;
      add_ring(y, torso_r * 0.80F * flare, torso_d * 0.76F * flare,
               k_builder_tunic_base_slot, 0.018F + t * 0.006F);
    }

    auto add_sleeve = [&](const AttachmentFrame &shoulder,
                          const AttachmentFrame &hand, float side_sign) {
      QVector3D const shoulder_local = torso_local.point(shoulder.origin);
      QVector3D const elbow_local = torso_local.point(
          shoulder.origin + (hand.origin - shoulder.origin) * 0.55F);
      for (int i = 0; i < 3; ++i) {
        float const t = static_cast<float>(i) / 3.0F;
        QVector3D const pos = shoulder_local * (1.0F - t) +
                              elbow_local * t * 0.6F +
                              QVector3D(side_sign * 0.008F, 0.0F, 0.0F);
        float const r = HumanProportions::UPPER_ARM_R * (1.40F - t * 0.25F);
        primitives.push_back(generated_sphere(
            pos, r,
            t < 0.34F ? k_builder_tunic_dark_slot : k_builder_tunic_base_slot));
      }
    };

    add_sleeve(bind_frames.shoulder_l, bind_frames.hand_l, -1.0F);
    add_sleeve(bind_frames.shoulder_r, bind_frames.hand_r, 1.0F);

    return build_generated_equipment_archetype(
        "roman_builder_work_tunic",
        std::span<const GP>(primitives.data(), primitives.size()));
  }();
  return archetype;
}

auto builder_work_tunic_make_static_attachment(std::uint16_t chest_bone_index,
                                               std::uint8_t base_role_byte)
    -> Render::Creature::StaticAttachmentSpec {
  const auto &bind_frames = Render::Humanoid::humanoid_bind_body_frames();
  const TorsoLocalFrame torso_local =
      make_torso_local_frame(QMatrix4x4{}, bind_frames.torso);
  auto spec = Render::Equipment::build_static_attachment({
      .archetype = &builder_work_tunic_archetype(),
      .socket_bone_index = chest_bone_index,
      .unit_local_pose_at_bind = torso_local.world,
  });
  spec.palette_role_remap[k_builder_tunic_base_slot] =
      static_cast<std::uint8_t>(base_role_byte + 0U);
  spec.palette_role_remap[k_builder_tunic_dark_slot] =
      static_cast<std::uint8_t>(base_role_byte + 1U);
  return spec;
}

auto builder_hammer_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    std::array<GeneratedEquipmentPrimitive, 4> const primitives{{
        generated_cylinder(QVector3D(0.0F, -0.20F, 0.02F),
                           QVector3D(0.0F, 0.12F, 0.02F), 0.016F,
                           k_builder_hammer_wood_slot),
        generated_cylinder(QVector3D(-0.05F, 0.155F, 0.02F),
                           QVector3D(0.05F, 0.155F, 0.02F), 0.030F,
                           k_builder_hammer_stone_slot),
        generated_sphere(QVector3D(0.05F, 0.155F, 0.02F), 0.0345F,
                         k_builder_hammer_stone_dark_slot),
        generated_sphere(QVector3D(-0.05F, 0.155F, 0.02F), 0.027F,
                         k_builder_hammer_stone_slot),
    }};
    return build_generated_equipment_archetype("roman_builder_hammer",
                                               primitives);
  }();
  return archetype;
}

auto builder_hammer_make_static_attachment(std::uint16_t hand_l_bone_index,
                                           std::uint8_t base_role_byte)
    -> Render::Creature::StaticAttachmentSpec {
  const auto hand_l_bind_matrix =
      Render::Humanoid::humanoid_bind_palette()[static_cast<std::size_t>(
          Render::Humanoid::HumanoidBone::HandL)];
  auto spec = Render::Equipment::build_static_attachment({
      .archetype = &builder_hammer_archetype(),
      .socket_bone_index = hand_l_bone_index,
      .unit_local_pose_at_bind = hand_l_bind_matrix,
  });
  spec.palette_role_remap[k_builder_hammer_wood_slot] =
      static_cast<std::uint8_t>(base_role_byte + 0U);
  spec.palette_role_remap[k_builder_hammer_stone_slot] =
      static_cast<std::uint8_t>(base_role_byte + 1U);
  spec.palette_role_remap[k_builder_hammer_stone_dark_slot] =
      static_cast<std::uint8_t>(base_role_byte + 2U);
  return spec;
}

} // namespace

void register_builder_style(const std::string &nation_id,
                            const BuilderStyleConfig &style) {
  style_registry()[nation_id] = style;
}

using Render::Geom::clamp01;
using Render::Geom::clamp_f;
using Render::GL::Humanoid::mix_palette_color;
using Render::GL::Humanoid::saturate_color;

class BuilderRenderer : public HumanoidRendererBase {
public:
  friend void
  register_builder_renderer(Render::GL::EntityRendererRegistry &registry);

  auto get_proportion_scaling() const -> QVector3D override {
    return {1.05F, 0.98F, 1.02F};
  }

  auto visual_spec() const
      -> const Render::Creature::Pipeline::UnitVisualSpec & override {
    using namespace Render::Creature::Pipeline;
    static const UnitVisualSpec spec = []() {
      static const auto k_head_bone =
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Head);
      static const auto k_pelvis_bone =
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Pelvis);
      static const auto k_chest_bone =
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Chest);
      static const auto k_helmet_base_role_byte =
          static_cast<std::uint8_t>(Render::Humanoid::kHumanoidRoleCount + 1U);
      static const auto k_tool_belt_base_role_byte = static_cast<std::uint8_t>(
          k_helmet_base_role_byte + Render::GL::kRomanLightHelmetRoleCount);
      static const auto k_work_apron_base_role_byte = static_cast<std::uint8_t>(
          k_tool_belt_base_role_byte + Render::GL::kToolBeltRoleCount);
      static const auto k_arm_guards_base_role_byte = static_cast<std::uint8_t>(
          k_work_apron_base_role_byte + Render::GL::kWorkApronRoleCount);
      static const auto k_work_tunic_base_role_byte = static_cast<std::uint8_t>(
          k_arm_guards_base_role_byte + Render::GL::kArmGuardsRoleCount);
      static const auto k_hammer_base_role_byte = static_cast<std::uint8_t>(
          k_work_tunic_base_role_byte + kBuilderWorkTunicRoleCount);
      static const auto k_forearm_l_bone =
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::ForearmL);
      static const auto k_forearm_r_bone =
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::ForearmR);
      static const auto k_hand_l_bone =
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::HandL);
      static const auto k_head_bind_matrix =
          Render::Humanoid::humanoid_bind_palette()[static_cast<std::size_t>(
              Render::Humanoid::HumanoidBone::Head)];
      static const Render::Creature::StaticAttachmentSpec k_helmet_spec =
          Render::GL::roman_light_helmet_make_static_attachment(
              k_head_bone, k_helmet_base_role_byte, k_head_bind_matrix);
      static const std::array<Render::Creature::StaticAttachmentSpec, 5>
          k_tool_belt_specs = Render::GL::tool_belt_make_static_attachments(
              k_pelvis_bone, k_tool_belt_base_role_byte);
      static const std::array<Render::Creature::StaticAttachmentSpec, 3>
          k_work_apron_specs = Render::GL::work_apron_make_static_attachments(
              k_pelvis_bone, k_chest_bone, k_work_apron_base_role_byte);
      static const std::array<Render::Creature::StaticAttachmentSpec, 2>
          k_arm_guards_specs = Render::GL::arm_guards_make_static_attachments(
              k_forearm_l_bone, k_forearm_r_bone, k_arm_guards_base_role_byte);
      static const Render::Creature::StaticAttachmentSpec k_work_tunic_spec =
          builder_work_tunic_make_static_attachment(
              k_chest_bone, k_work_tunic_base_role_byte);
      static const Render::Creature::StaticAttachmentSpec k_hammer_spec =
          builder_hammer_make_static_attachment(k_hand_l_bone,
                                                k_hammer_base_role_byte);
      static const std::array<Render::Creature::StaticAttachmentSpec, 13>
          k_attachments{k_helmet_spec,         k_tool_belt_specs[0],
                        k_tool_belt_specs[1],  k_tool_belt_specs[2],
                        k_tool_belt_specs[3],  k_tool_belt_specs[4],
                        k_work_apron_specs[0], k_work_apron_specs[1],
                        k_work_apron_specs[2], k_arm_guards_specs[0],
                        k_arm_guards_specs[1], k_work_tunic_spec,
                        k_hammer_spec};
      static const auto k_archetype =
          Render::Creature::ArchetypeRegistry::instance()
              .register_unit_archetype(
                  "troops/roman/builder", CreatureKind::Humanoid,
                  std::span<const Render::Creature::StaticAttachmentSpec>(
                      k_attachments.data(), k_attachments.size()),
                  +[](const void *variant_void, QVector3D *out,
                      std::uint32_t base_count,
                      std::size_t max_count) -> std::uint32_t {
                    if (variant_void == nullptr || max_count <= base_count) {
                      return base_count;
                    }
                    const auto &v =
                        *static_cast<const HumanoidVariant *>(variant_void);
                    auto count = base_count;
                    count += Render::GL::roman_light_helmet_fill_role_colors(
                        v.palette, out + count, max_count - count);
                    if (max_count <= count) {
                      return count;
                    }
                    count += Render::GL::tool_belt_fill_role_colors(
                        v.palette, out + count, max_count - count);
                    if (max_count <= count) {
                      return count;
                    }
                    count += Render::GL::work_apron_fill_role_colors(
                        v.palette, out + count, max_count - count);
                    if (max_count <= count) {
                      return count;
                    }
                    count += Render::GL::arm_guards_fill_role_colors(
                        v.palette, out + count, max_count - count);
                    if (max_count <= count) {
                      return count;
                    }
                    count += builder_work_tunic_fill_role_colors(
                        v.palette, out + count, max_count - count);
                    if (max_count <= count) {
                      return count;
                    }
                    count += builder_hammer_fill_role_colors(
                        v.palette, out + count, max_count - count);
                    return count;
                  });

      UnitVisualSpec s{};
      s.kind = CreatureKind::Humanoid;
      s.debug_name = "troops/roman/builder";
      s.scaling = ProportionScaling{1.05F, 0.98F, 1.02F};
      s.owned_legacy_slots = LegacySlotMask::Helmet | LegacySlotMask::Armor |
                             LegacySlotMask::Attachments;
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

    controller.place_hand_at(true, hammer_hand);
    controller.place_hand_at(false, rest_hand);
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

    controller.place_hand_at(true, hammer_hand);
    controller.place_hand_at(false, brace_hand);
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

    controller.place_hand_at(true, tool_hand);
    controller.place_hand_at(false, brace_hand);
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

    controller.place_hand_at(true, left_hand);
    controller.place_hand_at(false, right_hand);
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

    controller.place_hand_at(true, left_hand);
    controller.place_hand_at(false, right_hand);

    if (crouch > 0.0F) {
      controller.kneel(crouch);
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
    static const BuilderStyleConfig default_style{};
    return default_style;
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
    apply(style.leather_dark_color, variant.palette.leather_dark);
    apply(style.metal_color, variant.palette.metal);
    apply(style.wood_color, variant.palette.wood);
  }
};

void register_builder_renderer(Render::GL::EntityRendererRegistry &registry) {
  ensure_builder_styles_registered();
  static BuilderRenderer const renderer;
  registry.register_renderer("troops/roman/builder",
                             [](const DrawContext &ctx, ISubmitter &out) {
                               static BuilderRenderer const r;
                               r.render(ctx, out);
                             });
}

} // namespace Render::GL::Roman
