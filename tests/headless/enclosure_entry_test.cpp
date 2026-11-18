#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <unordered_map>
#include <vector>

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/default_content.h"
#include "game/systems/movement/command_service.h"
#include "game/systems/nation_registry.h"
#include "game/systems/navigation/nav_grid.h"
#include "game/systems/navigation/pathfinding.h"
#include "game/systems/navigation/wall_network_service.h"
#include "game/systems/owner_registry.h"
#include "game/systems/run_stamina.h"
#include "game/systems/runtime_system_registry.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"

namespace {

using Engine::Core::EntityID;
using Game::Session::SessionContext;
using Game::Systems::NavGrid;
using Game::Systems::Point;
using Game::Units::SpawnType;

constexpr int k_raider = 1;
constexpr int k_camp_owner = 2;
constexpr int k_grid = 200;

constexpr int k_west = 70;
constexpr int k_east = 140;
constexpr int k_north = 62;
constexpr int k_south = 144;

struct WallRun {
  int from_x;
  int from_z;
  int to_x;
  int to_z;
};

constexpr WallRun k_camp_walls[] = {
    {k_west, k_north, 102, k_north},
    {110, k_north, k_east, k_north},
    {k_west, k_south, 102, k_south},
    {110, k_south, k_east, k_south},
    {k_west, k_north, k_west, 80},
    {k_west, 88, k_west, 120},
    {k_west, 128, k_west, k_south},
    {k_east, k_north, k_east, 74},
    {k_east, 82, k_east, 100},
    {k_east, 108, k_east, 126},
    {k_east, 134, k_east, k_south},
};

constexpr float k_window_seconds = 0.5F;
constexpr float k_touching_wall = 0.35F;
constexpr float k_pinned_travel = 0.25F;
constexpr float k_squad_travel = 1.0F;

enum class Walls {
  Spawned,
  SpawnedOverAuthoredObstacles
};

struct Box {
  float min_x;
  float max_x;
  float min_z;
  float max_z;
};

struct EntryCensus {
  double seconds_pinned_at_a_wall{0.0};
  double seconds_inside_a_wall{0.0};
  float worst_stray{0.0F};
  int squads_inside{0};
};

class EnclosureEntryTest : public ::testing::Test {
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
    m_handles.clear();
    m_scope.reset();
    m_session.reset();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
  }

  static auto world_of(float grid_x, float grid_z) -> QVector3D {
    QVector3D const origin = NavGrid::grid_to_world(Point(0, 0));
    return {origin.x() + grid_x, 0.0F, origin.z() + grid_z};
  }

  void camp(Walls walls) {
    Game::Map::MapDefinition map;
    map.grid.width = k_grid;
    map.grid.height = k_grid;
    map.grid.tile_size = 1.0F;
    map.coordSystem = Game::Map::CoordSystem::World;
    map.biome.procedural_boulders_enabled = false;
    map.biome.procedural_iron_ore_enabled = false;
    map.biome.procedural_trees_enabled = false;
    NavGrid::initialize(k_grid, k_grid);
    for (auto const& run : k_camp_walls) {
      Game::Map::StructureEntry wall;
      wall.type = SpawnType::WallSegment;
      wall.player_id = k_camp_owner;
      wall.geometry =
          Game::Map::LineStructureGeometry{.start = world_of(run.from_x, run.from_z),
                                           .end = world_of(run.to_x, run.to_z),
                                           .width = 2.0F};
      map.structures.push_back(wall);
    }

    m_session = std::make_unique<SessionContext>();
    m_session->world().set_presentation_enabled(true);
    m_scope = std::make_unique<Game::Session::ScopedSession>(*m_session);
    m_session->owners().register_owner_with_id(
        k_raider, Game::Systems::OwnerType::Player, "raiders");
    m_session->owners().set_owner_team(k_raider, 1);
    m_session->owners().register_owner_with_id(
        k_camp_owner, Game::Systems::OwnerType::Player, "camp");
    m_session->owners().set_owner_team(k_camp_owner, 2);
    Game::Systems::initialize_default_content(m_session->nations());
    Game::Systems::register_runtime_systems(m_session->world());
    m_session->terrain().initialize(map);
    NavGrid::initialize(map.grid.width, map.grid.height);

    for (auto const& run : k_camp_walls) {
      for (auto const& link :
           Game::Systems::WallNetworkService::build_axis_aligned_chain(
               {.x = run.from_x, .z = run.from_z}, {.x = run.to_x, .z = run.to_z})) {
        spawn(SpawnType::WallSegment, k_camp_owner, world_of(link.x, link.z));
      }
    }
    auto& registry = m_session->building_collision();
    if (walls == Walls::Spawned) {
      registry.clear_authored_obstacles();
    }
    auto* pathfinder = NavGrid::get_pathfinder();
    pathfinder->mark_navigation_grid_dirty();
    pathfinder->update_navigation_grid();

    auto const add_body = [this](const Game::Systems::BuildingFootprint& footprint) {
      m_walls.push_back({footprint.body_center_x - footprint.body_width * 0.5F,
                         footprint.body_center_x + footprint.body_width * 0.5F,
                         footprint.body_center_z - footprint.body_depth * 0.5F,
                         footprint.body_center_z + footprint.body_depth * 0.5F});
    };
    for (auto const& footprint : registry.get_all_buildings()) {
      add_body(footprint);
    }
    for (auto const& footprint : registry.authored_obstacles()) {
      add_body(footprint);
    }
  }

  auto
  spawn(SpawnType type, int owner, const QVector3D& at, float yaw = 0.0F) -> EntityID {
    Game::Units::SpawnParams params;
    params.position = at;
    params.player_id = owner;
    params.spawn_type = type;
    params.rotation_y = yaw;
    params.nation_id = owner == k_camp_owner ? Game::Systems::NationID::RomanRepublic
                                             : Game::Systems::NationID::Carthage;
    auto unit = m_factory->create(type, m_session->world(), params);
    if (!unit) {
      return 0;
    }
    EntityID const id = unit->id();
    m_handles.push_back(std::move(unit));
    return id;
  }

  auto position(EntityID id) -> QVector3D {
    auto const* t = m_session->world().try_get<Engine::Core::TransformComponent>(id);
    return t == nullptr ? QVector3D() : QVector3D(t->position.x, 0.0F, t->position.z);
  }

  [[nodiscard]] auto wall_gap(float x, float z) const -> float {
    float best = 1.0e9F;
    for (auto const& wall : m_walls) {
      float const dx = std::max({wall.min_x - x, 0.0F, x - wall.max_x});
      float const dz = std::max({wall.min_z - z, 0.0F, z - wall.max_z});
      best = std::min(best, std::hypot(dx, dz));
    }
    return best;
  }

  [[nodiscard]] auto inside_camp(const QVector3D& at) const -> bool {
    QVector3D const low = world_of(k_west + 1, k_north + 1);
    QVector3D const high = world_of(k_east - 1, k_south - 1);
    return at.x() > low.x() && at.x() < high.x() && at.z() > low.z() &&
           at.z() < high.z();
  }

  auto storm_the_camp(double seconds) -> EntryCensus {
    QVector3D const corner = world_of(k_east, k_north);
    std::vector<EntityID> guards;
    for (float const south : {31.0F, 41.0F, 51.0F}) {
      guards.push_back(spawn(SpawnType::Swordsman,
                             k_camp_owner,
                             QVector3D(corner.x() - 21.0F, 0.0F, corner.z() + south),
                             90.0F));
    }
    std::vector<EntityID> raiders;
    for (int file = 0; file < 4; ++file) {
      for (int rank = 0; rank < 3; ++rank) {
        raiders.push_back(spawn(SpawnType::Swordsman,
                                k_raider,
                                QVector3D(corner.x() + 4.0F + 8.0F * rank,
                                          0.0F,
                                          corner.z() - 34.0F + 8.0F * file),
                                180.0F));
      }
    }
    for (auto const id : raiders) {
      auto* entity = m_session->world().get_entity(id);
      entity->get_component<Engine::Core::UnitComponent>()->speed = 2.2F;
      if (auto* stamina = Game::Systems::ensure_run_stamina(*entity)) {
        stamina->run_requested = true;
      }
    }
    for (std::size_t i = 0; i < raiders.size(); ++i) {
      Game::Systems::CommandService::attack_target(
          m_session->world(), {raiders[i]}, guards[i % guards.size()], true);
    }

    struct Mark {
      float x;
      float z;
      float squad_x;
      float squad_z;
    };
    std::unordered_map<std::uint64_t, Mark> marks;
    EntryCensus census;
    double const step = m_session->clock().tick_seconds();
    int const window =
        std::max(1, static_cast<int>(std::lround(k_window_seconds / step)));
    int tick = 0;
    for (double elapsed = 0.0; elapsed < seconds - 1e-9; elapsed += step, ++tick) {
      m_session->clock().advance(step);
      while (m_session->clock().consume_tick()) {
        m_session->world().update(static_cast<float>(step));
      }
      bool const closes_window = tick % window == window - 1;
      for (std::size_t squad = 0; squad < raiders.size(); ++squad) {
        EntityID const id = raiders[squad];
        auto const* unit = m_session->world().try_get<Engine::Core::UnitComponent>(id);
        auto const* formation =
            m_session->world().try_get<Engine::Core::FormationPresentationComponent>(
                id);
        if (unit == nullptr || unit->health <= 0 || formation == nullptr) {
          continue;
        }
        QVector3D const centre = position(id);
        for (auto const& soldier : formation->soldiers) {
          if (!soldier.alive || !soldier.world_motion_valid) {
            continue;
          }
          census.worst_stray = std::max(
              census.worst_stray,
              std::hypot(soldier.world_x - centre.x(), soldier.world_z - centre.z()));
          float const gap = wall_gap(soldier.world_x, soldier.world_z);
          if (gap <= 0.0F) {
            census.seconds_inside_a_wall += step;
          }
          if (!closes_window) {
            continue;
          }
          std::uint64_t const key =
              (static_cast<std::uint64_t>(squad) << 16U) | soldier.slot_index;
          Mark const now{soldier.world_x, soldier.world_z, centre.x(), centre.z()};
          if (auto const found = marks.find(key); found != marks.end()) {
            Mark const& then = found->second;
            bool const stood =
                std::hypot(now.x - then.x, now.z - then.z) < k_pinned_travel;
            bool const squad_left =
                std::hypot(now.squad_x - then.squad_x, now.squad_z - then.squad_z) >
                k_squad_travel;
            if (gap < k_touching_wall && stood && squad_left) {
              census.seconds_pinned_at_a_wall += k_window_seconds;
            }
          }
          marks[key] = now;
        }
      }
    }
    for (auto const id : raiders) {
      auto const* unit = m_session->world().try_get<Engine::Core::UnitComponent>(id);
      if (unit != nullptr && unit->health > 0 && inside_camp(position(id))) {
        ++census.squads_inside;
      }
    }
    return census;
  }

  static void expect_a_clean_entry(const EntryCensus& census) {
    EXPECT_EQ(census.squads_inside, 12) << "every squad reaches the camp through a gap";
    EXPECT_LT(census.seconds_pinned_at_a_wall, 1.0)
        << "soldiers stood against the palisade while their squad ran on";
    EXPECT_LT(census.seconds_inside_a_wall, 0.1)
        << "soldiers walked through the palisade";
    EXPECT_LT(census.worst_stray, 7.0F)
        << "a soldier was left behind by his squad and had to be pulled back";
  }

  std::unique_ptr<SessionContext> m_session;
  std::unique_ptr<Game::Session::ScopedSession> m_scope;
  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_factory;
  std::vector<std::unique_ptr<Game::Units::Unit>> m_handles;
  std::vector<Box> m_walls;
};

TEST_F(EnclosureEntryTest, AChargeIntoAWalledCampFlowsThroughTheGap) {
  camp(Walls::Spawned);
  expect_a_clean_entry(storm_the_camp(36.0));
}

TEST_F(EnclosureEntryTest, AChargeFlowsThroughAGapNarrowedByAuthoredObstacles) {
  camp(Walls::SpawnedOverAuthoredObstacles);
  expect_a_clean_entry(storm_the_camp(36.0));
}

} // namespace
