#include "horse_manifest.h"

#include <QMatrix4x4>

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

#include "animation/bpat/bpat_format.h"
#include "horse_source_asset.h"
#include "horse_spec.h"
#include "render/elephant/loop_subdivision.h"

namespace Render::Horse {

namespace {
constexpr std::uint8_t k_role_coat = 1;

constexpr std::array<std::string_view, 4> k_refined_horse_nodes{
    "horse.production.coat",
    "horse.production.coat_dark",
    "horse.production.coat_light",
    "horse.production.muzzle",
};

auto refine_mesh_nodes(int levels)
    -> std::vector<Render::Creature::Quadruped::MeshNode> {
  auto const source = horse_source_mesh_nodes();
  auto const bind = horse_source_bind_palette();
  std::vector<Render::Creature::Quadruped::MeshNode> out(source.begin(), source.end());
  for (auto& node : out) {
    auto* mesh = std::get_if<Render::Creature::Quadruped::CustomMeshNode>(&node.data);
    if (mesh == nullptr || std::find(k_refined_horse_nodes.begin(),
                                     k_refined_horse_nodes.end(),
                                     node.debug_name) == k_refined_horse_nodes.end()) {
      continue;
    }
    Render::CreatureMesh::loop_refine_skinned_mesh(
        mesh->vertices,
        mesh->indices,
        {.levels = levels,
         .volume_keep = k_horse_full_volume_keep,
         .bone_count = static_cast<int>(k_horse_source_bone_count),
         .height_plane = bind[node.anchor_bone].row(1)});
  }
  return out;
}

auto refined_full_mesh_nodes() noexcept
    -> std::span<const Render::Creature::Quadruped::MeshNode> {
  static const auto nodes = refine_mesh_nodes(k_horse_full_subdivision_levels);
  return nodes;
}

auto smoothed_minimal_mesh_nodes() noexcept
    -> std::span<const Render::Creature::Quadruped::MeshNode> {
  static const auto nodes = refine_mesh_nodes(0);
  return nodes;
}

} // namespace

auto horse_runtime_manifest() noexcept
    -> const Render::Creature::CreatureRuntimeManifest& {
  static const Render::Creature::CreatureRuntimeManifest manifest = [] {
    Render::Creature::CreatureRuntimeManifest m;
    m.species_name = "horse";
    m.species_id = Render::Creature::Bpat::k_species_horse;
    m.bpat_file_name = "horse.bpat";
    m.minimal_snapshot_file_name = "horse_minimal.bpsm";
    m.topology = &horse_topology();
    m.lod_full.primitive_name = "horse.full.body";
    m.lod_full.anchor_bone = static_cast<Render::Creature::BoneIndex>(HorseBone::Root);
    m.lod_full.mesh_skinning = Render::Creature::MeshSkinning::Authored;
    m.lod_full.color_role = k_role_coat;
    m.lod_full.material_id = k_horse_material_id;
    m.lod_full.lod_mask = Render::Creature::k_lod_full;
    m.lod_full.mesh_nodes = refined_full_mesh_nodes();
    m.lod_minimal.primitive_name = "horse.minimal.whole";
    m.lod_minimal.anchor_bone =
        static_cast<Render::Creature::BoneIndex>(HorseBone::Root);
    m.lod_minimal.mesh_skinning = Render::Creature::MeshSkinning::Authored;
    m.lod_minimal.color_role = k_role_coat;
    m.lod_minimal.material_id = k_horse_material_id;
    m.lod_minimal.lod_mask = Render::Creature::k_lod_minimal;
    m.lod_minimal.mesh_nodes = smoothed_minimal_mesh_nodes();
    m.bind_palette = &horse_bind_palette;
    m.creature_spec = &horse_creature_spec;
    return m;
  }();
  return manifest;
}

} // namespace Render::Horse
