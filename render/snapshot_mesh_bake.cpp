#include "snapshot_mesh_bake.h"

#include <QVector3D>
#include <QVector4D>

namespace Render::GL {

auto bake_snapshot_vertices(std::span<const RiggedVertex> source_vertices,
                            std::span<const QMatrix4x4> frame_palette)
    -> std::vector<RiggedVertex> {
  std::vector<RiggedVertex> baked(source_vertices.size());

  for (std::size_t vi = 0; vi < source_vertices.size(); ++vi) {
    const auto &sv = source_vertices[vi];

    QVector4D pos(sv.position_bone_local[0], sv.position_bone_local[1],
                  sv.position_bone_local[2], 1.0F);
    QVector4D nrm(sv.normal_bone_local[0], sv.normal_bone_local[1],
                  sv.normal_bone_local[2], 0.0F);

    QVector4D out_pos(0.0F, 0.0F, 0.0F, 0.0F);
    QVector4D out_nrm(0.0F, 0.0F, 0.0F, 0.0F);

    for (int k = 0; k < 4; ++k) {
      const float w = sv.bone_weights[k];
      if (w == 0.0F) {
        continue;
      }
      const auto bi = static_cast<std::uint32_t>(sv.bone_indices[k]);
      if (bi >= frame_palette.size()) {
        continue;
      }
      const auto &m = frame_palette[bi];
      out_pos += (m * pos) * w;
      out_nrm += (m * nrm) * w;
    }

    QVector3D n(out_nrm.x(), out_nrm.y(), out_nrm.z());
    n.normalize();

    RiggedVertex bv{};
    bv.position_bone_local = {out_pos.x(), out_pos.y(), out_pos.z()};
    bv.normal_bone_local = {n.x(), n.y(), n.z()};
    bv.tex_coord = sv.tex_coord;
    bv.bone_indices = {0, 0, 0, 0};
    bv.bone_weights = {1.0F, 0.0F, 0.0F, 0.0F};
    bv.color_role = sv.color_role;
    bv.padding = sv.padding;
    baked[vi] = bv;
  }

  return baked;
}

} // namespace Render::GL
