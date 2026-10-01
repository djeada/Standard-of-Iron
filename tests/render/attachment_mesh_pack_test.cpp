#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "animation/bpat/bpat_reader.h"
#include "game/core/component_core.h"
#include "game/core/world.h"
#include "game/session/session_context.h"
#include "game/systems/default_content.h"
#include "game/systems/nation_registry.h"
#include "game/systems/troop_profile_service.h"
#include "game/units/spawn_type.h"
#include "render/attachment_mesh_library.h"
#include "render/creature/archetype_registry.h"
#include "render/creature/pipeline/creature_asset.h"
#include "render/creature/pipeline/creature_asset_init.h"
#include "render/creature/runtime_bake_guard.h"
#include "render/scene_renderer.h"
#include "render/world_view.h"

namespace {

using namespace Render::GL;

auto sample_entry() -> AttachmentMeshEntry {
  AttachmentMeshEntry entry;
  entry.key = "humanoid/0/0/00000000000000ff";
  for (int i = 0; i < 3; ++i) {
    RiggedVertex vertex;
    vertex.position_bone_local = {static_cast<float>(i), 0.5F, -1.0F};
    vertex.bone_indices = {3U, 0U, 0U, 0U};
    vertex.bone_weights = {1.0F, 0.0F, 0.0F, 0.0F};
    vertex.color_role = 4U;
    entry.vertices.push_back(vertex);
  }
  entry.indices = {0U, 1U, 2U};
  return entry;
}

} // namespace

TEST(AttachmentMeshPack, RoundTripsEveryField) {
  const std::string bytes = serialize_attachment_mesh_pack({sample_entry()});
  std::vector<AttachmentMeshEntry> read;
  std::string error;
  ASSERT_TRUE(parse_attachment_mesh_pack(
      reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size(), read, error))
      << error;
  ASSERT_EQ(read.size(), 1U);
  EXPECT_EQ(read[0].key, sample_entry().key);
  ASSERT_EQ(read[0].vertices.size(), 3U);
  EXPECT_FLOAT_EQ(read[0].vertices[2].position_bone_local[0], 2.0F);
  EXPECT_EQ(read[0].vertices[1].bone_indices[0], 3U);
  EXPECT_EQ(read[0].vertices[1].color_role, 4U);
  EXPECT_EQ(read[0].indices, sample_entry().indices);
}

namespace {

auto add_every_troop(Engine::Core::World& world, std::size_t nation_limit) -> void {
  auto& nations = Game::Systems::NationRegistry::instance();
  int owner_id = 1;
  for (const auto& nation : nations.get_all_nations()) {
    if (static_cast<std::size_t>(owner_id) > nation_limit) {
      break;
    }
    for (int type = 0; type <= static_cast<int>(Game::Units::TroopType::Wolf); ++type) {
      const auto troop = static_cast<Game::Units::TroopType>(type);
      const auto profile =
          Game::Systems::TroopProfileService::instance().get_profile(nation.id, troop);
      if (profile.visuals.renderer_id.empty()) {
        continue;
      }
      auto* entity = world.create_entity();
      auto* unit = entity->add_component<Engine::Core::UnitComponent>();
      unit->spawn_type = Game::Units::spawn_typeFromTroopType(troop);
      unit->nation_id = nation.id;
      unit->owner_id = owner_id;
      unit->health = 100;
      unit->max_health = 100;
      entity->add_component<Engine::Core::TransformComponent>();
      auto* renderable = entity->add_component<Engine::Core::RenderableComponent>();
      renderable->renderer_id = profile.visuals.renderer_id;
      renderable->visible = true;
    }
    ++owner_id;
  }
}

auto prewarmed_renderer(Renderer& renderer,
                        Engine::Core::World& world,
                        std::size_t nation_limit = SIZE_MAX) -> bool {
  if (!renderer.initialize()) {
    return false;
  }
  add_every_troop(world, nation_limit);
  renderer.set_world_view(
      Render::WorldView::of(Game::Session::SessionContext::active()));
  renderer.prewarm_unit_templates(&world);
  Render::Creature::set_runtime_bake_forbidden(false);
  return true;
}

void load_default_content() {
  auto& nations = Game::Systems::NationRegistry::instance();
  nations.clear();
  Game::Systems::initialize_default_content(nations);
  Game::Systems::TroopProfileService::instance().clear();
  Game::Systems::TroopProfileService::instance().prime();
}

} // namespace

TEST(AttachmentMeshPack, ShippedPackCoversOneNationsTroops) {
  load_default_content();
  AttachmentMeshLibrary::instance().clear();
  Renderer renderer(Render::ShaderQuality::None);
  Engine::Core::World world;
  ASSERT_TRUE(prewarmed_renderer(renderer, world, 1U));
  EXPECT_EQ(renderer.rigged_mesh_cache().runtime_attachment_bakes(), 0U)
      << "equipment sets the pack does not have";
  std::size_t named = 0;
  renderer.rigged_mesh_cache().for_each_named_attachment_mesh(
      [&](const std::string&, const RiggedMesh&) { ++named; });
  EXPECT_GT(named, 10U);
}

TEST(CreatureSpeciesPrewarm, EverySpeciesBodyIsReadyBeforePlay) {
  load_default_content();
  Renderer renderer(Render::ShaderQuality::None);
  Engine::Core::World world;
  ASSERT_TRUE(prewarmed_renderer(renderer, world, 1U));

  auto const& archetypes = Render::Creature::ArchetypeRegistry::instance();
  auto& handles =
      Render::Creature::Pipeline::CreatureRenderAssetHandleRegistry::instance();
  std::size_t checked = 0;
  for (std::size_t index = 0; index < archetypes.size(); ++index) {
    const auto* archetype =
        archetypes.get(static_cast<Render::Creature::ArchetypeId>(index));
    if (archetype == nullptr || archetype->bake_attachment_count != 0U) {
      continue;
    }
    const auto* handle = handles.get(handles.get_or_create(
        Render::Creature::Pipeline::k_invalid_creature_asset, archetype->id));
    if (handle == nullptr || !handle->valid()) {
      continue;
    }
    for (const auto& playback : handle->playback) {
      if (playback.blob == nullptr || playback.frame_count == 0U) {
        continue;
      }
      const auto key = Render::Creature::Pipeline::rigged_asset_key(
          *handle, Render::Creature::CreatureLOD::Full, playback.blob->species_id());
      EXPECT_NE(renderer.rigged_mesh_cache().find_rigged_asset(key), nullptr)
          << archetype->debug_name << " has no rigged body after the prewarm";
      ++checked;
      break;
    }
  }
  EXPECT_GE(checked, 4U);
}

TEST(AttachmentMeshPack, ShippedPackCoversEveryTroopOfEveryNation) {
  load_default_content();
  ASSERT_FALSE(Game::Systems::NationRegistry::instance().get_all_nations().empty());

  auto& library = AttachmentMeshLibrary::instance();
  library.clear();
  ASSERT_GT(library.size(), 0U)
      << "assets/meshes/attachments.rmpk is missing; build bake_creature_assets";

  Renderer shipped(Render::ShaderQuality::None);
  Engine::Core::World shipped_world;
  ASSERT_TRUE(prewarmed_renderer(shipped, shipped_world));
  EXPECT_EQ(shipped.rigged_mesh_cache().runtime_attachment_bakes(), 0U)
      << "equipment sets the pack does not have";

  library.start_empty();
  Renderer fresh(Render::ShaderQuality::None);
  Engine::Core::World fresh_world;
  ASSERT_TRUE(prewarmed_renderer(fresh, fresh_world));
  library.clear();

  std::size_t compared = 0;
  fresh.rigged_mesh_cache().for_each_named_attachment_mesh(
      [&](const std::string& key, const RiggedMesh& mesh) {
        const auto baked = library.find(key);
        ASSERT_NE(baked, nullptr) << key;
        ASSERT_EQ(baked->indices, mesh.get_indices()) << key;
        ASSERT_EQ(baked->vertices.size(), mesh.get_vertices().size()) << key;
        for (std::size_t i = 0; i < baked->vertices.size(); ++i) {
          for (int axis = 0; axis < 3; ++axis) {
            ASSERT_NEAR(baked->vertices[i].position_bone_local[axis],
                        mesh.get_vertices()[i].position_bone_local[axis],
                        1.0e-4F)
                << key << " is stale";
          }
        }
        ++compared;
      });
  EXPECT_GT(compared, 100U);
}
