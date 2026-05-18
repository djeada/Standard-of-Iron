#include "builder_renderer.h"
#include "../builder_tool_palette.h"

#include <QMatrix4x4>
#include <QString>
#include <QVector3D>
#include <qmatrix4x4.h>
#include <qstringliteral.h>
#include <qvectornd.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>

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
#include "../../../equipment/humanoid_equipment_archetype.h"
#include "../../../equipment/render_archetype_registry.h"
#include "../../../geom/math_utils.h"
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
#include "../../registry.h"
#include "../../renderer_constants.h"
#include "../equipment_loadout_catalog.h"
#include "builder_style.h"

using Render::Geom::cylinder_between;
using Render::Geom::sphere_at;

namespace Render::GL::Roman {

namespace {

constexpr std::string_view k_default_style_key = "default";

auto style_registry() -> std::unordered_map<std::string, BuilderStyleConfig>& {
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
constexpr std::uint32_t k_builder_work_tunic_role_count = 2;
constexpr std::uint32_t k_builder_hammer_role_count = 3;
constexpr std::uint32_t k_builder_saw_role_count = 4;
constexpr std::uint32_t k_builder_chisel_role_count = 2;
constexpr std::uint32_t k_roman_civilian_mantle_role_count = 2;
constexpr auto k_builder_profile =
    Render::GL::Humanoid::k_laborer_proportion_profile.with_offset(
        {.x = 0.02F, .y = -0.01F, .z = 0.02F});
constexpr auto k_civilian_profile =
    Render::GL::Humanoid::k_civilian_proportion_profile.with_offset(
        {.x = 0.02F, .y = -0.01F, .z = 0.02F});

enum BuilderWorkTunicPaletteSlot : std::uint8_t {
  k_builder_tunic_base_slot = 0U,
  k_builder_tunic_dark_slot = 1U,
};

enum BuilderHammerPaletteSlot : std::uint8_t {
  k_builder_hammer_wood_slot = 0U,
  k_builder_hammer_metal_slot = 1U,
  k_builder_hammer_metal_dark_slot = 2U,
};

auto builder_work_tunic_fill_role_colors(const HumanoidPalette& palette,
                                         QVector3D* out,
                                         std::size_t max) -> std::uint32_t {
  if (max < k_builder_work_tunic_role_count) {
    return 0U;
  }
  out[0] = palette.cloth;
  out[1] = palette.cloth * 0.85F;
  return k_builder_work_tunic_role_count;
}

auto builder_tool_belt_fill_role_colors(const HumanoidPalette& palette,
                                        QVector3D* out,
                                        std::size_t max) -> std::uint32_t {
  if (max < Render::GL::k_tool_belt_role_count) {
    return 0U;
  }
  out[0] = palette.leather;
  out[1] = palette.leather_dark;
  out[2] = palette.metal;
  out[3] = palette.metal * 0.90F;
  out[4] = palette.wood;
  return Render::GL::k_tool_belt_role_count;
}

auto builder_work_apron_fill_role_colors(const HumanoidPalette& palette,
                                         QVector3D* out,
                                         std::size_t max) -> std::uint32_t {
  if (max < Render::GL::k_work_apron_role_count) {
    return 0U;
  }
  QVector3D const apron = palette.leather * 0.78F + palette.cloth * 0.22F;
  for (std::uint32_t i = 0; i < 7U; ++i) {
    float const t = static_cast<float>(i) / 6.0F * 0.16F;
    out[i] = apron * (1.0F - t);
  }
  out[7] = apron * 0.84F;
  out[8] = palette.leather_dark;
  return Render::GL::k_work_apron_role_count;
}

auto builder_arm_guards_fill_role_colors(const HumanoidPalette& palette,
                                         QVector3D* out,
                                         std::size_t max) -> std::uint32_t {
  if (max < Render::GL::k_arm_guards_role_count) {
    return 0U;
  }
  out[0] = palette.leather_dark;
  out[1] = palette.leather_dark * 0.78F + palette.metal * 0.22F;
  return Render::GL::k_arm_guards_role_count;
}

auto roman_civilian_mantle_fill_role_colors(const HumanoidPalette& palette,
                                            QVector3D* out,
                                            std::size_t max) -> std::uint32_t {
  if (max < k_roman_civilian_mantle_role_count) {
    return 0U;
  }
  out[0] = palette.leather;
  out[1] = palette.leather_dark;
  return k_roman_civilian_mantle_role_count;
}

auto builder_hammer_fill_role_colors(const HumanoidPalette& palette,
                                     QVector3D* out,
                                     std::size_t max) -> std::uint32_t {
  if (max < k_builder_hammer_role_count) {
    return 0U;
  }
  out[0] = palette.wood;
  out[1] = palette.metal;
  out[2] = palette.metal * 0.72F;
  return k_builder_hammer_role_count;
}

auto builder_saw_fill_role_colors(const HumanoidPalette& palette,
                                  QVector3D* out,
                                  std::size_t max) -> std::uint32_t {
  if (max < k_builder_saw_role_count) {
    return 0U;
  }
  out[0] = palette.wood;
  out[1] = palette.metal;
  out[2] = palette.metal * 0.72F;
  out[3] = palette.leather_dark;
  return k_builder_saw_role_count;
}

auto builder_chisel_fill_role_colors(const HumanoidPalette& palette,
                                     QVector3D* out,
                                     std::size_t max) -> std::uint32_t {
  if (max < k_builder_chisel_role_count) {
    return 0U;
  }
  out[0] = palette.wood;
  out[1] = palette.metal * 0.88F;
  return k_builder_chisel_role_count;
}

auto builder_work_tunic_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = [] {
    const auto& bind = Render::Humanoid::humanoid_bind_body_frames();
    const AttachmentFrame& torso = bind.torso;
    const AttachmentFrame& waist = bind.waist;
    const TorsoLocalFrame local = make_torso_local_frame(QMatrix4x4{}, torso);

    float const tr = torso.radius * 1.05F;
    float const td = (torso.depth > 0.0F) ? torso.depth * 0.95F : torso.radius * 0.84F;
    float const y_sh = 0.032F;
    float const y_w = local.point(waist.origin).y();
    float const y_hem = y_w - 0.18F;

    RenderArchetypeBuilder builder{"roman_builder_work_tunic"};

    {
      float const h = y_sh - y_w;
      float const cy = (y_sh + y_w) * 0.5F;
      QMatrix4x4 m;
      m.translate(0.0F, cy, 0.0F);
      m.scale(tr, h, td);
      builder.add_palette_mesh(
          get_unit_tapered_cylinder(0.86F, 1.02F, 8), m, k_builder_tunic_base_slot);
    }

    {
      float const h = y_w - y_hem;
      float const cy = (y_w + y_hem) * 0.5F;
      QMatrix4x4 m;
      m.translate(0.0F, cy, 0.0F);
      m.scale(tr, h, td);
      builder.add_palette_mesh(
          get_unit_tapered_cylinder(1.44F, 0.86F, 8), m, k_builder_tunic_base_slot);
    }

    {
      float const h = 0.026F;
      float const cy = y_w + 0.004F;
      QMatrix4x4 m;
      m.translate(0.0F, cy, 0.0F);
      m.scale(tr * 0.92F, h, td * 0.90F);
      builder.add_palette_mesh(
          get_unit_tapered_cylinder(1.0F, 1.0F, 8), m, k_builder_tunic_dark_slot);
    }

    {
      float const h = 0.022F;
      float const cy = y_hem + h * 0.5F;
      QMatrix4x4 m;
      m.translate(0.0F, cy, 0.0F);
      m.scale(tr * 1.50F, h, td * 1.50F);
      builder.add_palette_mesh(
          get_unit_tapered_cylinder(1.0F, 1.0F, 8), m, k_builder_tunic_dark_slot);
    }

    {
      const QVector3D sh_r = local.point(bind.shoulder_r.origin);
      QVector3D const pin_top(sh_r.x() * 0.50F, y_sh + 0.012F, td * 0.12F);
      QVector3D const pin_bot(sh_r.x() * 0.38F, y_sh - 0.032F, td * 0.22F);
      builder.add_palette_mesh(get_unit_cylinder(8),
                               cylinder_between(pin_top, pin_bot, tr * 0.11F),
                               k_builder_tunic_dark_slot);
    }

    {
      const QVector3D sh_l = local.point(bind.shoulder_l.origin);
      builder.add_palette_mesh(
          get_unit_sphere(),
          sphere_at(QVector3D(sh_l.x(), y_sh - 0.010F, td * 0.06F), tr * 0.21F),
          k_builder_tunic_base_slot);
    }

    return std::move(builder).build();
  }();
  return archetype;
}

auto builder_work_tunic_make_static_attachment(std::uint16_t chest_bone_index,
                                               std::uint8_t base_role_byte)
    -> Render::Creature::StaticAttachmentSpec {
  const auto& bind_frames = Render::Humanoid::humanoid_bind_body_frames();
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

auto builder_work_tunic_extra_role_colors(const void* variant_void,
                                          QVector3D* out,
                                          std::uint32_t base_count,
                                          std::size_t max_count) -> std::uint32_t {
  if (variant_void == nullptr || max_count <= base_count) {
    return base_count;
  }
  const auto& variant = *static_cast<const HumanoidVariant*>(variant_void);
  return base_count + builder_work_tunic_fill_role_colors(
                          variant.palette, out + base_count, max_count - base_count);
}

auto builder_work_tunic_contribution_attachments(std::uint8_t base_role_byte)
    -> std::vector<Render::Creature::StaticAttachmentSpec> {
  return {builder_work_tunic_make_static_attachment(
      static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Chest),
      base_role_byte)};
}

auto roman_civilian_mantle_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = [] {
    const auto& bind = Render::Humanoid::humanoid_bind_body_frames();
    const AttachmentFrame& torso = bind.torso;
    const AttachmentFrame& waist = bind.waist;
    const TorsoLocalFrame local = make_torso_local_frame(QMatrix4x4{}, torso);

    float const tr = torso.radius * 1.06F;
    float const td = (torso.depth > 0.0F) ? torso.depth * 0.94F : torso.radius * 0.82F;
    float const y_sh = 0.038F;
    float const y_w = local.point(waist.origin).y();
    float const y_hem = y_w - 0.20F;

    RenderArchetypeBuilder builder{"roman_civilian_mantle"};

    {
      float const h = y_sh - y_w;
      float const cy = (y_sh + y_w) * 0.5F;
      QMatrix4x4 m;
      m.translate(0.0F, cy, 0.0F);
      m.scale(tr, h, td);
      builder.add_palette_mesh(get_unit_tapered_cylinder(0.88F, 1.04F, 8), m, 0U);
    }

    {
      float const h = y_w - y_hem;
      float const cy = (y_w + y_hem) * 0.5F;
      QMatrix4x4 m;
      m.translate(0.0F, cy, 0.0F);
      m.scale(tr, h, td);
      builder.add_palette_mesh(get_unit_tapered_cylinder(1.38F, 0.88F, 8), m, 0U);
    }

    {
      const QVector3D sh_l = local.point(bind.shoulder_l.origin);
      QVector3D const drape_top(sh_l.x() * 0.80F, y_sh + 0.006F, td * 0.40F);
      QVector3D const drape_bot(tr * 0.30F, y_w + 0.020F, td * 0.68F);
      builder.add_palette_mesh(
          get_unit_cylinder(8), cylinder_between(drape_top, drape_bot, tr * 0.32F), 1U);
    }

    {
      QVector3D const roll_l(-tr * 0.72F, y_sh + 0.014F, td * 0.22F);
      QVector3D const roll_r(tr * 0.52F, y_sh + 0.014F, td * 0.22F);
      builder.add_palette_mesh(
          get_unit_cylinder(8), cylinder_between(roll_l, roll_r, tr * 0.14F), 1U);
    }

    {
      float const h = 0.024F;
      float const cy = y_w + 0.004F;
      QMatrix4x4 m;
      m.translate(0.0F, cy, 0.0F);
      m.scale(tr * 0.92F, h, td * 0.90F);
      builder.add_palette_mesh(get_unit_tapered_cylinder(1.0F, 1.0F, 8), m, 0U);
    }

    {
      QVector3D const umbo_top(tr * 0.22F, y_w - 0.006F, td * 0.82F);
      QVector3D const umbo_bot(tr * 0.26F, y_w - 0.110F, td * 0.88F);
      builder.add_palette_mesh(
          get_unit_cylinder(8), cylinder_between(umbo_top, umbo_bot, tr * 0.20F), 1U);
    }

    return std::move(builder).build();
  }();
  return archetype;
}

auto roman_civilian_mantle_make_static_attachment(std::uint16_t chest_bone_index,
                                                  std::uint8_t base_role_byte)
    -> Render::Creature::StaticAttachmentSpec {
  const auto& bind_frames = Render::Humanoid::humanoid_bind_body_frames();
  const TorsoLocalFrame torso_local =
      make_torso_local_frame(QMatrix4x4{}, bind_frames.torso);
  auto spec = Render::Equipment::build_static_attachment({
      .archetype = &roman_civilian_mantle_archetype(),
      .socket_bone_index = chest_bone_index,
      .unit_local_pose_at_bind = torso_local.world,
  });
  spec.palette_role_remap[0] = base_role_byte;
  spec.palette_role_remap[1] = static_cast<std::uint8_t>(base_role_byte + 1U);
  return spec;
}

auto roman_civilian_mantle_extra_role_colors(const void* variant_void,
                                             QVector3D* out,
                                             std::uint32_t base_count,
                                             std::size_t max_count) -> std::uint32_t {
  if (variant_void == nullptr || max_count <= base_count) {
    return base_count;
  }
  const auto& variant = *static_cast<const HumanoidVariant*>(variant_void);
  return base_count + roman_civilian_mantle_fill_role_colors(
                          variant.palette, out + base_count, max_count - base_count);
}

auto roman_civilian_mantle_contribution_attachments(std::uint8_t base_role_byte)
    -> std::vector<Render::Creature::StaticAttachmentSpec> {
  return {roman_civilian_mantle_make_static_attachment(
      static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Chest),
      base_role_byte)};
}

auto builder_hammer_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = [] {
    std::array<GeneratedEquipmentPrimitive, 4> const primitives{{
        generated_cylinder(QVector3D(0.0F, -0.20F, 0.02F),
                           QVector3D(0.0F, 0.12F, 0.02F),
                           0.016F,
                           k_builder_hammer_wood_slot),
        generated_cylinder(QVector3D(-0.05F, 0.155F, 0.02F),
                           QVector3D(0.05F, 0.155F, 0.02F),
                           0.030F,
                           k_builder_hammer_metal_slot),
        generated_sphere(
            QVector3D(0.05F, 0.155F, 0.02F), 0.0345F, k_builder_hammer_metal_dark_slot),
        generated_sphere(
            QVector3D(-0.05F, 0.155F, 0.02F), 0.027F, k_builder_hammer_metal_slot),
    }};
    return build_generated_equipment_archetype("roman_builder_hammer", primitives);
  }();
  return archetype;
}

auto builder_saw_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = [] {
    std::array<GeneratedEquipmentPrimitive, 8> const primitives{{
        generated_cylinder(QVector3D(-0.02F, -0.18F, 0.03F),
                           QVector3D(0.06F, -0.08F, 0.03F),
                           0.018F,
                           k_saw_wood_slot),
        generated_sphere(
            QVector3D(0.065F, -0.07F, 0.03F), 0.024F, k_saw_leather_slot),
        generated_cylinder(QVector3D(0.02F, -0.06F, 0.03F),
                           QVector3D(0.03F, 0.20F, 0.03F),
                           0.013F,
                           k_saw_metal_slot),
        generated_cylinder(QVector3D(0.03F, 0.20F, 0.03F),
                           QVector3D(-0.09F, 0.12F, 0.03F),
                           0.018F,
                           k_saw_metal_dark_slot),
        generated_sphere(
            QVector3D(-0.02F, 0.16F, 0.03F), 0.016F, k_saw_metal_slot),
        generated_sphere(
            QVector3D(-0.045F, 0.145F, 0.03F), 0.014F, k_saw_metal_slot),
        generated_sphere(
            QVector3D(-0.07F, 0.13F, 0.03F), 0.012F, k_saw_metal_slot),
        generated_sphere(
            QVector3D(-0.09F, 0.115F, 0.03F), 0.010F, k_saw_metal_slot),
    }};
    return build_generated_equipment_archetype("roman_builder_saw", primitives);
  }();
  return archetype;
}

auto builder_chisel_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = [] {
    std::array<GeneratedEquipmentPrimitive, 4> const primitives{{
        generated_cylinder(QVector3D(0.0F, -0.16F, 0.01F),
                           QVector3D(0.0F, 0.06F, 0.01F),
                           0.017F,
                           k_chisel_wood_slot),
        generated_sphere(
            QVector3D(0.0F, -0.18F, 0.01F), 0.022F, k_chisel_wood_slot),
        generated_cylinder(QVector3D(0.0F, 0.06F, 0.01F),
                           QVector3D(0.0F, 0.20F, 0.01F),
                           0.011F,
                           k_chisel_metal_slot),
        generated_sphere(
            QVector3D(0.0F, 0.215F, 0.01F), 0.012F, k_chisel_metal_slot),
    }};
    return build_generated_equipment_archetype("roman_builder_chisel", primitives);
  }();
  return archetype;
}

auto builder_tool_make_static_attachment(const RenderArchetype& archetype,
                                         std::uint8_t base_role_byte,
                                         std::span<const std::uint8_t> slot_indices)
    -> Render::Creature::StaticAttachmentSpec {
  constexpr auto k_socket = Render::Humanoid::HumanoidSocket::GripR;
  constexpr auto k_bone = Render::Humanoid::HumanoidBone::HandR;
  QMatrix4x4 const bind_bone =
      Render::Humanoid::humanoid_bind_palette()[static_cast<std::size_t>(k_bone)];
  QMatrix4x4 const bind_socket = Render::Humanoid::bind_socket_transform(k_socket);
  auto spec = Render::Equipment::build_socket_static_attachment({
      .archetype = &archetype,
      .socket_bone_index = static_cast<std::uint16_t>(k_bone),
      .bind_bone_transform = bind_bone,
      .bind_socket_transform = bind_socket,
      .mesh_from_socket = QMatrix4x4{},
  });
  for (std::size_t i = 0; i < slot_indices.size(); ++i) {
    spec.palette_role_remap[slot_indices[i]] =
        static_cast<std::uint8_t>(base_role_byte + i);
  }
  return spec;
}

auto register_builder_tool_variant_archetype(
    std::string_view debug_name,
    Render::Creature::ArchetypeId base_archetype_id,
    const Render::Creature::StaticAttachmentSpec& tool_spec,
    Render::Creature::ArchetypeDescriptor::ExtraRoleColorsFn tool_role_colors,
    std::uint8_t tool_role_count) -> Render::Creature::ArchetypeId {
  auto& registry = Render::Creature::ArchetypeRegistry::instance();
  const auto* base_desc = registry.get(base_archetype_id);
  if (base_desc == nullptr) {
    return Render::Creature::k_invalid_archetype;
  }

  auto desc = *base_desc;
  desc.debug_name = debug_name;
  desc.bake_attachments[desc.bake_attachment_count++] = tool_spec;
  desc.role_count = static_cast<std::uint8_t>(desc.role_count + tool_role_count);
  desc.append_extra_role_colors_fn(tool_role_colors);
  return registry.register_archetype(desc);
}

auto roman_builder_base_archetype() -> Render::Creature::ArchetypeId {
  static const auto archetype = []() {
    auto& registry = Render::Creature::ArchetypeRegistry::instance();
    const auto* base_desc =
        registry.get(Render::Creature::ArchetypeRegistry::k_humanoid_base);
    if (base_desc == nullptr) {
      return Render::Creature::k_invalid_archetype;
    }

    auto desc = *base_desc;
    desc.debug_name = "troops/roman/builder/base";
    desc.bake_attachments[desc.bake_attachment_count++] =
        builder_work_tunic_make_static_attachment(
            static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Chest),
            desc.role_count);
    desc.role_count =
        static_cast<std::uint8_t>(desc.role_count + k_builder_work_tunic_role_count);
    desc.append_extra_role_colors_fn(&builder_work_tunic_extra_role_colors);
    return registry.register_archetype(desc);
  }();
  return archetype;
}

auto roman_builder_idle_archetype() -> Render::Creature::ArchetypeId {
  static const auto archetype = []() {
    const auto loadout =
        Render::GL::Nation::resolve_equipment_loadout("troops/roman/builder");
    const std::array<EquipmentHandle, 4> handles{loadout.helmet_handle,
                                                 loadout.tool_belt_handle,
                                                 loadout.work_apron_handle,
                                                 loadout.arm_guards_handle};
    return resolve_humanoid_equipment_archetype(
        "troops/roman/builder", roman_builder_base_archetype(), handles);
  }();
  return archetype;
}

void ensure_roman_civilian_equipment_contributions_registered() {
  static const bool registered = []() {
    const auto loadout =
        Render::GL::Nation::resolve_equipment_loadout("troops/roman/civilian");
    if (loadout.armor_handle != k_invalid_equipment_handle) {
      register_humanoid_equipment_contribution(
          loadout.armor_handle,
          {.build_attachments = &builder_work_tunic_contribution_attachments,
           .append_role_colors = &builder_work_tunic_extra_role_colors,
           .role_count = static_cast<std::uint8_t>(k_builder_work_tunic_role_count)});
    }
    if (loadout.cloak_handle != k_invalid_equipment_handle) {
      register_humanoid_equipment_contribution(
          loadout.cloak_handle,
          {.build_attachments = &roman_civilian_mantle_contribution_attachments,
           .append_role_colors = &roman_civilian_mantle_extra_role_colors,
           .role_count =
               static_cast<std::uint8_t>(k_roman_civilian_mantle_role_count)});
    }
    return true;
  }();
  (void)registered;
}

auto roman_builder_tool_base_role_byte() -> std::uint8_t {
  static const auto base_role_byte = static_cast<std::uint8_t>(
      Render::Humanoid::k_humanoid_role_count + 1U +
      Render::GL::k_roman_light_helmet_role_count + Render::GL::k_tool_belt_role_count +
      Render::GL::k_work_apron_role_count + Render::GL::k_arm_guards_role_count +
      k_builder_work_tunic_role_count);
  return base_role_byte;
}

auto roman_builder_common_attachments()
    -> const std::array<Render::Creature::StaticAttachmentSpec, 13>& {
  static const auto k_head_bone =
      static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Head);
  static const auto k_pelvis_bone =
      static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Pelvis);
  static const auto k_chest_bone =
      static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Chest);
  static const auto k_helmet_base_role_byte =
      static_cast<std::uint8_t>(Render::Humanoid::k_humanoid_role_count + 1U);
  static const auto k_tool_belt_base_role_byte = static_cast<std::uint8_t>(
      k_helmet_base_role_byte + Render::GL::k_roman_light_helmet_role_count);
  static const auto k_work_apron_base_role_byte = static_cast<std::uint8_t>(
      k_tool_belt_base_role_byte + Render::GL::k_tool_belt_role_count);
  static const auto k_arm_guards_base_role_byte = static_cast<std::uint8_t>(
      k_work_apron_base_role_byte + Render::GL::k_work_apron_role_count);
  static const auto k_work_tunic_base_role_byte = static_cast<std::uint8_t>(
      k_arm_guards_base_role_byte + Render::GL::k_arm_guards_role_count);
  static const auto k_forearm_l_bone =
      static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::ForearmL);
  static const auto k_forearm_r_bone =
      static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::ForearmR);
  static const auto k_head_bind_matrix =
      Render::Humanoid::humanoid_bind_palette()[static_cast<std::size_t>(
          Render::Humanoid::HumanoidBone::Head)];
  static const Render::Creature::StaticAttachmentSpec k_helmet_spec =
      Render::GL::roman_light_helmet_make_static_attachment(
          k_head_bone, k_helmet_base_role_byte, k_head_bind_matrix);
  static const std::array<Render::Creature::StaticAttachmentSpec, 6> k_tool_belt_specs =
      Render::GL::tool_belt_make_static_attachments(k_pelvis_bone,
                                                    k_tool_belt_base_role_byte);
  static const std::array<Render::Creature::StaticAttachmentSpec, 3>
      k_work_apron_specs = Render::GL::work_apron_make_static_attachments(
          k_pelvis_bone, k_chest_bone, k_work_apron_base_role_byte);
  static const std::array<Render::Creature::StaticAttachmentSpec, 2>
      k_arm_guards_specs = Render::GL::arm_guards_make_static_attachments(
          k_forearm_l_bone, k_forearm_r_bone, k_arm_guards_base_role_byte);
  static const Render::Creature::StaticAttachmentSpec k_work_tunic_spec =
      builder_work_tunic_make_static_attachment(k_chest_bone,
                                                k_work_tunic_base_role_byte);
  static const std::array<Render::Creature::StaticAttachmentSpec, 13> k_attachments{
      k_helmet_spec,
      k_tool_belt_specs[0],
      k_tool_belt_specs[1],
      k_tool_belt_specs[2],
      k_tool_belt_specs[3],
      k_tool_belt_specs[4],
      k_tool_belt_specs[5],
      k_work_apron_specs[0],
      k_work_apron_specs[1],
      k_work_apron_specs[2],
      k_arm_guards_specs[0],
      k_arm_guards_specs[1],
      k_work_tunic_spec};
  return k_attachments;
}

auto roman_builder_attachments_with_tool(
    const Render::Creature::StaticAttachmentSpec& tool_spec)
    -> std::array<Render::Creature::StaticAttachmentSpec, 14> {
  std::array<Render::Creature::StaticAttachmentSpec, 14> attachments{};
  auto const& common = roman_builder_common_attachments();
  for (std::size_t i = 0; i < common.size(); ++i) {
    attachments[i] = common[i];
  }
  attachments.back() = tool_spec;
  return attachments;
}

auto roman_builder_extra_role_colors(const void* variant_void,
                                     QVector3D* out,
                                     std::uint32_t base_count,
                                     std::size_t max_count) -> std::uint32_t {
  if (variant_void == nullptr || max_count <= base_count) {
    return base_count;
  }
  const auto& v = *static_cast<const HumanoidVariant*>(variant_void);
  auto count = base_count;
  count += Render::GL::roman_light_helmet_fill_role_colors(
      v.palette, out + count, max_count - count);
  if (max_count <= count) {
    return count;
  }
  count +=
      builder_tool_belt_fill_role_colors(v.palette, out + count, max_count - count);
  if (max_count <= count) {
    return count;
  }
  count +=
      builder_work_apron_fill_role_colors(v.palette, out + count, max_count - count);
  if (max_count <= count) {
    return count;
  }
  count +=
      builder_arm_guards_fill_role_colors(v.palette, out + count, max_count - count);
  if (max_count <= count) {
    return count;
  }
  count +=
      builder_work_tunic_fill_role_colors(v.palette, out + count, max_count - count);
  return count;
}

auto roman_builder_hammer_unit_archetype() -> Render::Creature::ArchetypeId {
  static constexpr std::array<std::uint8_t, 3> k_slots{
      k_builder_hammer_wood_slot,
      k_builder_hammer_metal_slot,
      k_builder_hammer_metal_dark_slot};
  static const auto k_tool_spec = builder_tool_make_static_attachment(
      builder_hammer_archetype(),
      Render::Creature::ArchetypeRegistry::instance()
          .get(roman_builder_idle_archetype())
          ->role_count,
      k_slots);
  static const auto k_archetype = register_builder_tool_variant_archetype(
      "troops/roman/builder/construction_hammer",
      roman_builder_idle_archetype(),
      k_tool_spec,
      +[](const void* variant_void,
          QVector3D* out,
          std::uint32_t base_count,
          std::size_t max_count) -> std::uint32_t {
        if (variant_void == nullptr || max_count <= base_count) {
          return base_count;
        }
        const auto& v = *static_cast<const HumanoidVariant*>(variant_void);
        return base_count + builder_hammer_fill_role_colors(
                                v.palette, out + base_count, max_count - base_count);
      },
      static_cast<std::uint8_t>(k_builder_hammer_role_count));
  return k_archetype;
}

auto roman_builder_saw_unit_archetype() -> Render::Creature::ArchetypeId {
  static constexpr std::array<std::uint8_t, 4> k_slots{k_saw_wood_slot,
                                                       k_saw_metal_slot,
                                                       k_saw_metal_dark_slot,
                                                       k_saw_leather_slot};
  static const auto k_tool_spec = builder_tool_make_static_attachment(
      builder_saw_archetype(),
      Render::Creature::ArchetypeRegistry::instance()
          .get(roman_builder_idle_archetype())
          ->role_count,
      k_slots);
  static const auto k_archetype = register_builder_tool_variant_archetype(
      "troops/roman/builder/construction_saw",
      roman_builder_idle_archetype(),
      k_tool_spec,
      +[](const void* variant_void,
          QVector3D* out,
          std::uint32_t base_count,
          std::size_t max_count) -> std::uint32_t {
        if (variant_void == nullptr || max_count <= base_count) {
          return base_count;
        }
        const auto& v = *static_cast<const HumanoidVariant*>(variant_void);
        return base_count + builder_saw_fill_role_colors(
                                v.palette, out + base_count, max_count - base_count);
      },
      static_cast<std::uint8_t>(k_builder_saw_role_count));
  return k_archetype;
}

auto roman_builder_chisel_unit_archetype() -> Render::Creature::ArchetypeId {
  static constexpr std::array<std::uint8_t, 2> k_slots{k_chisel_wood_slot,
                                                       k_chisel_metal_slot};
  static const auto k_tool_spec = builder_tool_make_static_attachment(
      builder_chisel_archetype(),
      Render::Creature::ArchetypeRegistry::instance()
          .get(roman_builder_idle_archetype())
          ->role_count,
      k_slots);
  static const auto k_archetype = register_builder_tool_variant_archetype(
      "troops/roman/builder/construction_chisel",
      roman_builder_idle_archetype(),
      k_tool_spec,
      +[](const void* variant_void,
          QVector3D* out,
          std::uint32_t base_count,
          std::size_t max_count) -> std::uint32_t {
        if (variant_void == nullptr || max_count <= base_count) {
          return base_count;
        }
        const auto& v = *static_cast<const HumanoidVariant*>(variant_void);
        return base_count + builder_chisel_fill_role_colors(
                                v.palette, out + base_count, max_count - base_count);
      },
      static_cast<std::uint8_t>(k_builder_chisel_role_count));
  return k_archetype;
}

static auto
roman_builder_variant_table() -> const Render::Creature::ArchetypeVariantTable& {
  static const Render::Creature::ArchetypeVariantTable k_table = []() {
    Render::Creature::ArchetypeVariantTable t{};
    t.variant_trigger_pose = Render::Creature::PoseIntent::Construct;
    t.variant_stride = 3;
    t.variant_is_seed_based = true;

    t.archetype_for_variant[0] = roman_builder_hammer_unit_archetype();
    t.state_for_variant[0] = Render::Creature::AnimationStateId::AttackSword;

    t.archetype_for_variant[1] = roman_builder_saw_unit_archetype();
    t.state_for_variant[1] = Render::Creature::AnimationStateId::AttackSword;

    t.archetype_for_variant[2] = roman_builder_chisel_unit_archetype();
    t.state_for_variant[2] = Render::Creature::AnimationStateId::AttackSpear;
    return t;
  }();
  return k_table;
}

} // namespace

void register_builder_style(const std::string& nation_id,
                            const BuilderStyleConfig& style) {
  style_registry()[nation_id] = style;
}

using Render::GL::Humanoid::mix_palette_color;

auto resolve_builder_style(const DrawContext& ctx) -> const BuilderStyleConfig& {
  ensure_builder_styles_registered();
  auto& styles = style_registry();
  std::string nation_id;
  if (ctx.entity != nullptr) {
    if (auto* unit = ctx.entity->get_component<Engine::Core::UnitComponent>()) {
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

void apply_builder_palette_overrides(const BuilderStyleConfig& style,
                                     const QVector3D& team_tint,
                                     HumanoidVariant& variant) {
  auto apply = [&](const std::optional<QVector3D>& c, QVector3D& t) {
    t = mix_palette_color(t, c, team_tint, k_team_mix_weight, k_style_mix_weight);
  };
  apply(style.cloth_color, variant.palette.cloth);
  apply(style.leather_color, variant.palette.leather);
  apply(style.leather_dark_color, variant.palette.leather_dark);
  apply(style.metal_color, variant.palette.metal);
  apply(style.wood_color, variant.palette.wood);
}

void apply_roman_civilian_palette(const QVector3D& team_tint,
                                  std::uint32_t seed,
                                  HumanoidVariant& variant) {
  QVector3D const wood_color(0.52F, 0.42F, 0.28F);
  QVector3D const metal_color(0.72F, 0.55F, 0.35F);
  float const cloth_roll = hash_01(seed ^ 0x2A15U);
  float const sash_roll = hash_01(seed ^ 0x6D31U);

  QVector3D tunic_color = cloth_roll < 0.33F   ? QVector3D(0.80F, 0.72F, 0.60F)
                          : cloth_roll < 0.66F ? QVector3D(0.70F, 0.62F, 0.52F)
                                               : QVector3D(0.66F, 0.58F, 0.48F);
  QVector3D sash_color = sash_roll < 0.5F ? QVector3D(0.58F, 0.24F, 0.18F)
                                          : QVector3D(0.45F, 0.29F, 0.16F);

  variant.palette.cloth =
      mix_palette_color(variant.palette.cloth, tunic_color, team_tint, 0.10F, 0.90F);
  variant.palette.leather =
      mix_palette_color(variant.palette.leather, sash_color, team_tint, 0.08F, 0.92F);
  variant.palette.leather_dark = variant.palette.leather * 0.74F;
  variant.palette.wood =
      mix_palette_color(variant.palette.wood, wood_color, team_tint, 0.10F, 0.90F);
  variant.palette.metal =
      mix_palette_color(variant.palette.metal, metal_color, team_tint, 0.08F, 0.92F);
}

class BuilderRenderer : public HumanoidRendererBase {
public:
  friend void register_builder_renderer(Render::GL::EntityRendererRegistry& registry);

  auto get_proportion_scaling() const -> QVector3D override {
    return k_builder_profile.as_vector();
  }

  auto
  visual_spec() const -> const Render::Creature::Pipeline::UnitVisualSpec& override {
    using namespace Render::Creature::Pipeline;
    static const UnitVisualSpec spec = []() {
      UnitVisualSpec s{};
      s.kind = CreatureKind::Humanoid;
      s.debug_name = "troops/roman/builder";
      s.scaling = k_builder_profile.as_pipeline_scaling();
      s.owned_legacy_slots = LegacySlotMask::AllHumanoid;
      s.archetype_id = roman_builder_idle_archetype();
      s.variant_table = &roman_builder_variant_table();
      return s;
    }();
    return spec;
  }

  void get_variant(const DrawContext& ctx,
                   uint32_t seed,
                   HumanoidVariant& v) const override {
    QVector3D const team_tint = resolve_team_tint(ctx);
    v.palette = make_humanoid_palette(team_tint, seed);
    apply_builder_palette_overrides(resolve_builder_style(ctx), team_tint, v);
  }
};

class CivilianRenderer : public HumanoidRendererBase {
public:
  auto get_proportion_scaling() const -> QVector3D override {
    return k_civilian_profile.as_vector();
  }

  auto
  visual_spec() const -> const Render::Creature::Pipeline::UnitVisualSpec& override {
    using namespace Render::Creature::Pipeline;
    static const UnitVisualSpec spec = []() {
      UnitVisualSpec s{};
      ensure_roman_civilian_equipment_contributions_registered();
      const auto loadout =
          Render::GL::Nation::resolve_equipment_loadout("troops/roman/civilian");
      const std::array<EquipmentHandle, 2> handles{loadout.armor_handle,
                                                   loadout.cloak_handle};
      s.kind = CreatureKind::Humanoid;
      s.debug_name = "troops/roman/civilian";
      s.scaling = k_civilian_profile.as_pipeline_scaling();
      s.owned_legacy_slots = LegacySlotMask::AllHumanoid;
      s.archetype_id = resolve_humanoid_equipment_archetype(
          "troops/roman/civilian",
          Render::Creature::ArchetypeRegistry::k_humanoid_base,
          handles);
      return s;
    }();
    return spec;
  }

  void get_variant(const DrawContext& ctx,
                   std::uint32_t seed,
                   HumanoidVariant& v) const override {
    QVector3D const team_tint = resolve_team_tint(ctx);
    v.palette = make_humanoid_palette(team_tint, seed);
    apply_roman_civilian_palette(team_tint, seed, v);
  }
};

void register_builder_renderer(Render::GL::EntityRendererRegistry& registry) {
  ensure_builder_styles_registered();
  static BuilderRenderer const renderer;
  registry.register_renderer("troops/roman/builder",
                             [](const DrawContext& ctx, ISubmitter& out) {
                               static BuilderRenderer const r;
                               r.render(ctx, out);
                             });

  auto& ar = Render::GL::RenderArchetypeRegistry::instance();
  ar.register_archetype("roman_builder_work_tunic",
                        [] { (void)builder_work_tunic_archetype(); });
  ar.register_archetype("roman_civilian_mantle",
                        [] { (void)roman_civilian_mantle_archetype(); });
  ar.register_archetype("roman_builder_hammer",
                        [] { (void)builder_hammer_archetype(); });
  ar.register_archetype("roman_builder_saw", [] { (void)builder_saw_archetype(); });
  ar.register_archetype("roman_builder_chisel",
                        [] { (void)builder_chisel_archetype(); });
}

void register_civilian_renderer(Render::GL::EntityRendererRegistry& registry) {
  ensure_builder_styles_registered();
  static CivilianRenderer const renderer;
  registry.register_renderer("troops/roman/civilian",
                             [](const DrawContext& ctx, ISubmitter& out) {
                               static CivilianRenderer const r;
                               r.render(ctx, out);
                             });
}

} // namespace Render::GL::Roman
