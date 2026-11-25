#include "render/equipment/helmets/hair_bun.h"

#include <algorithm>
#include <array>
#include <utility>

#include "render/creature/humanoid/asset/humanoid_spec.h"
#include "render/equipment/attachment_builder.h"
#include "render/equipment/generated_equipment.h"
#include "render/equipment/helmets/headwear_mesh.h"
#include "render/equipment/helmets/helmet_alignment.h"

namespace Render::GL {

namespace {

enum HairPaletteSlot : std::uint8_t {
  k_hair_slot = 0U,
  k_tie_slot = 1U,
};

const QVector3D k_auburn{0.17F, 0.09F, 0.05F};
const QVector3D k_near_black{0.075F, 0.055F, 0.04F};

auto attach(const RenderArchetype& archetype,
            std::uint16_t socket_bone_index,
            const QMatrix4x4& bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec {
  constexpr float k_head_socket_radius = 0.16F;
  return Render::Equipment::build_static_attachment({
      .archetype = &archetype,
      .socket_bone_index = socket_bone_index,
      .uniform_scale = k_helmet_uniform_scale,
      .authored_local_offset = k_helmet_local_offset * k_helmet_uniform_scale,
      .bind_radius = k_head_socket_radius,
      .bind_socket_transform = bind_palette_socket_bone,
  });
}

} // namespace

auto hair_bun_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = [] {
    RenderArchetypeBuilder builder("hair_bun");
    builder.add_palette_mesh(Headwear::hair_cap(true), QMatrix4x4{}, k_hair_slot);
    add_generated_equipment_primitive(
        builder,
        generated_ellipsoid(QVector3D(0.0F, -0.28F, -1.42F),
                            QVector3D(0.49F, 0.45F, 0.28F),
                            k_hair_slot));
    builder.add_palette_mesh(Headwear::bun_coil(), QMatrix4x4{}, k_hair_slot);
    builder.add_palette_mesh(Headwear::bun_tie(), QMatrix4x4{}, k_tie_slot);
    return std::move(builder).build();
  }();
  return archetype;
}

auto hair_bun_fill_role_colors(const HumanoidPalette& palette,
                               QVector3D* out,
                               std::size_t max) -> std::uint32_t {
  if (max < k_hair_bun_role_count) {
    return 0;
  }
  out[0] = k_auburn;
  out[1] = palette.cloth;
  return k_hair_bun_role_count;
}

auto hair_bun_make_static_attachment(std::uint16_t socket_bone_index,
                                     std::uint8_t base_role_byte,
                                     const QMatrix4x4& bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec {
  auto spec = attach(hair_bun_archetype(), socket_bone_index, bind_palette_socket_bone);
  spec.palette_role_remap[k_hair_slot] = base_role_byte;
  spec.palette_role_remap[k_tie_slot] = static_cast<std::uint8_t>(base_role_byte + 1U);
  return spec;
}

auto hair_crop_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = [] {
    RenderArchetypeBuilder builder("hair_crop");
    builder.add_palette_mesh(Headwear::hair_cap(false), QMatrix4x4{}, k_hair_slot);
    return std::move(builder).build();
  }();
  return archetype;
}

auto hair_crop_fill_role_colors(const HumanoidPalette&,
                                QVector3D* out,
                                std::size_t max) -> std::uint32_t {
  if (max < k_hair_crop_role_count) {
    return 0;
  }
  out[0] = k_near_black;
  return k_hair_crop_role_count;
}

auto hair_crop_make_static_attachment(std::uint16_t socket_bone_index,
                                      std::uint8_t base_role_byte,
                                      const QMatrix4x4& bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec {
  auto spec =
      attach(hair_crop_archetype(), socket_bone_index, bind_palette_socket_bone);
  spec.palette_role_remap[k_hair_slot] = base_role_byte;
  return spec;
}

auto skull_face_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = [] {
    std::array<GeneratedEquipmentPrimitive, 15> const primitives{{
        generated_ellipsoid(QVector3D(-0.40F, 0.40F, 0.93F),
                            QVector3D(0.30F, 0.27F, 0.22F),
                            k_hair_slot),
        generated_ellipsoid(QVector3D(0.40F, 0.40F, 0.93F),
                            QVector3D(0.30F, 0.27F, 0.22F),
                            k_hair_slot),
        generated_ellipsoid(
            QVector3D(0.0F, 0.60F, 0.86F), QVector3D(0.82F, 0.09F, 0.16F), k_tie_slot),
        generated_ellipsoid(
            QVector3D(-0.62F, 0.12F, 0.86F), QVector3D(0.24F, 0.11F, 0.2F), k_tie_slot),
        generated_ellipsoid(
            QVector3D(0.62F, 0.12F, 0.86F), QVector3D(0.24F, 0.11F, 0.2F), k_tie_slot),
        generated_cone(QVector3D(0.0F, -0.04F, 1.02F),
                       QVector3D(0.0F, 0.22F, 1.02F),
                       0.11F,
                       k_hair_slot),
        generated_box(QVector3D(0.0F, -0.42F, 0.92F),
                      QVector3D(0.36F, 0.17F, 0.09F),
                      k_hair_slot),
        generated_box(QVector3D(-0.22F, -0.31F, 0.99F),
                      QVector3D(0.05F, 0.07F, 0.05F),
                      k_tie_slot),
        generated_box(QVector3D(-0.075F, -0.31F, 1.0F),
                      QVector3D(0.05F, 0.075F, 0.05F),
                      k_tie_slot),
        generated_box(QVector3D(0.075F, -0.31F, 1.0F),
                      QVector3D(0.05F, 0.075F, 0.05F),
                      k_tie_slot),
        generated_box(QVector3D(0.22F, -0.31F, 0.99F),
                      QVector3D(0.05F, 0.07F, 0.05F),
                      k_tie_slot),
        generated_ellipsoid(
            QVector3D(0.0F, -0.72F, 0.62F), QVector3D(0.62F, 0.15F, 0.5F), k_tie_slot),
        generated_box(QVector3D(-0.15F, -0.56F, 0.95F),
                      QVector3D(0.045F, 0.06F, 0.045F),
                      k_tie_slot),
        generated_box(QVector3D(0.0F, -0.56F, 0.96F),
                      QVector3D(0.045F, 0.065F, 0.045F),
                      k_tie_slot),
        generated_box(QVector3D(0.15F, -0.56F, 0.95F),
                      QVector3D(0.045F, 0.06F, 0.045F),
                      k_tie_slot),
    }};
    return build_generated_equipment_archetype("skull_face", primitives);
  }();
  return archetype;
}

auto skull_face_fill_role_colors(const HumanoidPalette&,
                                 QVector3D* out,
                                 std::size_t max) -> std::uint32_t {
  if (max < k_skull_face_role_count) {
    return 0;
  }
  out[0] = QVector3D(0.012F, 0.010F, 0.012F);
  out[1] = QVector3D(0.70F, 0.66F, 0.55F);
  return k_skull_face_role_count;
}

auto skull_face_make_static_attachment(std::uint16_t socket_bone_index,
                                       std::uint8_t base_role_byte,
                                       const QMatrix4x4& bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec {
  auto spec =
      attach(skull_face_archetype(), socket_bone_index, bind_palette_socket_bone);
  spec.palette_role_remap[k_hair_slot] = base_role_byte;
  spec.palette_role_remap[k_tie_slot] = static_cast<std::uint8_t>(base_role_byte + 1U);
  return spec;
}

auto palla_veil_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = [] {
    std::array<GeneratedEquipmentPrimitive, 3> const primitives{{
        generated_ellipsoid(
            QVector3D(0.0F, 0.15F, -0.3F), QVector3D(1.56F, 1.44F, 1.55F), 0U),
        generated_cone(
            QVector3D(0.0F, -3.0F, -0.85F), QVector3D(0.0F, 0.3F, -0.62F), 1.95F, 0U),
        generated_ellipsoid(
            QVector3D(0.0F, 0.55F, 0.92F), QVector3D(1.36F, 0.26F, 0.5F), 1U),
    }};
    return build_generated_equipment_archetype("palla_veil", primitives);
  }();
  return archetype;
}

auto palla_veil_fill_role_colors(const HumanoidPalette&,
                                 QVector3D* out,
                                 std::size_t max) -> std::uint32_t {
  if (max < k_palla_veil_role_count) {
    return 0;
  }
  out[0] = QVector3D(0.58F, 0.24F, 0.17F);
  out[1] = QVector3D(0.40F, 0.15F, 0.11F);
  return k_palla_veil_role_count;
}

auto palla_veil_make_static_attachment(std::uint16_t socket_bone_index,
                                       std::uint8_t base_role_byte,
                                       const QMatrix4x4& bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec {
  auto spec =
      attach(palla_veil_archetype(), socket_bone_index, bind_palette_socket_bone);
  spec.palette_role_remap[k_hair_slot] = base_role_byte;
  spec.palette_role_remap[k_tie_slot] = static_cast<std::uint8_t>(base_role_byte + 1U);
  return spec;
}

auto pileus_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = [] {
    std::array<GeneratedEquipmentPrimitive, 3> const primitives{{
        generated_ellipsoid(
            QVector3D(0.0F, 0.6F, -0.05F), QVector3D(1.46F, 0.95F, 1.46F), 0U),
        generated_cone(
            QVector3D(0.0F, 1.0F, -0.08F), QVector3D(0.0F, 2.15F, -0.24F), 1.12F, 0U),
        generated_cylinder(
            QVector3D(0.0F, 0.28F, -0.05F), QVector3D(0.0F, 0.54F, -0.05F), 1.5F, 1U),
    }};
    return build_generated_equipment_archetype("pileus", primitives);
  }();
  return archetype;
}

auto pileus_fill_role_colors(const HumanoidPalette&,
                             QVector3D* out,
                             std::size_t max) -> std::uint32_t {
  if (max < k_pileus_role_count) {
    return 0;
  }
  out[0] = QVector3D(0.43F, 0.30F, 0.19F);
  out[1] = QVector3D(0.30F, 0.20F, 0.12F);
  return k_pileus_role_count;
}

auto pileus_make_static_attachment(std::uint16_t socket_bone_index,
                                   std::uint8_t base_role_byte,
                                   const QMatrix4x4& bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec {
  auto spec = attach(pileus_archetype(), socket_bone_index, bind_palette_socket_bone);
  spec.palette_role_remap[k_hair_slot] = base_role_byte;
  spec.palette_role_remap[k_tie_slot] = static_cast<std::uint8_t>(base_role_byte + 1U);
  return spec;
}

auto turban_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = [] {
    RenderArchetypeBuilder builder("turban");
    add_generated_equipment_primitive(
        builder,
        generated_ellipsoid(
            QVector3D(0.0F, 0.37F, -0.05F), QVector3D(1.30F, 1.14F, 1.42F), 1U));
    builder.add_palette_mesh(Headwear::turban_wraps(), QMatrix4x4{}, 0U);
    builder.add_palette_mesh(Headwear::turban_tail(), QMatrix4x4{}, 0U);
    return std::move(builder).build();
  }();
  return archetype;
}

auto turban_fill_role_colors(const HumanoidPalette&,
                             QVector3D* out,
                             std::size_t max) -> std::uint32_t {
  if (max < k_turban_role_count) {
    return 0;
  }
  out[0] = QVector3D(0.86F, 0.83F, 0.74F);
  out[1] = QVector3D(0.78F, 0.74F, 0.65F);
  return k_turban_role_count;
}

auto turban_make_static_attachment(std::uint16_t socket_bone_index,
                                   std::uint8_t base_role_byte,
                                   const QMatrix4x4& bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec {
  auto spec = attach(turban_archetype(), socket_bone_index, bind_palette_socket_bone);
  spec.palette_role_remap[k_hair_slot] = base_role_byte;
  spec.palette_role_remap[k_tie_slot] = static_cast<std::uint8_t>(base_role_byte + 1U);
  return spec;
}

namespace {

enum WorkerHeadwearSlot : std::uint8_t {
  k_headwear_main_slot = 0U,
  k_headwear_band_slot = 1U,
  k_headwear_hair_slot = 2U,
};

auto worker_band_color(const HumanoidPalette& palette) -> QVector3D {
  QVector3D const cloth = palette.cloth;
  float const luma = 0.299F * cloth.x() + 0.587F * cloth.y() + 0.114F * cloth.z();
  QVector3D const vivid(std::max(luma + (cloth.x() - luma) * 1.5F, 0.0F),
                        std::max(luma + (cloth.y() - luma) * 1.5F, 0.0F),
                        std::max(luma + (cloth.z() - luma) * 1.5F, 0.0F));
  float const peak = std::max({vivid.x(), vivid.y(), vivid.z(), 1.0e-3F});
  float const lift = std::clamp(0.74F / peak, 1.0F, 1.6F);
  return {std::min(vivid.x() * lift, 1.0F),
          std::min(vivid.y() * lift, 1.0F),
          std::min(vivid.z() * lift, 1.0F)};
}

auto attach_worker_headwear(const RenderArchetype& archetype,
                            std::uint16_t socket_bone_index,
                            std::uint8_t base_role_byte,
                            const QMatrix4x4& bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec {
  auto spec = attach(archetype, socket_bone_index, bind_palette_socket_bone);
  spec.palette_role_remap[k_headwear_main_slot] = base_role_byte;
  spec.palette_role_remap[k_headwear_band_slot] =
      static_cast<std::uint8_t>(base_role_byte + 1U);
  spec.palette_role_remap[k_headwear_hair_slot] = Render::Humanoid::k_humanoid_hair_role;
  return spec;
}

} // namespace

auto straw_hat_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = [] {
    std::array<GeneratedEquipmentPrimitive, 7> const primitives{{
        generated_ellipsoid(QVector3D(0.0F, 0.10F, -0.06F),
                            QVector3D(2.36F, 0.15F, 2.36F),
                            k_headwear_main_slot),
        generated_ellipsoid(QVector3D(0.0F, 0.02F, -0.06F),
                            QVector3D(2.50F, 0.10F, 2.50F),
                            k_headwear_main_slot),
        generated_ellipsoid(QVector3D(0.0F, 0.34F, -0.06F),
                            QVector3D(1.38F, 1.16F, 1.46F),
                            k_headwear_main_slot),
        generated_ellipsoid(QVector3D(0.0F, 1.30F, -0.06F),
                            QVector3D(0.88F, 0.30F, 0.94F),
                            k_headwear_main_slot),
        generated_cylinder(QVector3D(0.0F, 0.18F, -0.06F),
                           QVector3D(0.0F, 0.46F, -0.06F),
                           1.43F,
                           k_headwear_band_slot),
        generated_ellipsoid(QVector3D(-1.30F, 0.28F, -0.52F),
                            QVector3D(0.22F, 0.20F, 0.30F),
                            k_headwear_band_slot),
        generated_ellipsoid(QVector3D(0.0F, -0.30F, -0.36F),
                            QVector3D(1.33F, 0.82F, 1.22F),
                            k_headwear_hair_slot),
    }};
    return build_generated_equipment_archetype("straw_hat", primitives);
  }();
  return archetype;
}

auto straw_hat_fill_role_colors(const HumanoidPalette& palette,
                                QVector3D* out,
                                std::size_t max) -> std::uint32_t {
  if (max < k_straw_hat_role_count) {
    return 0;
  }
  out[k_headwear_main_slot] = QVector3D(0.91F, 0.88F, 0.47F);
  out[k_headwear_band_slot] = worker_band_color(palette);
  return k_straw_hat_role_count;
}

auto straw_hat_make_static_attachment(std::uint16_t socket_bone_index,
                                      std::uint8_t base_role_byte,
                                      const QMatrix4x4& bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec {
  return attach_worker_headwear(straw_hat_archetype(),
                                socket_bone_index,
                                base_role_byte,
                                bind_palette_socket_bone);
}

auto craftsman_cap_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = [] {
    std::array<GeneratedEquipmentPrimitive, 7> const primitives{{
        generated_ellipsoid(QVector3D(0.0F, 0.46F, -0.10F),
                            QVector3D(1.36F, 1.12F, 1.50F),
                            k_headwear_main_slot),
        generated_ellipsoid(QVector3D(0.0F, 1.18F, -0.34F),
                            QVector3D(0.80F, 0.46F, 0.92F),
                            k_headwear_main_slot),
        generated_ellipsoid(QVector3D(0.0F, 0.18F, -0.06F),
                            QVector3D(1.50F, 0.36F, 1.62F),
                            k_headwear_band_slot),
        generated_ellipsoid(QVector3D(0.0F, 0.48F, -0.08F),
                            QVector3D(1.42F, 0.22F, 1.55F),
                            k_headwear_main_slot),
        generated_ellipsoid(QVector3D(0.30F, 0.12F, -1.60F),
                            QVector3D(0.42F, 0.34F, 0.30F),
                            k_headwear_band_slot),
        generated_cylinder(QVector3D(0.30F, 0.06F, -1.64F),
                           QVector3D(0.44F, -1.10F, -1.48F),
                           0.20F,
                           k_headwear_band_slot),
        generated_ellipsoid(QVector3D(0.0F, -0.34F, -0.40F),
                            QVector3D(1.32F, 0.74F, 1.18F),
                            k_headwear_hair_slot),
    }};
    return build_generated_equipment_archetype("craftsman_cap", primitives);
  }();
  return archetype;
}

auto craftsman_cap_fill_role_colors(const HumanoidPalette& palette,
                                    QVector3D* out,
                                    std::size_t max) -> std::uint32_t {
  if (max < k_craftsman_cap_role_count) {
    return 0;
  }
  out[k_headwear_main_slot] = QVector3D(0.88F, 0.84F, 0.73F);
  out[k_headwear_band_slot] = worker_band_color(palette);
  return k_craftsman_cap_role_count;
}

auto craftsman_cap_make_static_attachment(std::uint16_t socket_bone_index,
                                          std::uint8_t base_role_byte,
                                          const QMatrix4x4& bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec {
  return attach_worker_headwear(craftsman_cap_archetype(),
                                socket_bone_index,
                                base_role_byte,
                                bind_palette_socket_bone);
}

} // namespace Render::GL
