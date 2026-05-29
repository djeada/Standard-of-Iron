#include "healer_renderer.h"

#include <QMatrix4x4>
#include <QString>
#include <QVector3D>
#include <qmatrix4x4.h>
#include <qstringliteral.h>
#include <qvectornd.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "../../../../game/core/component.h"
#include "../../../../game/core/entity.h"
#include "../../../../game/systems/nation_id.h"
#include "../../../creature/archetype_registry.h"
#include "../../../creature/pipeline/unit_visual_spec.h"
#include "../../../equipment/armor/torso_local_archetype_utils.h"
#include "../../../equipment/attachment_builder.h"
#include "../../../equipment/generated_equipment.h"
#include "../../../equipment/humanoid_equipment_archetype.h"
#include "../../../geom/transforms.h"
#include "../../../gl/backend.h"
#include "../../../gl/primitives.h"
#include "../../../gl/render_constants.h"
#include "../../../gl/shader.h"
#include "../../../humanoid/humanoid_math.h"
#include "../../../humanoid/humanoid_proportion_profiles.h"
#include "../../../humanoid/humanoid_renderer_base.h"
#include "../../../humanoid/humanoid_spec.h"
#include "../../../humanoid/humanoid_specs.h"
#include "../../../humanoid/skeleton.h"
#include "../../../humanoid/style_palette.h"
#include "../../../palette.h"
#include "../../../scene_renderer.h"
#include "../../../submitter.h"
#include "../../healer_renderer_common.h"
#include "../../registry.h"
#include "../../renderer_constants.h"
#include "../equipment_loadout_catalog.h"
#include "healer_style.h"
#include "math/math_utils.h"

using Render::Geom::cylinder_between;
using Render::Geom::sphere_at;

namespace Render::GL::Roman {

namespace {

constexpr auto k_profile =
    Render::GL::Humanoid::k_support_proportion_profile.with_offset({.x = -0.01F});
constexpr std::uint32_t k_healer_tunic_role_count = 6;

enum HealerTunicPaletteSlot : std::uint8_t {
  k_healer_white_slot = 0U,
  k_healer_offwhite_slot = 1U,
  k_healer_cream_slot = 2U,
  k_healer_sash_slot = 3U,
  k_healer_trim_slot = 4U,
  k_healer_leather_slot = 5U,
};

auto healer_tunic_fill_role_colors(const HumanoidPalette& palette,
                                   QVector3D* out,
                                   std::size_t max) -> std::uint32_t {
  if (max < k_healer_tunic_role_count) {
    return 0U;
  }
  out[0] = QVector3D(0.96F, 0.95F, 0.92F);
  out[1] = QVector3D(0.93F, 0.91F, 0.86F);
  out[2] = QVector3D(0.89F, 0.86F, 0.80F);
  out[3] = QVector3D(0.72F, 0.18F, 0.15F);
  out[4] = Render::GL::Humanoid::saturate_color(palette.metal * 0.92F +
                                                QVector3D(0.05F, 0.04F, 0.0F));
  out[5] = palette.leather;
  return k_healer_tunic_role_count;
}

auto healer_tunic_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = [] {
    std::vector<GeneratedEquipmentPrimitive> primitives;
    const auto& bind_frames = Render::Humanoid::humanoid_bind_body_frames();
    const AttachmentFrame& torso = bind_frames.torso;
    const AttachmentFrame& waist = bind_frames.waist;
    const AttachmentFrame& back = bind_frames.back;
    const TorsoLocalFrame torso_local = make_torso_local_frame(QMatrix4x4{}, torso);

    float const torso_r = torso.radius * 1.05F;
    float const torso_depth =
        (torso.depth > 0.0F) ? torso.depth * 0.95F : torso.radius * 0.82F;
    float const y_shoulder = 0.030F;
    float const y_waist = torso_local.point(waist.origin).y();
    float const y_robe_bottom = y_waist - 0.38F;

    constexpr int segs = 14;
    constexpr float pi = std::numbers::pi_v<float>;
    auto add_ring =
        [&](float y, float width, float depth, std::uint8_t slot, float thickness) {
          for (int i = 0; i < segs; ++i) {
            float const a1 = (static_cast<float>(i) / segs) * 2.0F * pi;
            float const a2 = (static_cast<float>(i + 1) / segs) * 2.0F * pi;
            primitives.push_back(generated_cylinder(
                QVector3D(width * std::sin(a1), y, depth * std::cos(a1)),
                QVector3D(width * std::sin(a2), y, depth * std::cos(a2)),
                thickness,
                slot));
          }
        };
    auto add_section = [&](float y_top,
                           float y_bot,
                           float width_top,
                           float width_bot,
                           std::uint8_t slot) {
      float const avg_r = (width_top + width_bot) * 0.5F;
      primitives.push_back(generated_cylinder(
          QVector3D(0.0F, y_bot, 0.0F), QVector3D(0.0F, y_top, 0.0F), avg_r, slot));
    };

    add_ring(y_shoulder + 0.07F,
             torso_r * 0.72F,
             torso_depth * 0.64F,
             k_healer_cream_slot,
             0.024F);
    add_ring(y_shoulder + 0.05F,
             torso_r * 1.16F,
             torso_depth * 1.10F,
             k_healer_white_slot,
             0.036F);
    add_ring(y_shoulder + 0.010F,
             torso_r * 1.10F,
             torso_depth * 1.04F,
             k_healer_white_slot,
             0.034F);
    add_section(y_shoulder + 0.02F,
                y_shoulder - 0.10F,
                torso_r * 1.08F,
                torso_r * 1.02F,
                k_healer_white_slot);
    add_section(y_shoulder - 0.10F,
                y_shoulder - 0.20F,
                torso_r * 1.02F,
                torso_r * 0.92F,
                k_healer_offwhite_slot);
    add_ring(y_shoulder - 0.14F,
             torso_r * 0.98F,
             torso_depth * 0.92F,
             k_healer_offwhite_slot,
             0.030F);
    add_section(y_shoulder - 0.20F,
                y_waist + 0.02F,
                torso_r * 0.90F,
                torso_r * 0.82F,
                k_healer_offwhite_slot);

    float const sash_y = y_waist + 0.010F;
    primitives.push_back(generated_cylinder(QVector3D(0.0F, sash_y - 0.022F, 0.0F),
                                            QVector3D(0.0F, sash_y + 0.022F, 0.0F),
                                            torso_r * 0.86F,
                                            k_healer_sash_slot));
    primitives.push_back(generated_cylinder(QVector3D(0.0F, sash_y + 0.020F, 0.0F),
                                            QVector3D(0.0F, sash_y + 0.026F, 0.0F),
                                            torso_r * 0.88F,
                                            k_healer_trim_slot));
    primitives.push_back(generated_cylinder(QVector3D(0.0F, sash_y - 0.026F, 0.0F),
                                            QVector3D(0.0F, sash_y - 0.020F, 0.0F),
                                            torso_r * 0.88F,
                                            k_healer_trim_slot));

    QVector3D const shoulder_l = torso_local.point(bind_frames.shoulder_l.origin);
    QVector3D const shoulder_r = torso_local.point(bind_frames.shoulder_r.origin);
    QVector3D const back_forward = torso_local.direction(back.forward);
    float const cape_bottom_y = std::max(y_robe_bottom + 0.08F, y_waist - 0.20F);
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
    primitives.push_back(
        generated_cylinder(right_top, right_bottom, 0.028F, k_healer_sash_slot));
    primitives.push_back(
        generated_cylinder(left_bottom, right_bottom, 0.022F, k_healer_sash_slot));
    primitives.push_back(
        generated_sphere((left_top + right_top) * 0.5F + back_forward * 0.01F,
                         torso_r * 0.16F,
                         k_healer_trim_slot));

    float const robe_length = y_waist - y_robe_bottom;
    constexpr int skirt_layers = 10;
    for (int layer = 0; layer < skirt_layers; ++layer) {
      float const t = static_cast<float>(layer) / static_cast<float>(skirt_layers - 1);
      float const y = y_waist - 0.02F - t * robe_length;
      float const flare = 1.0F + t * 0.45F;
      add_ring(y,
               torso_r * 0.88F * flare,
               torso_depth * 0.82F * flare,
               t < 0.5F ? k_healer_white_slot : k_healer_cream_slot,
               0.018F + t * 0.014F);
    }
    add_ring(y_robe_bottom + 0.01F,
             torso_r * 1.276F,
             torso_depth * 1.189F,
             k_healer_cream_slot,
             0.035F);
    add_ring(y_robe_bottom - 0.002F,
             torso_r * 1.305F,
             torso_depth * 1.218F,
             k_healer_trim_slot,
             0.015F);

    QVector3D const emblem_center(0.0F, y_shoulder - 0.06F, torso_depth * 0.90F);
    float const cross_half = torso_r * 0.36F;
    float const cross_thickness = torso_r * 0.18F;
    primitives.push_back(
        generated_cylinder(emblem_center - QVector3D(cross_half, 0.0F, 0.0F),
                           emblem_center + QVector3D(cross_half, 0.0F, 0.0F),
                           cross_thickness,
                           k_healer_sash_slot));
    primitives.push_back(
        generated_cylinder(emblem_center - QVector3D(0.0F, cross_half * 1.1F, 0.0F),
                           emblem_center + QVector3D(0.0F, cross_half * 1.1F, 0.0F),
                           cross_thickness,
                           k_healer_sash_slot));

    QVector3D const satchel_pos(torso_r * 0.75F, y_waist - 0.08F, torso_depth * 0.15F);
    primitives.push_back(generated_box(
        satchel_pos, QVector3D(0.045F, 0.06F, 0.035F), k_healer_leather_slot));
    primitives.push_back(generated_box(satchel_pos + QVector3D(0.0F, 0.035F, 0.01F),
                                       QVector3D(0.048F, 0.015F, 0.038F),
                                       k_healer_leather_slot));
    primitives.push_back(
        generated_sphere(QVector3D(torso_r * 0.4F, y_shoulder, torso_depth * 0.3F),
                         0.022F,
                         k_healer_trim_slot));

    return build_generated_equipment_archetype(
        "roman_healer_tunic",
        std::span<const GeneratedEquipmentPrimitive>(primitives.data(),
                                                     primitives.size()));
  }();
  return archetype;
}

auto healer_tunic_make_static_attachment(std::uint16_t chest_bone_index,
                                         std::uint8_t base_role_byte)
    -> Render::Creature::StaticAttachmentSpec {
  const auto& bind_frames = Render::Humanoid::humanoid_bind_body_frames();
  const TorsoLocalFrame torso_local =
      make_torso_local_frame(QMatrix4x4{}, bind_frames.torso);
  auto spec = Render::Equipment::build_static_attachment({
      .archetype = &healer_tunic_archetype(),
      .socket_bone_index = chest_bone_index,
      .unit_local_pose_at_bind = torso_local.world,
  });
  for (std::uint8_t i = 0; i < static_cast<std::uint8_t>(k_healer_tunic_role_count);
       ++i) {
    spec.palette_role_remap[i] = static_cast<std::uint8_t>(base_role_byte + i);
  }
  return spec;
}

auto healer_tunic_extra_role_colors(const void* variant_void,
                                    QVector3D* out,
                                    std::uint32_t base_count,
                                    std::size_t max_count) -> std::uint32_t {
  if (variant_void == nullptr || max_count <= base_count) {
    return base_count;
  }
  const auto& variant = *static_cast<const HumanoidVariant*>(variant_void);
  return base_count + healer_tunic_fill_role_colors(
                          variant.palette, out + base_count, max_count - base_count);
}

const Render::Creature::Pipeline::UnitVisualSpec&
roman_healer_visual_spec(std::string_view,
                         std::string_view,
                         Render::Creature::Pipeline::CreatureAssetId,
                         const Render::GL::HealerStyleConfig& style,
                         const Render::GL::Humanoid::ProportionProfile& profile) {
  using namespace Render::Creature::Pipeline;

  static const UnitVisualSpec spec = [&]() {
    static const auto k_healer_base_archetype = []() {
      auto& registry = Render::Creature::ArchetypeRegistry::instance();
      const auto* base_desc =
          registry.get(Render::Creature::ArchetypeRegistry::k_humanoid_base);
      if (base_desc == nullptr) {
        return Render::Creature::k_invalid_archetype;
      }

      Render::Creature::ArchetypeDescriptor desc = *base_desc;
      desc.debug_name = "troops/roman/healer/base";
      desc.bake_attachments[desc.bake_attachment_count++] =
          healer_tunic_make_static_attachment(
              static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Chest),
              desc.role_count);
      desc.role_count =
          static_cast<std::uint8_t>(desc.role_count + k_healer_tunic_role_count);
      desc.append_extra_role_colors_fn(&healer_tunic_extra_role_colors);
      return registry.register_archetype(desc);
    }();
    const auto loadout =
        Render::GL::Nation::resolve_equipment_loadout("troops/roman/healer");
    const std::array<EquipmentHandle, 2> handles{
        style.show_helmet ? loadout.helmet_handle : k_invalid_equipment_handle,
        loadout.armor_handle};

    UnitVisualSpec out{};
    out.kind = CreatureKind::Humanoid;
    out.debug_name = "troops/roman/healer";
    out.scaling = profile.as_pipeline_scaling();
    out.owned_legacy_slots = LegacySlotMask::AllHumanoid;
    out.archetype_id = resolve_humanoid_equipment_archetype(
        "troops/roman/healer", k_healer_base_archetype, handles);
    return out;
  }();
  return spec;
}

const HealerRendererProfile k_healer_profile{
    .proportion_profile = k_profile,
    .visual_spec_factory = &roman_healer_visual_spec,
    .ensure_styles_registered = &register_roman_healer_style,
};

const std::array<HealerRendererRegistration, 1> k_healer_renderers{{
    {"troops/roman/healer", "roman_republic"},
}};

} // namespace

void register_healer_renderer(Render::GL::EntityRendererRegistry& registry) {
  register_healer_renderer_profile(registry, k_healer_profile, k_healer_renderers);
}

} // namespace Render::GL::Roman
