#include "snapshot_mesh_cache.h"

#include "bone_palette_arena.h"
#include "rigged_mesh_cache.h"

#include <QVector3D>
#include <QVector4D>
#include <array>
#include <limits>

namespace Render::GL {

namespace {

auto build_identity_palette() noexcept
    -> std::array<QMatrix4x4, BonePaletteArena::kPaletteWidth> {
  std::array<QMatrix4x4, BonePaletteArena::kPaletteWidth> arr{};
  for (auto &m : arr) {
    m.setToIdentity();
  }
  return arr;
}

} // namespace

auto SnapshotMeshCache::identity_palette() noexcept -> const QMatrix4x4 * {
  static const auto kPalette = build_identity_palette();
  return kPalette.data();
}

auto SnapshotMeshCache::get_or_bake(
    const Key &key, const RiggedMeshEntry &source,
    std::uint32_t global_frame) -> const SnapshotMeshEntry * {
  if (auto it = m_entries.find(key); it != m_entries.end()) {
    return &it->second;
  }

  if (source.mesh == nullptr || source.skinned_palettes.empty() ||
      source.skinned_bone_count == 0U ||
      global_frame >= source.skinned_frame_total) {
    return nullptr;
  }

  const auto &src_vertices = source.mesh->get_vertices();
  if (src_vertices.empty()) {
    return nullptr;
  }
  const auto &src_indices = source.mesh->get_indices();
  if (src_indices.empty()) {
    return nullptr;
  }

  const QMatrix4x4 *frame_palette =
      source.skinned_palettes.data() +
      static_cast<std::size_t>(global_frame) *
          static_cast<std::size_t>(source.skinned_bone_count);
  const auto bone_count = source.skinned_bone_count;

  std::vector<RiggedVertex> baked;
  baked.resize(src_vertices.size());

  for (std::size_t vi = 0; vi < src_vertices.size(); ++vi) {
    const auto &sv = src_vertices[vi];

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
      if (bi >= bone_count) {
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

  std::vector<std::uint32_t> indices_copy = src_indices;

  SnapshotMeshEntry entry{};
  entry.mesh =
      std::make_unique<RiggedMesh>(std::move(baked), std::move(indices_copy));
  auto [it, _ok] = m_entries.emplace(key, std::move(entry));
  return &it->second;
}

} // namespace Render::GL
