

#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "animation/bpat/bpat_format.h"
#include "animation/bpat/bpat_writer.h"
#include "animation/clip_manifest.h"
#include "game/session/session_context.h"
#include "render/creature/bake/creature_bake_recipe.h"
#include "render/creature/humanoid_clip_ids.h"
#include "render/creature/part_graph.h"
#include "render/creature/pipeline/preparation_common.h"
#include "render/creature/render_request.h"
#include "render/creature/rigged_mesh_asset.h"
#include "render/creature/schema/creature_runtime_manifest.h"
#include "render/creature/skeleton.h"
#include "render/creature/snapshot_mesh_asset.h"
#include "render/elephant/elephant_bake_recipe.h"
#include "render/horse/horse_bake_recipe.h"
#include "render/humanoid/asset/humanoid_manifest.h"
#include "render/rigged_mesh_bake.h"
#include "render/snapshot_mesh_bake.h"
#include "render/wildlife/sheep_manifest.h"
#include "render/wildlife/wolf_manifest.h"

namespace {

namespace bpat = Render::Creature::Bpat;
namespace snapshot = Render::Creature::Snapshot;
namespace rigged = Render::Creature::Rigged;

void apply_markers(const Animation::ClipMarkers& markers, bpat::ClipDescriptor& desc) {
  desc.marker_anticipation_start = markers.anticipation_start;
  desc.marker_weapon_release = markers.weapon_release;
  desc.marker_contact = markers.contact;
  desc.marker_recover_unlocked = markers.recover_unlocked;
  desc.marker_exit_safe = markers.exit_safe;
}

void apply_generic_markers(bpat::ClipDescriptor& desc) {
  apply_markers(Animation::authored_generic_clip_markers(desc.name), desc);
}

struct ClipVariantSlot {
  std::uint16_t family{0U};
  std::uint8_t ordinal{0U};
};

auto clip_manifests_for_species(std::uint32_t species_id)
    -> std::vector<Animation::ClipManifest> {
  switch (species_id) {
  case bpat::k_species_horse:
    return {Animation::horse_clip_manifest()};
  case bpat::k_species_elephant:
    return {Animation::elephant_clip_manifest()};
  case bpat::k_species_sheep:
    return {Animation::sheep_clip_manifest()};
  case bpat::k_species_wolf:
    return {Animation::wolf_clip_manifest()};
  default:
    return {Animation::humanoid_clip_manifest(), Animation::rider_clip_manifest()};
  }
}

auto build_clip_variant_table(std::uint32_t species_id,
                              std::size_t clip_count) -> std::vector<ClipVariantSlot> {
  std::vector<ClipVariantSlot> table(clip_count);
  for (std::size_t i = 0; i < clip_count; ++i) {
    table[i].family = static_cast<std::uint16_t>(i);
  }
  for (auto const& manifest : clip_manifests_for_species(species_id)) {
    for (std::size_t state = 0; state < manifest.clips.size(); ++state) {
      auto const base = manifest.clips[state];
      if (base == Animation::k_unmapped_clip || base >= clip_count) {
        continue;
      }
      std::uint8_t const count =
          std::max<std::uint8_t>(1U, manifest.variant_counts[state]);
      for (std::uint8_t ordinal = 0U; ordinal < count && base + ordinal < clip_count;
           ++ordinal) {
        table[base + ordinal] = {base, ordinal};
      }
    }
  }
  return table;
}

bool bake_species_manifest(const std::filesystem::path& out_dir,
                           const Render::Creature::CreatureBakeRecipe& recipe) {
  if (!recipe.complete()) {
    std::cerr << "[bpat_baker] bake recipe is incomplete\n";
    return false;
  }
  const Render::Creature::CreatureRuntimeManifest& manifest = *recipe.runtime;

  auto const bind_palette = manifest.bind_palette();
  if (bind_palette.empty()) {
    std::cerr << "[bpat_baker] " << manifest.species_name
              << ": runtime manifest returned an empty bind palette"
              << " (source creature package missing, unreadable, or rejected"
                 " by its integrity check); cannot bake\n";
    return false;
  }
  std::vector<QMatrix4x4> inverse_bind;
  inverse_bind.reserve(bind_palette.size());
  for (const QMatrix4x4& m : bind_palette) {
    inverse_bind.push_back(m.inverted());
  }
  bpat::BpatWriter writer(manifest.species_id,
                          static_cast<std::uint32_t>(bind_palette.size()));
  writer.set_bind_palette(bind_palette);
  {
    std::vector<std::uint8_t> parents(bind_palette.size(), bpat::k_no_parent_bone);
    auto const bones = manifest.topology->bones;
    for (std::size_t b = 0; b < parents.size() && b < bones.size(); ++b) {
      if (bones[b].parent != Render::Creature::k_invalid_bone) {
        parents[b] = static_cast<std::uint8_t>(bones[b].parent);
      }
    }
    writer.set_bone_parents(parents);
  }

  for (auto const& socket : recipe.sockets) {
    bpat::SocketDescriptor s{};
    s.name = socket.name;
    s.anchor_bone = socket.anchor_bone;
    s.local_offset = socket.local_offset;
    writer.add_socket(std::move(s));
  }

  auto const variant_table =
      build_clip_variant_table(manifest.species_id, recipe.clips.size());
  for (std::size_t i = 0; i < recipe.clips.size(); ++i) {
    auto const& clip = recipe.clips[i];
    bpat::ClipDescriptor desc{};
    desc.name = clip.name;
    desc.frame_count = clip.frame_count;
    desc.fps = clip.fps;
    desc.loops = clip.loops;
    desc.variant_family = variant_table[i].family;
    desc.variant_ordinal = variant_table[i].ordinal;
    if (recipe.clip_markers != nullptr) {
      Animation::ClipMarkers markers{};
      recipe.clip_markers(i, clip.name, markers);
      apply_markers(markers, desc);
    } else {
      apply_generic_markers(desc);
    }
    writer.add_clip(std::move(desc));

    std::vector<QMatrix4x4> palettes;
    palettes.reserve(static_cast<std::size_t>(clip.frame_count) * bind_palette.size());
    std::vector<QMatrix4x4> sockets;
    if (!recipe.sockets.empty()) {
      sockets.reserve(static_cast<std::size_t>(clip.frame_count) *
                      recipe.sockets.size());
    }
    for (std::uint32_t f = 0; f < clip.frame_count; ++f) {
      recipe.bake_clip_frame(
          i, f, palettes, recipe.sockets.empty() ? nullptr : &sockets);
    }
    for (std::size_t p = 0; p < palettes.size(); ++p) {
      palettes[p] = palettes[p] * inverse_bind[p % inverse_bind.size()];
    }
    writer.append_clip_palettes(palettes);
    if (!recipe.sockets.empty()) {
      writer.append_clip_socket_transforms(sockets);
    }

    auto const kind =
        Render::Creature::Pipeline::creature_kind_for_bpat_species(manifest.species_id);
    std::vector<bpat::BpatFrameContact> contacts(clip.frame_count);
    for (std::uint32_t f = 0; f < clip.frame_count; ++f) {
      std::span<const QMatrix4x4> const frame{
          palettes.data() + static_cast<std::size_t>(f) * bind_palette.size(),
          bind_palette.size()};
      contacts[f].sole_y = Render::Creature::Pipeline::palette_contact_y(kind, frame);
      contacts[f].foot_y =
          Render::Creature::Pipeline::palette_foot_contact_y(kind, frame);
    }
    writer.append_clip_contacts(contacts);
  }

  std::filesystem::create_directories(out_dir);
  std::filesystem::path const out_path = out_dir / std::string(manifest.bpat_file_name);
  std::ofstream out(out_path, std::ios::binary | std::ios::trunc);
  if (!out) {
    std::cerr << "[bpat_baker] cannot open " << out_path << " for writing\n";
    return false;
  }
  if (!writer.write(out)) {
    std::cerr << "[bpat_baker] write failed for " << out_path << "\n";
    return false;
  }
  out.flush();
  std::cout << "[bpat_baker] wrote " << out_path << " (" << writer.frame_total()
            << " frames, " << recipe.clips.size() << " clips, " << bind_palette.size()
            << " bones, " << recipe.sockets.size() << " sockets)\n";

  auto const body_name = manifest.creature_spec().species_name;
  for (auto const lod :
       {Render::Creature::CreatureLOD::Full, Render::Creature::CreatureLOD::Minimal}) {
    Render::Creature::BakeInput body_input{};
    body_input.graph = &Render::Creature::part_graph_for(manifest.creature_spec(), lod);
    body_input.bind_pose = bind_palette;
    body_input.lod = lod;
    auto const body = Render::Creature::bake_rigged_mesh_cpu(body_input);
    if (body.vertices.empty() || body.indices.empty()) {
      std::cerr << "[bpat_baker] warning: " << manifest.species_name
                << " has no geometry for the "
                << (lod == Render::Creature::CreatureLOD::Full ? "full" : "minimal")
                << " lod; skipping its body mesh\n";
      continue;
    }

    rigged::RiggedMeshWriter const body_writer(lod, body.vertices, body.indices);
    auto const body_path = out_dir / rigged::asset_file_name(body_name, lod);
    std::ofstream body_out(body_path, std::ios::binary | std::ios::trunc);
    if (!body_out || !body_writer.write(body_out)) {
      std::cerr << "[bpat_baker] write failed for " << body_path << "\n";
      return false;
    }
    body_out.flush();
    std::cout << "[bpat_baker] wrote " << body_path << " (" << body.vertices.size()
              << " verts, " << body.indices.size() / 3U << " tris)\n";
  }

  if (manifest.minimal_snapshot_file_name.empty()) {
    return true;
  }

  Render::Creature::BakeInput mesh_input{};
  mesh_input.graph = &Render::Creature::part_graph_for(
      manifest.creature_spec(), Render::Creature::CreatureLOD::Minimal);
  mesh_input.bind_pose = bind_palette;
  mesh_input.lod = Render::Creature::CreatureLOD::Minimal;
  auto source = Render::Creature::bake_rigged_mesh_cpu(mesh_input);
  snapshot::SnapshotMeshWriter snapshot_writer(
      manifest.species_id,
      Render::Creature::CreatureLOD::Minimal,
      static_cast<std::uint32_t>(source.vertices.size()),
      source.indices);
  for (std::size_t i = 0; i < recipe.clips.size(); ++i) {
    auto const& clip = recipe.clips[i];
    snapshot::ClipDescriptor desc{};
    desc.name = clip.name;
    desc.frame_count = clip.frame_count;
    snapshot_writer.add_clip(std::move(desc));

    std::vector<Render::GL::RiggedVertex> clip_vertices;
    clip_vertices.reserve(static_cast<std::size_t>(clip.frame_count) *
                          source.vertices.size());
    for (std::uint32_t f = 0; f < clip.frame_count; ++f) {
      std::vector<QMatrix4x4> frame_palette;
      frame_palette.reserve(bind_palette.size());
      recipe.bake_clip_frame(i, f, frame_palette, nullptr);
      std::size_t const n = std::min(frame_palette.size(), inverse_bind.size());
      for (std::size_t b = 0; b < n; ++b) {
        frame_palette[b] = frame_palette[b] * inverse_bind[b];
      }
      auto baked = Render::GL::bake_snapshot_vertices(source.vertices, frame_palette);
      clip_vertices.insert(clip_vertices.end(), baked.begin(), baked.end());
    }
    snapshot_writer.append_clip_vertices(clip_vertices);
  }

  if (source.vertices.empty() || source.indices.empty()) {
    std::cerr << "[bpat_baker] warning: no geometry baked for " << manifest.species_name
              << " minimal snapshot; skipping write of "
              << (out_dir / std::string(manifest.minimal_snapshot_file_name))
              << " (pre-baked asset on disk is preserved)\n";
    return true;
  }

  std::filesystem::path const snapshot_out_path =
      out_dir / std::string(manifest.minimal_snapshot_file_name);
  std::ofstream snapshot_out(snapshot_out_path, std::ios::binary | std::ios::trunc);
  if (!snapshot_out) {
    std::cerr << "[bpat_baker] cannot open " << snapshot_out_path << " for writing\n";
    return false;
  }
  if (!snapshot_writer.write(snapshot_out)) {
    std::cerr << "[bpat_baker] write failed for " << snapshot_out_path << "\n";
    return false;
  }
  snapshot_out.flush();
  std::cout << "[bpat_baker] wrote " << snapshot_out_path << " ("
            << source.vertices.size() << " verts/frame, " << source.indices.size()
            << " indices, " << static_cast<int>(Render::Creature::CreatureLOD::Minimal)
            << " lod)\n";
  return true;
}

} // namespace

int main(int argc, char** argv) {

  Game::Session::SessionContext session;
  Game::Session::ScopedSession const active_session(session);
  static_assert(Render::Creature::k_humanoid_idle_clip == 0U);
  static_assert(Render::Creature::k_humanoid_idle_squat_clip == 1U);
  static_assert(Render::Creature::k_humanoid_idle_jump_clip == 2U);
  static_assert(Render::Creature::k_humanoid_idle_weapon_clip == 3U);
  static_assert(Render::Creature::k_humanoid_idle_weave_clip == 4U);
  static_assert(Render::Creature::k_humanoid_idle_plant_flag_clip == 5U);
  static_assert(Render::Creature::k_humanoid_hold_clip == 8U);
  static_assert(Render::Creature::k_humanoid_hold_bow_clip == 9U);
  static_assert(Render::Creature::k_humanoid_attack_sword_a_clip == 10U);
  static_assert(Render::Creature::k_humanoid_attack_spear_a_clip == 13U);
  static_assert(Render::Creature::k_humanoid_attack_bow_clip == 16U);
  static_assert(Render::Creature::k_humanoid_riding_idle_clip == 17U);
  static_assert(Render::Creature::k_humanoid_riding_bow_shot_clip == 20U);
  static_assert(Render::Creature::k_humanoid_riding_sword_strike_clip == 21U);
  static_assert(Render::Creature::k_humanoid_riding_spear_thrust_clip == 22U);
  static_assert(Render::Creature::k_humanoid_die_infantry_clip == 23U);
  static_assert(Render::Creature::k_humanoid_dead_infantry_clip ==
                Render::Creature::k_humanoid_die_infantry_clip +
                    Render::Creature::k_humanoid_infantry_death_variant_count);
  static_assert(Render::Creature::k_humanoid_dead_mounted_clip == 30U);
  static_assert(Render::Creature::k_humanoid_rpg_sword_slash_left_clip == 31U);
  static_assert(Render::Creature::k_humanoid_rpg_sword_slash_right_clip == 32U);
  static_assert(Render::Creature::k_humanoid_rpg_sword_overhead_clip == 33U);
  static_assert(Render::Creature::k_humanoid_rpg_sword_thrust_clip == 34U);
  static_assert(Render::Creature::k_humanoid_rpg_sword_finisher_clip == 35U);
  static_assert(Render::Creature::k_humanoid_testudo_first_clip == 53U);
  static_assert(Render::Creature::k_humanoid_testudo_rear_clip ==
                Render::Creature::k_humanoid_testudo_first_clip +
                    Render::Creature::k_humanoid_testudo_clip_count - 1U);
  static_assert(Render::Creature::k_humanoid_carthage_shield_wall_first_clip == 58U);
  static_assert(Render::Creature::k_humanoid_carthage_shield_wall_right_clip ==
                Render::Creature::k_humanoid_carthage_shield_wall_first_clip +
                    Render::Creature::k_humanoid_carthage_shield_wall_clip_count - 1U);
  std::filesystem::path out_dir = "assets/creatures";
  if (argc >= 2) {
    out_dir = argv[1];
  }

  bool ok = true;
  for (auto const profile : Render::Humanoid::humanoid_bake_profiles()) {
    ok = bake_species_manifest(out_dir,
                               Render::Humanoid::humanoid_bake_recipe(profile)) &&
         ok;
  }
  ok = bake_species_manifest(out_dir, Render::Horse::horse_bake_recipe()) && ok;
  ok = bake_species_manifest(out_dir, Render::Elephant::elephant_bake_recipe()) && ok;
  ok = bake_species_manifest(out_dir, Render::Wildlife::sheep_bake_recipe()) && ok;
  ok = bake_species_manifest(out_dir, Render::Wildlife::wolf_bake_recipe()) && ok;
  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
