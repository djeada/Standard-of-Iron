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
#include "render/cache/static_attachment_spec.h"
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
#include "render/entity/nations/carthage/builder_renderer.h"
#include "render/entity/nations/carthage/builder_style.h"
#include "render/entity/nations/equipment_loadout_catalog.h"
#include "render/entity/registry.h"
#include "render/equipment/armor/arm_guards_renderer.h"
#include "render/equipment/armor/tool_belt_renderer.h"
#include "render/equipment/armor/torso_local_archetype_utils.h"
#include "render/equipment/armor/work_apron_renderer.h"
#include "render/equipment/attachment_builder.h"
#include "render/equipment/equipment_registry.h"
#include "render/equipment/equipment_submit.h"
#include "render/equipment/generated_equipment.h"
#include "render/equipment/humanoid_attachment_archetype.h"
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

namespace Render::GL::Carthage::builder_parts_detail {

constexpr std::uint32_t k_carthage_headwrap_role_count = 1U;

constexpr std::uint32_t k_carthage_robes_role_count = 2U;

constexpr std::uint32_t k_carthage_hammer_role_count = 2U;

constexpr std::uint32_t k_carthage_saw_role_count = 4U;

constexpr std::uint32_t k_carthage_chisel_role_count = 2U;

constexpr std::uint32_t k_carthage_sickle_role_count = 2U;

constexpr std::uint8_t k_sickle_wood_slot = 0U;

constexpr std::uint8_t k_sickle_metal_slot = 1U;

constexpr std::uint32_t k_carthage_civilian_sash_role_count = 2U;

constexpr std::uint32_t k_civilian_pack_role_count = 3U;

constexpr std::uint32_t k_civilian_cudgel_role_count = 2U;

constexpr std::uint8_t k_civilian_cudgel_wood_slot = 0U;

constexpr std::uint8_t k_civilian_cudgel_band_slot = 1U;

enum CivilianPackSlot : std::uint8_t {
  k_pack_clay_slot = 0U,
  k_pack_strap_slot = 1U,
  k_pack_weave_slot = 2U,
};

constexpr float k_pack_back_distance = 1.05F;

auto carthage_headwrap_archetype() -> const RenderArchetype&;

auto carthage_robes_archetype() -> const RenderArchetype&;

auto civilian_pack_fill_role_colors(const HumanoidPalette& palette,
                                    QVector3D* out,
                                    std::size_t max) -> std::uint32_t;

auto civilian_pack_extra_role_colors(const void* variant_void,
                                     QVector3D* out,
                                     std::uint32_t base_count,
                                     std::size_t max_count) -> std::uint32_t;

auto carthage_civilian_pack_archetype() -> const RenderArchetype&;

auto carthage_civilian_pack_make_static_attachment(std::uint16_t chest_bone_index,
                                                   std::uint8_t base_role_byte)
    -> Render::Creature::StaticAttachmentSpec;

auto carthage_civilian_pack_contribution_attachments(std::uint8_t base_role)
    -> std::vector<Render::Creature::StaticAttachmentSpec>;

auto carthage_civilian_sash_archetype() -> const RenderArchetype&;

auto carthage_hammer_archetype() -> const RenderArchetype&;

auto civilian_cudgel_archetype() -> const RenderArchetype&;

auto carthage_saw_archetype() -> const RenderArchetype&;

auto carthage_sickle_archetype() -> const RenderArchetype&;

auto carthage_chisel_archetype() -> const RenderArchetype&;

auto carthage_headwrap_make_static_attachment(std::uint16_t head_bone,
                                              std::uint8_t base_role)
    -> Render::Creature::StaticAttachmentSpec;

auto carthage_headwrap_contribution_attachments(std::uint8_t base_role)
    -> std::vector<Render::Creature::StaticAttachmentSpec>;

auto carthage_robes_make_static_attachment(std::uint16_t chest_bone,
                                           std::uint8_t base_role)
    -> Render::Creature::StaticAttachmentSpec;

auto carthage_robes_contribution_attachments(std::uint8_t base_role)
    -> std::vector<Render::Creature::StaticAttachmentSpec>;

auto carthage_tool_make_static_attachment(const RenderArchetype& archetype,
                                          std::uint8_t base_role,
                                          std::span<const std::uint8_t> slot_indices)
    -> Render::Creature::StaticAttachmentSpec;

auto carthage_civilian_sash_make_static_attachment(std::uint16_t chest_bone,
                                                   std::uint8_t base_role)
    -> Render::Creature::StaticAttachmentSpec;

auto carthage_civilian_sash_contribution_attachments(std::uint8_t base_role)
    -> std::vector<Render::Creature::StaticAttachmentSpec>;

auto carthage_robes_fill_role_colors(const HumanoidPalette& palette,
                                     QVector3D* out,
                                     std::size_t max) -> std::uint32_t;

auto carthage_headwrap_extra_role_colors(const void*,
                                         QVector3D* out,
                                         std::uint32_t base_count,
                                         std::size_t max_count) -> std::uint32_t;

auto carthage_robes_extra_role_colors(const void* variant_void,
                                      QVector3D* out,
                                      std::uint32_t base_count,
                                      std::size_t max_count) -> std::uint32_t;

auto carthage_hammer_fill_role_colors(const HumanoidPalette& palette,
                                      QVector3D* out,
                                      std::size_t max) -> std::uint32_t;

auto civilian_cudgel_fill_role_colors(const HumanoidPalette& palette,
                                      QVector3D* out,
                                      std::size_t max) -> std::uint32_t;

auto carthage_saw_fill_role_colors(const HumanoidPalette& palette,
                                   QVector3D* out,
                                   std::size_t max) -> std::uint32_t;

auto carthage_sickle_fill_role_colors(const HumanoidPalette& palette,
                                      QVector3D* out,
                                      std::size_t max) -> std::uint32_t;

auto carthage_chisel_fill_role_colors(const HumanoidPalette& palette,
                                      QVector3D* out,
                                      std::size_t max) -> std::uint32_t;

} // namespace Render::GL::Carthage::builder_parts_detail
