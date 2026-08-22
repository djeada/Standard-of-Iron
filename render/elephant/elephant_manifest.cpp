#include "elephant_manifest.h"

#include <QMatrix4x4>

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "animation/bpat/bpat_format.h"
#include "elephant_source_asset.h"
#include "elephant_spec.h"

namespace Render::Elephant {

namespace {}

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
    m.lod_full.mesh_nodes = elephant_source_mesh_nodes();
    m.lod_minimal.primitive_name = "elephant.minimal.whole";
    m.lod_minimal.anchor_bone =
        static_cast<Render::Creature::BoneIndex>(ElephantBone::Root);
    m.lod_minimal.mesh_skinning = Render::Creature::MeshSkinning::Authored;
    m.lod_minimal.color_role = k_elephant_role_skin;
    m.lod_minimal.material_id = k_elephant_material_id;
    m.lod_minimal.lod_mask = Render::Creature::k_lod_minimal;
    m.lod_minimal.mesh_nodes = elephant_source_mesh_nodes();
    m.bind_palette = &elephant_bind_palette;
    m.creature_spec = &elephant_creature_spec;
    return m;
  }();
  return manifest;
}

} // namespace Render::Elephant
