#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QString>

#include <gtest/gtest.h>
#include <unordered_set>

#include "core/component.h"
#include "core/world.h"
#include "game/core/ownership_constants.h"
#include "game/map/map_definition.h"
#include "game/map/map_loader.h"
#include "game/map/terrain_service.h"
#include "game/map/undead_shrine_placement.h"
#include "game/session/session_context.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/cursed_gold_vein_system.h"
#include "game/systems/default_content.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/resource_types.h"
#include "game/visuals/building_asset_key.h"

namespace {

using Game::Systems::k_cursed_gold_vein_curse_damage;
using Game::Systems::k_cursed_gold_vein_curse_radius;
using Game::Systems::k_cursed_gold_vein_gold_per_tick;
using Game::Systems::k_cursed_gold_vein_tick_seconds;

auto make_vein_map() -> Game::Map::MapDefinition {
  Game::Map::MapDefinition map_definition;
  map_definition.grid.width = 40;
  map_definition.grid.height = 40;
  map_definition.grid.tile_size = 1.0F;

  Game::Map::WorldProp vein;
  vein.type = Game::Map::WorldProp::Type::CursedGoldVein;
  vein.x = 20.0F;
  vein.z = 20.0F;
  map_definition.world_props.push_back(vein);
  return map_definition;
}

auto services() -> Game::Systems::CursedGoldVeinSystem::Services {
  auto& session = Game::Session::SessionContext::active();
  return {.terrain = session.terrain(),
          .owners = session.owners(),
          .economy = session.economy()};
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
  unit->spawn_type = Game::Units::SpawnType::Knight;
  unit->health = 100;
  unit->max_health = 100;
  return entity;
}

auto health_of(Engine::Core::Entity* entity) -> int {
  return entity->get_component<Engine::Core::UnitComponent>()->health;
}

class CursedGoldVeinSystemTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto& owners = Game::Systems::OwnerRegistry::instance();
    owners.clear();
    owners.register_owner_with_id(1, Game::Systems::OwnerType::Player, "Player");
    owners.set_owner_team(1, 1);
    owners.set_local_player_id(1);
    owners.register_owner_with_id(2, Game::Systems::OwnerType::AI, "Rival");
    owners.set_owner_team(2, 2);

    auto& nations = Game::Systems::NationRegistry::instance();
    nations.clear();
    Game::Systems::initialize_default_content(nations);
    nations.set_player_nation(1, Game::Systems::NationID::RomanRepublic);
    nations.set_player_nation(2, Game::Systems::NationID::Carthage);

    Game::Systems::PlayerResourceRegistry::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
  }

  void TearDown() override {
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::PlayerResourceRegistry::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
    Game::Systems::OwnerRegistry::instance().clear();
  }

  static auto anchor_unit(Engine::Core::World& world,
                          Game::Systems::CursedGoldVeinSystem& system)
      -> Engine::Core::UnitComponent* {
    auto* anchor = world.get_entity(system.anchor_entity(0));
    return anchor != nullptr ? anchor->get_component<Engine::Core::UnitComponent>()
                             : nullptr;
  }
};

} // namespace

TEST_F(CursedGoldVeinSystemTest, NeutralVeinRaisesAClaimFlagAndDoesNothing) {
  Engine::Core::World world;
  Game::Systems::CursedGoldVeinSystem system(services());

  const auto map_definition = make_vein_map();
  Game::Map::TerrainService::instance().initialize(map_definition);
  system.configure(map_definition);
  ASSERT_EQ(system.vein_count(), 1U);

  system.update(&world, 0.1F);

  const auto anchor_id = system.anchor_entity(0);
  ASSERT_NE(anchor_id, 0U);
  auto* anchor = world.get_entity(anchor_id);
  ASSERT_NE(anchor, nullptr);
  auto* unit = anchor->get_component<Engine::Core::UnitComponent>();
  ASSERT_NE(unit, nullptr);
  EXPECT_EQ(unit->spawn_type, Game::Units::SpawnType::Barracks);
  EXPECT_TRUE(Game::Core::is_neutral_owner(unit->owner_id));
  EXPECT_EQ(anchor->get_component<Engine::Core::ProductionComponent>(), nullptr);
  auto* renderable = anchor->get_component<Engine::Core::RenderableComponent>();
  ASSERT_NE(renderable, nullptr);
  EXPECT_EQ(renderable->renderer_id,
            std::string(Game::Visuals::k_cursed_gold_vein_flag_asset_key));

  auto* bystander =
      add_troop(world, 1, system.vein_world_position(0) + QVector3D(2, 0, 0));
  const int gold_before = Game::Systems::PlayerResourceRegistry::instance().get(
      1, Game::Systems::ResourceType::Gold);
  for (int i = 0; i < 40; ++i) {
    system.update(&world, 0.5F);
  }
  EXPECT_EQ(system.anchor_entity(0), anchor_id) << "the anchor is raised once";
  EXPECT_EQ(health_of(bystander), 100) << "a neutral vein curses nobody";
  EXPECT_EQ(Game::Systems::PlayerResourceRegistry::instance().get(
                1, Game::Systems::ResourceType::Gold),
            gold_before);
}

TEST_F(CursedGoldVeinSystemTest, ClaimedVeinPaysGoldAndBleedsItsOwnersNearbyTroops) {
  Engine::Core::World world;
  Game::Systems::CursedGoldVeinSystem system(services());

  const auto map_definition = make_vein_map();
  Game::Map::TerrainService::instance().initialize(map_definition);
  system.configure(map_definition);
  system.update(&world, 0.1F);

  const QVector3D vein = system.vein_world_position(0);
  auto* near_own = add_troop(world, 1, vein + QVector3D(3.0F, 0.0F, 1.0F));
  auto* far_own = add_troop(
      world, 1, vein + QVector3D(k_cursed_gold_vein_curse_radius + 4.0F, 0, 0));
  auto* near_enemy = add_troop(world, 2, vein + QVector3D(-2.0F, 0.0F, 2.0F));

  auto* unit = anchor_unit(world, system);
  ASSERT_NE(unit, nullptr);
  unit->owner_id = 1;

  auto& economy = Game::Systems::PlayerResourceRegistry::instance();
  const int gold_before = economy.get(1, Game::Systems::ResourceType::Gold);

  system.update(&world, k_cursed_gold_vein_tick_seconds * 0.5F);
  EXPECT_EQ(economy.get(1, Game::Systems::ResourceType::Gold), gold_before)
      << "nothing is paid before the first full interval";
  EXPECT_EQ(health_of(near_own), 100);

  system.update(&world, k_cursed_gold_vein_tick_seconds * 0.5F + 0.01F);
  EXPECT_EQ(economy.get(1, Game::Systems::ResourceType::Gold),
            gold_before + k_cursed_gold_vein_gold_per_tick);
  EXPECT_EQ(health_of(near_own), 100 - k_cursed_gold_vein_curse_damage)
      << "the owner's troop beside the vein is cursed";
  EXPECT_EQ(health_of(far_own), 100) << "the curse has a radius";
  EXPECT_EQ(health_of(near_enemy), 100) << "only the owner's men are cursed";
  EXPECT_EQ(economy.get(2, Game::Systems::ResourceType::Gold), 0);

  system.update(&world, k_cursed_gold_vein_tick_seconds);
  EXPECT_EQ(economy.get(1, Game::Systems::ResourceType::Gold),
            gold_before + 2 * k_cursed_gold_vein_gold_per_tick);
  EXPECT_EQ(health_of(near_own), 100 - 2 * k_cursed_gold_vein_curse_damage);
}

TEST_F(CursedGoldVeinSystemTest, ClaimedVeinNeverKeepsAProductionLine) {
  Engine::Core::World world;
  Game::Systems::CursedGoldVeinSystem system(services());

  const auto map_definition = make_vein_map();
  Game::Map::TerrainService::instance().initialize(map_definition);
  system.configure(map_definition);
  system.update(&world, 0.1F);

  auto* anchor = world.get_entity(system.anchor_entity(0));
  ASSERT_NE(anchor, nullptr);
  anchor->get_component<Engine::Core::UnitComponent>()->owner_id = 1;
  anchor->add_component<Engine::Core::ProductionComponent>();

  system.update(&world, 0.1F);
  EXPECT_EQ(anchor->get_component<Engine::Core::ProductionComponent>(), nullptr);
  EXPECT_EQ(system.vein_owner(0), 1);
}

TEST_F(CursedGoldVeinSystemTest, RazedVeinGoesInert) {
  Engine::Core::World world;
  Game::Systems::CursedGoldVeinSystem system(services());

  const auto map_definition = make_vein_map();
  Game::Map::TerrainService::instance().initialize(map_definition);
  system.configure(map_definition);
  system.update(&world, 0.1F);

  auto* unit = anchor_unit(world, system);
  ASSERT_NE(unit, nullptr);
  unit->owner_id = 1;
  system.update(&world, 0.1F);
  unit->health = 0;

  auto& economy = Game::Systems::PlayerResourceRegistry::instance();
  const int gold_before = economy.get(1, Game::Systems::ResourceType::Gold);
  system.update(&world, k_cursed_gold_vein_tick_seconds * 3.0F);
  EXPECT_EQ(economy.get(1, Game::Systems::ResourceType::Gold), gold_before);
  EXPECT_TRUE(system.vein_markers().front().destroyed);
}

TEST_F(CursedGoldVeinSystemTest, StateSurvivesASaveLoadWithoutASecondAnchor) {
  Engine::Core::World world;
  Game::Systems::CursedGoldVeinSystem system(services());

  const auto map_definition = make_vein_map();
  Game::Map::TerrainService::instance().initialize(map_definition);
  system.configure(map_definition);
  system.update(&world, 0.1F);
  anchor_unit(world, system)->owner_id = 1;
  system.update(&world, 2.0F);

  const QJsonArray state = system.serialize_state();
  const auto anchor_id = system.anchor_entity(0);

  Game::Systems::CursedGoldVeinSystem restored(services());
  restored.configure(map_definition);
  restored.restore_state(state);
  restored.update(&world, 0.1F);

  EXPECT_EQ(restored.anchor_entity(0), anchor_id);
  EXPECT_EQ(restored.vein_owner(0), 1);
  int barracks = 0;
  for (auto* entity : world.collect_entities_with<Engine::Core::UnitComponent>()) {
    if (entity->get_component<Engine::Core::UnitComponent>()->spawn_type ==
        Game::Units::SpawnType::Barracks) {
      ++barracks;
    }
  }
  EXPECT_EQ(barracks, 1);
}

TEST_F(CursedGoldVeinSystemTest, EveryShippedVeinStandsOnClearGround) {
  const QDir dir = QDir(QCoreApplication::applicationDirPath())
                       .absoluteFilePath(QStringLiteral("../../assets/maps"));
  ASSERT_TRUE(dir.exists()) << dir.path().toStdString();

  int checked = 0;
  for (const QString& file_name : dir.entryList({"*.json"}, QDir::Files, QDir::Name)) {
    Game::Map::MapDefinition map_definition;
    QString error;
    ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
        dir.absoluteFilePath(file_name), map_definition, &error))
        << file_name.toStdString() << ": " << error.toStdString();

    std::vector<Game::Map::WorldProp> veins;
    Game::Map::MapDefinition without_veins = map_definition;
    without_veins.world_props.clear();
    for (const auto& prop : map_definition.world_props) {
      (prop.type == Game::Map::WorldProp::Type::CursedGoldVein
           ? veins
           : without_veins.world_props)
          .push_back(prop);
    }

    Game::Systems::BuildingCollisionRegistry::instance().clear();
    auto& terrain = Game::Map::TerrainService::instance();
    terrain.initialize(without_veins);

    for (const auto& prop : veins) {
      const QVector3D site = terrain.world_prop_world_position(prop);
      const float clearance = Game::Map::k_undead_shrine_clearance;
      std::string blockers;
      for (const auto& other : terrain.world_props()) {
        const QVector3D at = terrain.world_prop_world_position(other);
        const float dx = at.x() - site.x();
        const float dz = at.z() - site.z();
        if (dx * dx + dz * dz < (clearance + 2.0F) * (clearance + 2.0F)) {
          blockers +=
              " prop:" +
              QString(Game::Map::world_prop_type_to_string(other.type)).toStdString();
        }
      }
      EXPECT_TRUE(Game::Map::is_undead_shrine_site_clear(terrain, site.x(), site.z()))
          << file_name.toStdString() << ": cursed gold vein at grid (" << prop.x << ", "
          << prop.z << ") is blocked ["
          << (terrain.is_forbidden_world(site.x(), site.z()) ? " forbidden" : "")
          << (terrain.is_point_near_water(site.x(), site.z(), clearance) ? " water"
                                                                         : "")
          << (terrain.is_point_near_road(site.x(), site.z(), 1.2F) ? " road" : "")
          << (terrain.is_point_near_bridge(site.x(), site.z(), clearance) ? " bridge"
                                                                          : "")
          << blockers << " ]";
      ++checked;
    }
  }
  EXPECT_GT(checked, 0) << "no shipped map places a cursed gold vein";
}
