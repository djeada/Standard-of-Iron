#include "render/entity/nations/roman/builder_parts.h"

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
  out[0] = palette.metal * 0.72F;
  return k_builder_hammer_role_count;
}

auto civilian_cudgel_fill_role_colors(const HumanoidPalette& palette,
                                      QVector3D* out,
                                      std::size_t max) -> std::uint32_t {
  if (max < k_civilian_cudgel_role_count) {
    return 0U;
  }
  out[0] = palette.wood;
  out[1] = palette.wood * 0.66F;
  return k_civilian_cudgel_role_count;
}

auto builder_saw_fill_role_colors(const HumanoidPalette& palette,
                                  QVector3D* out,
                                  std::size_t max) -> std::uint32_t {
  if (max < k_builder_saw_role_count) {
    return 0U;
  }
  out[0] = palette.metal * 0.72F;
  return k_builder_saw_role_count;
}

auto builder_chisel_fill_role_colors(const HumanoidPalette& palette,
                                     QVector3D* out,
                                     std::size_t max) -> std::uint32_t {
  if (max < k_builder_chisel_role_count) {
    return 0U;
  }
  out[0] = palette.metal * 0.88F;
  return k_builder_chisel_role_count;
}

auto builder_sickle_fill_role_colors(const HumanoidPalette& palette,
                                     QVector3D* out,
                                     std::size_t max) -> std::uint32_t {
  if (max < k_builder_sickle_role_count) {
    return 0U;
  }
  out[0] = palette.metal * 0.92F;
  return k_builder_sickle_role_count;
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

auto civilian_pack_fill_role_colors(const HumanoidPalette& palette,
                                    QVector3D* out,
                                    std::size_t max) -> std::uint32_t {
  if (max < k_civilian_pack_role_count) {
    return 0U;
  }
  out[k_pack_bedroll_slot] = Render::GL::Humanoid::saturate_color(
      palette.cloth * 0.62F + QVector3D(0.20F, 0.17F, 0.12F));
  out[k_pack_strap_slot] = palette.leather_dark;
  out[k_pack_wicker_slot] = QVector3D(0.68F, 0.55F, 0.32F);
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

auto roman_civilian_pack_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = [] {
    const auto& bind = Render::Humanoid::humanoid_bind_body_frames();
    const AttachmentFrame& torso = bind.torso;
    const AttachmentFrame& waist = bind.waist;
    const TorsoLocalFrame local = make_torso_local_frame(QMatrix4x4{}, torso);

    float const tr = torso.radius;
    float const y_sh = 0.010F;
    float const y_w = local.point(waist.origin).y();

    RenderArchetypeBuilder builder{"roman_civilian_pack"};

    QVector3D const roll_l(-tr * 1.16F, y_sh + 0.075F, -tr * 2.05F);
    QVector3D const roll_r(tr * 1.16F, y_sh + 0.045F, -tr * 2.12F);
    builder.add_palette_mesh(get_unit_cylinder(),
                             cylinder_between(roll_l, roll_r, tr * 0.60F),
                             k_pack_bedroll_slot);
    builder.add_palette_mesh(
        get_unit_sphere(),
        local_scale_model(roll_l, QVector3D(tr * 0.18F, tr * 0.52F, tr * 0.52F)),
        k_pack_strap_slot);
    builder.add_palette_mesh(
        get_unit_sphere(),
        local_scale_model(roll_r, QVector3D(tr * 0.18F, tr * 0.52F, tr * 0.52F)),
        k_pack_strap_slot);

    for (int side = -1; side <= 1; side += 2) {
      float const sx = static_cast<float>(side);
      builder.add_palette_mesh(
          get_unit_cylinder(),
          cylinder_between(QVector3D(sx * tr * 0.36F, y_sh + 0.140F, -tr * 2.40F),
                           QVector3D(sx * tr * 0.36F, y_sh - 0.120F, -tr * 0.80F),
                           tr * 0.060F),
          k_pack_strap_slot);
    }
    builder.add_palette_mesh(
        get_unit_cylinder(),
        cylinder_between(QVector3D(-tr * 0.72F, y_sh - 0.030F, -tr * 0.60F),
                         QVector3D(tr * 0.60F, y_w + 0.070F, tr * 0.82F),
                         tr * 0.075F),
        k_pack_strap_slot);

    QVector3D const basket(tr * 1.06F, y_w - 0.085F, -tr * 0.30F);
    {
      QMatrix4x4 m;
      m.translate(basket);
      m.scale(tr * 0.40F, 0.145F, tr * 0.34F);
      builder.add_palette_mesh(
          get_unit_tapered_cylinder(0.78F, 1.02F, 10), m, k_pack_wicker_slot);
    }
    builder.add_palette_mesh(
        get_unit_cylinder(),
        cylinder_between(basket + QVector3D(-tr * 0.30F, 0.080F, 0.0F),
                         basket + QVector3D(tr * 0.30F, 0.080F, 0.0F),
                         tr * 0.055F),
        k_pack_strap_slot);

    return std::move(builder).build();
  }();
  return archetype;
}

auto roman_civilian_pack_make_static_attachment(std::uint16_t chest_bone_index,
                                                std::uint8_t base_role_byte)
    -> Render::Creature::StaticAttachmentSpec {
  const auto& bind_frames = Render::Humanoid::humanoid_bind_body_frames();
  const TorsoLocalFrame torso_local =
      make_torso_local_frame(QMatrix4x4{}, bind_frames.torso);
  auto spec = Render::Equipment::build_static_attachment({
      .archetype = &roman_civilian_pack_archetype(),
      .socket_bone_index = chest_bone_index,
      .unit_local_pose_at_bind = torso_local.world,
  });
  for (std::uint8_t i = 0; i < static_cast<std::uint8_t>(k_civilian_pack_role_count);
       ++i) {
    spec.palette_role_remap[i] = static_cast<std::uint8_t>(base_role_byte + i);
  }
  return spec;
}

auto roman_civilian_pack_contribution_attachments(std::uint8_t base_role)
    -> std::vector<Render::Creature::StaticAttachmentSpec> {
  return {roman_civilian_pack_make_static_attachment(
      static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Chest), base_role)};
}

auto roman_civilian_mantle_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = [] {
    const auto& bind = Render::Humanoid::humanoid_bind_body_frames();
    const AttachmentFrame& torso = bind.torso;
    const AttachmentFrame& waist = bind.waist;
    const TorsoLocalFrame local = make_torso_local_frame(QMatrix4x4{}, torso);

    float const tr = torso.radius * 1.22F;
    float const td = (torso.depth > 0.0F) ? torso.depth * 1.12F : torso.radius * 0.98F;
    float const y_sh = 0.038F;
    float const y_w = local.point(waist.origin).y();
    float const y_hem = y_w - 0.20F;

    RenderArchetypeBuilder builder{"roman_civilian_mantle"};

    static const auto shell = make_garment_shell({
        {y_hem, tr * 1.38F, td * 1.38F},
        {y_w, tr * 0.88F, td * 0.88F},
        {y_sh, tr * 1.04F, td * 1.04F},
    });
    builder.add_palette_mesh(shell.get(), QMatrix4x4{}, 0U);

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
    return build_generated_equipment_archetype("roman_civilian_cudgel", primitives);
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
        generated_sphere(QVector3D(0.065F, -0.07F, 0.03F), 0.024F, k_saw_leather_slot),
        generated_cylinder(QVector3D(0.02F, -0.06F, 0.03F),
                           QVector3D(0.03F, 0.20F, 0.03F),
                           0.013F,
                           k_saw_metal_slot),
        generated_cylinder(QVector3D(0.03F, 0.20F, 0.03F),
                           QVector3D(-0.09F, 0.12F, 0.03F),
                           0.018F,
                           k_saw_metal_dark_slot),
        generated_sphere(QVector3D(-0.02F, 0.16F, 0.03F), 0.016F, k_saw_metal_slot),
        generated_sphere(QVector3D(-0.045F, 0.145F, 0.03F), 0.014F, k_saw_metal_slot),
        generated_sphere(QVector3D(-0.07F, 0.13F, 0.03F), 0.012F, k_saw_metal_slot),
        generated_sphere(QVector3D(-0.09F, 0.115F, 0.03F), 0.010F, k_saw_metal_slot),
    }};
    return build_generated_equipment_archetype("roman_builder_saw", primitives);
  }();
  return archetype;
}

auto builder_sickle_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = [] {
    constexpr float k_arc_cx = 0.085F;
    constexpr float k_arc_cy = 0.045F;
    constexpr float k_arc_r = 0.088F;
    constexpr std::array<float, 6> k_angles{
        180.0F, 148.0F, 116.0F, 84.0F, 52.0F, 22.0F};
    std::vector<GeneratedEquipmentPrimitive> primitives;
    primitives.push_back(generated_cylinder(QVector3D(0.0F, -0.17F, 0.01F),
                                            QVector3D(0.0F, 0.03F, 0.01F),
                                            0.015F,
                                            k_builder_sickle_wood_slot));
    primitives.push_back(generated_sphere(
        QVector3D(0.0F, -0.175F, 0.01F), 0.019F, k_builder_sickle_wood_slot));
    for (std::size_t i = 0; i + 1 < k_angles.size(); ++i) {
      auto point = [&](float degrees) {
        float const rad = degrees * 3.14159265F / 180.0F;
        return QVector3D(k_arc_cx + k_arc_r * std::cos(rad),
                         k_arc_cy + k_arc_r * std::sin(rad),
                         0.01F);
      };
      float const taper = 0.011F - 0.0016F * static_cast<float>(i);
      primitives.push_back(generated_cylinder(point(k_angles[i]),
                                              point(k_angles[i + 1]),
                                              taper,
                                              k_builder_sickle_metal_slot));
    }
    return build_generated_equipment_archetype("roman_builder_sickle", primitives);
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
        generated_sphere(QVector3D(0.0F, -0.18F, 0.01F), 0.022F, k_chisel_wood_slot),
        generated_cylinder(QVector3D(0.0F, 0.06F, 0.01F),
                           QVector3D(0.0F, 0.20F, 0.01F),
                           0.011F,
                           k_chisel_metal_slot),
        generated_sphere(QVector3D(0.0F, 0.215F, 0.01F), 0.012F, k_chisel_metal_slot),
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

} // namespace Render::GL::Roman::builder_parts_detail
