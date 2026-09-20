#include "farm_worker_props.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <array>
#include <cstdint>
#include <vector>

#include "render/equipment/attachment_builder.h"
#include "render/equipment/generated_equipment.h"
#include "render/equipment/humanoid_attachment_archetype.h"
#include "render/equipment/humanoid_equipment_archetype.h"
#include "render/equipment/render_archetype_registry.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/humanoid/asset/bind_skeleton.h"
#include "render/humanoid/asset/humanoid_spec.h"
#include "render/humanoid/schema/skeleton_schema.h"

namespace Render::GL {
namespace {

enum StrawSlot : std::uint8_t {
  k_straw_slot = 0U,
  k_straw_shade_slot = 1U,
};
constexpr std::uint8_t k_straw_role_count = 2U;

constexpr float k_head_silhouette = 0.168F;

auto sun_hat_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = []() {
    const float brim = k_head_silhouette * 1.62F;
    const std::array<GeneratedEquipmentPrimitive, 3> primitives{{

        generated_cylinder(QVector3D(0.0F, 0.055F, 0.0F),
                           QVector3D(0.0F, 0.073F, 0.0F),
                           brim,
                           k_straw_shade_slot),
        generated_cylinder(QVector3D(0.0F, 0.070F, 0.0F),
                           QVector3D(0.0F, 0.092F, 0.0F),
                           k_head_silhouette * 1.04F,
                           k_straw_slot),
        generated_cone(QVector3D(0.0F, 0.088F, 0.0F),
                       QVector3D(0.0F, 0.176F, 0.0F),
                       k_head_silhouette * 0.92F,
                       k_straw_slot),
    }};
    return build_generated_equipment_archetype("farm_sun_hat", primitives);
  }();
  return archetype;
}

auto tilted_sun_hat_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = []() {
    const float brim = k_head_silhouette * 1.62F;
    const QVector3D face(0.0F, -0.012F, 0.118F);
    const QVector3D axis(0.0F, 0.050F, 0.086F);
    const std::array<GeneratedEquipmentPrimitive, 3> primitives{{
        generated_cylinder(face, face + (axis * 0.18F), brim, k_straw_shade_slot),
        generated_cylinder(face + (axis * 0.14F),
                           face + (axis * 0.40F),
                           k_head_silhouette * 1.04F,
                           k_straw_slot),
        generated_cone(face + (axis * 0.38F),
                       face + (axis * 1.30F),
                       k_head_silhouette * 0.92F,
                       k_straw_slot),
    }};
    return build_generated_equipment_archetype("farm_sun_hat_tilted", primitives);
  }();
  return archetype;
}

auto wheat_sheaf_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = []() {
    const std::array<GeneratedEquipmentPrimitive, 5> primitives{{
        generated_cone(QVector3D(0.0F, -0.19F, 0.0F),
                       QVector3D(0.0F, 0.02F, 0.0F),
                       0.086F,
                       k_straw_slot),
        generated_cone(QVector3D(0.0F, 0.21F, 0.0F),
                       QVector3D(0.0F, 0.02F, 0.0F),
                       0.094F,
                       k_straw_slot),
        generated_cylinder(QVector3D(0.0F, -0.01F, 0.0F),
                           QVector3D(0.0F, 0.03F, 0.0F),
                           0.052F,
                           k_straw_shade_slot),
        generated_cylinder(QVector3D(-0.055F, 0.155F, 0.018F),
                           QVector3D(0.048F, 0.205F, -0.012F),
                           0.006F,
                           k_straw_shade_slot),
        generated_cylinder(QVector3D(0.041F, 0.148F, -0.026F),
                           QVector3D(-0.030F, 0.212F, 0.021F),
                           0.006F,
                           k_straw_shade_slot),
    }};
    return build_generated_equipment_archetype("farm_wheat_sheaf", primitives);
  }();
  return archetype;
}

auto head_attachment(const RenderArchetype& archetype,
                     std::uint8_t base_role) -> Render::Creature::StaticAttachmentSpec {
  const auto& bind = Render::Humanoid::humanoid_bind_body_frames();
  auto spec = Render::Equipment::build_static_attachment({
      .archetype = &archetype,
      .socket_bone_index =
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Head),
      .unit_local_pose_at_bind =
          make_humanoid_attachment_transform_scaled(QMatrix4x4{},
                                                    bind.head,
                                                    QVector3D(0.0F, 0.0F, 0.0F),
                                                    QVector3D(1.0F, 1.0F, 1.0F)),
  });
  spec.palette_role_remap[k_straw_slot] = base_role;
  spec.palette_role_remap[k_straw_shade_slot] =
      static_cast<std::uint8_t>(base_role + 1U);
  return spec;
}

auto sun_hat_attachments(std::uint8_t base_role)
    -> std::vector<Render::Creature::StaticAttachmentSpec> {
  return {head_attachment(sun_hat_archetype(), base_role)};
}

auto tilted_sun_hat_attachments(std::uint8_t base_role)
    -> std::vector<Render::Creature::StaticAttachmentSpec> {
  return {head_attachment(tilted_sun_hat_archetype(), base_role)};
}

auto wheat_sheaf_attachments(std::uint8_t base_role)
    -> std::vector<Render::Creature::StaticAttachmentSpec> {
  constexpr auto k_socket = Render::Humanoid::HumanoidSocket::GripL;
  constexpr auto k_bone = Render::Humanoid::HumanoidBone::HandL;
  auto spec = Render::Equipment::build_socket_static_attachment({
      .archetype = &wheat_sheaf_archetype(),
      .socket_bone_index = static_cast<std::uint16_t>(k_bone),
      .bind_bone_transform =
          Render::Humanoid::humanoid_bind_palette()[static_cast<std::size_t>(k_bone)],
      .bind_socket_transform = Render::Humanoid::bind_socket_transform(k_socket),
      .mesh_from_socket = QMatrix4x4{},
  });
  spec.palette_role_remap[k_straw_slot] = base_role;
  spec.palette_role_remap[k_straw_shade_slot] =
      static_cast<std::uint8_t>(base_role + 1U);
  return {spec};
}

auto straw_role_colors(const void* variant_void,
                       QVector3D* out,
                       std::uint32_t base_count,
                       std::size_t max_count) -> std::uint32_t {
  if (variant_void == nullptr || max_count < base_count + k_straw_role_count) {
    return base_count;
  }

  const auto& variant = *static_cast<const HumanoidVariant*>(variant_void);
  const float bleach = 0.88F + (variant.pattern_seed * 0.22F);
  out[base_count] = QVector3D(0.79F, 0.66F, 0.36F) * bleach;
  out[base_count + 1U] = QVector3D(0.61F, 0.48F, 0.24F) * bleach;
  return base_count + k_straw_role_count;
}

auto register_prop(EquipmentCategory category,
                   const char* id,
                   HumanoidEquipmentContribution::BuildAttachmentsFn build)
    -> EquipmentHandle {
  auto& registry = EquipmentRegistry::instance();
  registry.register_equipment_id(category, id);
  const auto handle = registry.resolve_handle(category, id);
  if (handle != k_invalid_equipment_handle) {
    register_humanoid_equipment_contribution(handle,
                                             {.build_attachments = build,
                                              .append_role_colors = &straw_role_colors,
                                              .role_count = k_straw_role_count});
  }
  return handle;
}

} // namespace

void register_farm_worker_prop_archetypes() {
  auto& registry = RenderArchetypeRegistry::instance();
  registry.register_archetype("farm_sun_hat", [] { (void)sun_hat_archetype(); });
  registry.register_archetype("farm_sun_hat_tilted",
                              [] { (void)tilted_sun_hat_archetype(); });
  registry.register_archetype("farm_wheat_sheaf",
                              [] { (void)wheat_sheaf_archetype(); });

  registry.register_archetype("farm_worker_props", [] { (void)farm_worker_props(); });
}

auto farm_worker_props() -> const FarmWorkerProps& {
  static const FarmWorkerProps props = []() {
    FarmWorkerProps result{};
    result.sun_hat =
        register_prop(EquipmentCategory::Helmet, "farm_sun_hat", &sun_hat_attachments);
    result.tilted_sun_hat = register_prop(
        EquipmentCategory::Helmet, "farm_sun_hat_tilted", &tilted_sun_hat_attachments);
    result.wheat_sheaf = register_prop(
        EquipmentCategory::Weapon, "farm_wheat_sheaf", &wheat_sheaf_attachments);
    return result;
  }();
  return props;
}

} // namespace Render::GL
