#include <QFile>
#include <QJsonObject>
#include <QTemporaryDir>

#include <gtest/gtest.h>
#include <optional>

#include "core/component_commander.h"
#include "core/component_core.h"
#include "core/component_gameplay.h"
#include "core/world.h"
#include "game/map/map_definition.h"
#include "game/map/map_loader.h"
#include "game/map/terrain_service.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/rockfall_system.h"
#include "units/spawn_type.h"

namespace {

using Game::Map::RockfallTriggerMode;
using Game::Systems::RockfallSystem;

constexpr float k_tick = 0.05F;

auto make_pass_map(RockfallTriggerMode trigger,
                   int owner_id) -> Game::Map::MapDefinition {
  Game::Map::MapDefinition map;
  map.coordSystem = Game::Map::CoordSystem::World;
  map.grid.width = 72;
  map.grid.height = 72;
  map.grid.tile_size = 1.0F;

  Game::Map::TerrainFeature mountain;
  mountain.type = Game::Map::TerrainType::Mountain;
  mountain.center_x = 0.0F;
  mountain.center_z = -6.0F;
  mountain.radius = 14.0F;
  mountain.height = 12.0F;
  map.terrain.push_back(mountain);

  Game::Map::RockfallTrap trap;
  trap.id = QStringLiteral("heights");
  trap.release_x = 0.0F;
  trap.release_z = -4.0F;
  trap.target_x = 0.0F;
  trap.target_z = 18.0F;
  trap.zone_radius = 7.0F;
  trap.trigger = trigger;
  trap.owner_id = owner_id;
  trap.boulder_count = 5;
  trap.release_spread = 2.0F;
  trap.release_interval = 0.2F;
  map.rockfall_traps.push_back(trap);
  return map;
}

auto services() -> RockfallSystem::Services {
  return {.terrain = Game::Map::TerrainService::instance(),
          .owners = Game::Systems::OwnerRegistry::instance()};
}

auto add_troop(Engine::Core::World& world,
               int owner_id,
               const QVector3D& position) -> Engine::Core::Entity* {
  auto* entity = world.create_entity();
  auto* transform = entity->add_component<Engine::Core::TransformComponent>();
  auto* unit = entity->add_component<Engine::Core::UnitComponent>();
  transform->position = {position.x(), position.y(), position.z()};
  unit->owner_id = owner_id;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;
  unit->spawn_type = Game::Units::SpawnType::Spearman;
  unit->health = 1000;
  unit->max_health = 1000;
  return entity;
}

auto health_of(Engine::Core::Entity* entity) -> int {
  return entity->get_component<Engine::Core::UnitComponent>()->health;
}

void run(Engine::Core::World& world, RockfallSystem& system, float seconds) {
  for (float t = 0.0F; t < seconds; t += k_tick) {
    system.update(&world, k_tick);
  }
}

class RockfallSystemTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto& owners = Game::Systems::OwnerRegistry::instance();
    owners.clear();
    owners.register_owner_with_id(1, Game::Systems::OwnerType::Player, "Hannibal");
    owners.set_owner_team(1, 1);
    owners.set_local_player_id(1);
    owners.register_owner_with_id(2, Game::Systems::OwnerType::AI, "Allobroges");
    owners.set_owner_team(2, 2);
    owners.register_owner_with_id(3, Game::Systems::OwnerType::Player, "Ally");
    owners.set_owner_team(3, 2);
    Game::Map::TerrainService::instance().clear();
  }

  void TearDown() override {
    Game::Map::TerrainService::instance().clear();
    Game::Systems::OwnerRegistry::instance().clear();
  }

  static auto configure(RockfallSystem& system,
                        RockfallTriggerMode trigger,
                        int owner_id) -> Game::Map::MapDefinition {
    auto map = make_pass_map(trigger, owner_id);
    Game::Map::TerrainService::instance().initialize(map);
    system.configure(map);
    return map;
  }
};

} // namespace

TEST_F(RockfallSystemTest, BouldersRollDownTheSlopeIntoThePassAndSettle) {
  Engine::Core::World world;
  RockfallSystem system(services());
  configure(system, RockfallTriggerMode::Scripted, -1);
  ASSERT_EQ(system.trap_count(), 1U);

  auto const trap = system.trap(0);
  ASSERT_GT(trap.release_world.y(), trap.target_world.y() + 3.0F)
      << "the release point has to sit on the heights for this test to mean anything";

  run(world, system, 2.0F);
  EXPECT_TRUE(system.boulders().empty()) << "a scripted trap waits for its cue";
  EXPECT_FALSE(system.trigger(QStringLiteral("unknown")));

  ASSERT_TRUE(system.trigger(QStringLiteral("heights")));
  EXPECT_FALSE(system.trigger(QStringLiteral("heights"))) << "a spent trap stays spent";
  run(world, system, 1.5F);
  auto boulders = system.boulders();
  ASSERT_EQ(boulders.size(), 5U);

  bool any_fast = false;
  bool all_settled = false;
  float rolled = 0.0F;
  for (; rolled < 20.0F && !all_settled; rolled += k_tick) {
    system.update(&world, k_tick);
    all_settled = true;
    for (auto const& boulder : system.boulders()) {
      any_fast = any_fast ||
                 boulder.velocity.length() > Game::Systems::k_rockfall_lethal_speed;
      all_settled = all_settled && boulder.settled;
    }
  }
  EXPECT_TRUE(any_fast) << "the slope accelerates the boulders to a lethal speed";
  ASSERT_TRUE(all_settled) << "boulders come to rest on the flat";

  boulders = system.boulders();
  ASSERT_FALSE(boulders.empty()) << "settled boulders linger as rubble";
  float const release_to_target = (trap.target_world - trap.release_world).length();
  for (auto const& boulder : boulders) {
    EXPECT_TRUE(boulder.settled);
    EXPECT_LT(boulder.position.y(), trap.release_world.y() - 2.0F);
    EXPECT_LT((boulder.position - trap.target_world).length(), release_to_target)
        << "boulders end up nearer the pass than the heights";
    float const ground = Game::Map::TerrainService::instance().get_terrain_height(
        boulder.position.x(), boulder.position.z());
    EXPECT_NEAR(boulder.position.y(), ground + boulder.radius, 0.6F)
        << "a resting boulder sits on the ground";
  }

  run(world, system, Game::Systems::k_rockfall_settled_linger_seconds + 1.0F);
  EXPECT_TRUE(system.boulders().empty()) << "rubble clears after its linger time";
}

TEST_F(RockfallSystemTest, ZoneTrapFiresOnAHostileColumnAndKnocksItDown) {
  Engine::Core::World world;
  RockfallSystem system(services());
  configure(system, RockfallTriggerMode::Zone, 2);
  auto const trap = system.trap(0);

  run(world, system, 1.0F);
  EXPECT_EQ(system.trap(0).times_fired, 0) << "nobody is in the pass yet";

  QVector3D const foot =
      trap.release_world + (trap.target_world - trap.release_world) * 0.7F;
  auto* friendly = add_troop(world, 2, QVector3D(foot.x() + 5.0F, 0.0F, foot.z()));
  run(world, system, 0.5F);
  EXPECT_EQ(system.trap(0).times_fired, 0) << "the trap's own troops never set it off";

  auto* lead = add_troop(world, 1, trap.target_world);
  auto* column = add_troop(world, 1, foot);
  system.update(&world, k_tick);
  EXPECT_EQ(system.trap(0).times_fired, 1);

  run(world, system, 8.0F);
  EXPECT_LT(health_of(column), 1000) << "the troop in the boulders' path is struck";
  EXPECT_GE(system.total_troops_struck(), 1);
  EXPECT_EQ(health_of(friendly), 1000) << "rocks are not rolled onto one's own men";
  if (health_of(column) > 0) {
    EXPECT_NE(column->get_component<Engine::Core::StaggerComponent>(), nullptr)
        << "survivors are knocked down";
  }
  (void)lead;
}

TEST_F(RockfallSystemTest, AiDefenderWaitsForTheColumnBeforeReleasing) {
  Engine::Core::World world;
  RockfallSystem system(services());
  configure(system, RockfallTriggerMode::AiDefender, 2);
  auto const trap = system.trap(0);

  add_troop(world, 1, trap.target_world);
  run(world, system, 1.0F);
  EXPECT_EQ(system.trap(0).times_fired, 0)
      << "one troop is not worth the rocks while the patience lasts";

  add_troop(world, 1, trap.target_world + QVector3D(3.0F, 0.0F, 0.0F));
  system.update(&world, k_tick);
  EXPECT_EQ(system.trap(0).times_fired, 1) << "a second troop fills the pass";
}

TEST_F(RockfallSystemTest, AiDefenderLosesPatienceWithALoneTroop) {
  Engine::Core::World world;
  RockfallSystem system(services());
  configure(system, RockfallTriggerMode::AiDefender, 2);

  add_troop(world, 1, system.trap(0).target_world);
  run(world, system, Game::Systems::k_rockfall_ai_patience_seconds + 0.5F);
  EXPECT_EQ(system.trap(0).times_fired, 1);
}

TEST_F(RockfallSystemTest, OnlyAnAiOwnerUsesTheAiDefenderTrigger) {
  Engine::Core::World world;
  RockfallSystem system(services());
  configure(system, RockfallTriggerMode::AiDefender, 3);

  add_troop(world, 1, system.trap(0).target_world);
  add_troop(world, 1, system.trap(0).target_world + QVector3D(2.0F, 0.0F, 0.0F));
  run(world, system, 5.0F);
  EXPECT_EQ(system.trap(0).times_fired, 0) << "a human owner releases it by script";
}

TEST_F(RockfallSystemTest, StateSurvivesASaveMidRoll) {
  Engine::Core::World world;
  RockfallSystem system(services());
  auto const map = configure(system, RockfallTriggerMode::Scripted, -1);

  ASSERT_TRUE(system.trigger(QStringLiteral("heights")));
  run(world, system, 0.5F);
  auto const saved = system.serialize_state();

  RockfallSystem restored(services());
  restored.configure(map);
  restored.restore_state(saved);
  EXPECT_EQ(restored.serialize_state(), saved);
  EXPECT_FALSE(restored.trigger(QStringLiteral("heights")));

  run(world, system, 2.0F);
  run(world, restored, 2.0F);
  auto const a = system.boulders();
  auto const b = restored.boulders();
  ASSERT_EQ(a.size(), b.size());
  for (std::size_t i = 0; i < a.size(); ++i) {
    EXPECT_LT((a[i].position - b[i].position).length(), 1.0e-3F);
  }
}

TEST_F(RockfallSystemTest, RearmingTrapCanFireAgain) {
  Engine::Core::World world;
  RockfallSystem system(services());
  auto map = make_pass_map(RockfallTriggerMode::Scripted, -1);
  map.rockfall_traps.front().rearm_seconds = 4.0F;
  Game::Map::TerrainService::instance().initialize(map);
  system.configure(map);

  ASSERT_TRUE(system.trigger(QStringLiteral("heights")));
  run(world, system, 2.0F);
  EXPECT_FALSE(system.trigger(QStringLiteral("heights")));
  run(world, system, 3.0F);
  EXPECT_TRUE(system.trigger(QStringLiteral("heights")));
  EXPECT_EQ(system.trap(0).times_fired, 2);
}

TEST_F(RockfallSystemTest, MapJsonDeclaresRockfallTraps) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  QString const path = dir.filePath(QStringLiteral("pass.json"));
  QFile file(path);
  ASSERT_TRUE(file.open(QIODevice::WriteOnly));
  file.write(R"({
    "name": "Alpine pass",
    "grid": {"width": 40, "height": 40, "tile_size": 1.0},
    "rockfall_traps": [
      {"id": "allobroges", "release": [10, 4], "target": {"x": 10, "z": 20},
       "radius": 6, "trigger": "ai", "owner_id": 2, "boulders": 7,
       "boulder_radius": 1.1, "damage": 55, "rearm": 30, "ai_min_targets": 3},
      {"release": [1, 1], "target": [5, 5]},
      {"id": "broken", "release": [1, 1]}
    ]
  })");
  file.close();

  Game::Map::MapDefinition map;
  QString error;
  ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(path, map, &error))
      << error.toStdString();
  ASSERT_EQ(map.rockfall_traps.size(), 2U) << "a trap without a target is skipped";
  auto const& trap = map.rockfall_traps[0];
  EXPECT_EQ(trap.id, QStringLiteral("allobroges"));
  EXPECT_FLOAT_EQ(trap.release_x, 10.0F);
  EXPECT_FLOAT_EQ(trap.release_z, 4.0F);
  EXPECT_FLOAT_EQ(trap.target_z, 20.0F);
  EXPECT_FLOAT_EQ(trap.zone_radius, 6.0F);
  EXPECT_EQ(trap.trigger, RockfallTriggerMode::AiDefender);
  EXPECT_EQ(trap.owner_id, 2);
  EXPECT_EQ(trap.boulder_count, 7);
  EXPECT_FLOAT_EQ(trap.boulder_radius, 1.1F);
  EXPECT_EQ(trap.damage, 55);
  EXPECT_FLOAT_EQ(trap.rearm_seconds, 30.0F);
  EXPECT_EQ(trap.ai_min_targets, 3);
  EXPECT_EQ(map.rockfall_traps[1].id, QStringLiteral("rockfall_2"));
  EXPECT_EQ(map.rockfall_traps[1].trigger, RockfallTriggerMode::Zone);
}

namespace {

auto make_hill_map(bool caches = true) -> Game::Map::MapDefinition {
  Game::Map::MapDefinition map;
  map.coordSystem = Game::Map::CoordSystem::World;
  map.grid.width = 72;
  map.grid.height = 72;
  map.grid.tile_size = 1.0F;
  map.hill_rockfall_caches = caches;

  Game::Map::TerrainFeature hill;
  hill.type = Game::Map::TerrainType::Hill;
  hill.center_x = 6.0F;
  hill.center_z = 0.0F;
  hill.radius = 12.0F;
  hill.height = 9.0F;
  hill.entrances.push_back(QVector3D(-7.0F, 0.0F, 0.0F));
  map.terrain.push_back(hill);
  return map;
}

auto first_hill_cache(const RockfallSystem& system) -> std::optional<std::size_t> {
  for (std::size_t i = 0; i < system.trap_count(); ++i) {
    if (system.trap(i).hill_cache) {
      return i;
    }
  }
  return std::nullopt;
}

} // namespace

TEST_F(RockfallSystemTest, EveryHillRampGetsAnUnclaimedStoneCache) {
  RockfallSystem system(services());
  auto const map = make_hill_map();
  Game::Map::TerrainService::instance().initialize(map);
  system.configure(map);

  auto const index = first_hill_cache(system);
  ASSERT_TRUE(index.has_value()) << "the ramp up the hill got no stone cache";
  auto const cache = system.trap(*index);
  EXPECT_EQ(cache.trigger, RockfallTriggerMode::Claimable);
  EXPECT_EQ(cache.owner_id, -1);
  EXPECT_TRUE(cache.armed);
  EXPECT_GT(cache.release_world.y(), cache.target_world.y() + 1.5F)
      << "the cache sits at the top of the ramp, above its foot";

  RockfallSystem none(services());
  none.configure(make_hill_map(false));
  EXPECT_FALSE(first_hill_cache(none).has_value()) << "a map can opt out";
}

TEST_F(RockfallSystemTest, FirstTroopUpTheHillClaimsTheStonesAndRollsThemOnce) {
  Engine::Core::World world;
  RockfallSystem system(services());
  auto const map = make_hill_map();
  Game::Map::TerrainService::instance().initialize(map);
  system.configure(map);
  auto const index = first_hill_cache(system);
  ASSERT_TRUE(index.has_value());
  auto const cache = system.trap(*index);

  auto* far = add_troop(world, 1, cache.target_world - QVector3D(20.0F, 0.0F, 0.0F));
  system.update(&world, k_tick);
  EXPECT_FALSE(system.cache_in_reach(world, far->get_id()).has_value())
      << "only troops beside the stones can roll them";

  auto* defenders = add_troop(world, 1, cache.release_world + QVector3D(0, 0, 1.5F));
  system.update(&world, k_tick);
  EXPECT_EQ(system.trap(*index).owner_id, 1) << "the first troop there claims it";
  ASSERT_TRUE(system.cache_in_reach(world, defenders->get_id()).has_value());

  auto* climbers =
      add_troop(world, 2, (cache.release_world + cache.target_world) * 0.5F);
  system.update(&world, k_tick);
  EXPECT_EQ(system.trap(*index).hostile_troops_in_zone, 1);
  EXPECT_EQ(system.trap(*index).times_fired, 0) << "a player's stones wait for orders";

  ASSERT_TRUE(system.order_release(world, defenders->get_id()));
  EXPECT_TRUE(system.trap(*index).pushing);
  EXPECT_NE(world.try_get<Engine::Core::RockfallPushComponent>(defenders->get_id()),
            nullptr)
      << "the troop plays the push while it heaves";
  EXPECT_FALSE(system.order_release(world, defenders->get_id()));

  run(world, system, Game::Systems::k_rockfall_push_seconds * 0.5F);
  EXPECT_TRUE(system.boulders().empty()) << "the stones wait for the heave to finish";
  run(world, system, Game::Systems::k_rockfall_push_seconds);
  EXPECT_FALSE(system.boulders().empty());
  EXPECT_EQ(world.try_get<Engine::Core::RockfallPushComponent>(defenders->get_id()),
            nullptr);

  run(world, system, 6.0F);
  EXPECT_LT(health_of(climbers), 600) << "the stones crush the troop on the ramp";
  EXPECT_EQ(health_of(defenders), 1000);
  EXPECT_TRUE(system.trap(*index).spent) << "a cache is used once";
  EXPECT_FALSE(system.cache_in_reach(world, defenders->get_id()).has_value());
}

TEST_F(RockfallSystemTest, AnAiHoldingTheStonesRollsThemWhenTheEnemyClimbs) {
  Engine::Core::World world;
  RockfallSystem system(services());
  auto const map = make_hill_map();
  Game::Map::TerrainService::instance().initialize(map);
  system.configure(map);
  auto const index = first_hill_cache(system);
  ASSERT_TRUE(index.has_value());
  auto const cache = system.trap(*index);

  add_troop(world, 2, cache.release_world + QVector3D(0, 0, 1.5F));
  run(world, system, 1.0F);
  EXPECT_EQ(system.trap(*index).owner_id, 2);
  EXPECT_EQ(system.trap(*index).times_fired, 0) << "nobody is climbing yet";

  auto* climbers =
      add_troop(world, 1, (cache.release_world + cache.target_world) * 0.5F);
  run(world, system, Game::Systems::k_rockfall_push_seconds + 4.0F);
  EXPECT_EQ(system.trap(*index).times_fired, 1);
  EXPECT_LT(health_of(climbers), 1000);
}

TEST_F(RockfallSystemTest, AnEnemyTakesStonesTheirOwnerLeftBehind) {
  Engine::Core::World world;
  RockfallSystem system(services());
  auto const map = make_hill_map();
  Game::Map::TerrainService::instance().initialize(map);
  system.configure(map);
  auto const index = first_hill_cache(system);
  ASSERT_TRUE(index.has_value());
  auto const cache = system.trap(*index);

  auto* first = add_troop(world, 1, cache.release_world + QVector3D(0, 0, 1.0F));
  system.update(&world, k_tick);
  ASSERT_EQ(system.trap(*index).owner_id, 1);

  first->get_component<Engine::Core::TransformComponent>()->position.x += 30.0F;
  add_troop(world, 2, cache.release_world + QVector3D(0, 0, -1.0F));
  system.update(&world, k_tick);
  EXPECT_EQ(system.trap(*index).owner_id, 2);
}
