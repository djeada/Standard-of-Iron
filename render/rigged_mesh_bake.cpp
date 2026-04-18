#include "rigged_mesh_bake.h"

#include "creature/part_graph.h"
#include "creature/skeleton.h"
#include "geom/parts.h"
#include "geom/transforms.h"
#include "gl/mesh.h"
#include "gl/primitives.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>
#include <algorithm>
#include <cmath>

namespace Render::Creature {

namespace {

using Render::GL::Mesh;
using Render::GL::RiggedVertex;
using Render::GL::Vertex;

// Resolve `local_offset` expressed in `bone`'s own basis to world
// space. At identity bind pose this is just `local_offset` itself, but
// keeping the general form lets 15.5b drop in real bind matrices
// without touching this code.
auto bone_world_offset(const QMatrix4x4 &bone,
                       const QVector3D &local_offset) -> QVector3D {
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
               const QVector3D &half_extents) -> QMatrix4x4 {
  QMatrix4x4 m = bone;
  QVector3D const world_origin = bone_world_offset(bone, local_offset);
  m.setColumn(3, QVector4D(world_origin, 1.0F));
  QMatrix4x4 scale;
  scale.scale(half_extents.x() * 2.0F, half_extents.y() * 2.0F,
              half_extents.z() * 2.0F);
  return m * scale;
}

auto mesh_model(const QMatrix4x4 &bone, const QVector3D &local_offset,
                const QVector3D &half_extents) -> QMatrix4x4 {
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

// True if the primitive is two-bone-blended in the rigged vertex
// format. Cone is NOT in this list per stage-15.5a spec: even though
// it references a tail bone for its world placement, every vertex is
// tagged to the anchor bone with weight 1.
auto is_two_bone_blend(PrimitiveShape shape) -> bool {
  switch (shape) {
  case PrimitiveShape::Cylinder:
  case PrimitiveShape::Capsule:
  case PrimitiveShape::OrientedCylinder:
    return true;
  default:
    return false;
  }
}

// Fetches the shared CPU unit mesh for a given shape. Returns
// `prim.custom_mesh` for PrimitiveShape::Mesh. Returns nullptr for
// shapes with no direct unit-mesh source (None).
auto resolve_unit_mesh(const PrimitiveInstance &prim) -> Mesh * {
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
  case PrimitiveShape::Mesh:
    return prim.custom_mesh;
  case PrimitiveShape::None:
  default:
    return nullptr;
  }
}

// Computes the unit-space model matrix for a primitive under the
// supplied bind pose. Mirrors `submit_part_graph`'s per-shape math;
// kept local to avoid taking a dependency on that walker's ISubmitter
// contract. Returns false (out_model untouched) if the primitive can't
// be baked (invalid bone refs, missing custom mesh, unsupported shape).
auto compute_unit_model(const PrimitiveInstance &prim,
                        std::span<const BoneWorldMatrix> bind_pose,
                        QMatrix4x4 &out_model) -> bool {
  BoneIndex const anchor = prim.params.anchor_bone;
  BoneIndex const tail = prim.params.tail_bone;
  if (anchor == kInvalidBone || anchor >= bind_pose.size()) {
    return false;
  }
  bool const needs_tail = (prim.shape == PrimitiveShape::Cylinder ||
                           prim.shape == PrimitiveShape::Capsule ||
                           prim.shape == PrimitiveShape::Cone ||
                           prim.shape == PrimitiveShape::OrientedCylinder);
  if (needs_tail && (tail == kInvalidBone || tail >= bind_pose.size())) {
    return false;
  }

  QMatrix4x4 const &anchor_m = bind_pose[anchor];
  QVector3D const head_world =
      bone_world_offset(anchor_m, prim.params.head_offset);

  switch (prim.shape) {
  case PrimitiveShape::Sphere:
    out_model = Render::Geom::sphere_at(head_world, prim.params.radius);
    return true;

  case PrimitiveShape::Cylinder: {
    QVector3D const tail_world =
        bone_world_offset(bind_pose[tail], prim.params.tail_offset);
    out_model = Render::Geom::cylinder_between(head_world, tail_world,
                                               prim.params.radius);
    return true;
  }

  case PrimitiveShape::Capsule: {
    QVector3D const tail_world =
        bone_world_offset(bind_pose[tail], prim.params.tail_offset);
    out_model = Render::Geom::capsule_between(head_world, tail_world,
                                              prim.params.radius);
    return true;
  }

  case PrimitiveShape::Cone: {
    QVector3D const tail_world =
        bone_world_offset(bind_pose[tail], prim.params.tail_offset);
    out_model = Render::Geom::cone_from_to(head_world, tail_world,
                                           prim.params.radius);
    return true;
  }

  case PrimitiveShape::Box:
    out_model = box_model(anchor_m, prim.params.head_offset,
                          prim.params.half_extents);
    return true;

  case PrimitiveShape::OrientedCylinder: {
    QVector3D const tail_world =
        bone_world_offset(bind_pose[tail], prim.params.tail_offset);
    QVector3D const right_ref = anchor_m.column(0).toVector3D();
    float const r_right = prim.params.radius;
    float const r_forward = (prim.params.depth_radius > 0.0F)
                                ? prim.params.depth_radius
                                : prim.params.radius;
    out_model = Render::Geom::oriented_cylinder(head_world, tail_world,
                                                right_ref, r_right,
                                                r_forward);
    return true;
  }

  case PrimitiveShape::OrientedSphere: {
    QVector3D const x = anchor_m.column(0).toVector3D();
    QVector3D const y = anchor_m.column(1).toVector3D();
    QVector3D const z = anchor_m.column(2).toVector3D();
    QVector3D const sx = x * (prim.params.half_extents.x() * 2.0F);
    QVector3D const sy = y * (prim.params.half_extents.y() * 2.0F);
    QVector3D const sz = z * (prim.params.half_extents.z() * 2.0F);
    QMatrix4x4 m;
    m.setColumn(0, QVector4D(sx, 0.0F));
    m.setColumn(1, QVector4D(sy, 0.0F));
    m.setColumn(2, QVector4D(sz, 0.0F));
    m.setColumn(3, QVector4D(head_world, 1.0F));
    out_model = m;
    return true;
  }

  case PrimitiveShape::Mesh:
    if (prim.custom_mesh == nullptr) {
      return false;
    }
    out_model = mesh_model(anchor_m, prim.params.head_offset,
                           prim.params.half_extents);
    return true;

  case PrimitiveShape::None:
  default:
    return false;
  }
}

// Transform a direction by the 3x3 of `m`, then normalise. Approximate
// for non-uniform scale (should be inverse-transpose), but good enough
// for identity-pose baking and far cheaper; 15.5b can revisit if any
// shader actually samples these normals.
auto transform_normal(const QMatrix4x4 &m, const QVector3D &n) -> QVector3D {
  QVector3D const mapped = m.mapVector(n);
  float const len_sq =
      mapped.x() * mapped.x() + mapped.y() * mapped.y() + mapped.z() * mapped.z();
  if (len_sq <= 1e-20F) {
    return QVector3D{0.0F, 1.0F, 0.0F};
  }
  float const inv_len = 1.0F / std::sqrt(len_sq);
  return QVector3D{mapped.x() * inv_len, mapped.y() * inv_len,
                   mapped.z() * inv_len};
}

void append_primitive_vertices(const PrimitiveInstance &prim,
                               const Mesh &unit_mesh,
                               const QMatrix4x4 &unit_model,
                               BakedRiggedMeshCpu &out) {
  auto const &src_verts = unit_mesh.get_vertices();
  auto const &src_idx = unit_mesh.get_indices();
  if (src_verts.empty() || src_idx.empty()) {
    return;
  }

  auto const base_vertex = static_cast<std::uint32_t>(out.vertices.size());
  auto const anchor = static_cast<std::uint8_t>(prim.params.anchor_bone);
  bool const two_bone = is_two_bone_blend(prim.shape);
  auto const tail =
      two_bone ? static_cast<std::uint8_t>(prim.params.tail_bone) : anchor;

  out.vertices.reserve(out.vertices.size() + src_verts.size());
  for (Vertex const &v : src_verts) {
    QVector3D const local_pos{v.position[0], v.position[1], v.position[2]};
    QVector3D const local_norm{v.normal[0], v.normal[1], v.normal[2]};
    QVector3D const world_pos = unit_model.map(local_pos);
    QVector3D const world_norm = transform_normal(unit_model, local_norm);

    RiggedVertex rv;
    rv.position_bone_local = {world_pos.x(), world_pos.y(), world_pos.z()};
    rv.normal_bone_local = {world_norm.x(), world_norm.y(), world_norm.z()};
    rv.tex_coord = v.tex_coord;

    if (two_bone) {
      // Unit cylinder/capsule meshes span Y ∈ [-0.5, +0.5]; oriented
      // cylinder reuses the unit cylinder. local +Y maps to the
      // anchor→tail direction (see Render::Geom::cylinder_between), so
      // t = 0 at y = -0.5 (anchor) and t = 1 at y = +0.5 (tail).
      float t = v.position[1] + 0.5F;
      t = std::clamp(t, 0.0F, 1.0F);
      rv.bone_indices = {anchor, tail, 0, 0};
      rv.bone_weights = {1.0F - t, t, 0.0F, 0.0F};
    } else {
      rv.bone_indices = {anchor, 0, 0, 0};
      rv.bone_weights = {1.0F, 0.0F, 0.0F, 0.0F};
    }

    out.vertices.push_back(rv);
  }

  out.indices.reserve(out.indices.size() + src_idx.size());
  for (unsigned int idx : src_idx) {
    out.indices.push_back(base_vertex + static_cast<std::uint32_t>(idx));
  }
}

} // namespace

auto bake_rigged_mesh_cpu(const BakeInput &in) -> BakedRiggedMeshCpu {
  BakedRiggedMeshCpu out;
  if (in.graph == nullptr) {
    return out;
  }

  for (PrimitiveInstance const &prim : in.graph->primitives) {
    if (prim.shape == PrimitiveShape::None) {
      continue;
    }
    Mesh *unit_mesh = resolve_unit_mesh(prim);
    if (unit_mesh == nullptr) {
      continue;
    }
    QMatrix4x4 unit_model;
    if (!compute_unit_model(prim, in.bind_pose, unit_model)) {
      continue;
    }
    append_primitive_vertices(prim, *unit_mesh, unit_model, out);
  }

  return out;
}

auto bake_rigged_mesh(const BakeInput &in)
    -> std::unique_ptr<Render::GL::RiggedMesh> {
  BakedRiggedMeshCpu cpu = bake_rigged_mesh_cpu(in);
  return std::make_unique<Render::GL::RiggedMesh>(std::move(cpu.vertices),
                                                  std::move(cpu.indices));
}

} // namespace Render::Creature
