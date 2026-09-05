#include "builder_renderer.h"

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
#include "builder_style.h"
#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/systems/nation_id.h"
#include "math/math_utils.h"
#include "render/creature/archetype_registry.h"
#include "render/creature/pipeline/unit_visual_spec.h"
#include "render/entity/nations/builder_tool_palette.h"
#include "render/entity/nations/equipment_loadout_catalog.h"
#include "render/entity/registry.h"
#include "render/entity/renderer_constants.h"
#include "render/equipment/armor/arm_guards_renderer.h"
#include "render/equipment/armor/garment_shell.h"
#include "render/equipment/armor/tool_belt_renderer.h"
#include "render/equipment/armor/torso_local_archetype_utils.h"
#include "render/equipment/armor/work_apron_renderer.h"
#include "render/equipment/attachment_builder.h"
#include "render/equipment/generated_equipment.h"
#include "render/equipment/helmets/roman_light_helmet.h"
#include "render/equipment/humanoid_equipment_archetype.h"
#include "render/equipment/render_archetype_registry.h"
#include "render/geom/transforms.h"
#include "render/gl/backend.h"
#include "render/gl/primitives.h"
#include "render/gl/render_constants.h"
#include "render/gl/shader.h"
#include "render/humanoid/asset/bind_skeleton.h"
#include "render/humanoid/asset/humanoid_spec.h"
#include "render/humanoid/runtime/humanoid_math.h"
#include "render/humanoid/runtime/humanoid_renderer.h"
#include "render/humanoid/runtime/skeleton_evaluator.h"
#include "render/humanoid/runtime/style_palette.h"
#include "render/humanoid/schema/humanoid_proportion_profiles.h"
#include "render/palette.h"
#include "render/scene_renderer.h"
#include "render/submitter.h"

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
constexpr std::uint32_t k_builder_sickle_role_count = 2;
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
  out[0] = palette.wood;
  out[1] = palette.metal;
  out[2] = palette.metal * 0.72F;
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

auto builder_sickle_fill_role_colors(const HumanoidPalette& palette,
                                     QVector3D* out,
                                     std::size_t max) -> std::uint32_t {
  if (max < k_builder_sickle_role_count) {
    return 0U;
  }
  out[0] = palette.wood;
  out[1] = palette.metal * 0.92F;
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
    if (loadout.work_apron_handle != k_invalid_equipment_handle) {
      register_humanoid_equipment_contribution(
          loadout.work_apron_handle,
          {.build_attachments = &roman_civilian_pack_contribution_attachments,
           .append_role_colors = &civilian_pack_extra_role_colors,
           .role_count = static_cast<std::uint8_t>(k_civilian_pack_role_count)});
    }
    return true;
  }();
  (void)registered;
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
  static constexpr std::array<std::uint8_t, 4> k_slots{
      k_saw_wood_slot, k_saw_metal_slot, k_saw_metal_dark_slot, k_saw_leather_slot};
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

auto roman_builder_sickle_unit_archetype() -> Render::Creature::ArchetypeId {
  static constexpr std::array<std::uint8_t, 2> k_slots{k_builder_sickle_wood_slot,
                                                       k_builder_sickle_metal_slot};
  static const auto k_tool_spec = builder_tool_make_static_attachment(
      builder_sickle_archetype(),
      Render::Creature::ArchetypeRegistry::instance()
          .get(roman_builder_idle_archetype())
          ->role_count,
      k_slots);
  static const auto k_archetype = register_builder_tool_variant_archetype(
      "troops/roman/builder/construction_sickle",
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
        return base_count + builder_sickle_fill_role_colors(
                                v.palette, out + base_count, max_count - base_count);
      },
      static_cast<std::uint8_t>(k_builder_sickle_role_count));
  return k_archetype;
}

auto roman_builder_variant_table() -> const Render::Creature::ArchetypeVariantTable& {
  static const Render::Creature::ArchetypeVariantTable k_table = []() {
    Render::Creature::ArchetypeVariantTable t{};
    t.variant_trigger_pose = Render::Creature::PoseIntent::Construct;
    t.variant_stride = 5;

    t.archetype_for_pose[static_cast<std::size_t>(
        Render::Creature::PoseIntent::AttackMelee)] =
        roman_builder_hammer_unit_archetype();
    t.state_for_pose[static_cast<std::size_t>(
        Render::Creature::PoseIntent::AttackMelee)] =
        Render::Creature::AnimationStateId::AttackSword;
    t.variant_is_seed_based = true;
    t.seed_variant_limit = 4;

    t.archetype_for_variant[0] = roman_builder_hammer_unit_archetype();
    t.state_for_variant[0] = Render::Creature::AnimationStateId::AttackSword;

    t.archetype_for_variant[1] = roman_builder_saw_unit_archetype();
    t.state_for_variant[1] = Render::Creature::AnimationStateId::AttackSword;

    t.archetype_for_variant[2] = roman_builder_chisel_unit_archetype();
    t.state_for_variant[2] = Render::Creature::AnimationStateId::AttackSword;

    t.archetype_for_variant[3] = roman_builder_chisel_unit_archetype();
    t.state_for_variant[3] = Render::Creature::AnimationStateId::AttackSword;

    t.archetype_for_variant[4] = roman_builder_sickle_unit_archetype();
    t.state_for_variant[4] = Render::Creature::AnimationStateId::AttackSword;
    return t;
  }();
  return k_table;
}

auto roman_civilian_idle_archetype() -> Render::Creature::ArchetypeId {
  static const auto archetype = []() {
    ensure_roman_civilian_equipment_contributions_registered();
    const auto loadout =
        Render::GL::Nation::resolve_equipment_loadout("troops/roman/civilian");
    const std::array<EquipmentHandle, 3> handles{
        loadout.armor_handle, loadout.cloak_handle, loadout.work_apron_handle};
    return resolve_humanoid_equipment_archetype(
        "troops/roman/civilian",
        Render::Creature::ArchetypeRegistry::k_humanoid_base,
        handles);
  }();
  return archetype;
}

auto roman_civilian_cudgel_unit_archetype() -> Render::Creature::ArchetypeId {
  static constexpr std::array<std::uint8_t, 2> k_slots{k_civilian_cudgel_wood_slot,
                                                       k_civilian_cudgel_band_slot};
  static const auto k_tool_spec = builder_tool_make_static_attachment(
      civilian_cudgel_archetype(),
      Render::Creature::ArchetypeRegistry::instance()
          .get(roman_civilian_idle_archetype())
          ->role_count,
      k_slots);
  static const auto k_archetype = register_builder_tool_variant_archetype(
      "troops/roman/civilian/cudgel",
      roman_civilian_idle_archetype(),
      k_tool_spec,
      +[](const void* variant_void,
          QVector3D* out,
          std::uint32_t base_count,
          std::size_t max_count) -> std::uint32_t {
        if (variant_void == nullptr || max_count <= base_count) {
          return base_count;
        }
        const auto& v = *static_cast<const HumanoidVariant*>(variant_void);
        return base_count + civilian_cudgel_fill_role_colors(
                                v.palette, out + base_count, max_count - base_count);
      },
      static_cast<std::uint8_t>(k_civilian_cudgel_role_count));
  return k_archetype;
}

auto roman_civilian_variant_table() -> const Render::Creature::ArchetypeVariantTable& {
  static const Render::Creature::ArchetypeVariantTable k_table = []() {
    Render::Creature::ArchetypeVariantTable t{};
    t.archetype_for_pose[static_cast<std::size_t>(
        Render::Creature::PoseIntent::AttackMelee)] =
        roman_civilian_cudgel_unit_archetype();
    t.state_for_pose[static_cast<std::size_t>(
        Render::Creature::PoseIntent::AttackMelee)] =
        Render::Creature::AnimationStateId::AttackSword;
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
  BuilderRenderer()
      : HumanoidRendererBase(make_visual_spec()) {}

  friend void register_builder_renderer(Render::GL::EntityRendererRegistry& registry);

  static auto make_visual_spec() -> Render::Creature::Pipeline::UnitVisualSpec {
    using namespace Render::Creature::Pipeline;
    UnitVisualSpec s{};
    s.kind = CreatureKind::Humanoid;
    s.debug_name = "troops/roman/builder";
    s.scaling = k_builder_profile.as_pipeline_scaling();
    s.archetype_id = roman_builder_idle_archetype();
    s.animation_manifest.variant_table = &roman_builder_variant_table();
    return s;
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
  CivilianRenderer()
      : HumanoidRendererBase(make_visual_spec()) {}

  static auto make_visual_spec() -> Render::Creature::Pipeline::UnitVisualSpec {
    using namespace Render::Creature::Pipeline;
    UnitVisualSpec s{};
    s.kind = CreatureKind::Humanoid;
    s.debug_name = "troops/roman/civilian";
    s.scaling = k_civilian_profile.as_pipeline_scaling();
    s.archetype_id = roman_civilian_idle_archetype();
    s.animation_manifest.variant_table = &roman_civilian_variant_table();
    return s;
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
  register_humanoid_renderer(
      registry, "troops/roman/builder", std::make_shared<BuilderRenderer const>());

  auto& ar = Render::GL::RenderArchetypeRegistry::instance();
  ar.register_archetype("roman_builder_work_tunic",
                        [] { (void)builder_work_tunic_archetype(); });
  ar.register_archetype("roman_civilian_mantle",
                        [] { (void)roman_civilian_mantle_archetype(); });
  ar.register_archetype("roman_civilian_pack",
                        [] { (void)roman_civilian_pack_archetype(); });
  ar.register_archetype("roman_builder_hammer",
                        [] { (void)builder_hammer_archetype(); });
  ar.register_archetype("roman_builder_saw", [] { (void)builder_saw_archetype(); });
  ar.register_archetype("roman_builder_chisel",
                        [] { (void)builder_chisel_archetype(); });
  ar.register_archetype("roman_builder_sickle",
                        [] { (void)builder_sickle_archetype(); });
}

void register_civilian_renderer(Render::GL::EntityRendererRegistry& registry) {
  ensure_builder_styles_registered();
  register_humanoid_renderer(
      registry, "troops/roman/civilian", std::make_shared<CivilianRenderer const>());

  auto& ar = Render::GL::RenderArchetypeRegistry::instance();
  ar.register_archetype("roman_civilian_cudgel",
                        [] { (void)civilian_cudgel_archetype(); });
}

} // namespace Render::GL::Roman
