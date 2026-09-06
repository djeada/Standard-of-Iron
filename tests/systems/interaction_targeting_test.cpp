#include <algorithm>
#include <gtest/gtest.h>
#include <vector>

#include "core/component_structures.h"
#include "core/world.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/interaction_targeting.h"
#include "game/systems/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/pathfinding.h"
#include "game/units/spawn_type.h"

namespace {

using Game::Map::WorldProp;
using Game::Systems::InteractionAction;

constexpr int k_owner = 1;
constexpr int k_enemy = 2;

class InteractionTargetingTest : public ::testing::Test {
protected:
  void SetUp() override {
    reset_shared_state();
    auto& owners = Game::Systems::OwnerRegistry::instance();
    owners.register_owner_with_id(k_owner, Game::Systems::OwnerType::Player, "Player");
    owners.register_owner_with_id(k_enemy, Game::Systems::OwnerType::AI, "Rome");
  }

  void TearDown() override { reset_shared_state(); }

  static void reset_shared_state() {
    Game::Map::TerrainService::instance().clear();
    Game::Map::VisibilityService::instance().reset();
    Game::Systems::OwnerRegistry::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
  }

  struct Node {
    WorldProp::Type type{WorldProp::Type::PineTree};
    float x{0.0F};
    float z{0.0F};
  };

  static constexpr int k_extent = 24;

  static void lay_out(const std::vector<Node>& nodes) {
    const float origin = (static_cast<float>(k_extent) * 0.5F) - 0.5F;
    Game::Map::MapDefinition map_def;
    map_def.grid.width = k_extent;
    map_def.grid.height = k_extent;
    map_def.grid.tile_size = 1.0F;
    map_def.biome.procedural_trees_enabled = false;
    map_def.biome.procedural_boulders_enabled = false;
    map_def.biome.procedural_iron_ore_enabled = false;
    for (const auto& node : nodes) {
      map_def.world_props.push_back(
          {.type = node.type, .x = node.x + origin, .z = node.z + origin});
    }
    Game::Map::TerrainService::instance().initialize(map_def);
    Game::Map::VisibilityService::instance().initialize(
        map_def.grid.width, map_def.grid.height, map_def.grid.tile_size);
    Game::Map::VisibilityService::instance().reveal_all();
    Game::Systems::NavGrid::initialize(map_def.grid.width, map_def.grid.height);
    if (auto* pathfinder = Game::Systems::NavGrid::get_pathfinder()) {
      pathfinder->update_navigation_grid();
    }
  }

  static auto add_building(Engine::Core::World& world,
                           Game::Units::SpawnType type,
                           int owner_id,
                           float x,
                           float z) -> Engine::Core::Entity* {
    auto* entity = world.create_entity();
    auto* transform = entity->add_component<Engine::Core::TransformComponent>();
    transform->position = {x, 0.0F, z};
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    unit->spawn_type = type;
    unit->owner_id = owner_id;
    unit->health = 500;
    unit->max_health = 500;
    entity->add_component<Engine::Core::BuildingComponent>();
    return entity;
  }

  static auto request_for(Engine::Core::World& world, bool builders, bool civilians)
      -> Game::Systems::InteractionTargetingRequest {
    Game::Systems::InteractionTargetingRequest request;
    request.world = &world;
    request.local_owner_id = k_owner;
    request.has_builders = builders;
    request.has_civilians = civilians;
    request.max_distance = Game::Systems::k_interaction_highlight_max_distance;
    request.max_markers = Game::Systems::k_interaction_highlight_max_markers;
    return request;
  }

  static auto count_of(const Game::Systems::InteractionTargetingHighlights& highlights,
                       InteractionAction action) -> int {
    return static_cast<int>(
        std::count_if(highlights.markers.begin(),
                      highlights.markers.end(),
                      [action](const Game::Systems::InteractionTargetMarker& marker) {
                        return marker.action == action;
                      }));
  }
};

TEST_F(InteractionTargetingTest, BuildersLightUpTheResourcesTheyCanWork) {
  lay_out({{.type = WorldProp::Type::PineTree, .x = 3.0F, .z = 0.0F},
           {.type = WorldProp::Type::Boulder, .x = -3.0F, .z = 0.0F},
           {.type = WorldProp::Type::IronOre, .x = 0.0F, .z = 4.0F}});
  Engine::Core::World world;

  const auto highlights = Game::Systems::collect_interaction_target_highlights(
      request_for(world, true, false));

  EXPECT_EQ(count_of(highlights, InteractionAction::Gather), 3);
}

TEST_F(InteractionTargetingTest, NothingIsLitUpWithoutABuilderOrCivilian) {
  lay_out({{.type = WorldProp::Type::PineTree, .x = 3.0F, .z = 0.0F}});
  Engine::Core::World world;

  const auto highlights = Game::Systems::collect_interaction_target_highlights(
      request_for(world, false, false));

  EXPECT_TRUE(highlights.markers.empty())
      << "deselecting the workers should clear the highlights";
}

TEST_F(InteractionTargetingTest, ANodeSomebodyElseIsAlreadyWorkingIsLeftAlone) {
  lay_out({{.type = WorldProp::Type::PineTree, .x = 3.0F, .z = 0.0F},
           {.type = WorldProp::Type::PineTree, .x = 6.0F, .z = 0.0F}});
  Engine::Core::World world;

  auto& terrain = Game::Map::TerrainService::instance();
  const auto claimed = terrain.world_props().front().id;
  ASSERT_TRUE(terrain.reserve_world_prop(claimed));

  const auto highlights = Game::Systems::collect_interaction_target_highlights(
      request_for(world, true, false));

  EXPECT_EQ(count_of(highlights, InteractionAction::Gather), 1);
  for (const auto& marker : highlights.markers) {
    EXPECT_NE(marker.world_prop_id, claimed);
  }
}

TEST_F(InteractionTargetingTest, ADepletedNodeIsNoLongerOffered) {
  lay_out({{.type = WorldProp::Type::PineTree, .x = 3.0F, .z = 0.0F}});
  Engine::Core::World world;

  auto& terrain = Game::Map::TerrainService::instance();
  const auto node = terrain.world_props().front().id;
  ASSERT_TRUE(terrain.harvest_world_prop(node));

  const auto highlights = Game::Systems::collect_interaction_target_highlights(
      request_for(world, true, false));

  EXPECT_EQ(count_of(highlights, InteractionAction::Gather), 0);
}

TEST_F(InteractionTargetingTest, GroundNobodyHasScoutedStaysDark) {
  lay_out({{.type = WorldProp::Type::PineTree, .x = 3.0F, .z = 0.0F}});
  Engine::Core::World world;

  auto& visibility = Game::Map::VisibilityService::instance();
  visibility.reset();
  visibility.initialize(k_extent, k_extent, 1.0F);
  const auto snapshot = visibility.snapshot_ptr();

  auto request = request_for(world, true, false);
  request.visibility = snapshot.get();

  const auto highlights = Game::Systems::collect_interaction_target_highlights(request);

  EXPECT_EQ(count_of(highlights, InteractionAction::Gather), 0)
      << "fog must not give away what is out there";
}

TEST_F(InteractionTargetingTest, CiviliansLightUpABarracksWithRoom) {
  lay_out({});
  Engine::Core::World world;

  auto* barracks =
      add_building(world, Game::Units::SpawnType::Barracks, k_owner, 4.0F, 0.0F);
  auto* production = barracks->add_component<Engine::Core::ProductionComponent>();
  production->manpower_available = 0;
  production->max_units = 20;

  const auto highlights = Game::Systems::collect_interaction_target_highlights(
      request_for(world, false, true));

  ASSERT_EQ(count_of(highlights, InteractionAction::Deliver), 1);
  EXPECT_EQ(highlights.markers.front().entity_id, barracks->get_id());
}

TEST_F(InteractionTargetingTest, AFullBarracksIsNotOffered) {
  lay_out({});
  Engine::Core::World world;

  auto* barracks =
      add_building(world, Game::Units::SpawnType::Barracks, k_owner, 4.0F, 0.0F);
  auto* production = barracks->add_component<Engine::Core::ProductionComponent>();
  production->manpower_available = 20;
  production->max_units = 20;

  const auto highlights = Game::Systems::collect_interaction_target_highlights(
      request_for(world, false, true));

  EXPECT_EQ(count_of(highlights, InteractionAction::Deliver), 0);
}

TEST_F(InteractionTargetingTest, TheEnemyCampIsNeverOffered) {
  lay_out({});
  Engine::Core::World world;

  auto* barracks =
      add_building(world, Game::Units::SpawnType::Barracks, k_enemy, 4.0F, 0.0F);
  auto* production = barracks->add_component<Engine::Core::ProductionComponent>();
  production->manpower_available = 0;
  production->max_units = 20;

  auto* damaged =
      add_building(world, Game::Units::SpawnType::DefenseTower, k_enemy, 6.0F, 0.0F);
  damaged->get_component<Engine::Core::UnitComponent>()->health = 100;

  const auto highlights = Game::Systems::collect_interaction_target_highlights(
      request_for(world, true, true));

  EXPECT_TRUE(highlights.markers.empty())
      << "nothing of the enemy's is a valid context action";
}

TEST_F(InteractionTargetingTest, BuildersAreOfferedTheirOwnDamagedBuildings) {
  lay_out({});
  Engine::Core::World world;

  auto* whole =
      add_building(world, Game::Units::SpawnType::DefenseTower, k_owner, 4.0F, 0.0F);
  auto* hurt =
      add_building(world, Game::Units::SpawnType::DefenseTower, k_owner, 6.0F, 0.0F);
  hurt->get_component<Engine::Core::UnitComponent>()->health = 120;

  const auto highlights = Game::Systems::collect_interaction_target_highlights(
      request_for(world, true, false));

  ASSERT_EQ(count_of(highlights, InteractionAction::Repair), 1);
  EXPECT_EQ(highlights.markers.front().entity_id, hurt->get_id());
  EXPECT_NE(highlights.markers.front().entity_id, whole->get_id());
}

TEST_F(InteractionTargetingTest, AMixedSelectionShowsEveryActionSomebodyCanDo) {
  lay_out({{.type = WorldProp::Type::PineTree, .x = 3.0F, .z = 0.0F}});
  Engine::Core::World world;

  auto* barracks =
      add_building(world, Game::Units::SpawnType::Barracks, k_owner, 4.0F, 0.0F);
  auto* production = barracks->add_component<Engine::Core::ProductionComponent>();
  production->manpower_available = 0;
  production->max_units = 20;

  const auto highlights = Game::Systems::collect_interaction_target_highlights(
      request_for(world, true, true));

  EXPECT_EQ(count_of(highlights, InteractionAction::Gather), 1);
  EXPECT_EQ(count_of(highlights, InteractionAction::Deliver), 1);
}

TEST_F(InteractionTargetingTest, ACivilianOnItsOwnIsNotOfferedResourceNodes) {
  lay_out({{.type = WorldProp::Type::PineTree, .x = 3.0F, .z = 0.0F}});
  Engine::Core::World world;

  const auto highlights = Game::Systems::collect_interaction_target_highlights(
      request_for(world, false, true));

  EXPECT_EQ(count_of(highlights, InteractionAction::Gather), 0);
}

TEST_F(InteractionTargetingTest, HoveringATargetNamesTheActionItOffers) {
  lay_out({});
  Engine::Core::World world;

  auto* barracks =
      add_building(world, Game::Units::SpawnType::Barracks, k_owner, 4.0F, 0.0F);
  auto* production = barracks->add_component<Engine::Core::ProductionComponent>();
  production->manpower_available = 0;
  production->max_units = 20;

  auto request = request_for(world, false, true);
  request.hovered_entity_id = barracks->get_id();

  const auto highlights = Game::Systems::collect_interaction_target_highlights(request);

  EXPECT_EQ(highlights.hovered_action, InteractionAction::Deliver);
  EXPECT_EQ(Game::Systems::interaction_action_key(highlights.hovered_action),
            "deliver");
  ASSERT_FALSE(highlights.markers.empty());
  EXPECT_TRUE(highlights.markers.front().hovered);
}

TEST_F(InteractionTargetingTest, HoveringAResourceNodeNamesItsActionToo) {
  lay_out({{.type = WorldProp::Type::Boulder, .x = 3.0F, .z = 0.0F}});
  Engine::Core::World world;

  auto request = request_for(world, true, false);
  request.has_hovered_ground = true;
  request.hovered_ground_x = 3.0F;
  request.hovered_ground_z = 0.0F;

  const auto highlights = Game::Systems::collect_interaction_target_highlights(request);

  ASSERT_EQ(highlights.markers.size(), 1U);
  EXPECT_TRUE(highlights.markers.front().hovered)
      << "a node has no entity, so the cursor has to be matched against the ground";
  EXPECT_EQ(highlights.hovered_action, InteractionAction::Gather);
}

TEST_F(InteractionTargetingTest, PointingAtEmptyGroundNamesNoAction) {
  lay_out({{.type = WorldProp::Type::Boulder, .x = 3.0F, .z = 0.0F}});
  Engine::Core::World world;

  auto request = request_for(world, true, false);
  request.has_hovered_ground = true;
  request.hovered_ground_x = -9.0F;
  request.hovered_ground_z = 0.0F;

  const auto highlights = Game::Systems::collect_interaction_target_highlights(request);

  EXPECT_EQ(highlights.hovered_action, InteractionAction::None);
  ASSERT_EQ(highlights.markers.size(), 1U);
  EXPECT_FALSE(highlights.markers.front().hovered);
}

TEST_F(InteractionTargetingTest, TheListIsCappedSoTheFieldDoesNotFillWithRings) {
  std::vector<Node> forest;
  for (int i = 0; i < 40; ++i) {
    forest.push_back({.type = WorldProp::Type::PineTree,
                      .x = static_cast<float>(-10 + (i % 20)),
                      .z = static_cast<float>(-4 + (i / 20) * 2)});
  }
  lay_out(forest);
  Engine::Core::World world;

  auto request = request_for(world, true, false);
  request.max_markers = 8;

  const auto highlights = Game::Systems::collect_interaction_target_highlights(request);

  EXPECT_LE(highlights.markers.size(), 8U);
}

} // namespace
