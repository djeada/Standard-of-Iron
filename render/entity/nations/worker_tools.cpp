#include "render/entity/nations/worker_tools.h"

#include <QMatrix4x4>

#include <array>
#include <vector>

#include "render/creature/humanoid/asset/bind_skeleton.h"
#include "render/creature/humanoid/asset/humanoid_spec.h"
#include "render/creature/humanoid/schema/skeleton_schema.h"
#include "render/equipment/armor/torso_local_archetype_utils.h"
#include "render/equipment/attachment_builder.h"
#include "render/equipment/generated_equipment.h"

namespace Render::GL::WorkerTools {

namespace {

enum ToolSlot : std::uint8_t {
  k_wood_slot = 0U,
  k_iron_slot = 1U,
  k_binding_slot = 2U,
};

constexpr QVector3D k_tool_iron{0.41F, 0.41F, 0.42F};

struct ToolShape {
  float grip_height;
};

constexpr auto shape_of(Kind kind) -> ToolShape {
  switch (kind) {
  case Kind::FellingAxe:
    return {0.20F};
  case Kind::QuarryPick:
    return {0.15F};
  case Kind::Dolabra:
    return {0.30F};
  case Kind::LongMallet:
    return {0.30F};
  }
  return {0.20F};
}

auto build_felling_axe() -> RenderArchetype {
  std::array<GeneratedEquipmentPrimitive, 6> const primitives{{
      generated_cylinder(QVector3D(0.0F, 0.0F, 0.0F),
                         QVector3D(0.0F, 0.60F, 0.0F),
                         0.017F,
                         k_wood_slot),
      generated_sphere(QVector3D(0.0F, 0.0F, 0.0F), 0.023F, k_wood_slot),
      generated_cylinder(QVector3D(0.0F, 0.18F, 0.0F),
                         QVector3D(0.0F, 0.23F, 0.0F),
                         0.020F,
                         k_binding_slot),
      generated_box(QVector3D(0.004F, 0.53F, 0.0F),
                    QVector3D(0.060F, 0.084F, 0.046F),
                    k_iron_slot),
      generated_box(QVector3D(0.050F, 0.53F, 0.0F),
                    QVector3D(0.070F, 0.074F, 0.030F),
                    k_iron_slot),
      generated_ellipsoid(QVector3D(0.104F, 0.522F, 0.0F),
                          QVector3D(0.060F, 0.108F, 0.013F),
                          k_iron_slot),
  }};
  return build_generated_equipment_archetype("worker_felling_axe", primitives);
}

auto build_quarry_pick() -> RenderArchetype {
  std::array<GeneratedEquipmentPrimitive, 6> const primitives{{
      generated_cylinder(QVector3D(0.0F, 0.0F, 0.0F),
                         QVector3D(0.0F, 0.40F, 0.0F),
                         0.016F,
                         k_wood_slot),
      generated_sphere(QVector3D(0.0F, 0.0F, 0.0F), 0.021F, k_wood_slot),
      generated_sphere(QVector3D(0.0F, 0.37F, 0.0F), 0.036F, k_iron_slot),
      generated_cone(QVector3D(0.012F, 0.37F, 0.0F),
                     QVector3D(0.185F, 0.340F, 0.0F),
                     0.027F,
                     k_iron_slot),
      generated_cylinder(QVector3D(-0.010F, 0.37F, 0.0F),
                         QVector3D(-0.085F, 0.37F, 0.0F),
                         0.027F,
                         k_iron_slot),
      generated_sphere(QVector3D(-0.085F, 0.37F, 0.0F), 0.030F, k_iron_slot),
  }};
  return build_generated_equipment_archetype("worker_quarry_pick", primitives);
}

auto build_dolabra() -> RenderArchetype {
  std::array<GeneratedEquipmentPrimitive, 7> const primitives{{
      generated_cylinder(QVector3D(0.0F, 0.0F, 0.0F),
                         QVector3D(0.0F, 0.84F, 0.0F),
                         0.019F,
                         k_wood_slot),
      generated_sphere(QVector3D(0.0F, 0.0F, 0.0F), 0.025F, k_wood_slot),
      generated_box(QVector3D(0.0F, 0.78F, 0.0F),
                    QVector3D(0.062F, 0.090F, 0.052F),
                    k_iron_slot),
      generated_box(QVector3D(0.052F, 0.78F, 0.0F),
                    QVector3D(0.076F, 0.078F, 0.032F),
                    k_iron_slot),
      generated_ellipsoid(QVector3D(0.112F, 0.772F, 0.0F),
                          QVector3D(0.064F, 0.116F, 0.014F),
                          k_iron_slot),
      generated_cone(QVector3D(-0.018F, 0.78F, 0.0F),
                     QVector3D(-0.230F, 0.730F, 0.0F),
                     0.030F,
                     k_iron_slot),
      generated_cylinder(QVector3D(0.0F, 0.30F, 0.0F),
                         QVector3D(0.0F, 0.36F, 0.0F),
                         0.022F,
                         k_binding_slot),
  }};
  return build_generated_equipment_archetype("worker_dolabra", primitives);
}

auto build_long_mallet() -> RenderArchetype {
  std::array<GeneratedEquipmentPrimitive, 6> const primitives{{
      generated_cylinder(QVector3D(0.0F, 0.0F, 0.0F),
                         QVector3D(0.0F, 0.76F, 0.0F),
                         0.020F,
                         k_wood_slot),
      generated_sphere(QVector3D(0.0F, 0.0F, 0.0F), 0.026F, k_wood_slot),
      generated_cylinder(QVector3D(-0.125F, 0.74F, 0.0F),
                         QVector3D(0.125F, 0.74F, 0.0F),
                         0.066F,
                         k_wood_slot),
      generated_cylinder(QVector3D(-0.132F, 0.74F, 0.0F),
                         QVector3D(-0.094F, 0.74F, 0.0F),
                         0.071F,
                         k_binding_slot),
      generated_cylinder(QVector3D(0.094F, 0.74F, 0.0F),
                         QVector3D(0.132F, 0.74F, 0.0F),
                         0.071F,
                         k_binding_slot),
      generated_cylinder(QVector3D(0.0F, 0.30F, 0.0F),
                         QVector3D(0.0F, 0.36F, 0.0F),
                         0.023F,
                         k_binding_slot),
  }};
  return build_generated_equipment_archetype("worker_long_mallet", primitives);
}

void remap_tool_roles(Render::Creature::StaticAttachmentSpec& spec,
                      std::uint8_t iron_role_byte) {
  spec.palette_role_remap[k_wood_slot] = Render::Humanoid::k_humanoid_wood_role;
  spec.palette_role_remap[k_iron_slot] = iron_role_byte;
  spec.palette_role_remap[k_binding_slot] =
      Render::Humanoid::k_humanoid_leather_dark_role;
}

} // namespace

auto archetype(Kind kind) -> const RenderArchetype& {
  static const RenderArchetype felling_axe = build_felling_axe();
  static const RenderArchetype quarry_pick = build_quarry_pick();
  static const RenderArchetype dolabra = build_dolabra();
  static const RenderArchetype long_mallet = build_long_mallet();
  switch (kind) {
  case Kind::FellingAxe:
    return felling_axe;
  case Kind::QuarryPick:
    return quarry_pick;
  case Kind::Dolabra:
    return dolabra;
  case Kind::LongMallet:
    return long_mallet;
  }
  return felling_axe;
}

auto hand_attachment(Kind kind, std::uint8_t iron_role_byte)
    -> Render::Creature::StaticAttachmentSpec {
  constexpr auto k_socket = Render::Humanoid::HumanoidSocket::GripR;
  constexpr auto k_bone = Render::Humanoid::HumanoidBone::HandR;
  QMatrix4x4 mesh_from_socket;
  mesh_from_socket.translate(0.0F, -shape_of(kind).grip_height, 0.02F);
  auto spec = Render::Equipment::build_socket_static_attachment({
      .archetype = &archetype(kind),
      .socket_bone_index = static_cast<std::uint16_t>(k_bone),
      .bind_bone_transform =
          Render::Humanoid::humanoid_bind_palette()[static_cast<std::size_t>(k_bone)],
      .bind_socket_transform = Render::Humanoid::bind_socket_transform(k_socket),
      .mesh_from_socket = mesh_from_socket,
  });
  remap_tool_roles(spec, iron_role_byte);
  return spec;
}

auto back_attachment(Kind kind, std::uint8_t iron_role_byte)
    -> Render::Creature::StaticAttachmentSpec {
  const auto& bind = Render::Humanoid::humanoid_bind_body_frames();
  const TorsoLocalFrame local = make_torso_local_frame(QMatrix4x4{}, bind.torso);
  float const waist_y = local.point(bind.waist.origin).y();
  float const back_z = -(bind.torso.depth + 0.040F);

  QVector3D const butt(-0.10F, waist_y + 0.05F, back_z);
  QVector3D const along = QVector3D(0.46F, 0.74F, 0.0F).normalized();
  QVector3D const edge(along.y(), -along.x(), 0.0F);
  QMatrix4x4 const pose =
      local.world * Render::Equipment::compose_basis_unit_local(
                        butt, edge, along, QVector3D(0.0F, 0.0F, 1.0F));
  auto spec = Render::Equipment::build_static_attachment({
      .archetype = &archetype(kind),
      .socket_bone_index =
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Chest),
      .unit_local_pose_at_bind = pose,
  });
  remap_tool_roles(spec, iron_role_byte);
  return spec;
}

auto iron_role_colors(const void*,
                      QVector3D* out,
                      std::uint32_t base_count,
                      std::size_t max_count) -> std::uint32_t {
  if (out == nullptr || max_count < base_count + k_iron_role_count) {
    return base_count;
  }
  out[base_count] = k_tool_iron;
  return base_count + k_iron_role_count;
}

auto with_attachment(std::string_view debug_name,
                     Render::Creature::ArchetypeId base_archetype_id,
                     const Render::Creature::StaticAttachmentSpec& spec,
                     bool adds_iron_role) -> Render::Creature::ArchetypeId {
  auto& registry = Render::Creature::ArchetypeRegistry::instance();
  const auto* base_desc = registry.get(base_archetype_id);
  if (base_desc == nullptr ||
      base_desc->bake_attachment_count >=
          Render::Creature::ArchetypeDescriptor::k_max_bake_attachments) {
    return base_archetype_id;
  }

  auto desc = *base_desc;
  desc.debug_name = debug_name;
  desc.bake_attachments[desc.bake_attachment_count++] = spec;
  if (adds_iron_role) {
    desc.role_count = static_cast<std::uint8_t>(desc.role_count + k_iron_role_count);
    desc.append_extra_role_colors_fn(&iron_role_colors);
  }
  return registry.register_archetype(desc);
}

} // namespace Render::GL::WorkerTools
