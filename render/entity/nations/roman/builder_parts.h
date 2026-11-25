#pragma once

#include <QMatrix4x4>
#include <QString>
#include <QVector3D>
#include <qmatrix4x4.h>
#include <qstringliteral.h>
#include <qvectornd.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "animation/rig/humanoid_proportions.h"
#include "game/core/component_core.h"
#include "game/core/entity.h"
#include "render/creature/archetype_registry.h"
#include "render/creature/humanoid/asset/bind_skeleton.h"
#include "render/creature/humanoid/asset/humanoid_spec.h"
#include "render/creature/humanoid/runtime/humanoid_math.h"
#include "render/creature/humanoid/runtime/humanoid_renderer.h"
#include "render/creature/humanoid/runtime/skeleton_evaluator.h"
#include "render/creature/humanoid/runtime/style_palette.h"
#include "render/creature/humanoid/schema/humanoid_proportion_profiles.h"
#include "render/creature/pipeline/unit_visual_spec.h"
#include "render/entity/civilian_actor.h"
#include "render/entity/nations/builder_tool_palette.h"
#include "render/entity/nations/equipment_loadout_catalog.h"
#include "render/entity/nations/roman/builder_renderer.h"
#include "render/entity/nations/roman/builder_style.h"
#include "render/entity/registry.h"
#include "render/equipment/armor/arm_guards_renderer.h"
#include "render/equipment/armor/tool_belt_renderer.h"
#include "render/equipment/armor/torso_local_archetype_utils.h"
#include "render/equipment/armor/work_apron_renderer.h"
#include "render/equipment/attachment_builder.h"
#include "render/equipment/generated_equipment.h"
#include "render/equipment/helmets/roman_light_helmet.h"
#include "render/equipment/humanoid_equipment_archetype.h"
#include "render/equipment/render_archetype_registry.h"
#include "render/equipment/renderer_constants.h"
#include "render/frame/frame_submitter.h"
#include "render/frame/palette.h"
#include "render/frame/submitter.h"
#include "render/geom/garment_shell.h"
#include "render/geom/primitives.h"
#include "render/geom/transforms.h"
#include "render/math/math_utils.h"
#include "render/math/render_constants.h"

using Render::Geom::cylinder_between;
using Render::Geom::sphere_at;

namespace Render::GL::Roman::builder_parts_detail {

constexpr std::uint32_t k_builder_work_tunic_role_count = 2;

constexpr std::uint32_t k_builder_hammer_role_count = 1;

constexpr std::uint32_t k_builder_saw_role_count = 1;

constexpr std::uint32_t k_builder_chisel_role_count = 1;

constexpr std::uint32_t k_builder_sickle_role_count = 1;

constexpr std::uint32_t k_roman_civilian_mantle_role_count = 2;

constexpr std::uint32_t k_civilian_pack_role_count = 3;

constexpr std::uint32_t k_civilian_cudgel_role_count = 2;

enum CivilianPackSlot : std::uint8_t {
  k_pack_bedroll_slot = 0U,
  k_pack_strap_slot = 1U,
  k_pack_wicker_slot = 2U,
};

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

enum BuilderSicklePaletteSlot : std::uint8_t {
  k_builder_sickle_wood_slot = 0U,
  k_builder_sickle_metal_slot = 1U,
};

enum CivilianCudgelPaletteSlot : std::uint8_t {
  k_civilian_cudgel_wood_slot = 0U,
  k_civilian_cudgel_band_slot = 1U,
};

enum BuilderHammerPaletteSlot : std::uint8_t {
  k_builder_hammer_wood_slot = 0U,
  k_builder_hammer_metal_slot = 1U,
  k_builder_hammer_metal_dark_slot = 2U,
};

auto builder_work_tunic_fill_role_colors(const HumanoidPalette& palette,
                                         QVector3D* out,
                                         std::size_t max) -> std::uint32_t;

auto roman_civilian_mantle_fill_role_colors(const HumanoidPalette& palette,
                                            QVector3D* out,
                                            std::size_t max) -> std::uint32_t;

auto builder_hammer_fill_role_colors(const HumanoidPalette& palette,
                                     QVector3D* out,
                                     std::size_t max) -> std::uint32_t;

auto civilian_cudgel_fill_role_colors(const HumanoidPalette& palette,
                                      QVector3D* out,
                                      std::size_t max) -> std::uint32_t;

auto builder_saw_fill_role_colors(const HumanoidPalette& palette,
                                  QVector3D* out,
                                  std::size_t max) -> std::uint32_t;

auto builder_chisel_fill_role_colors(const HumanoidPalette& palette,
                                     QVector3D* out,
                                     std::size_t max) -> std::uint32_t;

auto builder_sickle_fill_role_colors(const HumanoidPalette& palette,
                                     QVector3D* out,
                                     std::size_t max) -> std::uint32_t;

auto builder_work_tunic_archetype() -> const RenderArchetype&;

auto builder_work_tunic_make_static_attachment(std::uint16_t chest_bone_index,
                                               std::uint8_t base_role_byte)
    -> Render::Creature::StaticAttachmentSpec;

auto builder_work_tunic_extra_role_colors(const void* variant_void,
                                          QVector3D* out,
                                          std::uint32_t base_count,
                                          std::size_t max_count) -> std::uint32_t;

auto builder_work_tunic_contribution_attachments(std::uint8_t base_role_byte)
    -> std::vector<Render::Creature::StaticAttachmentSpec>;

auto civilian_pack_fill_role_colors(const HumanoidPalette& palette,
                                    QVector3D* out,
                                    std::size_t max) -> std::uint32_t;

auto civilian_pack_extra_role_colors(const void* variant_void,
                                     QVector3D* out,
                                     std::uint32_t base_count,
                                     std::size_t max_count) -> std::uint32_t;

auto roman_civilian_pack_archetype() -> const RenderArchetype&;

auto roman_civilian_pack_make_static_attachment(std::uint16_t chest_bone_index,
                                                std::uint8_t base_role_byte)
    -> Render::Creature::StaticAttachmentSpec;

auto roman_civilian_pack_contribution_attachments(std::uint8_t base_role)
    -> std::vector<Render::Creature::StaticAttachmentSpec>;

auto roman_civilian_mantle_archetype() -> const RenderArchetype&;

auto roman_civilian_mantle_make_static_attachment(std::uint16_t chest_bone_index,
                                                  std::uint8_t base_role_byte)
    -> Render::Creature::StaticAttachmentSpec;

auto roman_civilian_mantle_extra_role_colors(const void* variant_void,
                                             QVector3D* out,
                                             std::uint32_t base_count,
                                             std::size_t max_count) -> std::uint32_t;

auto roman_civilian_mantle_contribution_attachments(std::uint8_t base_role_byte)
    -> std::vector<Render::Creature::StaticAttachmentSpec>;

auto builder_hammer_archetype() -> const RenderArchetype&;

auto civilian_cudgel_archetype() -> const RenderArchetype&;

auto builder_saw_archetype() -> const RenderArchetype&;

auto builder_sickle_archetype() -> const RenderArchetype&;

auto builder_chisel_archetype() -> const RenderArchetype&;

auto builder_tool_make_static_attachment(const RenderArchetype& archetype,
                                         std::uint8_t base_role_byte,
                                         std::span<const std::uint8_t> slot_indices)
    -> Render::Creature::StaticAttachmentSpec;

auto register_builder_tool_variant_archetype(
    std::string_view debug_name,
    Render::Creature::ArchetypeId base_archetype_id,
    const Render::Creature::StaticAttachmentSpec& tool_spec,
    Render::Creature::ArchetypeDescriptor::ExtraRoleColorsFn tool_role_colors,
    std::uint8_t tool_role_count) -> Render::Creature::ArchetypeId;

} // namespace Render::GL::Roman::builder_parts_detail
