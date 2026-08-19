#include "part_graph.h"

#include <QMatrix4x4>
#include <QVector3D>

#include "primitive_geometry.h"
#include "render/gl/mesh.h"
#include "render/submitter.h"

namespace Render::Creature {

auto submit_part_graph(const SkeletonTopology& topology,
                       const PartGraph& graph,
                       std::span<const QMatrix4x4> palette,
                       CreatureLOD lod,
                       const QMatrix4x4& world_from_unit,
                       Render::GL::ISubmitter& out,
                       std::span<const QVector3D> role_colors) -> PartSubmissionStats {
  PartSubmissionStats stats;
  std::uint8_t const lod_filter = lod_bit(lod);
  std::size_t const bone_count = topology.bones.size();

  for (PrimitiveInstance const& prim : graph.primitives) {
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
    if (anchor == k_invalid_bone || anchor >= bone_count || anchor >= palette.size()) {
      ++stats.skipped_invalid;
      continue;
    }
    bool const needs_tail = primitive_needs_tail(prim.shape);
    if (needs_tail &&
        (tail == k_invalid_bone || tail >= bone_count || tail >= palette.size())) {
      ++stats.skipped_invalid;
      continue;
    }

    QMatrix4x4 const& anchor_m = palette[anchor];
    QMatrix4x4 const& tail_m =
        (tail != k_invalid_bone && tail < palette.size()) ? palette[tail] : anchor_m;

    Render::GL::Mesh* mesh_ptr = primitive_unit_mesh(prim, lod);
    QMatrix4x4 unit_model;
    if (mesh_ptr == nullptr ||
        !primitive_unit_model(prim, anchor_m, tail_m, unit_model)) {
      ++stats.skipped_invalid;
      continue;
    }

    QMatrix4x4 const model = world_from_unit * unit_model;
    QVector3D const color =
        (prim.color_role > 0 && prim.color_role <= role_colors.size())
            ? role_colors[prim.color_role - 1]
            : prim.color;
    out.part(
        mesh_ptr, prim.material, model, color, nullptr, prim.alpha, prim.material_id);
    ++stats.submitted;
  }
  return stats;
}

auto validate_part_graph(const SkeletonTopology& topology,
                         const PartGraph& graph) noexcept -> bool {
  std::size_t const n = topology.bones.size();
  for (PrimitiveInstance const& p : graph.primitives) {
    if (p.shape == PrimitiveShape::None) {
      return false;
    }
    if (p.params.anchor_bone == k_invalid_bone || p.params.anchor_bone >= n) {
      return false;
    }
    if (primitive_needs_tail(p.shape)) {
      if (p.params.tail_bone == k_invalid_bone || p.params.tail_bone >= n) {
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
