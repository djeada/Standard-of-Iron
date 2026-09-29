#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "game/command/command.h"
#include "game/command/command_queue.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/default_content.h"
#include "game/systems/formation_combat_geometry.h"
#include "game/systems/movement/command_service.h"
#include "game/systems/nation_registry.h"
#include "game/systems/navigation/nav_grid.h"
#include "game/systems/navigation/pathfinding.h"
#include "game/systems/owner_registry.h"
#include "game/systems/runtime_system_registry.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"

namespace {

using Engine::Core::EntityID;
using Game::Session::SessionContext;
using Game::Systems::NavGrid;
using Game::Units::SpawnType;

constexpr int k_player = 1;
constexpr int k_enemy = 2;
constexpr int k_grid = 160;

class HillContainmentTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::NationRegistry::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Systems::initialize_default_content(
        Game::Systems::NationRegistry::instance());
    m_factory = std::make_shared<Game::Units::UnitFactoryRegistry>();
    Game::Units::register_built_in_units(*m_factory);
  }

  void TearDown() override {
    m_scope.reset();
    m_session.reset();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
  }

  static auto hill(float x, float z, float radius, float entrance_x, float entrance_z)
      -> Game::Map::TerrainFeature {
    Game::Map::TerrainFeature feature;
    feature.type = Game::Map::TerrainType::Hill;
    feature.center_x = x;
    feature.center_z = z;
    feature.radius = radius;
    feature.height = 6.0F;
    feature.shape = Game::Map::HillShape::Blob;
    feature.entrances.emplace_back(entrance_x, 0.0F, entrance_z);
    return feature;
  }

  void field(const std::vector<Game::Map::TerrainFeature>& hills) {
    Game::Map::MapDefinition map;
    map.grid.width = k_grid;
    map.grid.height = k_grid;
    map.grid.tile_size = 1.0F;
    map.coordSystem = Game::Map::CoordSystem::World;
    map.biome.procedural_boulders_enabled = false;
    map.biome.procedural_iron_ore_enabled = false;
    map.biome.procedural_trees_enabled = false;
    map.terrain = hills;

    m_session = std::make_unique<SessionContext>();
    m_session->world().set_presentation_enabled(true);
    m_scope = std::make_unique<Game::Session::ScopedSession>(*m_session);
    m_session->owners().register_owner_with_id(
        k_player, Game::Systems::OwnerType::Player, "carthage");
    m_session->owners().set_owner_team(k_player, 1);
    m_session->owners().register_owner_with_id(
        k_enemy, Game::Systems::OwnerType::AI, "rome");
    m_session->owners().set_owner_team(k_enemy, 2);
    Game::Systems::initialize_default_content(m_session->nations());
    Game::Systems::register_runtime_systems(m_session->world());
    m_session->terrain().initialize(map);
    NavGrid::initialize(map.grid.width, map.grid.height);
    NavGrid::get_pathfinder()->update_navigation_grid();
  }

  auto spawn(SpawnType type, int owner, const QVector3D& at) -> EntityID {
    Game::Units::SpawnParams params;
    params.position = at;
    params.player_id = owner;
    params.spawn_type = type;
    params.nation_id = owner == k_enemy ? Game::Systems::NationID::RomanRepublic
                                        : Game::Systems::NationID::Carthage;
    auto unit = m_factory->create(type, m_session->world(), params);
    return unit ? unit->id() : 0;
  }

  void building(float x, float z, float width, float depth) {
    auto* entity = m_session->world().create_entity();
    entity->add_component<Engine::Core::TransformComponent>(x, 0.0F, z);
    auto* unit =
        entity->add_component<Engine::Core::UnitComponent>(400, 400, 0.0F, 0.0F);
    unit->owner_id = k_enemy;
    unit->spawn_type = SpawnType::Barracks;
    entity->add_component<Engine::Core::BuildingComponent>();
    Game::Systems::BuildingCollisionRegistry::instance().register_building(
        entity->get_id(), "house", x, z, k_enemy, {.width = width, .depth = depth});
    auto* pathfinder = NavGrid::get_pathfinder();
    pathfinder->mark_navigation_grid_dirty();
    pathfinder->update_navigation_grid();
  }

  [[nodiscard]] auto terrain_open(float x, float z) const -> bool {
    auto const cell = NavGrid::world_to_grid(x, z);
    return m_session->terrain().is_walkable(cell.x, cell.y);
  }

  [[nodiscard]] auto on_slope(float x, float z) const -> bool {
    auto const cell = NavGrid::world_to_grid(x, z);
    return m_session->terrain().get_terrain_type(cell.x, cell.y) ==
               Game::Map::TerrainType::Hill &&
           !m_session->terrain().is_walkable(cell.x, cell.y);
  }

  auto position(EntityID id) -> QVector3D {
    auto const* t = m_session->world().try_get<Engine::Core::TransformComponent>(id);
    return t == nullptr ? QVector3D() : QVector3D(t->position.x, 0.0F, t->position.z);
  }

  struct SlopeSamples {
    int centres{0};
    int soldiers{0};
    std::optional<QVector3D> first;
  };

  auto run_watching(const std::vector<EntityID>& units,
                    double seconds) -> SlopeSamples {
    SlopeSamples samples;
    double const step = m_session->clock().tick_seconds();
    for (double elapsed = 0.0; elapsed < seconds - 1e-9; elapsed += step) {
      m_session->clock().advance(step);
      while (m_session->clock().consume_tick()) {
        m_session->world().update(static_cast<float>(step));
      }
      for (auto const id : units) {
        auto* entity = m_session->world().get_entity(id);
        auto const* unit = m_session->world().try_get<Engine::Core::UnitComponent>(id);
        if (entity == nullptr || unit == nullptr || unit->health <= 0) {
          continue;
        }
        auto const centre = position(id);
        if (on_slope(centre.x(), centre.z())) {
          ++samples.centres;
          if (!samples.first) {
            samples.first = centre;
          }
        }
        for (auto const& anchor :
             Game::Systems::FormationCombat::soldier_spatial_anchors(*entity)) {
          if (on_slope(anchor.world_x, anchor.world_z)) {
            ++samples.soldiers;
            if (!samples.first) {
              samples.first = QVector3D(anchor.world_x, 0.0F, anchor.world_z);
            }
          }
        }
      }
    }
    return samples;
  }

  [[nodiscard]] auto
  foot_of(const QVector3D& centre, float dir_x, float dir_z) const -> QVector3D {
    for (float r = 1.0F; r < 60.0F; r += 0.5F) {
      float const x = centre.x() + dir_x * r;
      float const z = centre.z() + dir_z * r;
      if (!on_slope(x, z) && terrain_open(x, z) &&
          m_session->terrain().get_terrain_type(NavGrid::world_to_grid(x, z).x,
                                                NavGrid::world_to_grid(x, z).y) !=
              Game::Map::TerrainType::Hill) {
        return {x, 0.0F, z};
      }
    }
    return centre;
  }

  [[nodiscard]] auto
  rim_of(const QVector3D& centre, float dir_x, float dir_z) const -> QVector3D {
    QVector3D last = centre;
    for (float r = 0.0F; r < 60.0F; r += 0.5F) {
      float const x = centre.x() + dir_x * r;
      float const z = centre.z() + dir_z * r;
      if (!terrain_open(x, z)) {
        break;
      }
      last = QVector3D(x, 0.0F, z);
    }
    return last;
  }

  static void expect_no_slope(const SlopeSamples& samples, const char* scenario) {
    EXPECT_EQ(samples.centres, 0)
        << scenario << ": a squad centre stood on the slope"
        << (samples.first ? " (first near " + std::to_string(samples.first->x()) +
                                ", " + std::to_string(samples.first->z()) + ")"
                          : std::string());
    EXPECT_EQ(samples.soldiers, 0)
        << scenario << ": soldiers stood on the slope"
        << (samples.first ? " (first near " + std::to_string(samples.first->x()) +
                                ", " + std::to_string(samples.first->z()) + ")"
                          : std::string());
  }

  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_factory;
  std::unique_ptr<SessionContext> m_session;
  std::unique_ptr<Game::Session::ScopedSession> m_scope;
};

TEST_F(HillContainmentTest, AnArmyMarchingAlongTheFootOfAHillStaysOffTheSlope) {
  field({hill(0.0F, 0.0F, 18.0F, -30.0F, 0.0F)});
  QVector3D const foot = foot_of({}, 0.0F, 1.0F);
  std::vector<EntityID> army;
  for (int i = 0; i < 4; ++i) {
    army.push_back(spawn(
        SpawnType::Swordsman, k_player, {-40.0F + i * 6.0F, 0.0F, foot.z() + 2.0F}));
  }
  std::vector<QVector3D> targets;
  for (int i = 0; i < 4; ++i) {
    targets.emplace_back(22.0F + i * 6.0F, 0.0F, foot.z() + 1.0F);
  }
  Game::Systems::CommandService::move_units(m_session->world(), army, targets);
  expect_no_slope(run_watching(army, 40.0), "march along the foot");
}

TEST_F(HillContainmentTest, TwoArmiesFightingAtTheFootOfAHillStayOffTheSlope) {
  field({hill(0.0F, 0.0F, 18.0F, -30.0F, 0.0F)});
  QVector3D const foot = foot_of({}, 0.0F, 1.0F);
  std::vector<EntityID> everyone;
  std::vector<EntityID> mine;
  std::vector<EntityID> theirs;
  for (int i = 0; i < 3; ++i) {
    mine.push_back(spawn(
        SpawnType::Swordsman, k_player, {-14.0F + i * 5.0F, 0.0F, foot.z() + 1.5F}));
    theirs.push_back(
        spawn(SpawnType::Spearman, k_enemy, {4.0F + i * 5.0F, 0.0F, foot.z() + 1.5F}));
  }
  everyone.insert(everyone.end(), mine.begin(), mine.end());
  everyone.insert(everyone.end(), theirs.begin(), theirs.end());
  for (std::size_t i = 0; i < mine.size(); ++i) {
    Game::Systems::CommandService::attack_target(
        m_session->world(), {mine[i]}, theirs[i]);
  }
  expect_no_slope(run_watching(everyone, 30.0), "battle at the foot");
}

TEST_F(HillContainmentTest, TwoArmiesFightingOnThePlateauEdgeStayOnThePlateau) {
  field({hill(0.0F, 0.0F, 22.0F, -34.0F, 0.0F)});
  QVector3D const rim = rim_of({}, 0.0F, 1.0F);
  ASSERT_GT(rim.z(), 6.0F) << "the synthetic hill has no crown to fight on";
  std::vector<EntityID> everyone;
  std::vector<EntityID> mine;
  std::vector<EntityID> theirs;
  for (int i = 0; i < 2; ++i) {
    float const x = -4.0F + static_cast<float>(i) * 8.0F;
    QVector3D const edge = rim_of({x, 0.0F, 0.0F}, 0.0F, 1.0F);
    mine.push_back(spawn(SpawnType::Swordsman, k_player, {x, 0.0F, edge.z() - 7.0F}));
    theirs.push_back(spawn(SpawnType::Swordsman, k_enemy, {x, 0.0F, edge.z() - 2.0F}));
  }
  everyone.insert(everyone.end(), mine.begin(), mine.end());
  everyone.insert(everyone.end(), theirs.begin(), theirs.end());
  for (std::size_t i = 0; i < mine.size(); ++i) {
    Game::Systems::CommandService::attack_target(
        m_session->world(), {mine[i]}, theirs[i]);
  }
  expect_no_slope(run_watching(everyone, 30.0), "battle on the plateau edge");
}

TEST_F(HillContainmentTest, ATroopOnAPlateauLeavesItByTheRampNotTheSlope) {
  field({hill(0.0F, 0.0F, 22.0F, -34.0F, 0.0F)});
  QVector3D const rim = rim_of({}, 0.0F, 1.0F);
  QVector3D const below = foot_of({}, 0.0F, 1.0F);
  EntityID const troop =
      spawn(SpawnType::Spearman, k_player, {0.0F, 0.0F, rim.z() - 2.0F});
  Game::Systems::CommandService::move_unit(
      m_session->world(), troop, {0.0F, 0.0F, below.z() + 4.0F});
  auto const samples = run_watching({troop}, 60.0);
  expect_no_slope(samples, "leave the plateau");
  EXPECT_LT((position(troop) - QVector3D(0.0F, 0.0F, below.z() + 4.0F)).length(), 3.0F)
      << "the troop never reached the ground below by the ramp";
}

TEST_F(HillContainmentTest, ABuildingBetweenTwoHillsLeavesTheLanePassable) {

  for (float const lane : {2.0F, 2.5F, 3.0F}) {
    SCOPED_TRACE("lane " + std::to_string(lane) + " m");
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    field({hill(-24.0F, 0.0F, 16.0F, -50.0F, 0.0F),
           hill(24.0F, 0.0F, 16.0F, 50.0F, 0.0F)});

    float gap_west = -40.0F;
    float gap_east = 40.0F;
    for (float z = -3.0F; z <= 3.0F; z += 0.5F) {
      float west = 0.0F;
      for (float x = 0.0F; x > -40.0F; x -= 0.25F) {
        if (!terrain_open(x, z) || on_slope(x, z)) {
          break;
        }
        west = x;
      }
      float east = 0.0F;
      for (float x = 0.0F; x < 40.0F; x += 0.25F) {
        if (!terrain_open(x, z) || on_slope(x, z)) {
          break;
        }
        east = x;
      }
      gap_west = std::max(gap_west, west);
      gap_east = std::min(gap_east, east);
    }
    ASSERT_GT(gap_east - gap_west, 5.0F) << "the two hills leave no corridor to test";

    float const building_west = gap_west - 0.5F + lane;
    float const width = gap_east + 1.0F - building_west;
    building(building_west + width * 0.5F, 0.0F, width, 4.0F);

    float const lane_x = gap_west + 0.25F;
    auto* pathfinder = NavGrid::get_pathfinder();

    int const row = NavGrid::world_to_grid(0.0F, 0.0F).y;
    for (int x = 0; x < k_grid; ++x) {
      for (int z = row - 1; z <= row + 1; ++z) {
        QVector3D const world = NavGrid::grid_to_world({x, z});
        if (world.x() < gap_west - 1.5F || world.x() > gap_east + 1.5F) {
          pathfinder->set_obstacle(x, z, true);
        }
      }
    }
    EXPECT_TRUE(pathfinder->can_reach(NavGrid::world_to_grid(lane_x, -30.0F),
                                      NavGrid::world_to_grid(lane_x, 30.0F),
                                      Game::Systems::Pathfinding::Passability::Light))
        << "the building sealed the lane between the hill and its wall";

    EntityID const troop =
        spawn(SpawnType::Swordsman, k_player, {lane_x, 0.0F, -30.0F});
    QVector3D const goal(lane_x, 0.0F, 30.0F);
    Game::Systems::CommandService::move_unit(m_session->world(), troop, goal);
    auto const samples = run_watching({troop}, 60.0);
    EXPECT_LT((position(troop) - goal).length(), 3.0F)
        << "the troop could not squeeze through the lane beside the building";
    expect_no_slope(samples, "lane beside a building");
    m_scope.reset();
    m_session.reset();
  }
}

TEST_F(HillContainmentTest, ATroopSealedOnAPlateauNeverLeapsTheSlope) {
  field({hill(0.0F, 0.0F, 22.0F, -34.0F, 0.0F)});
  QVector3D const rim = rim_of({}, 0.0F, 1.0F);
  QVector3D const below = foot_of({}, 0.0F, 1.0F);
  EntityID const troop =
      spawn(SpawnType::Spearman, k_player, {0.0F, 0.0F, rim.z() - 2.0F});

  building(-30.0F, 0.0F, 12.0F, 16.0F);
  Game::Systems::CommandService::move_unit(
      m_session->world(), troop, {0.0F, 0.0F, below.z() + 4.0F});
  expect_no_slope(run_watching({troop}, 30.0), "sealed plateau");
}

TEST_F(HillContainmentTest, ABuilderCrewGatheringStoneOnASlopeWorksFromTheFoot) {

  field({hill(0.0F, 0.0F, 18.0F, -30.0F, 0.0F)});
  QVector3D const foot = foot_of({}, 0.0F, 1.0F);
  QVector3D stone_at(0.0F, 0.0F, foot.z() - 1.5F);
  ASSERT_TRUE(on_slope(stone_at.x(), stone_at.z()))
      << "the test stone should lie on the slope";
  Game::Map::WorldProp stone;
  stone.type = Game::Map::WorldProp::Type::Boulder;
  auto const stone_id =
      m_session->terrain().add_world_prop_at_world(stone, stone_at.x(), stone_at.z());
  NavGrid::get_pathfinder()->mark_navigation_grid_dirty();
  NavGrid::get_pathfinder()->update_navigation_grid();

  EntityID const crew =
      spawn(SpawnType::Builder, k_player, {0.0F, 0.0F, foot.z() + 6.0F});
  ASSERT_NE(crew, 0U);
  Game::Command::submit(
      m_session->world(),
      Game::Command::Source::LocalPlayer,
      k_player,
      Game::Command::StartHarvest{.units = {crew},
                                  .construction_type = "collect_stone",
                                  .resource_target = stone_id,
                                  .site = stone_at});
  expect_no_slope(run_watching({crew}, 40.0), "gathering a stone on the slope");
}

} // namespace
