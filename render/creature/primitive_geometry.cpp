#include "primitive_geometry.h"

#include <algorithm>

#include "../geom/parts.h"
#include "../geom/transforms.h"
#include "../gl/primitives.h"

namespace Render::Creature {

namespace {

auto box_model(const QMatrix4x4& bone,
               const QVector3D& local_offset,
               const QVector3D& half_extents) noexcept -> QMatrix4x4 {
  QMatrix4x4 m = bone;
  QVector3D const world_origin = bone_world_offset(bone, local_offset);
  m.setColumn(3, QVector4D(world_origin, 1.0F));

  QMatrix4x4 scale;
  scale.scale(
      half_extents.x() * 2.0F, half_extents.y() * 2.0F, half_extents.z() * 2.0F);
  return m * scale;
}

auto mesh_model(const QMatrix4x4& bone,
                const QVector3D& local_offset,
                const QVector3D& half_extents) noexcept -> QMatrix4x4 {
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

auto oriented_span_model(const PrimitiveInstance& prim,
                         const QMatrix4x4& anchor_bone,
                         const QVector3D& head_world,
                         const QVector3D& tail_world) noexcept -> QMatrix4x4 {
  QVector3D const right_ref = anchor_bone.column(0).toVector3D();
  float const r_right = prim.params.radius;
  float const r_forward =
      (prim.params.depth_radius > 0.0F) ? prim.params.depth_radius : prim.params.radius;
  return Render::Geom::oriented_cylinder(
      head_world, tail_world, right_ref, r_right, r_forward);
}

} // namespace

auto primitive_needs_tail(PrimitiveShape shape) noexcept -> bool {
  switch (shape) {
  case PrimitiveShape::Cylinder:
  case PrimitiveShape::Capsule:
  case PrimitiveShape::Cone:
  case PrimitiveShape::OrientedCylinder:
  case PrimitiveShape::TaperedCylinder:
    return true;
  default:
    return false;
  }
}

auto bone_world_offset(const QMatrix4x4& bone,
                       const QVector3D& local_offset) noexcept -> QVector3D {
  if (local_offset.isNull()) {
    return bone.column(3).toVector3D();
  }
  QVector3D const origin = bone.column(3).toVector3D();
  QVector3D const x = bone.column(0).toVector3D();
  QVector3D const y = bone.column(1).toVector3D();
  QVector3D const z = bone.column(2).toVector3D();
  return origin + x * local_offset.x() + y * local_offset.y() + z * local_offset.z();
}

auto primitive_unit_mesh(const PrimitiveInstance& prim) noexcept -> Render::GL::Mesh* {
  if (prim.custom_mesh != nullptr) {
    return prim.custom_mesh;
  }

  switch (prim.shape) {
  case PrimitiveShape::Sphere:
  case PrimitiveShape::OrientedSphere:
    return Render::GL::get_unit_sphere();
  case PrimitiveShape::Cylinder:
  case PrimitiveShape::OrientedCylinder:
    return Render::GL::get_unit_cylinder();
  case PrimitiveShape::Capsule:
    return Render::GL::get_unit_capsule();
  case PrimitiveShape::Cone:
    return Render::GL::get_unit_cone();
  case PrimitiveShape::Box:
    return Render::GL::get_unit_cube();
  case PrimitiveShape::TaperedCylinder: {
    float const anchor = std::max(prim.params.radius, 1.0e-4F);
    float const tail =
        prim.params.tail_radius > 0.0F ? prim.params.tail_radius : anchor;
    return Render::GL::get_unit_tapered_cylinder(1.0F, tail / anchor);
  }
  case PrimitiveShape::Mesh:
  case PrimitiveShape::BoneSpanMesh:
  case PrimitiveShape::None:
  default:
    return nullptr;
  }
}

auto primitive_unit_model(const PrimitiveInstance& prim,
                          const QMatrix4x4& anchor_bone,
                          const QMatrix4x4& tail_bone,
                          QMatrix4x4& out_model) noexcept -> bool {
  QVector3D const head_world = bone_world_offset(anchor_bone, prim.params.head_offset);
  QVector3D const tail_world = bone_world_offset(tail_bone, prim.params.tail_offset);

  switch (prim.shape) {
  case PrimitiveShape::Sphere:
    out_model = Render::Geom::sphere_at(head_world, prim.params.radius);
    return true;

  case PrimitiveShape::Cylinder:
  case PrimitiveShape::Capsule:
  case PrimitiveShape::Cone:
  case PrimitiveShape::TaperedCylinder:
    out_model =
        Render::Geom::cylinder_between(head_world, tail_world, prim.params.radius);
    return true;

  case PrimitiveShape::Box:
    out_model =
        box_model(anchor_bone, prim.params.head_offset, prim.params.half_extents);
    return true;

  case PrimitiveShape::OrientedCylinder:
    out_model = oriented_span_model(prim, anchor_bone, head_world, tail_world);
    return true;

  case PrimitiveShape::OrientedSphere: {
    QVector3D const x = anchor_bone.column(0).toVector3D();
    QVector3D const y = anchor_bone.column(1).toVector3D();
    QVector3D const z = anchor_bone.column(2).toVector3D();
    QMatrix4x4 m;
    m.setColumn(0, QVector4D(x * (prim.params.half_extents.x() * 2.0F), 0.0F));
    m.setColumn(1, QVector4D(y * (prim.params.half_extents.y() * 2.0F), 0.0F));
    m.setColumn(2, QVector4D(z * (prim.params.half_extents.z() * 2.0F), 0.0F));
    m.setColumn(3, QVector4D(head_world, 1.0F));
    out_model = m;
    return true;
  }

  case PrimitiveShape::Mesh:
    if (prim.custom_mesh == nullptr) {
      return false;
    }
    out_model =
        mesh_model(anchor_bone, prim.params.head_offset, prim.params.half_extents);
    return true;

  case PrimitiveShape::BoneSpanMesh:
    if (prim.custom_mesh == nullptr) {
      return false;
    }
    out_model = oriented_span_model(prim, anchor_bone, head_world, tail_world);
    return true;

  case PrimitiveShape::None:
  default:
    return false;
  }
}

} // namespace Render::Creature
