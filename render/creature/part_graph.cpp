#include "part_graph.h"

#include "../geom/parts.h"
#include "../geom/transforms.h"
#include "../gl/primitives.h"
#include "../submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <cassert>

namespace Render::Creature {

namespace {

auto resolved_mesh(Render::Creature::PrimitiveShape shape,
                   Render::GL::Mesh *custom_mesh) noexcept
    -> Render::GL::Mesh * {
  if (custom_mesh != nullptr) {
    return custom_mesh;
  }

  switch (shape) {
  case Render::Creature::PrimitiveShape::Sphere:
  case Render::Creature::PrimitiveShape::OrientedSphere:
    return Render::GL::get_unit_sphere();
  case Render::Creature::PrimitiveShape::Cylinder:
  case Render::Creature::PrimitiveShape::OrientedCylinder:
    return Render::GL::get_unit_cylinder();
  case Render::Creature::PrimitiveShape::Capsule:
    return Render::GL::get_unit_capsule();
  case Render::Creature::PrimitiveShape::Cone:
    return Render::GL::get_unit_cone();
  case Render::Creature::PrimitiveShape::Box:
    return Render::GL::get_unit_cube();
  case Render::Creature::PrimitiveShape::Mesh:
    return custom_mesh;
  case Render::Creature::PrimitiveShape::None:
  default:
    return nullptr;
  }
}

auto bone_world_offset(const QMatrix4x4 &bone,
                       const QVector3D &local_offset) noexcept -> QVector3D {
  if (local_offset.isNull()) {
    return bone.column(3).toVector3D();
  }
  QVector3D const origin = bone.column(3).toVector3D();
  QVector3D const x = bone.column(0).toVector3D();
  QVector3D const y = bone.column(1).toVector3D();
  QVector3D const z = bone.column(2).toVector3D();
  return origin + x * local_offset.x() + y * local_offset.y() +
         z * local_offset.z();
}

auto box_model(const QMatrix4x4 &bone, const QVector3D &local_offset,
               const QVector3D &half_extents) noexcept -> QMatrix4x4 {
  QMatrix4x4 m = bone;
  QVector3D const world_origin = bone_world_offset(bone, local_offset);
  m.setColumn(3, QVector4D(world_origin, 1.0F));

  QMatrix4x4 scale;
  scale.scale(half_extents.x() * 2.0F, half_extents.y() * 2.0F,
              half_extents.z() * 2.0F);
  return m * scale;
}

auto mesh_model(const QMatrix4x4 &bone, const QVector3D &local_offset,
                const QVector3D &half_extents) noexcept -> QMatrix4x4 {
  QMatrix4x4 m = bone;
  QVector3D const world_origin = bone_world_offset(bone, local_offset);
  m.setColumn(3, QVector4D(world_origin, 1.0F));
  if (half_extents == QVector3D(1.0F, 1.0F, 1.0F)) {
    return m;
  }
  QMatrix4x4 scale;
  scale.scale(half_extents.x(), half_extents.y(), half_extents.z());
  return m * scale;
}

} // namespace

auto submit_part_graph(
    const SkeletonTopology &topology, const PartGraph &graph,
    std::span<const QMatrix4x4> palette, CreatureLOD lod,
    const QMatrix4x4 &world_from_unit, Render::GL::ISubmitter &out,
    std::span<const QVector3D> role_colors) -> PartSubmissionStats {
  PartSubmissionStats stats;
  std::uint8_t const lod_filter = lod_bit(lod);
  std::size_t const bone_count = topology.bones.size();

  for (PrimitiveInstance const &prim : graph.primitives) {
    if ((prim.lod_mask & lod_filter) == 0U) {
      ++stats.skipped_lod;
      continue;
    }
    if (prim.shape == PrimitiveShape::None) {
      ++stats.skipped_invalid;
      continue;
    }

    BoneIndex const anchor = prim.params.anchor_bone;
    BoneIndex const tail = prim.params.tail_bone;
    if (anchor == kInvalidBone || anchor >= bone_count ||
        anchor >= palette.size()) {
      ++stats.skipped_invalid;
      continue;
    }
    bool const needs_tail = (prim.shape == PrimitiveShape::Cylinder ||
                             prim.shape == PrimitiveShape::Capsule ||
                             prim.shape == PrimitiveShape::Cone ||
                             prim.shape == PrimitiveShape::OrientedCylinder);
    if (needs_tail && (tail == kInvalidBone || tail >= bone_count ||
                       tail >= palette.size())) {
      ++stats.skipped_invalid;
      continue;
    }

    QMatrix4x4 const &anchor_m = palette[anchor];
    QVector3D const head_world =
        bone_world_offset(anchor_m, prim.params.head_offset);

    QMatrix4x4 unit_model;
    Render::GL::Mesh *mesh_ptr = nullptr;

    switch (prim.shape) {
    case PrimitiveShape::Sphere:
      mesh_ptr = resolved_mesh(prim.shape, prim.custom_mesh);
      unit_model = Render::Geom::sphere_at(head_world, prim.params.radius);
      break;

    case PrimitiveShape::Cylinder: {
      mesh_ptr = resolved_mesh(prim.shape, prim.custom_mesh);
      QMatrix4x4 const &tail_m = palette[tail];
      QVector3D const tail_world =
          bone_world_offset(tail_m, prim.params.tail_offset);
      unit_model = Render::Geom::cylinder_between(head_world, tail_world,
                                                  prim.params.radius);
      break;
    }

    case PrimitiveShape::Capsule: {
      mesh_ptr = resolved_mesh(prim.shape, prim.custom_mesh);
      QMatrix4x4 const &tail_m = palette[tail];
      QVector3D const tail_world =
          bone_world_offset(tail_m, prim.params.tail_offset);
      unit_model = Render::Geom::capsule_between(head_world, tail_world,
                                                 prim.params.radius);
      break;
    }

    case PrimitiveShape::Cone: {
      mesh_ptr = resolved_mesh(prim.shape, prim.custom_mesh);
      QMatrix4x4 const &tail_m = palette[tail];
      QVector3D const tail_world =
          bone_world_offset(tail_m, prim.params.tail_offset);
      unit_model = Render::Geom::cone_from_to(head_world, tail_world,
                                              prim.params.radius);
      break;
    }

    case PrimitiveShape::Box:
      mesh_ptr = resolved_mesh(prim.shape, prim.custom_mesh);
      unit_model = box_model(anchor_m, prim.params.head_offset,
                             prim.params.half_extents);
      break;

    case PrimitiveShape::OrientedCylinder: {
      mesh_ptr = resolved_mesh(prim.shape, prim.custom_mesh);
      QMatrix4x4 const &tail_m = palette[tail];
      QVector3D const tail_world =
          bone_world_offset(tail_m, prim.params.tail_offset);
      QVector3D const right_ref = anchor_m.column(0).toVector3D();
      float const r_right = prim.params.radius;
      float const r_forward = (prim.params.depth_radius > 0.0F)
                                  ? prim.params.depth_radius
                                  : prim.params.radius;
      unit_model = Render::Geom::oriented_cylinder(
          head_world, tail_world, right_ref, r_right, r_forward);
      break;
    }

    case PrimitiveShape::OrientedSphere: {
      mesh_ptr = resolved_mesh(prim.shape, prim.custom_mesh);
      QVector3D const x = anchor_m.column(0).toVector3D();
      QVector3D const y = anchor_m.column(1).toVector3D();
      QVector3D const z = anchor_m.column(2).toVector3D();
      QVector3D const sx = x * (prim.params.half_extents.x() * 2.0F);
      QVector3D const sy = y * (prim.params.half_extents.y() * 2.0F);
      QVector3D const sz = z * (prim.params.half_extents.z() * 2.0F);
      unit_model.setColumn(0, QVector4D(sx, 0.0F));
      unit_model.setColumn(1, QVector4D(sy, 0.0F));
      unit_model.setColumn(2, QVector4D(sz, 0.0F));
      unit_model.setColumn(3, QVector4D(head_world, 1.0F));
      break;
    }

    case PrimitiveShape::Mesh:
      mesh_ptr = prim.custom_mesh;
      if (mesh_ptr == nullptr) {
        ++stats.skipped_invalid;
        continue;
      }
      unit_model = mesh_model(anchor_m, prim.params.head_offset,
                              prim.params.half_extents);
      break;

    case PrimitiveShape::None:
    default:
      ++stats.skipped_invalid;
      continue;
    }

    if (mesh_ptr == nullptr) {
      ++stats.skipped_invalid;
      continue;
    }

    QMatrix4x4 const model = world_from_unit * unit_model;
    QVector3D const color =
        (prim.color_role > 0 && prim.color_role <= role_colors.size())
            ? role_colors[prim.color_role - 1]
            : prim.color;
    out.part(mesh_ptr, prim.material, model, color, nullptr, prim.alpha,
             prim.material_id);
    ++stats.submitted;
  }
  return stats;
}

auto validate_part_graph(const SkeletonTopology &topology,
                         const PartGraph &graph) noexcept -> bool {
  std::size_t const n = topology.bones.size();
  for (PrimitiveInstance const &p : graph.primitives) {
    if (p.shape == PrimitiveShape::None) {
      return false;
    }
    if (p.params.anchor_bone == kInvalidBone || p.params.anchor_bone >= n) {
      return false;
    }
    bool const needs_tail = (p.shape == PrimitiveShape::Cylinder ||
                             p.shape == PrimitiveShape::Capsule ||
                             p.shape == PrimitiveShape::Cone ||
                             p.shape == PrimitiveShape::OrientedCylinder);
    if (needs_tail) {
      if (p.params.tail_bone == kInvalidBone || p.params.tail_bone >= n) {
        return false;
      }
    }
    if (p.shape == PrimitiveShape::Mesh && p.custom_mesh == nullptr) {
      return false;
    }
  }
  return true;
}

} // namespace Render::Creature
