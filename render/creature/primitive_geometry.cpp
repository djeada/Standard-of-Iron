#include "primitive_geometry.h"

#include <algorithm>
#include <array>

#include "render/geom/parts.h"
#include "render/geom/transforms.h"
#include "render/gl/primitives.h"

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

struct ShapeTraits {
  bool spans_two_bones;
  Render::GL::Mesh* (*unit_mesh)();
  Render::GL::Mesh* (*minimal_unit_mesh)();
};

constexpr std::size_t k_shape_count =
    static_cast<std::size_t>(PrimitiveShape::TaperedCylinder) + 1U;

auto shape_traits(PrimitiveShape shape) noexcept -> const ShapeTraits& {
  static const auto sphere = +[]() {
    return Render::GL::get_unit_sphere();
  };
  static const auto minimal_sphere = +[]() {
    return Render::GL::get_unit_sphere(k_minimal_latitude_segments,
                                       k_minimal_radial_segments);
  };
  static const auto cylinder = +[]() {
    return Render::GL::get_unit_cylinder();
  };
  static const auto minimal_cylinder = +[]() {
    return Render::GL::get_unit_cylinder(k_minimal_radial_segments);
  };
  static const std::array<ShapeTraits, k_shape_count> k_traits{{
      {false, nullptr, nullptr},
      {false, sphere, minimal_sphere},
      {true, cylinder, minimal_cylinder},
      {true,
       +[]() { return Render::GL::get_unit_capsule(); },
       +[]() {
         return Render::GL::get_unit_capsule(k_minimal_radial_segments);
       }},
      {true,
       +[]() { return Render::GL::get_unit_cone(); },
       +[]() {
         return Render::GL::get_unit_cone(k_minimal_radial_segments);
       }},
      {false,
       +[]() { return Render::GL::get_unit_cube(); },
       +[]() {
         return Render::GL::get_unit_cube();
       }},
      {false, nullptr, nullptr},
      {false, nullptr, nullptr},
      {true, cylinder, minimal_cylinder},
      {false, sphere, minimal_sphere},
      {true, nullptr, nullptr},
  }};
  auto const index = static_cast<std::size_t>(shape);
  return index < k_traits.size() ? k_traits[index] : k_traits[0];
}

} // namespace

auto primitive_needs_tail(PrimitiveShape shape) noexcept -> bool {
  return shape_traits(shape).spans_two_bones;
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

auto primitive_unit_mesh(const PrimitiveInstance& prim,
                         CreatureLOD lod) noexcept -> Render::GL::Mesh* {
  if (prim.custom_mesh != nullptr) {
    return prim.custom_mesh;
  }

  const bool minimal = lod == CreatureLOD::Minimal;

  if (prim.shape == PrimitiveShape::TaperedCylinder) {
    float const anchor = std::max(prim.params.radius, 1.0e-4F);
    float const tail =
        prim.params.tail_radius > 0.0F ? prim.params.tail_radius : anchor;
    return Render::GL::get_unit_tapered_cylinder(
        1.0F,
        tail / anchor,
        minimal ? k_minimal_radial_segments : Render::GL::k_default_radial_segments);
  }

  auto const& traits = shape_traits(prim.shape);
  auto const getter = minimal ? traits.minimal_unit_mesh : traits.unit_mesh;
  return getter != nullptr ? getter() : nullptr;
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
