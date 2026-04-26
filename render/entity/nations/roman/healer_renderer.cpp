#include "healer_renderer.h"
#include "../../../../game/core/component.h"
#include "../../../../game/core/entity.h"
#include "../../../../game/systems/nation_id.h"
#include "../../../creature/archetype_registry.h"
#include "../../../creature/pipeline/unit_visual_spec.h"
#include "../../../equipment/armor/roman_armor.h"
#include "../../../equipment/armor/torso_local_archetype_utils.h"
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
constexpr std::uint32_t kHealerTunicRoleCount = 6;

enum HealerTunicPaletteSlot : std::uint8_t {
  k_healer_white_slot = 0U,
  k_healer_offwhite_slot = 1U,
  k_healer_cream_slot = 2U,
  k_healer_sash_slot = 3U,
  k_healer_trim_slot = 4U,
  k_healer_leather_slot = 5U,
};

auto healer_tunic_fill_role_colors(const HumanoidPalette &palette,
                                   QVector3D *out,
                                   std::size_t max) -> std::uint32_t {
  if (max < kHealerTunicRoleCount) {
    return 0U;
  }
  out[0] = QVector3D(0.96F, 0.95F, 0.92F);
  out[1] = QVector3D(0.93F, 0.91F, 0.86F);
  out[2] = QVector3D(0.89F, 0.86F, 0.80F);
  out[3] = QVector3D(0.72F, 0.18F, 0.15F);
  out[4] = Render::GL::Humanoid::saturate_color(palette.metal * 0.92F +
                                                QVector3D(0.05F, 0.04F, 0.0F));
  out[5] = palette.leather;
  return kHealerTunicRoleCount;
}

auto healer_tunic_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    std::vector<GeneratedEquipmentPrimitive> primitives;
    const auto &bind_frames = Render::Humanoid::humanoid_bind_body_frames();
    const AttachmentFrame &torso = bind_frames.torso;
    const AttachmentFrame &waist = bind_frames.waist;
    const AttachmentFrame &back = bind_frames.back;
    const TorsoLocalFrame torso_local =
        make_torso_local_frame(QMatrix4x4{}, torso);

    float const torso_r = torso.radius * 1.05F;
    float const torso_depth =
        (torso.depth > 0.0F) ? torso.depth * 0.95F : torso.radius * 0.82F;
    float const y_shoulder = 0.030F;
    float const y_waist = torso_local.point(waist.origin).y();
    float const y_robe_bottom = y_waist - 0.38F;

    constexpr int segs = 14;
    constexpr float pi = std::numbers::pi_v<float>;
    auto add_ring = [&](float y, float width, float depth, std::uint8_t slot,
                        float thickness) {
      for (int i = 0; i < segs; ++i) {
        float const a1 = (static_cast<float>(i) / segs) * 2.0F * pi;
        float const a2 = (static_cast<float>(i + 1) / segs) * 2.0F * pi;
        primitives.push_back(generated_cylinder(
            QVector3D(width * std::sin(a1), y, depth * std::cos(a1)),
            QVector3D(width * std::sin(a2), y, depth * std::cos(a2)), thickness,
            slot));
      }
    };
    auto add_section = [&](float y_top, float y_bot, float width_top,
                           float width_bot, std::uint8_t slot) {
      float const avg_r = (width_top + width_bot) * 0.5F;
      primitives.push_back(generated_cylinder(QVector3D(0.0F, y_bot, 0.0F),
                                              QVector3D(0.0F, y_top, 0.0F),
                                              avg_r, slot));
    };

    add_ring(y_shoulder + 0.07F, torso_r * 0.72F, torso_depth * 0.64F,
             k_healer_cream_slot, 0.024F);
    add_ring(y_shoulder + 0.05F, torso_r * 1.16F, torso_depth * 1.10F,
             k_healer_white_slot, 0.036F);
    add_ring(y_shoulder + 0.010F, torso_r * 1.10F, torso_depth * 1.04F,
             k_healer_white_slot, 0.034F);
    add_section(y_shoulder + 0.02F, y_shoulder - 0.10F, torso_r * 1.08F,
                torso_r * 1.02F, k_healer_white_slot);
    add_section(y_shoulder - 0.10F, y_shoulder - 0.20F, torso_r * 1.02F,
                torso_r * 0.92F, k_healer_offwhite_slot);
    add_ring(y_shoulder - 0.14F, torso_r * 0.98F, torso_depth * 0.92F,
             k_healer_offwhite_slot, 0.030F);
    add_section(y_shoulder - 0.20F, y_waist + 0.02F, torso_r * 0.90F,
                torso_r * 0.82F, k_healer_offwhite_slot);

    float const sash_y = y_waist + 0.010F;
    primitives.push_back(
        generated_cylinder(QVector3D(0.0F, sash_y - 0.022F, 0.0F),
                           QVector3D(0.0F, sash_y + 0.022F, 0.0F),
                           torso_r * 0.86F, k_healer_sash_slot));
    primitives.push_back(
        generated_cylinder(QVector3D(0.0F, sash_y + 0.020F, 0.0F),
                           QVector3D(0.0F, sash_y + 0.026F, 0.0F),
                           torso_r * 0.88F, k_healer_trim_slot));
    primitives.push_back(
        generated_cylinder(QVector3D(0.0F, sash_y - 0.026F, 0.0F),
                           QVector3D(0.0F, sash_y - 0.020F, 0.0F),
                           torso_r * 0.88F, k_healer_trim_slot));

    QVector3D const shoulder_l =
        torso_local.point(bind_frames.shoulder_l.origin);
    QVector3D const shoulder_r =
        torso_local.point(bind_frames.shoulder_r.origin);
    QVector3D const back_forward = torso_local.direction(back.forward);
    float const cape_bottom_y =
        std::max(y_robe_bottom + 0.08F, y_waist - 0.20F);
    QVector3D const left_top =
        shoulder_l + back_forward * 0.03F + QVector3D(0.0F, 0.015F, 0.0F);
    QVector3D const right_top =
        shoulder_r + back_forward * 0.03F + QVector3D(0.0F, 0.015F, 0.0F);
    QVector3D const left_bottom =
        left_top + QVector3D(0.0F, cape_bottom_y - left_top.y(), 0.05F);
    QVector3D const right_bottom =
        right_top + QVector3D(0.0F, cape_bottom_y - right_top.y(), 0.05F);
    primitives.push_back(
        generated_cylinder(left_top, right_top, 0.020F, k_healer_sash_slot));
    primitives.push_back(
        generated_cylinder(left_top, left_bottom, 0.028F, k_healer_sash_slot));
    primitives.push_back(generated_cylinder(right_top, right_bottom, 0.028F,
                                            k_healer_sash_slot));
    primitives.push_back(generated_cylinder(left_bottom, right_bottom, 0.022F,
                                            k_healer_sash_slot));
    primitives.push_back(
        generated_sphere((left_top + right_top) * 0.5F + back_forward * 0.01F,
                         torso_r * 0.16F, k_healer_trim_slot));

    float const robe_length = y_waist - y_robe_bottom;
    constexpr int skirt_layers = 10;
    for (int layer = 0; layer < skirt_layers; ++layer) {
      float const t =
          static_cast<float>(layer) / static_cast<float>(skirt_layers - 1);
      float const y = y_waist - 0.02F - t * robe_length;
      float const flare = 1.0F + t * 0.45F;
      add_ring(y, torso_r * 0.88F * flare, torso_depth * 0.82F * flare,
               t < 0.5F ? k_healer_white_slot : k_healer_cream_slot,
               0.018F + t * 0.014F);
    }
    add_ring(y_robe_bottom + 0.01F, torso_r * 1.276F, torso_depth * 1.189F,
             k_healer_cream_slot, 0.035F);
    add_ring(y_robe_bottom - 0.002F, torso_r * 1.305F, torso_depth * 1.218F,
             k_healer_trim_slot, 0.015F);

    QVector3D const emblem_center(0.0F, y_shoulder - 0.06F,
                                  torso_depth * 0.90F);
    float const cross_half = torso_r * 0.36F;
    float const cross_thickness = torso_r * 0.18F;
    primitives.push_back(
        generated_cylinder(emblem_center - QVector3D(cross_half, 0.0F, 0.0F),
                           emblem_center + QVector3D(cross_half, 0.0F, 0.0F),
                           cross_thickness, k_healer_sash_slot));
    primitives.push_back(generated_cylinder(
        emblem_center - QVector3D(0.0F, cross_half * 1.1F, 0.0F),
        emblem_center + QVector3D(0.0F, cross_half * 1.1F, 0.0F),
        cross_thickness, k_healer_sash_slot));

    QVector3D const satchel_pos(torso_r * 0.75F, y_waist - 0.08F,
                                torso_depth * 0.15F);
    primitives.push_back(generated_box(
        satchel_pos, QVector3D(0.045F, 0.06F, 0.035F), k_healer_leather_slot));
    primitives.push_back(generated_box(
        satchel_pos + QVector3D(0.0F, 0.035F, 0.01F),
        QVector3D(0.048F, 0.015F, 0.038F), k_healer_leather_slot));
    primitives.push_back(generated_sphere(
        QVector3D(torso_r * 0.4F, y_shoulder, torso_depth * 0.3F), 0.022F,
        k_healer_trim_slot));

    return build_generated_equipment_archetype(
        "roman_healer_tunic", std::span<const GeneratedEquipmentPrimitive>(
                                  primitives.data(), primitives.size()));
  }();
  return archetype;
}

auto healer_tunic_make_static_attachment(std::uint16_t chest_bone_index,
                                         std::uint8_t base_role_byte)
    -> Render::Creature::StaticAttachmentSpec {
  const auto &bind_frames = Render::Humanoid::humanoid_bind_body_frames();
  const TorsoLocalFrame torso_local =
      make_torso_local_frame(QMatrix4x4{}, bind_frames.torso);
  auto spec = Render::Equipment::build_static_attachment({
      .archetype = &healer_tunic_archetype(),
      .socket_bone_index = chest_bone_index,
      .unit_local_pose_at_bind = torso_local.world,
  });
  for (std::uint8_t i = 0; i < static_cast<std::uint8_t>(kHealerTunicRoleCount);
       ++i) {
    spec.palette_role_remap[i] = static_cast<std::uint8_t>(base_role_byte + i);
  }
  return spec;
}

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

    return {0.86F, 0.99F, 0.90F};
  }

  auto visual_spec() const
      -> const Render::Creature::Pipeline::UnitVisualSpec & override {
    using namespace Render::Creature::Pipeline;

    ensure_healer_styles_registered();
    static const HealerStyleConfig captured_style = []() {
      auto &styles = style_registry();
      auto it = styles.find("roman_republic");
      if (it != styles.end()) {
        return it->second;
      }
      auto it_default = styles.find(std::string(k_default_style_key));
      if (it_default != styles.end()) {
        return it_default->second;
      }
      return HealerStyleConfig{};
    }();

    static const UnitVisualSpec spec = []() {
      static const auto k_head_bone =
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Head);
      static const auto k_chest_bone =
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Chest);
      static const auto k_helmet_base_role_byte =
          static_cast<std::uint8_t>(Render::Humanoid::kHumanoidRoleCount + 1U);
      static const auto k_armor_base_role_byte = static_cast<std::uint8_t>(
          k_helmet_base_role_byte + Render::GL::kRomanLightHelmetRoleCount);
      static const auto k_tunic_base_role_byte = static_cast<std::uint8_t>(
          k_armor_base_role_byte + Render::GL::kRomanLightArmorRoleCount);
      static const auto k_head_bind_matrix =
          Render::Humanoid::humanoid_bind_palette()[static_cast<std::size_t>(
              Render::Humanoid::HumanoidBone::Head)];
      static const Render::Creature::StaticAttachmentSpec k_helmet_spec =
          Render::GL::roman_light_helmet_make_static_attachment(
              k_head_bone, k_helmet_base_role_byte, k_head_bind_matrix);
      static const Render::Creature::StaticAttachmentSpec k_armor_spec =
          Render::GL::roman_light_armor_make_static_attachment(
              k_chest_bone, k_armor_base_role_byte);
      static const Render::Creature::StaticAttachmentSpec k_tunic_spec =
          healer_tunic_make_static_attachment(k_chest_bone,
                                              k_tunic_base_role_byte);
      static const std::array<Render::Creature::StaticAttachmentSpec, 3>
          k_attachments_full{k_helmet_spec, k_armor_spec, k_tunic_spec};
      const std::span<const Render::Creature::StaticAttachmentSpec> bake_span =
          captured_style.show_helmet
              ? std::span<const Render::Creature::StaticAttachmentSpec>(
                    k_attachments_full.data(), 3)
              : std::span<const Render::Creature::StaticAttachmentSpec>(
                    k_attachments_full.data() + 1, 2);
      static const auto extra_roles_fn =
          +[](const void *variant_void, QVector3D *out,
              std::uint32_t base_count,
              std::size_t max_count) -> std::uint32_t {
        if (variant_void == nullptr || max_count <= base_count) {
          return base_count;
        }
        const auto &v = *static_cast<const HumanoidVariant *>(variant_void);
        auto count = base_count;
        count += Render::GL::roman_light_helmet_fill_role_colors(
            v.palette, out + count, max_count - count);
        if (max_count <= count) {
          return count;
        }
        count += Render::GL::roman_light_armor_fill_role_colors(
            v.palette, out + count, max_count - count);
        if (max_count <= count) {
          return count;
        }
        count += healer_tunic_fill_role_colors(v.palette, out + count,
                                               max_count - count);
        return count;
      };
      static const auto k_archetype =
          Render::Creature::ArchetypeRegistry::instance()
              .register_unit_archetype("troops/roman/healer",
                                       CreatureKind::Humanoid, bake_span,
                                       extra_roles_fn);

      UnitVisualSpec s{};
      s.kind = CreatureKind::Humanoid;
      s.debug_name = "troops/roman/healer";
      s.scaling = ProportionScaling{0.86F, 0.99F, 0.90F};
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

      controller.place_hand_at(true, heal_hand_l);
      controller.place_hand_at(false, heal_hand_r);

      QVector3D const look_dir(target_dir_x, 0.0F, target_dir_z);
      QVector3D const head_focus =
          pose.head_pos +
          QVector3D(look_dir.x() * 0.18F, 0.0F, look_dir.z() * 0.45F);
      controller.look_at(head_focus);

    } else {

      float const forward_offset = 0.16F + (anim.is_moving ? 0.05F : 0.0F);
      float const hand_height = HP::WAIST_Y + 0.04F + arm_height_jitter;
      QVector3D const idle_hand_l(-0.16F + arm_asymmetry, hand_height,
                                  forward_offset);
      QVector3D const idle_hand_r(0.12F - arm_asymmetry * 0.6F,
                                  hand_height + 0.01F, forward_offset * 0.9F);

      controller.place_hand_at(true, idle_hand_l);
      controller.place_hand_at(false, idle_hand_r);
    }
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
                           QVector3D &target) {
      target = mix_palette_color(target, override_color, team_tint,
                                 k_team_mix_weight, k_style_mix_weight);
    };

    apply_color(style.cloth_color, variant.palette.cloth);
    apply_color(style.leather_color, variant.palette.leather);
    apply_color(style.leather_dark_color, variant.palette.leather_dark);
    apply_color(style.metal_color, variant.palette.metal);
    apply_color(style.wood_color, variant.palette.wood);
  }
};

void register_healer_renderer(Render::GL::EntityRendererRegistry &registry) {
  ensure_healer_styles_registered();
  static HealerRenderer const renderer;
  registry.register_renderer("troops/roman/healer",
                             [](const DrawContext &ctx, ISubmitter &out) {
                               static HealerRenderer const static_renderer;
                               static_renderer.render(ctx, out);
                             });
}

} // namespace Render::GL::Roman
