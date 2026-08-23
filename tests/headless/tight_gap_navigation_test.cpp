

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <gtest/gtest.h>
#include <limits>
#include <memory>
#include <vector>

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/command_service.h"
#include "game/systems/default_content.h"
#include "game/systems/formation_combat_geometry.h"
#include "game/systems/nation_registry.h"
#include "game/systems/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/pathfinding.h"
#include "game/systems/runtime_system_registry.h"
#include "game/systems/wall_network_service.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"

namespace {

using Engine::Core::EntityID;
using Engine::Core::TransformComponent;
using Engine::Core::UnitComponent;
using Game::Session::SessionContext;
using Game::Systems::CommandService;
using Game::Systems::NavGrid;
using Game::Systems::Point;

constexpr int k_owner = 1;

constexpr int k_bare_field = 31;
constexpr int k_map = 48;

class TightGapNavigationTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::NationRegistry::instance().clear();
    Game::Systems::initialize_default_content(
        Game::Systems::NationRegistry::instance());
    NavGrid::initialize(k_map, k_map);
    m_factory = std::make_shared<Game::Units::UnitFactoryRegistry>();
    Game::Units::register_built_in_units(*m_factory);
  }

  void TearDown() override {
    m_scope.reset();
    m_session.reset();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
  }

  auto open_field(int size = k_map) -> SessionContext& {
    Game::Map::MapDefinition map;
    map.grid.width = size;
    map.grid.height = size;
    map.grid.tile_size = 1.0F;
    map.biome.procedural_boulders_enabled = false;
    map.biome.procedural_iron_ore_enabled = false;
    map.biome.procedural_trees_enabled = false;
    return match(map);
  }

  auto match(const Game::Map::MapDefinition& map) -> SessionContext& {
    m_session = std::make_unique<SessionContext>();
    m_session->world().set_presentation_enabled(false);
    m_scope = std::make_unique<Game::Session::ScopedSession>(*m_session);
    m_session->owners().register_owner_with_id(
        k_owner, Game::Systems::OwnerType::Player, "carthage");
    m_session->owners().set_owner_team(k_owner, 1);
    Game::Systems::initialize_default_content(m_session->nations());
    Game::Systems::register_runtime_systems(m_session->world());
    m_session->terrain().initialize(map);
    NavGrid::initialize(map.grid.width, map.grid.height);
    m_grid_size = map.grid.width;
    return *m_session;
  }

  static auto world_of(int grid_x, int grid_z) -> QVector3D {
    return NavGrid::grid_to_world(Point(grid_x, grid_z));
  }

  static auto cell_of(const QVector3D& position) -> Point {
    return NavGrid::world_to_grid(position.x(), position.z());
  }

  auto spawn(Game::Units::SpawnType type,
             const QVector3D& position,
             float rotation_y = 0.0F) -> EntityID {
    Game::Units::SpawnParams params;
    params.position = position;
    params.player_id = k_owner;
    params.spawn_type = type;
    params.rotation_y = rotation_y;
    params.nation_id = Game::Systems::NationID::Carthage;
    auto unit = m_factory->create(type, m_session->world(), params);
    return unit ? unit->id() : 0;
  }

  void block_cell(int grid_x, int grid_z) {
    const auto position = world_of(grid_x, grid_z);
    auto* entity = m_session->world().create_entity();
    entity->add_component<TransformComponent>(position.x(), 0.0F, position.z());
    auto* unit = entity->add_component<UnitComponent>(400, 400, 0.0F, 0.0F);
    unit->owner_id = k_owner;
    unit->spawn_type = Game::Units::SpawnType::WallSegment;
    entity->add_component<Engine::Core::BuildingComponent>();
    Game::Systems::BuildingCollisionRegistry::instance().register_building(
        entity->get_id(),
        "wall_segment",
        position.x(),
        position.z(),
        k_owner,
        {.width = 1.0F, .depth = 1.0F});
  }

  void refresh_grid() {
    pathfinder().mark_navigation_grid_dirty();
    pathfinder().update_navigation_grid();
  }

  void wall_off_column(int grid_x, int gap_z) {
    for (int grid_z = 0; grid_z < m_grid_size; ++grid_z) {
      if (grid_z == gap_z) {
        continue;
      }
      block_cell(grid_x, grid_z);
    }
    refresh_grid();
  }

  static auto pathfinder() -> Game::Systems::Pathfinding& {
    auto* pf = NavGrid::get_pathfinder();
    EXPECT_NE(pf, nullptr);
    return *pf;
  }

  void run_for(double seconds) {
    const double step = m_session->clock().tick_seconds();
    for (double elapsed = 0.0; elapsed < seconds; elapsed += step) {
      m_session->clock().advance(step);
      while (m_session->clock().consume_tick()) {
        m_session->world().update(static_cast<float>(step));
      }
    }
  }

  auto position_of(EntityID id) -> QVector3D {
    auto* entity = m_session->world().get_entity(id);
    if (entity == nullptr) {
      return {};
    }
    const auto* transform = entity->get_component<TransformComponent>();
    return transform == nullptr
               ? QVector3D()
               : QVector3D(transform->position.x, 0.0F, transform->position.z);
  }

  auto march_east(const std::vector<EntityID>& army,
                  const QVector3D& target,
                  double seconds) -> int {
    std::vector<QVector3D> targets(army.size(), target);
    CommandService::move_units(m_session->world(), army, targets);
    run_for(seconds);
    int arrived = 0;
    for (const auto id : army) {
      if (position_of(id).x() > target.x() - 4.0F) {
        arrived++;
      }
    }
    return arrived;
  }

  int m_grid_size{k_map};
  std::unique_ptr<SessionContext> m_session;
  std::unique_ptr<Game::Session::ScopedSession> m_scope;
  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_factory;
};

TEST_F(TightGapNavigationTest, RouteTakesTheMiddleOfAWideCorridor) {
  open_field(k_bare_field);
  constexpr int k_corridor_z = 15;

  for (int grid_x = 10; grid_x <= 20; ++grid_x) {
    for (int grid_z = 0; grid_z < k_bare_field; ++grid_z) {
      if (grid_z >= k_corridor_z - 1 && grid_z <= k_corridor_z + 1) {
        continue;
      }
      block_cell(grid_x, grid_z);
    }
  }
  refresh_grid();

  const auto path = pathfinder().find_path({4, k_corridor_z}, {26, k_corridor_z});
  ASSERT_FALSE(path.empty());
  ASSERT_EQ(path.back().x, 26);

  for (const auto& cell : path) {
    if (cell.x < 10 || cell.x > 20) {
      continue;
    }
    EXPECT_EQ(cell.y, k_corridor_z)
        << "route left the free middle of the corridor at x=" << cell.x;
  }
}

TEST_F(TightGapNavigationTest, RouteKeepsClearOfAWallItRunsAlong) {
  open_field();
  constexpr int k_wall_z = 24;
  for (int grid_x = 10; grid_x <= 38; ++grid_x) {
    block_cell(grid_x, k_wall_z);
  }
  refresh_grid();

  const auto path = pathfinder().find_path({6, k_wall_z + 2}, {42, k_wall_z + 2});
  ASSERT_FALSE(path.empty());
  ASSERT_EQ(path.back().x, 42);

  for (const auto& cell : path) {
    if (cell.x < 10 || cell.x > 38) {
      continue;
    }
    EXPECT_GT(cell.y, k_wall_z + 1) << "route scraped along the wall at x=" << cell.x
                                    << " instead of keeping clear";
  }
}

TEST_F(TightGapNavigationTest, ADiagonalWallOneCellThickIsStillAWall) {
  open_field(k_bare_field);
  constexpr int k_anti_diagonal = 30;
  for (int grid_x = 0; grid_x <= k_anti_diagonal; ++grid_x) {
    block_cell(grid_x, k_anti_diagonal - grid_x);
  }
  refresh_grid();

  const Point near_corner{14, 15};
  const Point far_corner{15, 16};
  ASSERT_TRUE(pathfinder().is_walkable(near_corner.x, near_corner.y));
  ASSERT_TRUE(pathfinder().is_walkable(far_corner.x, far_corner.y));
  ASSERT_FALSE(pathfinder().is_walkable(far_corner.x, near_corner.y));
  ASSERT_FALSE(pathfinder().is_walkable(near_corner.x, far_corner.y));

  EXPECT_FALSE(pathfinder().is_world_segment_walkable(
      world_of(near_corner.x, near_corner.y), world_of(far_corner.x, far_corner.y)))
      << "a straight line squeezed between two corners that touch";

  const auto path = pathfinder().find_path({4, 4}, {26, 26});
  ASSERT_FALSE(path.empty());
  EXPECT_FALSE(path.back().x == 26 && path.back().y == 26)
      << "the search found a way through a wall with no gap in it";

  for (std::size_t i = 1; i < path.size(); ++i) {
    const auto& previous = path[i - 1];
    const auto& cell = path[i];
    if (previous.x == cell.x || previous.y == cell.y) {
      continue;
    }
    EXPECT_TRUE(pathfinder().is_walkable(cell.x, previous.y) &&
                pathfinder().is_walkable(previous.x, cell.y))
        << "route cut the corner from (" << previous.x << "," << previous.y << ") to ("
        << cell.x << "," << cell.y << ")";
  }
}

TEST_F(TightGapNavigationTest, NobodyWalksThroughADiagonalWall) {
  open_field(k_bare_field);
  constexpr int k_anti_diagonal = 30;
  for (int grid_x = 0; grid_x <= k_anti_diagonal; ++grid_x) {
    block_cell(grid_x, k_anti_diagonal - grid_x);
  }
  refresh_grid();

  std::vector<EntityID> army;
  for (int i = 0; i < 8; ++i) {
    const EntityID id =
        spawn(Game::Units::SpawnType::Spearman, world_of(4 + (i % 4), 4 + (i / 4)));
    ASSERT_NE(id, 0U);
    army.push_back(id);
  }

  std::vector<QVector3D> targets(army.size(), world_of(26, 26));
  CommandService::move_units(m_session->world(), army, targets);
  run_for(60.0);

  for (const auto id : army) {
    const auto cell = cell_of(position_of(id));
    EXPECT_LT(cell.x + cell.y, k_anti_diagonal)
        << "a unit got through a wall with no gap in it, at (" << cell.x << ","
        << cell.y << ")";
    EXPECT_TRUE(pathfinder().is_walkable(cell.x, cell.y))
        << "a unit stood on a blocked cell at (" << cell.x << "," << cell.y << ")";
  }
}

TEST_F(TightGapNavigationTest, EveryUnitFitsThroughTheSameOneCellGap) {
  open_field();
  constexpr int k_gap_z = 24;
  wall_off_column(24, k_gap_z);

  ASSERT_TRUE(pathfinder().is_walkable(24, k_gap_z));

  for (const auto type : {Game::Units::SpawnType::Spearman,
                          Game::Units::SpawnType::Elephant,
                          Game::Units::SpawnType::Catapult,
                          Game::Units::SpawnType::HorseSpearman}) {
    const EntityID id = spawn(type, world_of(14, k_gap_z));
    ASSERT_NE(id, 0U);
    CommandService::move_unit(m_session->world(), id, world_of(34, k_gap_z));
    run_for(30.0);
    EXPECT_GT(position_of(id).x(), world_of(30, k_gap_z).x())
        << "a unit of type " << static_cast<int>(type)
        << " could not use a gap the grid says is open";
    m_session->world().destroy_entity(id);
    run_for(0.2);
  }
}

TEST_F(TightGapNavigationTest, AnArmyFunnelsThroughAOneCellGap) {
  open_field();
  constexpr int k_gap_z = 24;
  wall_off_column(24, k_gap_z);

  std::vector<EntityID> army;
  for (int i = 0; i < 20; ++i) {
    const int row = k_gap_z - 4 + (i % 9);
    const int column = 10 + (i / 9);
    const EntityID id = spawn(Game::Units::SpawnType::Spearman, world_of(column, row));
    ASSERT_NE(id, 0U);
    army.push_back(id);
  }

  const int arrived = march_east(army, world_of(36, k_gap_z), 90.0);
  EXPECT_GE(arrived, static_cast<int>(army.size()))
      << arrived << " of " << army.size() << " made it through the gap";
}

TEST_F(TightGapNavigationTest, FormationSqueezeChangesPresentationButNotCombatLayout) {
  open_field();
  m_session->world().set_presentation_enabled(true);
  constexpr int k_gap_z = 24;
  wall_off_column(24, k_gap_z);

  const EntityID id = spawn(Game::Units::SpawnType::Spearman, world_of(14, k_gap_z));
  auto* entity = m_session->world().get_entity(id);
  ASSERT_NE(entity, nullptr);
  auto const original_layout = Game::Systems::FormationCombat::resolve_layout(*entity);
  float original_half_width = 0.0F;
  for (auto const& slot : original_layout.live_slots) {
    original_half_width = std::max(original_half_width, std::abs(slot.local_x));
  }
  ASSERT_GT(original_half_width, 0.5F);

  CommandService::move_unit(m_session->world(), id, world_of(42, k_gap_z));
  bool squeezed = false;
  float narrowest_presented_half_width = original_half_width;
  const double step = m_session->clock().tick_seconds();
  for (double elapsed = 0.0; elapsed < 30.0; elapsed += step) {
    run_for(step);
    auto const* motion =
        entity->get_component<Engine::Core::MotionPresentationComponent>();
    auto const* presentation =
        entity->get_component<Engine::Core::FormationPresentationComponent>();
    if (motion == nullptr || presentation == nullptr ||
        motion->traversal_target_lateral_scale >= 0.999F) {
      continue;
    }

    squeezed = true;
    float presented_half_width = 0.0F;
    for (auto const& soldier : presentation->soldiers) {
      if (soldier.alive) {
        presented_half_width =
            std::max(presented_half_width, std::abs(soldier.local_x));
      }
    }
    narrowest_presented_half_width =
        std::min(narrowest_presented_half_width, presented_half_width);

    auto const authoritative_layout =
        Game::Systems::FormationCombat::resolve_layout(*entity);
    ASSERT_EQ(authoritative_layout.live_slots.size(),
              original_layout.live_slots.size());
    for (std::size_t index = 0; index < original_layout.live_slots.size(); ++index) {
      EXPECT_FLOAT_EQ(authoritative_layout.live_slots[index].local_x,
                      original_layout.live_slots[index].local_x);
      EXPECT_FLOAT_EQ(authoritative_layout.live_slots[index].local_z,
                      original_layout.live_slots[index].local_z);
    }
  }

  EXPECT_TRUE(squeezed);
  EXPECT_LT(narrowest_presented_half_width, original_half_width * 0.8F);
  EXPECT_GT(position_of(id).x(), world_of(38, k_gap_z).x());

  run_for(2.0);
  auto const* restored =
      entity->get_component<Engine::Core::MotionPresentationComponent>();
  ASSERT_NE(restored, nullptr);
  EXPECT_FALSE(restored->traversal_squeeze_active);
  EXPECT_NEAR(restored->traversal_lateral_scale, 1.0F, 0.001F);
}

TEST_F(TightGapNavigationTest, LargeBodySqueezeExistsOnlyInTheRenderSnapshot) {
  open_field();
  m_session->world().set_presentation_enabled(true);
  m_session->world().request_render_snapshots();
  constexpr int k_gap_z = 24;
  wall_off_column(24, k_gap_z);

  const EntityID id = spawn(Game::Units::SpawnType::Elephant, world_of(14, k_gap_z));
  auto* entity = m_session->world().get_entity(id);
  ASSERT_NE(entity, nullptr);
  auto* authoritative = entity->get_component<TransformComponent>();
  ASSERT_NE(authoritative, nullptr);
  float const original_scale_x = authoritative->scale.x;
  CommandService::move_unit(m_session->world(), id, world_of(34, k_gap_z));

  bool observed_render_squeeze = false;
  const double step = m_session->clock().tick_seconds();
  for (double elapsed = 0.0; elapsed < 30.0; elapsed += step) {
    run_for(step);
    auto const* motion =
        entity->get_component<Engine::Core::MotionPresentationComponent>();
    if (motion == nullptr || motion->traversal_lateral_scale >= 0.99F) {
      continue;
    }
    auto const snapshot = m_session->world().acquire_render_snapshot();
    auto const* rendered = snapshot != nullptr ? snapshot->get_entity(id) : nullptr;
    auto const* rendered_transform =
        rendered != nullptr ? rendered->get_component<TransformComponent>() : nullptr;
    ASSERT_NE(rendered_transform, nullptr);
    EXPECT_FLOAT_EQ(authoritative->scale.x, original_scale_x);
    EXPECT_LT(rendered_transform->scale.x, original_scale_x);
    observed_render_squeeze = true;
    break;
  }
  EXPECT_TRUE(observed_render_squeeze);
  EXPECT_FLOAT_EQ(authoritative->scale.x, original_scale_x);

  run_for(30.0);
  run_for(2.0);
  auto const* restored_motion =
      entity->get_component<Engine::Core::MotionPresentationComponent>();
  ASSERT_NE(restored_motion, nullptr);
  EXPECT_GT(position_of(id).x(), world_of(30, k_gap_z).x())
      << "target_scale=" << restored_motion->traversal_target_lateral_scale
      << " available=" << restored_motion->traversal_available_half_width
      << " desired=" << restored_motion->traversal_desired_half_width;
  EXPECT_FALSE(restored_motion->traversal_squeeze_active)
      << "position=(" << position_of(id).x() << "," << position_of(id).z()
      << ") yaw=" << authoritative->rotation.y
      << " target_scale=" << restored_motion->traversal_target_lateral_scale
      << " available=" << restored_motion->traversal_available_half_width
      << " desired=" << restored_motion->traversal_desired_half_width;
  auto const restored_snapshot = m_session->world().acquire_render_snapshot();
  auto const* restored_entity =
      restored_snapshot != nullptr ? restored_snapshot->get_entity(id) : nullptr;
  auto const* restored_transform =
      restored_entity != nullptr ? restored_entity->get_component<TransformComponent>()
                                 : nullptr;
  ASSERT_NE(restored_transform, nullptr);
  EXPECT_FLOAT_EQ(restored_transform->scale.x, original_scale_x);
}

TEST_F(TightGapNavigationTest, SiegeBodyTurnsAtABoundedRateBeforeFullSpeed) {
  open_field();
  const QVector3D start = world_of(14, 24);
  const EntityID id = spawn(Game::Units::SpawnType::Catapult, start, 0.0F);
  auto* entity = m_session->world().get_entity(id);
  ASSERT_NE(entity, nullptr);
  auto* transform = entity->get_component<TransformComponent>();
  auto const* unit = entity->get_component<UnitComponent>();
  ASSERT_NE(transform, nullptr);
  ASSERT_NE(unit, nullptr);

  CommandService::move_unit(m_session->world(), id, world_of(34, 24));
  float const step = static_cast<float>(m_session->clock().tick_seconds());
  m_session->world().update(step);

  EXPECT_GT(transform->rotation.y, 0.0F);
  EXPECT_LE(transform->rotation.y, 100.0F * step + 0.01F);
  EXPECT_LT(
      std::hypot(transform->position.x - start.x(), transform->position.z - start.z()),
      unit->speed * step * 0.5F);

  run_for(3.0);
  EXPECT_GT(transform->rotation.y, 75.0F);
  EXPECT_LT(transform->rotation.y, 95.0F);
  EXPECT_GT(transform->position.x, start.x() + 0.5F);
}

TEST_F(TightGapNavigationTest, AnArmyCrossesARiverOnTheBridgeDeck) {
  Game::Map::MapDefinition map;
  map.grid.width = k_map;
  map.grid.height = k_map;
  map.grid.tile_size = 1.0F;
  map.coordSystem = Game::Map::CoordSystem::World;
  map.rivers.push_back(
      {QVector3D(0.0F, 0.0F, -24.0F), QVector3D(0.0F, 0.0F, 24.0F), 6.0F});
  map.bridges.push_back(
      {QVector3D(-6.0F, 0.0F, 0.0F), QVector3D(6.0F, 0.0F, 0.0F), 3.0F, 0.6F});
  match(map);

  auto& terrain = Game::Map::TerrainService::instance();
  auto& pf = pathfinder();
  pf.update_navigation_grid();

  const Point deck = cell_of(QVector3D(0.0F, 0.0F, 0.0F));
  ASSERT_TRUE(pf.is_walkable(deck.x, deck.y)) << "the bridge deck must be walkable";

  std::vector<EntityID> army;
  for (int i = 0; i < 12; ++i) {
    const QVector3D start(
        -14.0F - static_cast<float>(i / 6), 0.0F, static_cast<float>(-3 + (i % 6)));
    const EntityID id = spawn(Game::Units::SpawnType::Spearman, start);
    ASSERT_NE(id, 0U);
    army.push_back(id);
  }

  std::vector<QVector3D> targets(army.size(), QVector3D(14.0F, 0.0F, 0.0F));
  CommandService::move_units(m_session->world(), army, targets);

  int drowned = 0;
  std::vector<bool> crossed_centerline(army.size(), false);
  std::vector<float> max_centerline_offset(army.size(), 0.0F);
  const double step = m_session->clock().tick_seconds();
  for (double elapsed = 0.0; elapsed < 90.0; elapsed += step) {
    run_for(step);
    for (std::size_t index = 0; index < army.size(); ++index) {
      const QVector3D position = position_of(army[index]);
      const auto cell = cell_of(position);
      const bool on_water =
          Game::Map::is_water_terrain(terrain.get_terrain_type(cell.x, cell.y));
      const auto* height_map = terrain.get_height_map();
      const bool on_deck =
          height_map != nullptr && (height_map->isBridgeCell(cell.x, cell.y) ||
                                    height_map->isBridgeCenterline(cell.x, cell.y));
      if (on_water && !on_deck) {
        drowned++;
      }
      if (on_deck && std::abs(position.x()) < 5.0F) {
        crossed_centerline[index] = true;
        max_centerline_offset[index] =
            std::max(max_centerline_offset[index], std::abs(position.z()));
      }
    }
  }
  EXPECT_EQ(drowned, 0) << "units stood in the river instead of on the deck";
  EXPECT_TRUE(std::all_of(crossed_centerline.begin(),
                          crossed_centerline.end(),
                          [](bool crossed) { return crossed; }))
      << "not every unit traversed the bridge centerline independently";
  for (std::size_t index = 0; index < army.size(); ++index) {
    EXPECT_LE(max_centerline_offset[index], 0.75F)
        << "unit " << index << " drifted off the bridge centerline";
  }

  int crossed = 0;
  for (const auto id : army) {
    if (position_of(id).x() > 6.0F) {
      crossed++;
    }
  }
  EXPECT_EQ(crossed, static_cast<int>(army.size()))
      << crossed << " of " << army.size() << " crossed the bridge";
}

TEST_F(TightGapNavigationTest, AnArmyClimbsAHillThroughItsEntrance) {
  Game::Map::MapDefinition map;
  map.grid.width = k_map;
  map.grid.height = k_map;
  map.grid.tile_size = 1.0F;
  map.coordSystem = Game::Map::CoordSystem::World;
  Game::Map::TerrainFeature hill;
  hill.type = Game::Map::TerrainType::Hill;
  hill.center_x = 0.0F;
  hill.center_z = 0.0F;
  hill.width = 16.0F;
  hill.depth = 16.0F;
  hill.height = 5.0F;
  hill.entrances.emplace_back(-8.0F, 0.0F, 0.0F);
  map.terrain.push_back(hill);
  match(map);

  auto& terrain = Game::Map::TerrainService::instance();
  auto& pf = pathfinder();
  pf.update_navigation_grid();

  const Point crown = cell_of(QVector3D(0.0F, 0.0F, 0.0F));
  ASSERT_TRUE(pf.is_walkable(crown.x, crown.y));

  std::vector<EntityID> army;
  for (int i = 0; i < 12; ++i) {
    const QVector3D start(
        -16.0F - static_cast<float>(i / 6), 0.0F, static_cast<float>(-3 + (i % 6)));
    const EntityID id = spawn(Game::Units::SpawnType::Spearman, start);
    ASSERT_NE(id, 0U);
    army.push_back(id);
  }

  std::vector<QVector3D> targets(army.size(), QVector3D(0.0F, 0.0F, 0.0F));
  CommandService::move_units(m_session->world(), army, targets);

  std::vector<bool> used_entrance_centerline(army.size(), false);
  std::vector<float> closest_entrance_distance(army.size(),
                                               std::numeric_limits<float>::infinity());
  std::vector<float> entrance_offset(army.size(), 0.0F);
  const double step = m_session->clock().tick_seconds();
  for (double elapsed = 0.0; elapsed < 90.0; elapsed += step) {
    run_for(step);
    for (std::size_t index = 0; index < army.size(); ++index) {
      const QVector3D position = position_of(army[index]);
      const Point cell = cell_of(position);

      if (terrain.is_hill_entrance(cell.x, cell.y)) {
        used_entrance_centerline[index] = true;
        float const anchor_distance =
            std::abs(position.x() - hill.entrances.front().x());
        if (anchor_distance < closest_entrance_distance[index]) {
          closest_entrance_distance[index] = anchor_distance;
          entrance_offset[index] = std::abs(position.z());
        }
      }
    }
  }

  EXPECT_TRUE(std::all_of(used_entrance_centerline.begin(),
                          used_entrance_centerline.end(),
                          [](bool used) { return used; }))
      << "not every unit traversed the hill entrance independently";
  for (std::size_t index = 0; index < army.size(); ++index) {
    EXPECT_LE(closest_entrance_distance[index], 0.25F)
        << "unit " << index << " never crossed the hill entrance throat";
    EXPECT_LE(entrance_offset[index], 0.75F)
        << "unit " << index << " drifted off the hill entrance centerline";
  }

  int on_the_hill = 0;
  for (const auto id : army) {
    const auto cell = cell_of(position_of(id));
    if (terrain.get_terrain_type(cell.x, cell.y) == Game::Map::TerrainType::Hill) {
      on_the_hill++;
    }
    EXPECT_TRUE(pf.is_walkable(cell.x, cell.y))
        << "a unit ended up on a cell the grid calls blocked";
  }
  EXPECT_GT(on_the_hill, 0) << "nobody found the ramp up the hill";
}

TEST_F(TightGapNavigationTest, NobodyEverStandsOnABlockedCell) {
  open_field();
  constexpr int k_gap_z = 24;
  wall_off_column(24, k_gap_z);

  std::vector<EntityID> army;
  for (int i = 0; i < 16; ++i) {
    const EntityID id = spawn(Game::Units::SpawnType::Spearman,
                              world_of(12 + (i / 8), k_gap_z - 4 + (i % 8)));
    ASSERT_NE(id, 0U);
    army.push_back(id);
  }

  std::vector<QVector3D> targets(army.size(), world_of(36, k_gap_z));
  CommandService::move_units(m_session->world(), army, targets);

  int trespasses = 0;
  const double step = m_session->clock().tick_seconds();
  for (double elapsed = 0.0; elapsed < 60.0; elapsed += step) {
    run_for(step);
    for (const auto id : army) {
      const auto cell = cell_of(position_of(id));
      if (!pathfinder().is_walkable(cell.x, cell.y)) {
        trespasses++;
      }
    }
  }

  EXPECT_EQ(trespasses, 0) << "units stood inside the barrier while squeezing through";
}

} // namespace

TEST_F(TightGapNavigationTest, ScratchGateLineHalfCell) {
  open_field(30);
  auto& pf = pathfinder();
  auto place = [&](Game::Units::SpawnType type, float x, float z) {
    Game::Units::SpawnParams params;
    params.position = QVector3D(x, 0.0F, z);
    params.player_id = k_owner;
    params.spawn_type = type;
    params.nation_id = Game::Systems::NationID::RomanRepublic;
    m_factory->create(type, m_session->world(), params);
  };
  for (float x : {-7.0F, -5.0F, -3.0F, -1.0F, 7.0F, 9.0F, 11.0F, 13.0F}) {
    place(Game::Units::SpawnType::WallSegment, x, 0.0F);
  }
  place(Game::Units::SpawnType::WallGate, 0.0F, 0.0F);
  Game::Systems::WallNetworkService::refresh_world(m_session->world());
  pf.mark_navigation_grid_dirty();
  pf.update_navigation_grid();

  const auto origin = cell_of(QVector3D(0.0F, 0.0F, 0.0F));
  std::printf("origin cell (%d,%d) -> world (%.2f,%.2f)\n",
              origin.x,
              origin.y,
              NavGrid::grid_to_world(origin).x(),
              NavGrid::grid_to_world(origin).z());
  for (int dz = -5; dz <= 4; ++dz) {
    std::string row;
    for (int dx = -8; dx <= 8; ++dx) {
      row += pf.is_walkable(origin.x + dx, origin.y + dz) ? '.' : '#';
    }
    std::printf("z=%6.1f %s\n",
                NavGrid::grid_to_world({origin.x, origin.y + dz}).z(),
                row.c_str());
  }
  for (float z : {-3.5F, -2.5F, -1.5F}) {
    for (float x : {0.5F, 1.5F}) {
      std::printf("walkable(%.1f,%.1f)=%d\n",
                  x,
                  z,
                  static_cast<int>(pf.is_world_position_walkable(QVector3D(x, 0, z))));
    }
  }
}
