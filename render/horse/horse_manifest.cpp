#include "horse_manifest.h"

#include <QMatrix4x4>

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "animation/bpat/bpat_format.h"
#include "horse_source_asset.h"
#include "horse_spec.h"

namespace Render::Horse {

namespace {
constexpr std::uint8_t k_role_coat = 1;
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
    m.lod_full.mesh_nodes = horse_source_mesh_nodes();
    m.lod_minimal.primitive_name = "horse.minimal.whole";
    m.lod_minimal.anchor_bone =
        static_cast<Render::Creature::BoneIndex>(HorseBone::Root);
    m.lod_minimal.mesh_skinning = Render::Creature::MeshSkinning::Authored;
    m.lod_minimal.color_role = k_role_coat;
    m.lod_minimal.material_id = k_horse_material_id;
    m.lod_minimal.lod_mask = Render::Creature::k_lod_minimal;
    m.lod_minimal.mesh_nodes = horse_source_mesh_nodes();
    m.bind_palette = &horse_bind_palette;
    m.creature_spec = &horse_creature_spec;
    return m;
  }();
  return manifest;
}

} // namespace Render::Horse
