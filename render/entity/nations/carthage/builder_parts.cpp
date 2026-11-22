#include "render/entity/nations/carthage/builder_parts.h"

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

auto carthage_headwrap_archetype() -> const RenderArchetype& {
  static const RenderArchetype arch = []() {
    constexpr float k_head_silhouette_r = 0.168F;
    QVector3D const crown_local(0.0F, k_head_silhouette_r * 0.40F, 0.0F);
    QVector3D const nape_local(
        0.0F, -k_head_silhouette_r * 0.12F, -k_head_silhouette_r * 0.92F);
    QVector3D const fold_local(
        0.0F, k_head_silhouette_r * 0.30F, -k_head_silhouette_r * 0.86F);
    std::array<GeneratedEquipmentPrimitive, 3> const prims{{
        generated_sphere(crown_local, k_head_silhouette_r * 0.86F, 0U),
        generated_sphere(fold_local, k_head_silhouette_r * 0.46F, 0U),
        generated_sphere(nape_local, k_head_silhouette_r * 0.40F, 0U),
    }};
    return build_generated_equipment_archetype("carthage_headwrap", prims);
  }();
  return arch;
}

auto carthage_robes_archetype() -> const RenderArchetype& {
  static const RenderArchetype arch = []() {
    const auto& bind = Render::Humanoid::humanoid_bind_body_frames();
    const AttachmentFrame& torso = bind.torso;
    const AttachmentFrame& waist = bind.waist;

    constexpr std::uint8_t k_main = 0U;
    constexpr std::uint8_t k_dark = 1U;

    float const tr = torso.radius * 1.06F;
    float const td = (torso.depth > 0.0F) ? torso.depth * 0.90F : torso.radius * 0.78F;
    float const y_sh = 0.035F;
    float const y_w = waist.origin.y() - torso.origin.y();
    float const y_hem = y_w - 0.24F;

    RenderArchetypeBuilder builder{"carthage_robes"};

    {
      float const h = y_sh - y_w;
      float const cy = (y_sh + y_w) * 0.5F;
      QMatrix4x4 m;
      m.translate(0.0F, cy, 0.0F);
      m.scale(tr * 1.05F, h, td);
      builder.add_palette_mesh(get_unit_tapered_cylinder(0.88F, 1.04F, 8), m, k_main);
    }

    {
      float const h = y_w - y_hem;
      float const cy = (y_w + y_hem) * 0.5F;
      QMatrix4x4 m;
      m.translate(0.0F, cy, 0.0F);
      m.scale(tr * 1.05F, h, td);
      builder.add_palette_mesh(get_unit_tapered_cylinder(1.42F, 0.88F, 8), m, k_main);
    }

    {
      float const h = 0.030F;
      float const cy = y_w + 0.004F;
      QMatrix4x4 m;
      m.translate(0.0F, cy, 0.0F);
      m.scale(tr * 1.10F, h, td * 1.05F);
      builder.add_palette_mesh(get_unit_tapered_cylinder(1.0F, 1.0F, 8), m, k_dark);
    }

    {
      float const h = 0.024F;
      float const cy = y_hem + h * 0.5F;
      QMatrix4x4 m;
      m.translate(0.0F, cy, 0.0F);
      m.scale(tr * 1.58F, h, td * 1.50F);
      builder.add_palette_mesh(get_unit_tapered_cylinder(1.0F, 1.0F, 8), m, k_dark);
    }

    {
      builder.add_palette_mesh(
          get_unit_sphere(),
          sphere_at(QVector3D(-(tr * 0.76F), y_sh - 0.008F, td * 0.06F), tr * 0.24F),
          k_main);
    }

    {
      builder.add_palette_mesh(
          get_unit_sphere(),
          sphere_at(QVector3D(tr * 0.76F, y_sh - 0.008F, td * 0.06F), tr * 0.24F),
          k_main);
    }

    {
      float const h = 0.018F;
      float const cy = y_sh + 0.028F;
      QMatrix4x4 m;
      m.translate(0.0F, cy, 0.0F);
      m.scale(tr * 0.72F, h, td * 0.66F);
      builder.add_palette_mesh(get_unit_tapered_cylinder(1.0F, 1.0F, 8), m, k_dark);
    }

    return std::move(builder).build();
  }();
  return arch;
}

auto civilian_pack_fill_role_colors(const HumanoidPalette& palette,
                                    QVector3D* out,
                                    std::size_t max) -> std::uint32_t {
  if (max < k_civilian_pack_role_count) {
    return 0U;
  }
  out[k_pack_clay_slot] = QVector3D(0.60F, 0.38F, 0.24F);
  out[k_pack_strap_slot] = palette.leather_dark;
  out[k_pack_weave_slot] = Render::GL::Humanoid::saturate_color(
      palette.cloth * 0.48F + QVector3D(0.26F, 0.21F, 0.13F));
  return k_civilian_pack_role_count;
}

auto civilian_pack_extra_role_colors(const void* variant_void,
                                     QVector3D* out,
                                     std::uint32_t base_count,
                                     std::size_t max_count) -> std::uint32_t {
  if (variant_void == nullptr || max_count <= base_count) {
    return base_count;
  }
  const auto& variant = *static_cast<const HumanoidVariant*>(variant_void);
  return base_count + civilian_pack_fill_role_colors(
                          variant.palette, out + base_count, max_count - base_count);
}

auto carthage_civilian_pack_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = [] {
    const auto& bind = Render::Humanoid::humanoid_bind_body_frames();
    const AttachmentFrame& torso = bind.torso;
    const AttachmentFrame& waist = bind.waist;
    const TorsoLocalFrame local = make_torso_local_frame(QMatrix4x4{}, torso);

    float const tr = torso.radius;
    float const y_sh = 0.010F;
    float const y_w = local.point(waist.origin).y();

    RenderArchetypeBuilder builder{"carthage_civilian_pack"};

    float const pack_z = -tr * k_pack_back_distance;
    QVector3D const jar(0.0F, y_sh - 0.115F, pack_z);
    builder.add_palette_mesh(
        get_unit_sphere(),
        local_scale_model(jar, QVector3D(tr * 0.52F, 0.135F, tr * 0.44F)),
        k_pack_clay_slot);
    builder.add_palette_mesh(get_unit_cylinder(),
                             cylinder_between(jar + QVector3D(0.0F, 0.110F, 0.0F),
                                              jar + QVector3D(0.0F, 0.205F, 0.010F),
                                              tr * 0.16F),
                             k_pack_clay_slot);
    builder.add_palette_mesh(
        get_unit_sphere(),
        local_scale_model(jar + QVector3D(0.0F, 0.212F, 0.010F),
                          QVector3D(tr * 0.22F, 0.020F, tr * 0.20F)),
        k_pack_clay_slot);
    builder.add_palette_mesh(
        get_unit_cone(),
        Render::Geom::cone_from_to(jar - QVector3D(0.0F, 0.105F, 0.0F),
                                   jar - QVector3D(0.0F, 0.215F, 0.0F),
                                   tr * 0.26F),
        k_pack_clay_slot);
    for (int side = -1; side <= 1; side += 2) {
      float const sx = static_cast<float>(side);
      builder.add_palette_mesh(
          get_unit_cylinder(),
          cylinder_between(jar + QVector3D(sx * tr * 0.18F, 0.170F, 0.0F),
                           jar + QVector3D(sx * tr * 0.44F, 0.070F, 0.0F),
                           tr * 0.055F),
          k_pack_clay_slot);
      builder.add_palette_mesh(
          get_unit_cylinder(),
          cylinder_between(
              QVector3D(sx * tr * 0.40F, y_sh + 0.010F, pack_z + tr * 0.10F),
              QVector3D(sx * tr * 0.52F, y_sh - 0.060F, tr * 0.80F),
              tr * 0.060F),
          k_pack_strap_slot);
    }

    QVector3D const satchel(-tr * 1.00F, y_w - 0.070F, tr * 0.10F);
    {
      QMatrix4x4 m;
      m.translate(satchel);
      m.scale(tr * 0.40F, 0.140F, tr * 0.30F);
      builder.add_palette_mesh(
          get_unit_tapered_cylinder(1.05F, 0.85F, 10), m, k_pack_weave_slot);
    }
    builder.add_palette_mesh(
        get_unit_cylinder(),
        cylinder_between(satchel + QVector3D(0.0F, 0.070F, 0.0F),
                         QVector3D(tr * 0.42F, y_sh - 0.020F, tr * 0.30F),
                         tr * 0.050F),
        k_pack_strap_slot);

    return std::move(builder).build();
  }();
  return archetype;
}

auto carthage_civilian_pack_make_static_attachment(std::uint16_t chest_bone_index,
                                                   std::uint8_t base_role_byte)
    -> Render::Creature::StaticAttachmentSpec {
  const auto& bind_frames = Render::Humanoid::humanoid_bind_body_frames();
  const TorsoLocalFrame torso_local =
      make_torso_local_frame(QMatrix4x4{}, bind_frames.torso);
  auto spec = Render::Equipment::build_static_attachment({
      .archetype = &carthage_civilian_pack_archetype(),
      .socket_bone_index = chest_bone_index,
      .unit_local_pose_at_bind = torso_local.world,
  });
  for (std::uint8_t i = 0; i < static_cast<std::uint8_t>(k_civilian_pack_role_count);
       ++i) {
    spec.palette_role_remap[i] = static_cast<std::uint8_t>(base_role_byte + i);
  }
  return spec;
}

auto carthage_civilian_pack_contribution_attachments(std::uint8_t base_role)
    -> std::vector<Render::Creature::StaticAttachmentSpec> {
  return {carthage_civilian_pack_make_static_attachment(
      static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Chest), base_role)};
}

auto carthage_civilian_sash_archetype() -> const RenderArchetype& {
  static const RenderArchetype arch = []() {
    const auto& bind = Render::Humanoid::humanoid_bind_body_frames();
    const AttachmentFrame& torso = bind.torso;
    const AttachmentFrame& waist = bind.waist;

    float const tr = torso.radius * 1.18F;
    float const td = (torso.depth > 0.0F) ? torso.depth * 1.08F : torso.radius * 0.94F;
    float const y_w = waist.origin.y() - torso.origin.y();
    float const y_sh = 0.035F;
    float const y_hem = y_w - 0.22F;

    RenderArchetypeBuilder builder{"carthage_civilian_sash"};

    static const auto shell = make_garment_shell({
        {y_hem, tr * 1.08F * 1.40F, td * 1.04F * 1.40F},
        {y_w, tr * 1.08F * 0.90F, td * 1.04F * 0.90F},
        {y_sh, tr * 1.08F * 1.06F, td * 1.04F * 1.06F},
    });
    builder.add_palette_mesh(shell.get(), QMatrix4x4{}, 0U);

    {
      QVector3D const sash_top(tr * 0.72F, y_sh + 0.008F, td * 0.28F);
      QVector3D const sash_bot(-tr * 0.68F, y_w + 0.018F, td * 0.52F);
      builder.add_palette_mesh(
          get_unit_cylinder(8), cylinder_between(sash_top, sash_bot, tr * 0.28F), 1U);
    }

    {
      builder.add_palette_mesh(
          get_unit_sphere(),
          sphere_at(QVector3D(-tr * 0.08F, y_w + 0.020F, td * 0.62F), tr * 0.22F),
          1U);
    }

    return std::move(builder).build();
  }();
  return arch;
}

auto carthage_hammer_archetype() -> const RenderArchetype& {
  static const RenderArchetype arch = []() {
    constexpr std::uint8_t k_wood = 0U;
    constexpr std::uint8_t k_bronze = 1U;
    QVector3D const handle_top(0.0F, 0.11F, 0.02F);
    QVector3D const handle_bot(0.0F, -0.19F, 0.02F);
    QVector3D const head_axis(1.0F, 0.0F, 0.0F);
    QVector3D const head_center = handle_top + QVector3D(0.0F, 0.03F, 0.0F);
    float const head_len = 0.09F;
    float const head_r = 0.028F;
    std::array<GeneratedEquipmentPrimitive, 4> const prims{{
        generated_cylinder(handle_bot, handle_top, 0.015F, k_wood),
        generated_cylinder(head_center - head_axis * (head_len * 0.5F),
                           head_center + head_axis * (head_len * 0.5F),
                           head_r,
                           k_bronze),
        generated_sphere(
            head_center + head_axis * (head_len * 0.5F), head_r * 1.1F, k_bronze),
        generated_sphere(
            head_center - head_axis * (head_len * 0.5F), head_r * 0.85F, k_bronze),
    }};
    return build_generated_equipment_archetype("carthage_stone_hammer", prims);
  }();
  return arch;
}

auto civilian_cudgel_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = [] {
    std::array<GeneratedEquipmentPrimitive, 4> const primitives{{
        generated_cylinder(QVector3D(0.0F, -0.21F, 0.02F),
                           QVector3D(0.0F, 0.14F, 0.02F),
                           0.017F,
                           k_civilian_cudgel_wood_slot),
        generated_cylinder(QVector3D(0.0F, 0.10F, 0.02F),
                           QVector3D(0.0F, 0.19F, 0.02F),
                           0.024F,
                           k_civilian_cudgel_wood_slot),
        generated_sphere(
            QVector3D(0.0F, 0.20F, 0.02F), 0.029F, k_civilian_cudgel_wood_slot),
        generated_cylinder(QVector3D(0.0F, -0.10F, 0.02F),
                           QVector3D(0.0F, -0.045F, 0.02F),
                           0.019F,
                           k_civilian_cudgel_band_slot),
    }};
    return build_generated_equipment_archetype("carthage_civilian_cudgel", primitives);
  }();
  return archetype;
}

auto carthage_saw_archetype() -> const RenderArchetype& {
  static const RenderArchetype arch = []() {
    std::array<GeneratedEquipmentPrimitive, 8> const prims{{
        generated_cylinder(QVector3D(-0.01F, -0.17F, 0.03F),
                           QVector3D(0.07F, -0.08F, 0.03F),
                           0.019F,
                           k_saw_wood_slot),
        generated_sphere(QVector3D(0.07F, -0.07F, 0.03F), 0.024F, k_saw_leather_slot),
        generated_cylinder(QVector3D(0.02F, -0.06F, 0.03F),
                           QVector3D(0.04F, 0.18F, 0.03F),
                           0.013F,
                           k_saw_metal_slot),
        generated_cylinder(QVector3D(0.04F, 0.18F, 0.03F),
                           QVector3D(-0.08F, 0.11F, 0.03F),
                           0.018F,
                           k_saw_metal_dark_slot),
        generated_sphere(QVector3D(-0.01F, 0.145F, 0.03F), 0.017F, k_saw_metal_slot),
        generated_sphere(QVector3D(-0.04F, 0.13F, 0.03F), 0.014F, k_saw_metal_slot),
        generated_sphere(QVector3D(-0.065F, 0.117F, 0.03F), 0.012F, k_saw_metal_slot),
        generated_sphere(QVector3D(-0.085F, 0.105F, 0.03F), 0.010F, k_saw_metal_slot),
    }};
    return build_generated_equipment_archetype("carthage_builder_saw", prims);
  }();
  return arch;
}

auto carthage_sickle_archetype() -> const RenderArchetype& {
  static const RenderArchetype arch = []() {
    constexpr float k_arc_cx = 0.085F;
    constexpr float k_arc_cy = 0.045F;
    constexpr float k_arc_r = 0.090F;
    constexpr std::array<float, 6> k_angles{
        180.0F, 148.0F, 116.0F, 84.0F, 52.0F, 20.0F};
    std::vector<GeneratedEquipmentPrimitive> prims;
    prims.push_back(generated_cylinder(QVector3D(0.0F, -0.16F, 0.01F),
                                       QVector3D(0.0F, 0.03F, 0.01F),
                                       0.016F,
                                       k_sickle_wood_slot));
    prims.push_back(
        generated_sphere(QVector3D(0.0F, -0.165F, 0.01F), 0.02F, k_sickle_wood_slot));
    for (std::size_t i = 0; i + 1 < k_angles.size(); ++i) {
      auto point = [&](float degrees) {
        float const rad = degrees * 3.14159265F / 180.0F;
        return QVector3D(k_arc_cx + k_arc_r * std::cos(rad),
                         k_arc_cy + k_arc_r * std::sin(rad),
                         0.01F);
      };
      float const taper = 0.011F - 0.0016F * static_cast<float>(i);
      prims.push_back(generated_cylinder(
          point(k_angles[i]), point(k_angles[i + 1]), taper, k_sickle_metal_slot));
    }
    return build_generated_equipment_archetype("carthage_builder_sickle", prims);
  }();
  return arch;
}

auto carthage_chisel_archetype() -> const RenderArchetype& {
  static const RenderArchetype arch = []() {
    std::array<GeneratedEquipmentPrimitive, 4> const prims{{
        generated_cylinder(QVector3D(0.0F, -0.15F, 0.01F),
                           QVector3D(0.0F, 0.05F, 0.01F),
                           0.017F,
                           k_chisel_wood_slot),
        generated_sphere(QVector3D(0.0F, -0.17F, 0.01F), 0.022F, k_chisel_wood_slot),
        generated_cylinder(QVector3D(0.0F, 0.05F, 0.01F),
                           QVector3D(0.0F, 0.20F, 0.01F),
                           0.011F,
                           k_chisel_metal_slot),
        generated_sphere(QVector3D(0.0F, 0.215F, 0.01F), 0.011F, k_chisel_metal_slot),
    }};
    return build_generated_equipment_archetype("carthage_builder_chisel", prims);
  }();
  return arch;
}

auto carthage_headwrap_make_static_attachment(std::uint16_t head_bone,
                                              std::uint8_t base_role)
    -> Render::Creature::StaticAttachmentSpec {
  const auto& bind = Render::Humanoid::humanoid_bind_body_frames();
  QMatrix4x4 const bind_mat =
      make_humanoid_attachment_transform_scaled(QMatrix4x4{},
                                                bind.head,
                                                QVector3D(0.0F, 0.0F, 0.0F),
                                                QVector3D(1.0F, 1.0F, 1.0F));
  auto spec = Render::Equipment::build_static_attachment({
      .archetype = &carthage_headwrap_archetype(),
      .socket_bone_index = head_bone,
      .unit_local_pose_at_bind = bind_mat,
  });
  spec.palette_role_remap[0] = base_role;
  return spec;
}

auto carthage_headwrap_contribution_attachments(std::uint8_t base_role)
    -> std::vector<Render::Creature::StaticAttachmentSpec> {
  return {carthage_headwrap_make_static_attachment(
      static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Head), base_role)};
}

auto carthage_robes_make_static_attachment(std::uint16_t chest_bone,
                                           std::uint8_t base_role)
    -> Render::Creature::StaticAttachmentSpec {
  const auto& bind = Render::Humanoid::humanoid_bind_body_frames();
  QMatrix4x4 const bind_mat =
      make_humanoid_attachment_transform_scaled(QMatrix4x4{},
                                                bind.torso,
                                                QVector3D(0.0F, 0.0F, 0.0F),
                                                QVector3D(1.0F, 1.0F, 1.0F));
  auto spec = Render::Equipment::build_static_attachment({
      .archetype = &carthage_robes_archetype(),
      .socket_bone_index = chest_bone,
      .unit_local_pose_at_bind = bind_mat,
  });
  spec.palette_role_remap[0] = base_role;
  spec.palette_role_remap[1] = static_cast<std::uint8_t>(base_role + 1U);
  return spec;
}

auto carthage_robes_contribution_attachments(std::uint8_t base_role)
    -> std::vector<Render::Creature::StaticAttachmentSpec> {
  return {carthage_robes_make_static_attachment(
      static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Chest), base_role)};
}

auto carthage_tool_make_static_attachment(const RenderArchetype& archetype,
                                          std::uint8_t base_role,
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
    spec.palette_role_remap[slot_indices[i]] = static_cast<std::uint8_t>(base_role + i);
  }
  return spec;
}

auto carthage_civilian_sash_make_static_attachment(std::uint16_t chest_bone,
                                                   std::uint8_t base_role)
    -> Render::Creature::StaticAttachmentSpec {
  const auto& bind = Render::Humanoid::humanoid_bind_body_frames();
  QMatrix4x4 const bind_mat =
      make_humanoid_attachment_transform_scaled(QMatrix4x4{},
                                                bind.torso,
                                                QVector3D(0.0F, 0.0F, 0.0F),
                                                QVector3D(1.0F, 1.0F, 1.0F));
  auto spec = Render::Equipment::build_static_attachment({
      .archetype = &carthage_civilian_sash_archetype(),
      .socket_bone_index = chest_bone,
      .unit_local_pose_at_bind = bind_mat,
  });
  spec.palette_role_remap[0] = base_role;
  spec.palette_role_remap[1] = static_cast<std::uint8_t>(base_role + 1U);
  return spec;
}

auto carthage_civilian_sash_contribution_attachments(std::uint8_t base_role)
    -> std::vector<Render::Creature::StaticAttachmentSpec> {
  return {carthage_civilian_sash_make_static_attachment(
      static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Chest), base_role)};
}

auto carthage_robes_fill_role_colors(const HumanoidPalette& palette,
                                     QVector3D* out,
                                     std::size_t max) -> std::uint32_t {
  if (max < k_carthage_robes_role_count) {
    return 0U;
  }
  out[0] = Render::GL::Humanoid::saturate_color(palette.cloth * 0.22F +
                                                QVector3D(0.64F, 0.58F, 0.45F));
  out[1] = Render::GL::Humanoid::saturate_color(palette.cloth * 0.20F +
                                                QVector3D(0.44F, 0.38F, 0.28F));
  return k_carthage_robes_role_count;
}

auto carthage_headwrap_extra_role_colors(const void*,
                                         QVector3D* out,
                                         std::uint32_t base_count,
                                         std::size_t max_count) -> std::uint32_t {
  if (max_count - base_count < k_carthage_headwrap_role_count) {
    return base_count;
  }
  out[base_count] = QVector3D(0.60F, 0.54F, 0.43F);
  return base_count + k_carthage_headwrap_role_count;
}

auto carthage_robes_extra_role_colors(const void* variant_void,
                                      QVector3D* out,
                                      std::uint32_t base_count,
                                      std::size_t max_count) -> std::uint32_t {
  if (variant_void == nullptr || max_count <= base_count) {
    return base_count;
  }
  const auto& variant = *static_cast<const HumanoidVariant*>(variant_void);
  return base_count + carthage_robes_fill_role_colors(
                          variant.palette, out + base_count, max_count - base_count);
}

auto carthage_hammer_fill_role_colors(const HumanoidPalette& palette,
                                      QVector3D* out,
                                      std::size_t max) -> std::uint32_t {
  if (max < k_carthage_hammer_role_count) {
    return 0U;
  }
  out[0] = palette.wood;
  out[1] = palette.metal * 0.88F + QVector3D(0.08F, 0.05F, 0.01F);
  return k_carthage_hammer_role_count;
}

auto civilian_cudgel_fill_role_colors(const HumanoidPalette& palette,
                                      QVector3D* out,
                                      std::size_t max) -> std::uint32_t {
  if (max < k_civilian_cudgel_role_count) {
    return 0U;
  }
  out[0] = palette.wood;
  out[1] = palette.leather * 0.85F;
  return k_civilian_cudgel_role_count;
}

auto carthage_saw_fill_role_colors(const HumanoidPalette& palette,
                                   QVector3D* out,
                                   std::size_t max) -> std::uint32_t {
  if (max < k_carthage_saw_role_count) {
    return 0U;
  }
  out[0] = palette.wood;
  out[1] = palette.metal * 0.92F + QVector3D(0.06F, 0.04F, 0.01F);
  out[2] = palette.metal * 0.62F;
  out[3] = palette.leather_dark;
  return k_carthage_saw_role_count;
}

auto carthage_sickle_fill_role_colors(const HumanoidPalette& palette,
                                      QVector3D* out,
                                      std::size_t max) -> std::uint32_t {
  if (max < k_carthage_sickle_role_count) {
    return 0U;
  }
  out[0] = palette.wood;
  out[1] = palette.metal * 0.90F;
  return k_carthage_sickle_role_count;
}

auto carthage_chisel_fill_role_colors(const HumanoidPalette& palette,
                                      QVector3D* out,
                                      std::size_t max) -> std::uint32_t {
  if (max < k_carthage_chisel_role_count) {
    return 0U;
  }
  out[0] = palette.wood;
  out[1] = palette.metal * 0.86F + QVector3D(0.05F, 0.03F, 0.01F);
  return k_carthage_chisel_role_count;
}

} // namespace Render::GL::Carthage::builder_parts_detail
