

#include <QCoreApplication>
#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "animation/bpat/asset_compression.h"
#include "animation/bpat/bpat_format.h"
#include "animation/bpat/bpat_writer.h"
#include "animation/clip_manifest.h"
#include "game/core/component_core.h"
#include "game/core/world.h"
#include "game/session/session_context.h"
#include "game/systems/default_content.h"
#include "game/systems/nation_registry.h"
#include "game/systems/troop_profile_service.h"
#include "game/units/spawn_type.h"
#include "render/attachment_mesh_library.h"
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
#include "render/entity/building_archetype_library.h"
#include "render/horse/horse_bake_recipe.h"
#include "render/humanoid/asset/humanoid_beard_mesh.h"
#include "render/humanoid/asset/humanoid_manifest.h"
#include "render/humanoid/asset/humanoid_spec.h"
#include "render/rigged_mesh_bake.h"
#include "render/scene_renderer.h"
#include "render/snapshot_mesh_bake.h"
#include "render/wildlife/sheep_manifest.h"
#include "render/wildlife/wolf_manifest.h"
#include "render/world_view.h"

namespace {

auto write_asset(const std::filesystem::path& path,
                 const std::string& bytes,
                 int level = Render::Creature::Bpat::k_asset_compression_level)
    -> bool {
  std::string error;
  if (!Render::Creature::Bpat::write_compressed_asset(path, bytes, error, level)) {
    std::cerr << "[bpat_baker] " << error << "\n";
    return false;
  }
  return true;
}

namespace bpat = Render::Creature::Bpat;

constexpr int k_mesh_pack_compression_level = 6;
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

auto write_body_meshes(const std::filesystem::path& out_dir,
                       const Render::Creature::CreatureSpec& spec,
                       std::span<const QMatrix4x4> bind_palette) -> bool {
  auto const body_name = spec.species_name;
  for (auto const lod :
       {Render::Creature::CreatureLOD::Full, Render::Creature::CreatureLOD::Minimal}) {
    Render::Creature::BakeInput body_input{};
    body_input.graph = &Render::Creature::part_graph_for(spec, lod);
    body_input.bind_pose = bind_palette;
    body_input.lod = lod;
    auto const body = Render::Creature::bake_rigged_mesh_cpu(body_input);
    if (body.vertices.empty() || body.indices.empty()) {
      std::cerr << "[bpat_baker] warning: " << body_name << " has no geometry for the "
                << (lod == Render::Creature::CreatureLOD::Full ? "full" : "minimal")
                << " lod; skipping its body mesh\n";
      continue;
    }

    rigged::RiggedMeshWriter const body_writer(lod, body.vertices, body.indices);
    auto const body_path = out_dir / rigged::asset_file_name(body_name, lod);
    std::ostringstream body_out(std::ios::binary);
    if (!body_writer.write(body_out)) {
      std::cerr << "[bpat_baker] write failed for " << body_path << "\n";
      return false;
    }
    if (!write_asset(body_path, body_out.str())) {
      return false;
    }
    std::cout << "[bpat_baker] wrote " << body_path << " (" << body.vertices.size()
              << " verts, " << body.indices.size() / 3U << " tris)\n";
  }
  return true;
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
  std::ostringstream out(std::ios::binary);
  if (!writer.write(out)) {
    std::cerr << "[bpat_baker] write failed for " << out_path << "\n";
    return false;
  }
  if (!write_asset(out_path, out.str())) {
    return false;
  }
  std::cout << "[bpat_baker] wrote " << out_path << " (" << writer.frame_total()
            << " frames, " << recipe.clips.size() << " clips, " << bind_palette.size()
            << " bones, " << recipe.sockets.size() << " sockets)\n";

  if (!write_body_meshes(out_dir, manifest.creature_spec(), bind_palette)) {
    return false;
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
  std::ostringstream snapshot_out(std::ios::binary);
  if (!snapshot_writer.write(snapshot_out)) {
    std::cerr << "[bpat_baker] write failed for " << snapshot_out_path << "\n";
    return false;
  }
  if (!write_asset(snapshot_out_path, snapshot_out.str())) {
    return false;
  }
  std::cout << "[bpat_baker] wrote " << snapshot_out_path << " ("
            << source.vertices.size() << " verts/frame, " << source.indices.size()
            << " indices, " << static_cast<int>(Render::Creature::CreatureLOD::Minimal)
            << " lod)\n";
  return true;
}

auto write_building_meshes(const std::filesystem::path& out_dir) -> bool {
  std::vector<Render::GL::StaticMeshPackEntry> entries =
      Render::GL::bake_building_meshes();
  bool ok = true;
  for (const auto& entry : entries) {

    if (!entry.mesh.dynamic_draws.empty() || entry.mesh.indices.empty()) {
      std::cerr << "[bpat_baker] building " << entry.name << " has "
                << entry.mesh.dynamic_draws.size() << " unmergeable parts and "
                << entry.mesh.indices.size() << " merged indices\n";
      ok = false;
    }
    for (const auto& range : entry.mesh.ranges) {
      if (range.texture != nullptr) {
        std::cerr << "[bpat_baker] building " << entry.name
                  << " has a textured part; baked buildings use the instance texture\n";
        ok = false;
      }
    }
  }
  std::error_code ec;
  std::filesystem::create_directories(out_dir, ec);
  return write_asset(out_dir / std::string(Render::GL::k_building_mesh_pack),
                     Render::GL::serialize_static_mesh_pack(entries),
                     k_mesh_pack_compression_level) &&
         ok;
}

auto write_attachment_meshes(Game::Session::SessionContext& session,
                             const std::filesystem::path& out_dir) -> bool {
  auto& nations = session.nations();
  Game::Systems::initialize_default_content(nations);
  Game::Systems::TroopProfileService::instance().clear();
  Game::Systems::TroopProfileService::instance().prime();
  Render::GL::AttachmentMeshLibrary::instance().start_empty();

  Render::GL::Renderer renderer(Render::ShaderQuality::None);
  if (!renderer.initialize()) {
    std::cerr << "[bpat_baker] attachment bake: renderer failed to initialise\n";
    return false;
  }
  Engine::Core::World world;
  int owner_id = 1;
  for (const auto& nation : nations.get_all_nations()) {
    for (int type = 0; type <= static_cast<int>(Game::Units::TroopType::Velites);
         ++type) {
      const auto troop = static_cast<Game::Units::TroopType>(type);
      const auto profile =
          Game::Systems::TroopProfileService::instance().get_profile(nation.id, troop);
      if (profile.visuals.renderer_id.empty()) {
        continue;
      }
      const Engine::Core::EntityID id = world.create_entity()->get_id();
      auto* unit = world.emplace<Engine::Core::UnitComponent>(id);
      unit->spawn_type = Game::Units::spawn_typeFromTroopType(troop);
      unit->nation_id = nation.id;
      unit->owner_id = owner_id;
      unit->health = 100;
      unit->max_health = 100;
      world.emplace<Engine::Core::TransformComponent>(id);
      auto* renderable = world.emplace<Engine::Core::RenderableComponent>(id);
      renderable->renderer_id = profile.visuals.renderer_id;
      renderable->visible = true;
    }
    ++owner_id;
  }
  renderer.set_world_view(Render::WorldView::of(session));
  renderer.prewarm_unit_templates(&world);

  std::vector<Render::GL::AttachmentMeshEntry> entries;
  renderer.rigged_mesh_cache().for_each_named_attachment_mesh(
      [&](const std::string& key, const Render::GL::RiggedMesh& mesh) {
        entries.push_back(
            Render::GL::AttachmentMeshEntry{.key = key,
                                            .vertices = mesh.get_vertices(),
                                            .indices = mesh.get_indices()});
      });
  std::sort(entries.begin(), entries.end(), [](const auto& lhs, const auto& rhs) {
    return lhs.key < rhs.key;
  });
  std::size_t triangles = 0;
  for (const auto& entry : entries) {
    triangles += entry.indices.size() / 3U;
  }
  std::cerr << "[bpat_baker] attachment sets: " << entries.size() << " (" << triangles
            << " tris)\n";
  std::error_code ec;
  std::filesystem::create_directories(out_dir, ec);
  return !entries.empty() &&
         write_asset(out_dir / std::string(Render::GL::k_attachment_mesh_pack),
                     Render::GL::serialize_attachment_mesh_pack(entries),
                     k_mesh_pack_compression_level);
}

} // namespace

int main(int argc, char** argv) {
  QCoreApplication application(argc, argv);
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
  std::filesystem::path mesh_dir = "assets/meshes";
  if (argc >= 3) {
    mesh_dir = argv[2];
  }

  bool ok = true;
  for (auto const profile : Render::Humanoid::humanoid_bake_profiles()) {
    ok = bake_species_manifest(out_dir,
                               Render::Humanoid::humanoid_bake_recipe(profile)) &&
         ok;
  }
  for (std::size_t variant = 1;
       variant < Render::Humanoid::k_humanoid_body_variant_count;
       ++variant) {
    const auto* spec = Render::Humanoid::humanoid_creature_spec_for_body_variant(
        static_cast<std::uint8_t>(variant));
    ok = spec != nullptr &&
         write_body_meshes(out_dir, *spec, Render::Humanoid::humanoid_bind_palette()) &&
         ok;
  }
  ok = bake_species_manifest(out_dir, Render::Horse::horse_bake_recipe()) && ok;
  ok = bake_species_manifest(out_dir, Render::Elephant::elephant_bake_recipe()) && ok;
  ok = bake_species_manifest(out_dir, Render::Wildlife::sheep_bake_recipe()) && ok;
  ok = bake_species_manifest(out_dir, Render::Wildlife::wolf_bake_recipe()) && ok;
  ok = write_building_meshes(mesh_dir) && ok;
  ok = write_attachment_meshes(session, mesh_dir) && ok;
  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
