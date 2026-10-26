#include "elephant_manifest.h"

#include <QMatrix4x4>

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

#include "animation/bpat/bpat_format.h"
#include "elephant_source_asset.h"
#include "elephant_spec.h"
#include "loop_subdivision.h"

namespace Render::Elephant {

namespace {

auto refine_mesh_nodes(int levels)
    -> std::vector<Render::Creature::Quadruped::MeshNode> {
  auto const source = elephant_source_mesh_nodes();
  auto const bind = elephant_source_bind_palette();
  std::vector<Render::Creature::Quadruped::MeshNode> out(source.begin(), source.end());
  for (auto& node : out) {
    auto* mesh = std::get_if<Render::Creature::Quadruped::CustomMeshNode>(&node.data);
    if (mesh == nullptr || node.debug_name != "elephant.production.skin") {
      continue;
    }
    Render::CreatureMesh::loop_refine_skinned_mesh(
        mesh->vertices,
        mesh->indices,
        {.levels = levels,
         .volume_keep = k_elephant_full_volume_keep,
         .bone_count = static_cast<int>(k_elephant_source_bone_count),
         .sole_band = k_elephant_full_sole_band,
         .height_plane = bind[node.anchor_bone].row(1)});
  }
  return out;
}

auto refined_full_mesh_nodes() noexcept
    -> std::span<const Render::Creature::Quadruped::MeshNode> {
  static const auto nodes = refine_mesh_nodes(k_elephant_full_subdivision_levels);
  return nodes;
}

auto smoothed_minimal_mesh_nodes() noexcept
    -> std::span<const Render::Creature::Quadruped::MeshNode> {
  static const auto nodes = refine_mesh_nodes(0);
  return nodes;
}

} // namespace

auto elephant_runtime_manifest() noexcept
    -> const Render::Creature::CreatureRuntimeManifest& {
  static const Render::Creature::CreatureRuntimeManifest manifest = [] {
    Render::Creature::CreatureRuntimeManifest m;
    m.species_name = "elephant";
    m.species_id = Render::Creature::Bpat::k_species_elephant;
    m.bpat_file_name = "elephant.bpat";
    m.minimal_snapshot_file_name = "elephant_minimal.bpsm";
    m.topology = &elephant_topology();
    m.lod_full.primitive_name = "elephant.full.body";
    m.lod_full.anchor_bone =
        static_cast<Render::Creature::BoneIndex>(ElephantBone::Root);
    m.lod_full.mesh_skinning = Render::Creature::MeshSkinning::Authored;
    m.lod_full.color_role = k_elephant_role_skin;
    m.lod_full.material_id = k_elephant_material_id;
    m.lod_full.lod_mask = Render::Creature::k_lod_full;
    m.lod_full.mesh_nodes = refined_full_mesh_nodes();
    m.lod_minimal.primitive_name = "elephant.minimal.whole";
    m.lod_minimal.anchor_bone =
        static_cast<Render::Creature::BoneIndex>(ElephantBone::Root);
    m.lod_minimal.mesh_skinning = Render::Creature::MeshSkinning::Authored;
    m.lod_minimal.color_role = k_elephant_role_skin;
    m.lod_minimal.material_id = k_elephant_material_id;
    m.lod_minimal.lod_mask = Render::Creature::k_lod_minimal;
    m.lod_minimal.mesh_nodes = smoothed_minimal_mesh_nodes();
    m.bind_palette = &elephant_bind_palette;
    m.creature_spec = &elephant_creature_spec;
    return m;
  }();
  return manifest;
}

} // namespace Render::Elephant
