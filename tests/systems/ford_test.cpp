#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryFile>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <optional>
#include <vector>

#include "core/component_core.h"
#include "core/component_gameplay.h"
#include "core/world.h"
#include "game/map/map_definition.h"
#include "game/map/map_loader.h"
#include "game/map/terrain.h"
#include "game/map/terrain_service.h"
#include "game/save/entity_codec.h"
#include "game/save/serialization.h"
#include "game/session/session_context.h"
#include "game/systems/ai_system/ai_command_applier.h"
#include "game/systems/ai_system/ai_types.h"
#include "game/systems/default_content.h"
#include "game/systems/ford_rules.h"
#include "game/systems/ford_system.h"
#include "game/systems/movement/command_service.h"
#include "game/systems/movement/route_follow_system_gate.h"
#include "game/systems/nation_registry.h"
#include "game/systems/navigation/nav_grid.h"
#include "game/systems/navigation/pathfinding.h"
#include "game/systems/owner_registry.h"
#include "game/systems/runtime_system_registry.h"
#include "units/spawn_type.h"

namespace {

using Game::Map::FordCrossing;
using Game::Map::FordProfile;
using Game::Map::TerrainService;
namespace FordRules = Game::Systems::FordRules;

constexpr int k_grid = 64;
constexpr float k_river_width = 8.0F;
constexpr float k_tick = 1.0F / 30.0F;

// A river running north-south down the middle of the map (x = 0).
auto make_river_map() -> Game::Map::MapDefinition {
  Game::Map::MapDefinition map;
  map.coordSystem = Game::Map::CoordSystem::World;
  map.grid.width = k_grid;
  map.grid.height = k_grid;
  map.grid.tile_size = 1.0F;
  map.biome.procedural_trees_enabled = false;
  map.biome.procedural_boulders_enabled = false;
  map.biome.procedural_iron_ore_enabled = false;
  map.rivers.push_back(
      {QVector3D(0.0F, 0.0F, -40.0F), QVector3D(0.0F, 0.0F, 40.0F), k_river_width});
  return map;
}

auto ford_at(float z, float length = 10.0F, FordProfile profile = {}) -> FordCrossing {
  FordCrossing ford;
  ford.id = QStringLiteral("ford");
  ford.position = QVector3D(0.0F, 0.0F, z);
  ford.length = length;
  ford.profile = profile;
  return ford;
}

auto build_navigation(const Game::Map::MapDefinition& map)
    -> Game::Systems::Pathfinding* {
  TerrainService::instance().initialize(map);
  Game::Systems::NavGrid::initialize(map.grid.width, map.grid.height);
  auto* pathfinder = Game::Systems::NavGrid::get_pathfinder();
  if (pathfinder != nullptr) {
    pathfinder->mark_navigation_grid_dirty();
    pathfinder->update_navigation_grid();
  }
  return pathfinder;
}

auto walkable_at(float x, float z) -> bool {
  auto const* height_map = TerrainService::instance().get_height_map();
  float const half = static_cast<float>(k_grid) * 0.5F - 0.5F;
  return height_map->is_walkable(static_cast<int>(std::lround(x + half)),
                                 static_cast<int>(std::lround(z + half)));
}

auto path_between(Game::Systems::Pathfinding& pathfinder,
                  const QVector3D& from,
                  const QVector3D& to) -> std::vector<Game::Systems::Point> {
  return pathfinder.find_path(pathfinder.world_to_grid(from.x(), from.z()),
                              pathfinder.world_to_grid(to.x(), to.z()));
}

auto reaches(const std::vector<Game::Systems::Point>& path,
             Game::Systems::Pathfinding& pathfinder,
             const QVector3D& to) -> bool {
  auto const goal = pathfinder.world_to_grid(to.x(), to.z());
  return !path.empty() && path.back().x == goal.x && path.back().y == goal.y;
}

auto wades(const std::vector<Game::Systems::Point>& path) -> bool {
  auto const* height_map = TerrainService::instance().get_height_map();
  return std::any_of(path.begin(), path.end(), [height_map](const auto& cell) {
    return height_map->is_ford_cell(cell.x, cell.y);
  });
}

class FordTest : public ::testing::Test {
protected:
  void SetUp() override {
    TerrainService::instance().clear();
    auto& session = Game::Session::SessionContext::active();
    session.owners().clear();
    session.owners().register_owner_with_id(1, Game::Systems::OwnerType::Player, "P1");
    session.owners().register_owner_with_id(2, Game::Systems::OwnerType::AI, "AI");
    session.owners().set_local_player_id(1);
    session.nations().clear();
    Game::Systems::initialize_default_content(session.nations());
    session.nations().set_player_nation(1, Game::Systems::NationID::RomanRepublic);
    session.nations().set_player_nation(2, Game::Systems::NationID::Carthage);
  }

  void TearDown() override {
    TerrainService::instance().clear();
    Game::Session::SessionContext::active().owners().clear();
    Game::Session::SessionContext::active().nations().clear();
  }

  static auto add_unit(Engine::Core::World& world,
                       const QVector3D& at,
                       int owner = 1) -> Engine::Core::Entity* {
    auto* entity = world.create_entity();
    auto* transform = entity->add_component<Engine::Core::TransformComponent>();
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    entity->add_component<Engine::Core::MovementComponent>();
    entity->add_component<Engine::Core::StaminaComponent>();
    transform->position = {at.x(), 0.0F, at.z()};
    unit->owner_id = owner;
    unit->spawn_type = Game::Units::SpawnType::Swordsman;
    unit->nation_id = owner == 1 ? Game::Systems::NationID::RomanRepublic
                                 : Game::Systems::NationID::Carthage;
    unit->health = 1000;
    unit->max_health = 1000;
    unit->speed = 2.1F;
    return entity;
  }
};

} // namespace

TEST_F(FordTest, AFordOpensOnlyItsStretchOfTheRiver) {
  auto map = make_river_map();
  map.fords = {ford_at(10.0F, 10.0F)};
  TerrainService::instance().initialize(map);
  auto const& terrain = TerrainService::instance();

  EXPECT_TRUE(walkable_at(0.0F, 10.0F)) << "mid-stream in the ford can be waded";
  EXPECT_TRUE(walkable_at(2.0F, 13.0F));
  EXPECT_FALSE(walkable_at(0.0F, -15.0F)) << "the rest of the river stays impassable";
  EXPECT_FALSE(walkable_at(0.0F, 25.0F));

  ASSERT_NE(terrain.ford_profile_at(0.0F, 10.0F), nullptr);
  EXPECT_EQ(terrain.ford_profile_at(0.0F, -15.0F), nullptr);
  EXPECT_EQ(terrain.ford_profile_at(20.0F, 10.0F), nullptr) << "dry land is no ford";

  float const depth = terrain.ford_water_depth_at(0.0F, 10.0F);
  EXPECT_NEAR(depth, Game::Map::k_default_ford_depth, 0.08F)
      << "the bed is lowered so men stand waist-deep mid-stream";
  EXPECT_LT(terrain.ford_water_depth_at(3.9F, 10.0F), depth)
      << "the water shallows towards the bank";
  EXPECT_FLOAT_EQ(terrain.ford_water_depth_at(20.0F, 10.0F), 0.0F);
}

TEST_F(FordTest, AWholeRiverCanBeFlaggedFordableAndAFordOffTheWaterIsDropped) {
  auto map = make_river_map();
  map.rivers.front().ford = FordProfile{.depth = 0.5F, .speed = 0.6F, .cold = 0.8F};
  map.fords = {ford_at(0.0F)};
  map.fords.front().position = QVector3D(25.0F, 0.0F, 0.0F);
  TerrainService::instance().initialize(map);

  EXPECT_TRUE(walkable_at(0.0F, -25.0F));
  EXPECT_TRUE(walkable_at(0.0F, 0.0F));
  EXPECT_TRUE(walkable_at(0.0F, 25.0F));
  auto const* profile = TerrainService::instance().ford_profile_at(0.0F, 20.0F);
  ASSERT_NE(profile, nullptr);
  EXPECT_FLOAT_EQ(profile->cold, 0.8F);
  EXPECT_FLOAT_EQ(profile->speed, 0.6F);
  EXPECT_TRUE(TerrainService::instance().get_height_map()->get_fords().empty())
      << "a ford authored on dry land is skipped";
}

TEST_F(FordTest, MapJsonDeclaresFordsOnRiversAndAsZones) {
  QJsonObject root{
      {"name", "Ford test"},
      {"coord_system", "world"},
      {"grid", QJsonObject{{"width", k_grid}, {"height", k_grid}, {"tile_size", 1.0}}},
      {"rivers",
       QJsonArray{QJsonObject{{"start", QJsonArray{0, -40}},
                              {"end", QJsonArray{0, 40}},
                              {"width", 8},
                              {"ford", QJsonObject{{"depth", 0.5}, {"cold", 0.7}}}},
                  QJsonObject{{"start", QJsonArray{-30, 20}},
                              {"end", QJsonArray{-30, 30}},
                              {"width", 6},
                              {"ford", true}}}},
      {"fords",
       QJsonArray{QJsonObject{{"id", "trebia_ford"},
                              {"position", QJsonArray{0, 12}},
                              {"length", 18},
                              {"speed", 0.5},
                              {"exposure", 1.4}},
                  QJsonObject{{"start", QJsonArray{-6, -10}},
                              {"end", QJsonArray{6, -10}},
                              {"width", 12}}}}};
  QTemporaryFile file;
  ASSERT_TRUE(file.open());
  file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  file.flush();
  Game::Map::MapDefinition map;
  QString error;
  ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(file.fileName(), map, &error))
      << error.toStdString();

  ASSERT_GE(map.rivers.size(), 2U);
  ASSERT_TRUE(map.rivers.front().ford.has_value());
  EXPECT_FLOAT_EQ(map.rivers.front().ford->depth, 0.5F);
  EXPECT_FLOAT_EQ(map.rivers.front().ford->cold, 0.7F);
  EXPECT_FLOAT_EQ(map.rivers.front().ford->speed, Game::Map::k_default_ford_speed);
  ASSERT_TRUE(map.rivers.back().ford.has_value());
  EXPECT_EQ(*map.rivers.back().ford, FordProfile{});

  ASSERT_EQ(map.fords.size(), 2U);
  EXPECT_EQ(map.fords[0].id, QStringLiteral("trebia_ford"));
  EXPECT_FLOAT_EQ(map.fords[0].position.z(), 12.0F);
  EXPECT_FLOAT_EQ(map.fords[0].length, 18.0F);
  EXPECT_FLOAT_EQ(map.fords[0].profile.speed, 0.5F);
  EXPECT_FLOAT_EQ(map.fords[0].profile.exposure, 1.4F);
  EXPECT_EQ(map.fords[1].id, QStringLiteral("ford_2"));
  EXPECT_FLOAT_EQ(map.fords[1].position.x(), 0.0F)
      << "a ford drawn bank to bank sits at the middle of its line";
  EXPECT_FLOAT_EQ(map.fords[1].position.z(), -10.0F);
  EXPECT_FLOAT_EQ(map.fords[1].length, 12.0F);
}

TEST_F(FordTest, SavedTerrainKeepsItsFords) {
  auto map = make_river_map();
  map.fords = {ford_at(10.0F, 12.0F, {.depth = 0.5F, .speed = 0.4F, .cold = 0.9F})};
  TerrainService::instance().initialize(map);
  auto const* original = TerrainService::instance().get_height_map();

  QJsonObject const saved =
      Engine::Core::Serialization::serialize_terrain(original, map.biome, {}, {}, {});
  Game::Map::TerrainHeightMap restored(k_grid, k_grid, 1.0F);
  Game::Map::BiomeSettings biome;
  std::vector<Game::Map::RoadSegment> roads;
  std::vector<Game::Map::WorldProp> props;
  std::vector<Game::Map::WorldProp> authored;
  Engine::Core::Serialization::deserialize_terrain(
      &restored, biome, roads, props, authored, saved);

  ASSERT_EQ(restored.get_fords().size(), 1U);
  EXPECT_EQ(restored.get_fords().front().id, QStringLiteral("ford"));
  EXPECT_FLOAT_EQ(restored.get_fords().front().length, 12.0F);
  for (int z = 0; z < k_grid; ++z) {
    for (int x = 0; x < k_grid; ++x) {
      ASSERT_EQ(restored.is_ford_cell(x, z), original->is_ford_cell(x, z))
          << "cell " << x << "," << z;
      ASSERT_EQ(restored.is_walkable(x, z), original->is_walkable(x, z));
    }
  }
  auto const* profile = restored.ford_profile_at(0.0F, 10.0F);
  ASSERT_NE(profile, nullptr);
  EXPECT_FLOAT_EQ(profile->cold, 0.9F);
  EXPECT_NEAR(restored.ford_water_depth_at(0.0F, 10.0F),
              original->ford_water_depth_at(0.0F, 10.0F),
              1.0e-4F)
      << "the lowered bed is saved with the heights, not lowered twice";

  auto river_map = make_river_map();
  river_map.rivers.front().ford = FordProfile{.cold = 0.5F};
  TerrainService::instance().initialize(river_map);
  QJsonObject const river_saved = Engine::Core::Serialization::serialize_terrain(
      TerrainService::instance().get_height_map(), river_map.biome, {}, {}, {});
  Game::Map::TerrainHeightMap river_restored(k_grid, k_grid, 1.0F);
  Engine::Core::Serialization::deserialize_terrain(
      &river_restored, biome, roads, props, authored, river_saved);
  ASSERT_FALSE(river_restored.get_river_segments().empty());
  ASSERT_TRUE(river_restored.get_river_segments().front().ford.has_value());
  EXPECT_FLOAT_EQ(river_restored.get_river_segments().front().ford->cold, 0.5F);
  EXPECT_NE(river_restored.ford_profile_at(0.0F, -30.0F), nullptr);
}

TEST_F(FordTest, ChillFromIcyWaterSurvivesASave) {
  Engine::Core::World world;
  auto* entity = add_unit(world, QVector3D(20.0F, 0.0F, 0.0F));
  auto* wading = world.emplace<Engine::Core::WadingComponent>(entity->get_id());
  wading->chill = 0.6F;
  wading->cold = 0.8F;

  QJsonObject saved;
  Engine::Core::EntityCodec::write_core(entity, saved);
  Engine::Core::World restored_world;
  auto* restored = restored_world.create_entity();
  Engine::Core::EntityCodec::read_core(restored, saved);
  auto const* back =
      restored_world.try_get<Engine::Core::WadingComponent>(restored->get_id());
  ASSERT_NE(back, nullptr);
  EXPECT_FLOAT_EQ(back->chill, 0.6F);
  EXPECT_FLOAT_EQ(back->cold, 0.8F);
}

TEST_F(FordTest, PathfindingWadesTheFordWhenNoBridgeIsNear) {
  auto map = make_river_map();
  auto* dry = build_navigation(map);
  ASSERT_NE(dry, nullptr);
  QVector3D const west(-15.0F, 0.0F, 0.0F);
  QVector3D const east(15.0F, 0.0F, 0.0F);
  EXPECT_FALSE(reaches(path_between(*dry, west, east), *dry, east))
      << "without a ford the river cannot be crossed";

  map.fords = {ford_at(0.0F, 10.0F)};
  auto* pathfinder = build_navigation(map);
  auto const path = path_between(*pathfinder, west, east);
  ASSERT_TRUE(reaches(path, *pathfinder, east));
  EXPECT_TRUE(wades(path));
  EXPECT_TRUE(pathfinder->can_reach(pathfinder->world_to_grid(west.x(), west.z()),
                                    pathfinder->world_to_grid(east.x(), east.z())))
      << "region connectivity (what the AI checks) joins the banks";
}

TEST_F(FordTest, PathfindingPrefersABridgeCloseByAndWadesWhenItIsFar) {
  auto map = make_river_map();
  map.fords = {ford_at(0.0F, 10.0F)};
  Game::Map::Bridge bridge;
  bridge.start = QVector3D(-9.0F, 0.0F, 9.0F);
  bridge.end = QVector3D(9.0F, 0.0F, 9.0F);
  bridge.width = 8.0F;
  map.bridges = {bridge};
  auto* pathfinder = build_navigation(map);
  ASSERT_NE(pathfinder, nullptr);

  QVector3D const west(-15.0F, 0.0F, 3.0F);
  QVector3D const east(15.0F, 0.0F, 3.0F);
  auto const near_bridge = path_between(*pathfinder, west, east);
  ASSERT_TRUE(reaches(near_bridge, *pathfinder, east));
  EXPECT_FALSE(wades(near_bridge)) << "a bridge a few steps away beats wading";

  map.bridges.front().start = QVector3D(-9.0F, 0.0F, 28.0F);
  map.bridges.front().end = QVector3D(9.0F, 0.0F, 28.0F);
  pathfinder = build_navigation(map);
  QVector3D const west_far(-15.0F, 0.0F, -10.0F);
  QVector3D const east_far(15.0F, 0.0F, -10.0F);
  auto const far_bridge = path_between(*pathfinder, west_far, east_far);
  ASSERT_TRUE(reaches(far_bridge, *pathfinder, east_far));
  EXPECT_TRUE(wades(far_bridge)) << "a long detour to the bridge loses to the ford";
}

TEST_F(FordTest, WadeStepCostTracksTheFordsSpeed) {
  EXPECT_EQ(Game::Systems::Pathfinding::wade_step_penalty(nullptr), 0);
  FordProfile slow{.speed = 0.4F};
  FordProfile quick{.speed = 0.9F};
  EXPECT_GT(Game::Systems::Pathfinding::wade_step_penalty(&slow),
            Game::Systems::Pathfinding::wade_step_penalty(&quick));
  EXPECT_GE(Game::Systems::Pathfinding::wade_step_penalty(&quick),
            Game::Systems::Pathfinding::k_wade_exposure_penalty);
}

TEST_F(FordTest, WadingRulesSlowExposeAndChill) {
  Engine::Core::WadingComponent wading;
  wading.depth = 0.45F;
  wading.speed = 0.55F;
  wading.exposure = 1.25F;
  EXPECT_FLOAT_EQ(FordRules::speed_multiplier(&wading), 0.55F);
  EXPECT_FALSE(FordRules::can_run(&wading));
  EXPECT_FALSE(FordRules::can_brace(&wading));
  EXPECT_FLOAT_EQ(FordRules::damage_multiplier(nullptr, &wading, false), 1.25F);
  EXPECT_FLOAT_EQ(FordRules::damage_multiplier(nullptr, &wading, true),
                  1.25F * FordRules::k_ranged_exposure_bonus)
      << "missiles find men in the water easier still";
  EXPECT_FLOAT_EQ(FordRules::damage_multiplier(&wading, nullptr, false),
                  FordRules::k_wading_attack_multiplier);

  Engine::Core::WadingComponent bank;
  bank.depth = 0.05F;
  bank.speed = 0.55F;
  bank.exposure = 1.25F;
  EXPECT_FLOAT_EQ(FordRules::speed_multiplier(&bank), 1.0F)
      << "the shallow shelf at the bank does not slow anyone";
  EXPECT_FLOAT_EQ(FordRules::damage_multiplier(&bank, &bank, true), 1.0F);
  EXPECT_TRUE(FordRules::can_brace(&bank));

  float chill = 0.0F;
  for (int i = 0; i < 30 * 20; ++i) {
    chill = FordRules::next_chill(chill, true, 1.0F, k_tick);
  }
  EXPECT_FLOAT_EQ(chill, 1.0F) << "twenty seconds in icy water chills to the bone";
  Engine::Core::WadingComponent chilled;
  chilled.chill = chill;
  EXPECT_FLOAT_EQ(FordRules::speed_multiplier(&chilled),
                  1.0F - FordRules::k_chill_speed_penalty);
  EXPECT_FLOAT_EQ(FordRules::damage_multiplier(&chilled, nullptr, false),
                  1.0F - FordRules::k_chill_attack_penalty);
  EXPECT_FLOAT_EQ(FordRules::next_chill(0.0F, true, 0.0F, 1.0F), 0.0F)
      << "a temperate ford leaves no chill";
}

TEST_F(FordTest, FordSystemTagsWadersAndTheChillLingers) {
  auto map = make_river_map();
  map.fords = {ford_at(0.0F, 10.0F, {.speed = 0.5F, .cold = 1.0F})};
  TerrainService::instance().initialize(map);
  Engine::Core::World world;
  Game::Systems::FordSystem system({.terrain = TerrainService::instance()});

  auto* wader = add_unit(world, QVector3D(0.0F, 0.0F, 0.0F));
  auto* dry = add_unit(world, QVector3D(25.0F, 0.0F, 0.0F));
  auto* stamina = world.try_get<Engine::Core::StaminaComponent>(wader->get_id());
  float const stamina_before = stamina->stamina;

  for (int i = 0; i < 30 * 5; ++i) {
    system.update(&world, k_tick);
  }
  auto const* wading = world.try_get<Engine::Core::WadingComponent>(wader->get_id());
  ASSERT_NE(wading, nullptr);
  EXPECT_TRUE(wading->in_water());
  EXPECT_FLOAT_EQ(wading->speed, 0.5F);
  EXPECT_GT(wading->chill, 0.3F);
  EXPECT_LT(stamina->stamina, stamina_before) << "icy water saps the men";
  EXPECT_EQ(world.try_get<Engine::Core::WadingComponent>(dry->get_id()), nullptr);

  auto const* unit = world.try_get<Engine::Core::UnitComponent>(wader->get_id());
  float const wading_speed =
      Game::Systems::formation_navigation_speed(*wader, *unit, nullptr);
  float const dry_speed = Game::Systems::formation_navigation_speed(
      *dry, *world.try_get<Engine::Core::UnitComponent>(dry->get_id()), nullptr);
  EXPECT_LT(wading_speed, dry_speed * 0.55F)
      << "wading is slow, and chill slows it more";

  wader->get_component<Engine::Core::TransformComponent>()->position.x = 25.0F;
  system.update(&world, k_tick);
  wading = world.try_get<Engine::Core::WadingComponent>(wader->get_id());
  ASSERT_NE(wading, nullptr) << "the chill lingers after climbing out";
  EXPECT_FALSE(wading->in_water());
  EXPECT_GT(wading->chill, 0.0F);

  for (int i = 0; i < 30 * 90; ++i) {
    system.update(&world, k_tick);
  }
  EXPECT_EQ(world.try_get<Engine::Core::WadingComponent>(wader->get_id()), nullptr)
      << "a dried-off unit sheds the component";
}

TEST_F(FordTest, AnAiPlannerMoveWadesAcrossTheFord) {
  auto map = make_river_map();
  map.fords = {ford_at(-12.0F, 10.0F)};
  build_navigation(map);

  Engine::Core::World world;
  Game::Systems::register_runtime_systems(world);
  std::vector<Engine::Core::EntityID> squad;
  for (int index = 0; index < 4; ++index) {
    auto* entity = add_unit(
        world, QVector3D(-16.0F, 0.0F, 8.0F + static_cast<float>(index) * 1.5F), 2);
    entity->add_component<Engine::Core::AIControlledComponent>();
    squad.push_back(entity->get_id());
  }

  QVector3D const target(16.0F, 0.0F, 8.0F);
  Game::Systems::AI::AICommand command;
  command.type = Game::Systems::AI::AICommandType::MoveUnits;
  command.units = squad;
  command.move_target_x = {target.x()};
  command.move_target_y = {0.0F};
  command.move_target_z = {target.z()};
  Game::Systems::AI::AICommandApplier::apply(world, 2, {command});

  bool waded = false;
  int arrived = 0;
  for (int tick = 0; tick < 30 * 90 && arrived < static_cast<int>(squad.size());
       ++tick) {
    world.update(k_tick);
    arrived = 0;
    for (auto const id : squad) {
      if (auto const* wading = world.try_get<Engine::Core::WadingComponent>(id);
          wading != nullptr && wading->in_water()) {
        waded = true;
      }
      auto const* transform = world.try_get<Engine::Core::TransformComponent>(id);
      if (transform != nullptr && transform->position.x > 6.0F &&
          std::hypot(transform->position.x - target.x(),
                     transform->position.z - target.z()) < 8.0F) {
        ++arrived;
      }
    }
  }
  EXPECT_TRUE(waded) << "the AI's troops took the ford";
  EXPECT_GE(arrived, static_cast<int>(squad.size()) - 1)
      << "the AI's squad reached the far bank";
}
