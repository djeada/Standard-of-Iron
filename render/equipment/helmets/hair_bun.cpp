#include "hair_bun.h"

#include <array>

#include "helmet_alignment.h"
#include "render/equipment/attachment_builder.h"
#include "render/equipment/generated_equipment.h"

namespace Render::GL {

namespace {

enum HairPaletteSlot : std::uint8_t {
  k_hair_slot = 0U,
  k_tie_slot = 1U,
};

const QVector3D k_auburn{0.17F, 0.09F, 0.05F};
const QVector3D k_near_black{0.075F, 0.055F, 0.04F};

auto crown(float lift) -> GeneratedEquipmentPrimitive {
  return generated_ellipsoid(QVector3D(0.0F, 0.62F + lift, -0.12F),
                             QVector3D(1.24F, 1.02F, 1.25F),
                             k_hair_slot);
}

auto nape() -> GeneratedEquipmentPrimitive {
  return generated_ellipsoid(
      QVector3D(0.0F, 0.15F, -0.55F), QVector3D(1.2F, 0.95F, 0.85F), k_hair_slot);
}

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
    std::array<GeneratedEquipmentPrimitive, 5> const primitives{{
        generated_ellipsoid(
            QVector3D(0.0F, 0.18F, -0.14F), QVector3D(1.5F, 1.38F, 1.48F), k_hair_slot),
        generated_ellipsoid(QVector3D(0.0F, -0.55F, -0.75F),
                            QVector3D(1.36F, 0.8F, 0.85F),
                            k_hair_slot),
        generated_sphere(QVector3D(0.0F, -0.42F, -1.52F), 0.6F, k_hair_slot),
        generated_sphere(QVector3D(0.0F, -0.08F, -1.58F), 0.4F, k_hair_slot),
        generated_cylinder(QVector3D(0.0F, 0.56F, -0.13F),
                           QVector3D(0.0F, 0.7F, -0.15F),
                           1.53F,
                           k_tie_slot),
    }};
    return build_generated_equipment_archetype("hair_bun", primitives);
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
    std::array<GeneratedEquipmentPrimitive, 2> const primitives{{
        generated_ellipsoid(
            QVector3D(0.0F, 0.3F, -0.16F), QVector3D(1.49F, 1.25F, 1.47F), k_hair_slot),
        generated_ellipsoid(
            QVector3D(0.0F, -0.3F, -0.7F), QVector3D(1.3F, 0.7F, 0.8F), k_hair_slot),
    }};
    return build_generated_equipment_archetype("hair_crop", primitives);
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
    std::array<GeneratedEquipmentPrimitive, 4> const primitives{{
        generated_cylinder(
            QVector3D(0.0F, 0.22F, -0.05F), QVector3D(0.0F, 0.82F, -0.08F), 1.54F, 0U),
        generated_cylinder(
            QVector3D(0.0F, 0.72F, -0.1F), QVector3D(0.0F, 1.22F, -0.16F), 1.46F, 1U),
        generated_ellipsoid(
            QVector3D(0.0F, 1.18F, -0.12F), QVector3D(1.4F, 0.62F, 1.4F), 0U),
        generated_cone(
            QVector3D(0.0F, 0.65F, -1.38F), QVector3D(0.0F, -0.75F, -1.58F), 0.48F, 1U),
    }};
    return build_generated_equipment_archetype("turban", primitives);
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
  out[1] = QVector3D(0.70F, 0.66F, 0.58F);
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

} // namespace Render::GL
