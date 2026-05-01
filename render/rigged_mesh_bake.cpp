#include "rigged_mesh_bake.h"

#include "creature/part_graph.h"
#include "creature/skeleton.h"
#include "elephant/elephant_spec.h"
#include "geom/parts.h"
#include "geom/transforms.h"
#include "gl/mesh.h"
#include "gl/primitives.h"
#include "horse/horse_spec.h"
#include "render_archetype.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>
#include <algorithm>
#include <array>
#include <cmath>

namespace Render::Creature {

namespace {

using Render::GL::Mesh;
using Render::GL::RiggedVertex;
using Render::GL::Vertex;

struct VertexBoneBlend {
  std::array<std::uint8_t, 4> indices{0, 0, 0, 0};
  std::array<float, 4> weights{1.0F, 0.0F, 0.0F, 0.0F};
};

struct LegChain {
  std::uint8_t shoulder{};
  std::uint8_t knee{};
  std::uint8_t foot{};
  QVector3D shoulder_pos{};
  QVector3D knee_pos{};
  QVector3D foot_pos{};
};

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

auto is_two_bone_blend(PrimitiveShape shape) -> bool {
  switch (shape) {
  case PrimitiveShape::Cylinder:
  case PrimitiveShape::Capsule:
  case PrimitiveShape::OrientedCylinder:
  case PrimitiveShape::BoneSpanMesh:
    return true;
  default:
    return false;
  }
}

auto bone_origin(std::span<const BoneWorldMatrix> bind_pose,
                 BoneIndex bone) -> QVector3D {
  if (bone == kInvalidBone || bone >= bind_pose.size()) {
    return QVector3D{};
  }
  return bind_pose[bone].column(3).toVector3D();
}

auto make_single_blend(std::uint8_t bone) -> VertexBoneBlend {
  VertexBoneBlend blend;
  blend.indices = {bone, 0, 0, 0};
  blend.weights = {1.0F, 0.0F, 0.0F, 0.0F};
  return blend;
}

auto make_two_bone_blend(std::uint8_t a, std::uint8_t b,
                         float t) -> VertexBoneBlend {
  t = std::clamp(t, 0.0F, 1.0F);
  VertexBoneBlend blend;
  blend.indices = {a, b, 0, 0};
  blend.weights = {1.0F - t, t, 0.0F, 0.0F};
  return blend;
}

auto projection_factor(const QVector3D &point, const QVector3D &start,
                       const QVector3D &end) -> float {
  QVector3D const axis = end - start;
  float const denom = QVector3D::dotProduct(axis, axis);
  if (denom <= 1.0e-6F) {
    return 0.0F;
  }
  return std::clamp(QVector3D::dotProduct(point - start, axis) / denom, 0.0F,
                    1.0F);
}

auto point_to_segment_distance_sq(const QVector3D &point,
                                  const QVector3D &start,
                                  const QVector3D &end) -> float {
  float const t = projection_factor(point, start, end);
  QVector3D const closest = start + (end - start) * t;
  return (point - closest).lengthSquared();
}

auto select_leg_chain(const QVector3D &pos,
                      std::span<const LegChain> legs) -> const LegChain * {
  const LegChain *best = nullptr;
  float best_score = 1.0e9F;
  for (auto const &leg : legs) {
    float const dx = pos.x() - leg.foot_pos.x();
    float const dz = pos.z() - leg.shoulder_pos.z();
    float const dy = pos.y() - leg.shoulder_pos.y();
    float const x_limit =
        std::max(std::abs(leg.shoulder_pos.x()), std::abs(leg.foot_pos.x())) *
            0.80F +
        0.12F;
    float const z_limit =
        std::abs(leg.shoulder_pos.z() - leg.foot_pos.z()) * 0.90F + 0.26F;
    if (std::abs(dx) > x_limit || std::abs(dz) > z_limit ||
        pos.y() > leg.shoulder_pos.y() + 0.12F) {
      continue;
    }
    float const score = dx * dx + dz * dz + dy * dy * 0.15F;
    if (score < best_score) {
      best_score = score;
      best = &leg;
    }
  }
  return best;
}

auto select_horse_leg_chain(
    const QVector3D &pos, std::span<const LegChain> legs) -> const LegChain * {
  const LegChain *best = nullptr;
  float best_score = 1.0e9F;
  for (auto const &leg : legs) {
    if (pos.y() > leg.shoulder_pos.y() - 0.03F) {
      continue;
    }

    float const lateral_gate = std::abs(leg.shoulder_pos.x()) * 0.55F;
    if (std::abs(pos.x()) < lateral_gate) {
      continue;
    }

    float const dz = pos.z() - leg.shoulder_pos.z();
    float const z_limit =
        std::abs(leg.shoulder_pos.z() - leg.foot_pos.z()) * 0.60F + 0.10F;
    if (std::abs(dz) > z_limit) {
      continue;
    }

    float const upper_dist_sq =
        point_to_segment_distance_sq(pos, leg.shoulder_pos, leg.knee_pos);
    float const lower_dist_sq =
        point_to_segment_distance_sq(pos, leg.knee_pos, leg.foot_pos);
    float const leg_len = (leg.foot_pos - leg.shoulder_pos).length();
    float const dist_limit = leg_len * 0.14F + 0.03F;
    float const dist_limit_sq = dist_limit * dist_limit;
    float const score = std::min(upper_dist_sq, lower_dist_sq);
    if (score > dist_limit_sq) {
      continue;
    }

    if (score < best_score) {
      best_score = score;
      best = &leg;
    }
  }
  return best;
}

auto blend_leg_chain(const QVector3D &pos,
                     const LegChain &leg) -> VertexBoneBlend {
  if (pos.y() >= leg.knee_pos.y()) {
    float const t = projection_factor(pos, leg.shoulder_pos, leg.knee_pos);
    return make_two_bone_blend(leg.shoulder, leg.knee, t);
  }
  float const t = projection_factor(pos, leg.knee_pos, leg.foot_pos);
  return make_two_bone_blend(leg.knee, leg.foot, t);
}

auto horse_whole_mesh_blend(const QVector3D &pos,
                            std::span<const BoneWorldMatrix> bind_pose)
    -> VertexBoneBlend {
  using Bone = Render::Horse::HorseBone;
  auto const root = static_cast<std::uint8_t>(Bone::Root);
  QVector3D const root_pos =
      bone_origin(bind_pose, static_cast<BoneIndex>(Bone::Root));
  QVector3D const neck_pos =
      bone_origin(bind_pose, static_cast<BoneIndex>(Bone::NeckTop));
  QVector3D const head_pos =
      bone_origin(bind_pose, static_cast<BoneIndex>(Bone::Head));

  std::array<LegChain, 4> const legs{{
      {static_cast<std::uint8_t>(Bone::ShoulderFL),
       static_cast<std::uint8_t>(Bone::KneeFL),
       static_cast<std::uint8_t>(Bone::FootFL),
       bone_origin(bind_pose, static_cast<BoneIndex>(Bone::ShoulderFL)),
       bone_origin(bind_pose, static_cast<BoneIndex>(Bone::KneeFL)),
       bone_origin(bind_pose, static_cast<BoneIndex>(Bone::FootFL))},
      {static_cast<std::uint8_t>(Bone::ShoulderFR),
       static_cast<std::uint8_t>(Bone::KneeFR),
       static_cast<std::uint8_t>(Bone::FootFR),
       bone_origin(bind_pose, static_cast<BoneIndex>(Bone::ShoulderFR)),
       bone_origin(bind_pose, static_cast<BoneIndex>(Bone::KneeFR)),
       bone_origin(bind_pose, static_cast<BoneIndex>(Bone::FootFR))},
      {static_cast<std::uint8_t>(Bone::ShoulderBL),
       static_cast<std::uint8_t>(Bone::KneeBL),
       static_cast<std::uint8_t>(Bone::FootBL),
       bone_origin(bind_pose, static_cast<BoneIndex>(Bone::ShoulderBL)),
       bone_origin(bind_pose, static_cast<BoneIndex>(Bone::KneeBL)),
       bone_origin(bind_pose, static_cast<BoneIndex>(Bone::FootBL))},
      {static_cast<std::uint8_t>(Bone::ShoulderBR),
       static_cast<std::uint8_t>(Bone::KneeBR),
       static_cast<std::uint8_t>(Bone::FootBR),
       bone_origin(bind_pose, static_cast<BoneIndex>(Bone::ShoulderBR)),
       bone_origin(bind_pose, static_cast<BoneIndex>(Bone::KneeBR)),
       bone_origin(bind_pose, static_cast<BoneIndex>(Bone::FootBR))},
  }};
  if (auto const *leg = select_horse_leg_chain(pos, legs); leg != nullptr) {
    return blend_leg_chain(pos, *leg);
  }

  float const neck_start = root_pos.z() + (neck_pos.z() - root_pos.z()) * 0.28F;
  if (pos.z() > neck_start) {
    if (pos.z() < neck_pos.z()) {
      float const t = projection_factor(pos, root_pos, neck_pos);
      return make_two_bone_blend(root, static_cast<std::uint8_t>(Bone::NeckTop),
                                 t);
    }
    float const t = projection_factor(pos, neck_pos, head_pos);
    return make_two_bone_blend(static_cast<std::uint8_t>(Bone::NeckTop),
                               static_cast<std::uint8_t>(Bone::Head), t);
  }

  return make_single_blend(root);
}

auto elephant_whole_mesh_blend(const QVector3D &pos,
                               std::span<const BoneWorldMatrix> bind_pose)
    -> VertexBoneBlend {
  using Bone = Render::Elephant::ElephantBone;
  auto const root = static_cast<std::uint8_t>(Bone::Root);
  QVector3D const root_pos =
      bone_origin(bind_pose, static_cast<BoneIndex>(Bone::Root));
  QVector3D const head_pos =
      bone_origin(bind_pose, static_cast<BoneIndex>(Bone::Head));
  QVector3D const trunk_pos =
      bone_origin(bind_pose, static_cast<BoneIndex>(Bone::TrunkTip));

  std::array<LegChain, 4> const legs{{
      {static_cast<std::uint8_t>(Bone::ShoulderFL),
       static_cast<std::uint8_t>(Bone::KneeFL),
       static_cast<std::uint8_t>(Bone::FootFL),
       bone_origin(bind_pose, static_cast<BoneIndex>(Bone::ShoulderFL)),
       bone_origin(bind_pose, static_cast<BoneIndex>(Bone::KneeFL)),
       bone_origin(bind_pose, static_cast<BoneIndex>(Bone::FootFL))},
      {static_cast<std::uint8_t>(Bone::ShoulderFR),
       static_cast<std::uint8_t>(Bone::KneeFR),
       static_cast<std::uint8_t>(Bone::FootFR),
       bone_origin(bind_pose, static_cast<BoneIndex>(Bone::ShoulderFR)),
       bone_origin(bind_pose, static_cast<BoneIndex>(Bone::KneeFR)),
       bone_origin(bind_pose, static_cast<BoneIndex>(Bone::FootFR))},
      {static_cast<std::uint8_t>(Bone::ShoulderBL),
       static_cast<std::uint8_t>(Bone::KneeBL),
       static_cast<std::uint8_t>(Bone::FootBL),
       bone_origin(bind_pose, static_cast<BoneIndex>(Bone::ShoulderBL)),
       bone_origin(bind_pose, static_cast<BoneIndex>(Bone::KneeBL)),
       bone_origin(bind_pose, static_cast<BoneIndex>(Bone::FootBL))},
      {static_cast<std::uint8_t>(Bone::ShoulderBR),
       static_cast<std::uint8_t>(Bone::KneeBR),
       static_cast<std::uint8_t>(Bone::FootBR),
       bone_origin(bind_pose, static_cast<BoneIndex>(Bone::ShoulderBR)),
       bone_origin(bind_pose, static_cast<BoneIndex>(Bone::KneeBR)),
       bone_origin(bind_pose, static_cast<BoneIndex>(Bone::FootBR))},
  }};
  if (auto const *leg = select_leg_chain(pos, legs); leg != nullptr) {
    return blend_leg_chain(pos, *leg);
  }

  float const head_start = root_pos.z() + (head_pos.z() - root_pos.z()) * 0.45F;
  if (pos.z() > head_start) {
    if (pos.y() < head_pos.y() - 0.04F && pos.z() > head_pos.z()) {
      float const t = projection_factor(pos, head_pos, trunk_pos);
      return make_two_bone_blend(static_cast<std::uint8_t>(Bone::Head),
                                 static_cast<std::uint8_t>(Bone::TrunkTip), t);
    }
    float const t = projection_factor(pos, root_pos, head_pos);
    return make_two_bone_blend(root, static_cast<std::uint8_t>(Bone::Head), t);
  }

  return make_single_blend(root);
}

auto resolve_mesh_blend(
    const PrimitiveInstance &prim, const QVector3D &world_pos,
    std::span<const BoneWorldMatrix> bind_pose) -> VertexBoneBlend {
  switch (prim.mesh_skinning) {
  case MeshSkinning::HorseWhole:
    return horse_whole_mesh_blend(world_pos, bind_pose);
  case MeshSkinning::ElephantWhole:
    return elephant_whole_mesh_blend(world_pos, bind_pose);
  case MeshSkinning::Rigid:
  default:
    return make_single_blend(
        static_cast<std::uint8_t>(prim.params.anchor_bone));
  }
}

auto resolve_unit_mesh(const PrimitiveInstance &prim) -> Mesh * {
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
  case PrimitiveShape::Mesh:
    return prim.custom_mesh;
  case PrimitiveShape::BoneSpanMesh:
    return prim.custom_mesh;
  case PrimitiveShape::None:
  default:
    return nullptr;
  }
}

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
    out_model =
        Render::Geom::cone_from_to(head_world, tail_world, prim.params.radius);
    return true;
  }

  case PrimitiveShape::Box:
    out_model =
        box_model(anchor_m, prim.params.head_offset, prim.params.half_extents);
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
                                                right_ref, r_right, r_forward);
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
    out_model =
        mesh_model(anchor_m, prim.params.head_offset, prim.params.half_extents);
    return true;

  case PrimitiveShape::BoneSpanMesh: {
    if (prim.custom_mesh == nullptr) {
      return false;
    }
    QVector3D const tail_world =
        bone_world_offset(bind_pose[tail], prim.params.tail_offset);
    QVector3D const right_ref = anchor_m.column(0).toVector3D();
    float const r_right = prim.params.radius;
    float const r_forward = (prim.params.depth_radius > 0.0F)
                                ? prim.params.depth_radius
                                : prim.params.radius;
    out_model = Render::Geom::oriented_cylinder(head_world, tail_world,
                                                right_ref, r_right, r_forward);
    return true;
  }

  case PrimitiveShape::None:
  default:
    return false;
  }
}

auto transform_normal(const QMatrix4x4 &m, const QVector3D &n) -> QVector3D {
  QVector3D const mapped = m.mapVector(n);
  float const len_sq = mapped.x() * mapped.x() + mapped.y() * mapped.y() +
                       mapped.z() * mapped.z();
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
                               std::span<const BoneWorldMatrix> bind_pose,
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
    rv.color_role = prim.color_role;
    if (prim.shape == PrimitiveShape::Mesh && v.color_role != 0U) {
      rv.color_role = v.color_role;
    }

    if (two_bone) {
      float t = v.position[1] + 0.5F;
      t = std::clamp(t, 0.0F, 1.0F);
      rv.bone_indices = {anchor, tail, 0, 0};
      rv.bone_weights = {1.0F - t, t, 0.0F, 0.0F};
    } else if (prim.shape == PrimitiveShape::Mesh &&
               prim.mesh_skinning != MeshSkinning::Rigid) {
      VertexBoneBlend const blend =
          resolve_mesh_blend(prim, world_pos, bind_pose);
      rv.bone_indices = blend.indices;
      rv.bone_weights = blend.weights;
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

void append_static_attachment(const StaticAttachmentSpec &spec,
                              BakedRiggedMeshCpu &out) {
  if (spec.archetype == nullptr) {
    return;
  }

  const auto &slice = spec.archetype->lods[0];
  if (slice.draws.empty()) {
    return;
  }

  for (const Render::GL::RenderArchetypeDraw &draw : slice.draws) {
    Mesh *src = draw.mesh;
    if (src == nullptr) {
      continue;
    }
    auto const &src_verts = src->get_vertices();
    auto const &src_idx = src->get_indices();
    if (src_verts.empty() || src_idx.empty()) {
      continue;
    }

    QMatrix4x4 scale_mat;
    if (spec.uniform_scale != 1.0F) {
      scale_mat.scale(spec.uniform_scale);
    }
    const QMatrix4x4 attach_model =
        spec.local_offset * scale_mat * draw.local_model;

    std::uint8_t role = 0;
    if (draw.palette_slot != Render::GL::kRenderArchetypeFixedColorSlot) {
      const auto slot = static_cast<std::size_t>(draw.palette_slot);
      if (slot < spec.palette_role_remap.size()) {
        role = spec.palette_role_remap[slot];
      }
    } else {
      role = spec.override_color_role;
    }

    auto const base_vertex = static_cast<std::uint32_t>(out.vertices.size());
    auto const bone = static_cast<std::uint8_t>(spec.socket_bone_index & 0xFFu);

    out.vertices.reserve(out.vertices.size() + src_verts.size());
    for (Render::GL::Vertex const &v : src_verts) {
      QVector3D const local_pos{v.position[0], v.position[1], v.position[2]};
      QVector3D const local_norm{v.normal[0], v.normal[1], v.normal[2]};
      QVector3D const baked_pos = attach_model.map(local_pos);
      QVector3D const baked_norm = transform_normal(attach_model, local_norm);

      RiggedVertex rv;
      rv.position_bone_local = {baked_pos.x(), baked_pos.y(), baked_pos.z()};
      rv.normal_bone_local = {baked_norm.x(), baked_norm.y(), baked_norm.z()};
      rv.tex_coord = v.tex_coord;
      rv.color_role = role;
      rv.bone_indices = {bone, 0, 0, 0};
      rv.bone_weights = {1.0F, 0.0F, 0.0F, 0.0F};
      out.vertices.push_back(rv);
    }

    out.indices.reserve(out.indices.size() + src_idx.size());
    for (unsigned int idx : src_idx) {
      out.indices.push_back(base_vertex + static_cast<std::uint32_t>(idx));
    }
  }
}

} // namespace

auto bake_rigged_mesh_cpu(const BakeInput &in) -> BakedRiggedMeshCpu {
  BakedRiggedMeshCpu out;
  if (in.graph != nullptr) {
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
      append_primitive_vertices(prim, *unit_mesh, unit_model, in.bind_pose,
                                out);
    }
  }

  for (StaticAttachmentSpec const &spec : in.attachments) {
    if (!in.bind_pose.empty() &&
        spec.socket_bone_index >= in.bind_pose.size()) {
      continue;
    }
    append_static_attachment(spec, out);
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
