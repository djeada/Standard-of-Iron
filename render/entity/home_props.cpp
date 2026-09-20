#include "home_props.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <array>
#include <cstdint>
#include <vector>

#include "render/equipment/attachment_builder.h"
#include "render/equipment/generated_equipment.h"
#include "render/equipment/humanoid_equipment_archetype.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/humanoid/asset/bind_skeleton.h"
#include "render/humanoid/asset/humanoid_spec.h"
#include "render/humanoid/schema/skeleton_schema.h"

namespace Render::GL {
namespace {

enum BowlSlot : std::uint8_t {
  k_clay_slot = 0U,
  k_broth_slot = 1U,
};
constexpr std::uint8_t k_bowl_role_count = 2U;

auto soup_bowl_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = []() {
    const std::array<GeneratedEquipmentPrimitive, 3> primitives{{
        generated_cylinder(QVector3D(0.0F, 0.020F, 0.045F),
                           QVector3D(0.0F, 0.075F, 0.045F),
                           0.086F,
                           k_clay_slot),
        // The broth sits just under the rim, so it reads as full on the way
        // out and is conspicuously gone afterwards.
        generated_cylinder(QVector3D(0.0F, 0.064F, 0.045F),
                           QVector3D(0.0F, 0.070F, 0.045F),
                           0.074F,
                           k_broth_slot),
        generated_cylinder(QVector3D(0.0F, 0.012F, 0.045F),
                           QVector3D(0.0F, 0.024F, 0.045F),
                           0.042F,
                           k_clay_slot),
    }};
    return build_generated_equipment_archetype("home_soup_bowl", primitives);
  }();
  return archetype;
}

auto soup_bowl_attachments(std::uint8_t base_role)
    -> std::vector<Render::Creature::StaticAttachmentSpec> {
  constexpr auto k_socket = Render::Humanoid::HumanoidSocket::GripR;
  constexpr auto k_bone = Render::Humanoid::HumanoidBone::HandR;
  auto spec = Render::Equipment::build_socket_static_attachment({
      .archetype = &soup_bowl_archetype(),
      .socket_bone_index = static_cast<std::uint16_t>(k_bone),
      .bind_bone_transform =
          Render::Humanoid::humanoid_bind_palette()[static_cast<std::size_t>(k_bone)],
      .bind_socket_transform = Render::Humanoid::bind_socket_transform(k_socket),
      .mesh_from_socket = QMatrix4x4{},
  });
  spec.palette_role_remap[k_clay_slot] = base_role;
  spec.palette_role_remap[k_broth_slot] = static_cast<std::uint8_t>(base_role + 1U);
  return {spec};
}

auto bowl_role_colors(const void* variant_void,
                      QVector3D* out,
                      std::uint32_t base_count,
                      std::size_t max_count) -> std::uint32_t {
  if (variant_void == nullptr || max_count < base_count + k_bowl_role_count) {
    return base_count;
  }
  out[base_count] = QVector3D(0.55F, 0.35F, 0.24F);      // Red-brown kitchenware.
  out[base_count + 1U] = QVector3D(0.72F, 0.55F, 0.24F); // Lentil broth.
  return base_count + k_bowl_role_count;
}

} // namespace

auto home_props() -> const HomeProps& {
  static const HomeProps props = []() {
    HomeProps result{};
    auto& registry = EquipmentRegistry::instance();
    registry.register_equipment_id(EquipmentCategory::Weapon, "home_soup_bowl");
    result.soup_bowl =
        registry.resolve_handle(EquipmentCategory::Weapon, "home_soup_bowl");
    if (result.soup_bowl != k_invalid_equipment_handle) {
      register_humanoid_equipment_contribution(
          result.soup_bowl,
          {.build_attachments = &soup_bowl_attachments,
           .append_role_colors = &bowl_role_colors,
           .role_count = k_bowl_role_count});
    }
    return result;
  }();
  return props;
}

auto soup_puddle_archetype() -> const RenderArchetype& {
  // Discs, not flattened spheres: flat ellipsoids shade black in this shader.
  // Several small overlapping lobes read as a splash; one big disc reads as a
  // painted circle.
  static const RenderArchetype archetype = []() {
    RenderArchetypeBuilder builder{"home_soup_puddle"};
    const QVector3D broth(0.42F, 0.30F, 0.15F);
    const QVector3D thin = broth * 0.82F;
    struct Lobe {
      QVector3D at;
      float radius;
      QVector3D color;
    };
    const std::array<Lobe, 5> lobes{{
        {QVector3D(0.0F, 0.0F, 0.0F), 0.165F, broth},
        {QVector3D(0.13F, 0.0F, 0.07F), 0.095F, broth},
        {QVector3D(-0.10F, 0.0F, -0.08F), 0.078F, thin},
        {QVector3D(0.19F, 0.0F, -0.11F), 0.046F, thin},
        {QVector3D(-0.17F, 0.0F, 0.13F), 0.038F, thin},
    }};
    for (const auto& lobe : lobes) {
      builder.add_cylinder(lobe.at + QVector3D(0.0F, 0.004F, 0.0F),
                           lobe.at + QVector3D(0.0F, 0.011F, 0.0F),
                           lobe.radius,
                           lobe.color);
    }
    return std::move(builder).build();
  }();
  return archetype;
}

} // namespace Render::GL
