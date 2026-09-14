#include <QJsonDocument>
#include <QString>

#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "core/component_gameplay.h"
#include "core/event_manager.h"
#include "core/system_schedule.h"
#include "core/world.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "game/save/serialization.h"
#include "game/session/map_session.h"
#include "game/session/session_context.h"
#include "game/session/session_snapshot.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/cursed_gold_vein_system.h"
#include "game/systems/default_content.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/undead_awakening_system.h"
#include "game/systems/victory_service.h"
#include "game/systems/world_restore.h"

namespace {

using Engine::Core::World;
using Game::Systems::CursedGoldVeinSystem;
using Game::Systems::UndeadAwakeningSystem;
using Game::Systems::VictoryService;

constexpr char k_zone_id[] = "barrow";
constexpr int k_player = 1;

auto make_map() -> Game::Map::MapDefinition {
  Game::Map::MapDefinition map_definition;
  map_definition.grid.width = 48;
  map_definition.grid.height = 48;
  map_definition.grid.tile_size = 1.0F;

  Game::Map::WorldProp ruins;
  ruins.type = Game::Map::WorldProp::Type::Ruins;
  ruins.x = 24.0F;
  ruins.z = 24.0F;
  map_definition.world_props.push_back(ruins);

  Game::Map::WorldProp vein;
  vein.type = Game::Map::WorldProp::Type::CursedGoldVein;
  vein.x = 10.0F;
  vein.z = 38.0F;
  map_definition.world_props.push_back(vein);

  Game::Map::UndeadZone zone;
  zone.id = QString::fromLatin1(k_zone_id);
  zone.anchor_type = Game::Map::WorldProp::Type::Ruins;
  zone.x = 24.0F;
  zone.z = 24.0F;
  zone.radius = 6.0F;
  zone.owner_id = 99;
  zone.team_id = 99;
  zone.awaken_on = {QStringLiteral("unit_enters_radius")};
  Game::Map::UndeadWave wave;
  wave.trigger = QStringLiteral("initial");
  wave.units.push_back({Game::Units::SpawnType::SkeletonSwordsman, 2});
  zone.waves.push_back(wave);
  map_definition.undead_zones.push_back(zone);
  return map_definition;
}

auto count_props(Game::Map::WorldProp::Type type) -> int {
  int count = 0;
  for (const auto& prop : Game::Map::TerrainService::instance().world_props()) {
    if (prop.type == type) {
      ++count;
    }
  }
  return count;
}

auto count_barracks(World& world) -> int {
  int count = 0;
  for (auto [id, unit] : world.view<Engine::Core::UnitComponent>()) {
    (void)id;
    if (unit.spawn_type == Game::Units::SpawnType::Barracks && unit.health > 0) {
      ++count;
    }
  }
  return count;
}

auto alive(World& world, Engine::Core::EntityID id) -> bool {
  const auto* unit = world.try_get<Engine::Core::UnitComponent>(id);
  return unit != nullptr && unit->health > 0;
}

auto survive_rules(float seconds) -> Game::Systems::VictoryRuleSet {
  Game::Systems::VictoryRuleSet rules;
  rules.victory_rules.push_back(Game::Systems::SurviveTimeVictoryRule{seconds});
  return rules;
}

class MapSessionRestoreTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto& owners = Game::Systems::OwnerRegistry::instance();
    owners.clear();
    owners.register_owner_with_id(k_player, Game::Systems::OwnerType::Player, "Player");
    owners.set_owner_team(k_player, k_player);
    owners.set_local_player_id(k_player);

    auto& nations = Game::Systems::NationRegistry::instance();
    nations.clear();
    Game::Systems::initialize_default_content(nations);
    nations.set_player_nation(k_player, Game::Systems::NationID::RomanRepublic);

    Game::Systems::GlobalStatsRegistry::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();

    auto& session = Game::Session::SessionContext::active();
    m_victory = std::make_unique<VictoryService>(
        VictoryService::Services{.stats = session.stats(),
                                 .owners = session.owners(),
                                 .nations = session.nations(),
                                 .economy = session.economy()});
    m_victory->set_victory_callback(
        [this](const QString& state) { m_reported_state = state; });

    Game::Session::register_built_in_snapshot_contributors();
    Game::Session::SessionSnapshot::register_contributor(
        {.key = "victory",
         .capture = [service = m_victory.get()](const Game::Session::SnapshotScope&)
             -> QJsonValue { return service->serialize_state(); },
         .restore =
             [service = m_victory.get()](const Game::Session::SnapshotScope&,
                                         const QJsonValue& value) {
               service->restore_state(value.toObject());
             }});

    m_world.add_system(
        std::make_unique<UndeadAwakeningSystem>(
            UndeadAwakeningSystem::Services{.terrain = session.terrain(),
                                            .owners = session.owners(),
                                            .nations = session.nations(),
                                            .stats = session.stats(),
                                            .economy = session.economy()}),
        Engine::Core::SystemPhase::Strategy);
    m_world.add_system(
        std::make_unique<CursedGoldVeinSystem>(
            CursedGoldVeinSystem::Services{.terrain = session.terrain(),
                                           .owners = session.owners(),
                                           .economy = session.economy()}),
        Engine::Core::SystemPhase::Strategy);
  }

  void TearDown() override {
    Game::Session::SessionSnapshot::forget_contributor("victory");
    m_world.clear();
    Engine::Core::EventManager::instance().clear_all_subscriptions();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::GlobalStatsRegistry::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
    Game::Systems::OwnerRegistry::instance().clear();
  }

  void launch(const Game::Map::MapDefinition& map_definition) {
    recycle_entity_slots();
    Game::Map::TerrainService::instance().initialize(map_definition);
    Game::Session::configure_map_systems(m_world, map_definition, m_victory.get());
    m_victory->configure(survive_rules(1.0F), k_player);
    tick(0.1F);
  }

  void load(const Game::Map::MapDefinition& map_definition,
            const QJsonDocument& world_state,
            const QJsonObject& session_state) {
    m_world.clear();
    Game::Map::TerrainService::instance().clear();
    Engine::Core::Serialization::deserialize_world(&m_world, world_state);
    Game::Map::TerrainService::instance().initialize_keeping_world_props(
        map_definition);
    static_cast<void>(
        Game::Persistence::rebuild_registries_after_load(&m_world, k_player));
    static_cast<void>(Game::Session::restore_map_session(
        {.world = &m_world,
         .map = &map_definition,
         .victory_service = m_victory.get(),
         .configure_victory_rules =
             [this]() { m_victory->configure(survive_rules(1.0F), k_player); },
         .snapshot = session_state}));
  }

  void recycle_entity_slots() {
    std::vector<Engine::Core::EntityID> ids;
    for (int i = 0; i < 32; ++i) {
      ids.push_back(m_world.create_entity()->get_id());
    }
    for (const auto id : ids) {
      m_world.destroy_entity(id);
    }
  }

  void tick(float seconds) {
    m_world.update(seconds);
    m_victory->update(m_world, seconds);
  }

  [[nodiscard]] auto undead() -> UndeadAwakeningSystem& {
    return *m_world.get_system<UndeadAwakeningSystem>();
  }
  [[nodiscard]] auto veins() -> CursedGoldVeinSystem& {
    return *m_world.get_system<CursedGoldVeinSystem>();
  }

  World m_world;
  std::unique_ptr<VictoryService> m_victory;
  QString m_reported_state;
};

TEST_F(MapSessionRestoreTest, ALoadedMatchKeepsItsSepulcherVeinAndStructures) {
  const auto map_definition = make_map();
  launch(map_definition);

  const QString zone = QString::fromLatin1(k_zone_id);
  ASSERT_TRUE(undead().has_shrine(zone));
  ASSERT_EQ(veins().vein_count(), 1U);
  const auto shrine_prop = undead().shrine_prop_id(zone);
  const auto shrine_position = undead().shrine_world_position(zone);
  const auto sepulcher_anchor = undead().anchor_entity(zone);
  const auto vein_anchor = veins().anchor_entity(0);
  ASSERT_TRUE(alive(m_world, sepulcher_anchor));
  ASSERT_TRUE(alive(m_world, vein_anchor));
  ASSERT_GT(sepulcher_anchor, 0xFFFFFFFFULL)
      << "entity ids carry a generation in the high bits; this test relies on it";
  const auto prop_count = Game::Map::TerrainService::instance().world_props().size();
  const int barracks = count_barracks(m_world);

  const QJsonDocument world_state =
      Engine::Core::Serialization::serialize_world(&m_world);
  const QJsonObject session_state = Game::Session::SessionSnapshot::capture(
      {.world = &m_world, .map = &map_definition});

  for (int round = 1; round <= 3; ++round) {
    load(map_definition, world_state, session_state);
    tick(0.1F);

    EXPECT_EQ(undead().shrine_prop_id(zone), shrine_prop)
        << "round " << round << ": the sepulcher lost its shrine";
    EXPECT_TRUE(undead().shrine_world_position(zone) == shrine_position)
        << "round " << round << ": the shrine moved away from its structure";
    EXPECT_EQ(count_props(Game::Map::WorldProp::Type::MagicShrine), 1)
        << "round " << round << ": the load placed a second shrine";
    EXPECT_EQ(Game::Map::TerrainService::instance().world_props().size(), prop_count);

    EXPECT_EQ(undead().anchor_entity(zone), sepulcher_anchor)
        << "round " << round << ": the sepulcher points at a different structure";
    EXPECT_FALSE(undead().is_zone_cleared(zone))
        << "round " << round << ": loading broke the sepulcher's garrison";
    EXPECT_TRUE(alive(m_world, sepulcher_anchor));

    ASSERT_EQ(veins().vein_count(), 1U) << "round " << round;
    EXPECT_EQ(veins().anchor_entity(0), vein_anchor);
    EXPECT_TRUE(alive(m_world, vein_anchor));
    EXPECT_EQ(count_barracks(m_world), barracks)
        << "round " << round << ": a load raised a duplicate structure";
  }
}

TEST_F(MapSessionRestoreTest, ALoadedMatchCanStillBeWon) {
  const auto map_definition = make_map();
  launch(map_definition);
  tick(0.4F);
  tick(0.5F);
  ASSERT_TRUE(m_reported_state.isEmpty());

  const QJsonDocument world_state =
      Engine::Core::Serialization::serialize_world(&m_world);
  const QJsonObject session_state = Game::Session::SessionSnapshot::capture(
      {.world = &m_world, .map = &map_definition});

  load(map_definition, world_state, session_state);
  tick(0.6F);

  EXPECT_EQ(m_reported_state, QStringLiteral("victory"))
      << "the rules were configured after the saved clock was restored, or the "
         "reset dropped the callback that tells the game it was won";
}

TEST_F(MapSessionRestoreTest, AnOldSaveWithoutSessionStateStillBuildsTheMapSystems) {
  const auto map_definition = make_map();
  launch(map_definition);
  const QJsonDocument world_state =
      Engine::Core::Serialization::serialize_world(&m_world);

  load(map_definition, world_state, QJsonObject());

  EXPECT_TRUE(undead().has_shrine(QString::fromLatin1(k_zone_id)));
  EXPECT_EQ(veins().vein_count(), 1U);
}

} // namespace
