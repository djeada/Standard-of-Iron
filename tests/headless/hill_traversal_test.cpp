

#include <QDir>
#include <QString>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <gtest/gtest.h>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "game/core/component_core.h"
#include "game/core/movement_facts.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/map/map_loader.h"
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
#include "game/units/factory.h"
#include "game/units/spawn_type.h"
#include "tests/support/hill_plateaus.h"

namespace {

using Engine::Core::EntityID;
using Engine::Core::TransformComponent;
using Game::Session::SessionContext;
using Game::Systems::CommandService;
using Game::Systems::NavGrid;
using Game::Systems::Point;

constexpr int k_owner = 1;
constexpr int k_synthetic_grid = 160;
constexpr float k_hill_radius = 26.0F;
constexpr float k_hill_height = 7.0F;
constexpr float k_shape_extent = 60.0F;

struct ShapeCase {
  const char* name;
  Game::Map::HillShape shape;
  float thickness;
};

const ShapeCase k_shapes[] = {
    {"round", Game::Map::HillShape::Blob, 0.0F},
    {"ridge", Game::Map::HillShape::Corridor, 20.0F},
    {"crescent", Game::Map::HillShape::Arc, 20.0F},
    {"elbow", Game::Map::HillShape::Elbow, 20.0F},
    {"ring", Game::Map::HillShape::Ring, 16.0F},
};

class HillTraversalTest : public ::testing::Test {
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

  auto match(const Game::Map::MapDefinition& map) -> SessionContext& {
    m_scope.reset();
    m_session.reset();
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
    NavGrid::get_pathfinder()->update_navigation_grid();
    m_grid = map.grid.width;
    return *m_session;
  }

  auto one_hill(Game::Map::HillShape shape,
                float thickness,
                int entrances) -> SessionContext& {
    Game::Map::MapDefinition map;
    map.grid.width = k_synthetic_grid;
    map.grid.height = k_synthetic_grid;
    map.grid.tile_size = 1.0F;
    map.coordSystem = Game::Map::CoordSystem::World;
    map.biome.procedural_boulders_enabled = false;
    map.biome.procedural_iron_ore_enabled = false;
    map.biome.procedural_trees_enabled = false;

    Game::Map::TerrainFeature hill;
    hill.type = Game::Map::TerrainType::Hill;
    hill.center_x = 0.0F;
    hill.center_z = 0.0F;
    hill.radius = k_hill_radius;
    hill.height = k_hill_height;
    hill.shape = shape;
    if (shape != Game::Map::HillShape::Blob) {
      hill.width = k_shape_extent;
      hill.depth = k_shape_extent;
      hill.thickness = thickness;
    }
    if (entrances >= 1) {
      hill.entrances.emplace_back(-(k_hill_radius + 2.0F), 0.0F, 0.0F);
    }
    if (entrances >= 2) {
      hill.entrances.emplace_back(k_hill_radius + 2.0F, 0.0F, 0.0F);
    }
    map.terrain.push_back(hill);
    return match(map);
  }

  auto spawn(const QVector3D& position) -> EntityID {
    Game::Units::SpawnParams params;
    params.position = position;
    params.player_id = k_owner;
    params.spawn_type = Game::Units::SpawnType::Spearman;
    params.nation_id = Game::Systems::NationID::Carthage;
    auto unit =
        m_factory->create(Game::Units::SpawnType::Spearman, m_session->world(), params);
    return unit ? unit->id() : 0;
  }

  auto position_of(EntityID id) -> QVector3D {
    auto* entity = m_session->world().get_entity(id);
    if (entity == nullptr) {
      return {};
    }
    const auto* transform = entity->get_component<TransformComponent>();
    return transform == nullptr ? QVector3D()
                                : QVector3D(transform->position.x,
                                            transform->position.y,
                                            transform->position.z);
  }

  struct Trace {
    float lowest{0.0F};
    float highest{0.0F};
    int off_walkable_samples{0};
    QVector3D first_off_walkable;
  };

  auto trace(const std::vector<EntityID>& troops, double seconds) -> Trace {
    Trace result;
    result.lowest = std::numeric_limits<float>::infinity();
    result.highest = -std::numeric_limits<float>::infinity();
    auto const* heights = m_session->terrain().get_height_map();
    double const step = m_session->clock().tick_seconds();
    for (double elapsed = 0.0; elapsed < seconds; elapsed += step) {
      m_session->clock().advance(step);
      while (m_session->clock().consume_tick()) {
        m_session->world().update(static_cast<float>(step));
      }
      for (auto const troop : troops) {
        auto const position = position_of(troop);
        float const ground = heights->get_height_at(position.x(), position.z());
        result.lowest = std::min(result.lowest, ground);
        result.highest = std::max(result.highest, ground);
        auto const cell = NavGrid::world_to_grid(position.x(), position.z());
        if (!m_session->terrain().is_walkable(cell.x, cell.y)) {
          if (result.off_walkable_samples == 0) {
            result.first_off_walkable = position;
          }
          ++result.off_walkable_samples;
        }
      }
    }
    return result;
  }

  [[nodiscard]] auto crown() const -> std::vector<QVector3D> {
    auto const plateaus = TestSupport::hill_plateaus(m_grid);
    std::vector<QVector3D> cells;
    if (plateaus.empty()) {
      return cells;
    }
    auto const* heights = m_session->terrain().get_height_map();
    for (auto const& cell : TestSupport::crown_of(plateaus.front(), *heights)) {
      cells.push_back(NavGrid::grid_to_world(cell));
    }
    return cells;
  }

  std::unique_ptr<SessionContext> m_session;
  std::unique_ptr<Game::Session::ScopedSession> m_scope;
  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_factory;
  int m_grid{0};
};

auto ends_of(const std::vector<QVector3D>& cells) -> std::pair<QVector3D, QVector3D> {
  QVector3D first = cells.front();
  QVector3D second = cells.front();
  float best = 0.0F;
  for (std::size_t i = 0; i < cells.size(); i += 3U) {
    for (std::size_t j = i + 1U; j < cells.size(); j += 5U) {
      float const span = (cells[i] - cells[j]).length();
      if (span > best) {
        best = span;
        first = cells[i];
        second = cells[j];
      }
    }
  }
  return {first, second};
}

auto crossings(const std::vector<QVector3D>& cells,
               std::size_t wanted) -> std::vector<std::pair<QVector3D, QVector3D>> {
  std::vector<std::pair<QVector3D, QVector3D>> candidates;
  for (std::size_t i = 0; i < cells.size(); i += 37U) {
    for (std::size_t j = i + 1U; j < cells.size(); j += 53U) {
      float const span = (cells[i] - cells[j]).length();
      if (span < 12.0F || span > 40.0F) {
        continue;
      }
      candidates.emplace_back(cells[i], cells[j]);
    }
  }
  if (candidates.size() <= wanted) {
    return candidates;
  }

  std::vector<std::pair<QVector3D, QVector3D>> found;
  found.reserve(wanted);
  for (std::size_t pick = 0; pick < wanted; ++pick) {
    found.push_back(candidates[pick * candidates.size() / wanted]);
  }
  return found;
}

TEST_F(HillTraversalTest, ATroopCrossingAHilltopStaysOnIt) {
  for (auto const& shape : k_shapes) {
    one_hill(shape.shape, shape.thickness, 1);
    auto const cells = crown();
    ASSERT_GE(cells.size(), 2U) << shape.name << " grew no plateau";

    auto const [near_end, far_end] = ends_of(cells);
    auto const* heights = m_session->terrain().get_height_map();
    float const crown_height = heights->get_height_at(near_end.x(), near_end.z());

    auto const troop = spawn(near_end);
    ASSERT_NE(troop, 0U);
    trace({troop}, 2.0);

    CommandService::move_units(m_session->world(), {troop}, {far_end});
    auto const crossing = trace({troop}, 60.0);

    EXPECT_GT(crossing.lowest, crown_height - 4.0F)
        << shape.name << ": crossing the hilltop from (" << near_end.x() << ", "
        << near_end.z() << ") to (" << far_end.x() << ", " << far_end.z()
        << ") dropped to " << crossing.lowest << " m off a " << crown_height
        << " m crown";
    EXPECT_EQ(crossing.off_walkable_samples, 0)
        << shape.name << ": stood on ground it may not stand on near ("
        << crossing.first_off_walkable.x() << ", " << crossing.first_off_walkable.z()
        << ")";
  }
}

TEST_F(HillTraversalTest, ATroopOrderedOntoAHillClimbsItAndStaysOnWalkableGround) {
  for (auto const& shape : k_shapes) {
    one_hill(shape.shape, shape.thickness, 1);
    auto const cells = crown();
    ASSERT_FALSE(cells.empty()) << shape.name << " grew no plateau";

    auto const troop = spawn(QVector3D(-(k_hill_radius + 30.0F), 0.0F, 0.0F));
    ASSERT_NE(troop, 0U);
    trace({troop}, 1.0);

    CommandService::move_units(m_session->world(), {troop}, {cells.front()});
    auto const climb = trace({troop}, 90.0);

    EXPECT_GT(climb.highest, k_hill_height * 2.0F)
        << shape.name << ": never got up the hill (reached " << climb.highest << " m)";
    EXPECT_EQ(climb.off_walkable_samples, 0)
        << shape.name << ": climbed over ground it may not stand on near ("
        << climb.first_off_walkable.x() << ", " << climb.first_off_walkable.z() << ")";
  }
}

TEST_F(HillTraversalTest, EveryCrossingOfAnAuthoredHilltopStaysOnIt) {
  Game::Map::MapDefinition map;
  QString error;
  ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
      QDir(QStringLiteral("assets/maps"))
          .filePath(QStringLiteral("map_copper_canyons.json")),
      map,
      &error))
      << error.toStdString();
  match(map);

  auto const cells = crown();
  ASSERT_GE(cells.size(), 40U);
  auto const* heights = m_session->terrain().get_height_map();
  auto const pairs = crossings(cells, 12U);
  ASSERT_FALSE(pairs.empty());

  for (auto const& [from, to] : pairs) {
    float const crown_height = heights->get_height_at(from.x(), from.z());
    auto const troop = spawn(from);
    ASSERT_NE(troop, 0U);
    trace({troop}, 1.0);

    CommandService::move_units(m_session->world(), {troop}, {to});
    auto const crossing = trace({troop}, 40.0);

    EXPECT_GT(crossing.lowest, crown_height - 4.0F)
        << "crossing the hilltop from (" << from.x() << ", " << from.z() << ") to ("
        << to.x() << ", " << to.z() << ") dropped to " << crossing.lowest << " m off a "
        << crown_height << " m crown";
    EXPECT_EQ(crossing.off_walkable_samples, 0);
    m_session->world().destroy_entity(troop);
  }
}

TEST_F(HillTraversalTest, AGroupCrossingAnAuthoredHilltopStaysOnIt) {
  Game::Map::MapDefinition map;
  QString error;
  ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
      QDir(QStringLiteral("assets/maps"))
          .filePath(QStringLiteral("map_copper_canyons.json")),
      map,
      &error))
      << error.toStdString();
  match(map);

  auto const cells = crown();
  ASSERT_GE(cells.size(), 40U);
  auto const* heights = m_session->terrain().get_height_map();
  auto const pairs = crossings(cells, 4U);
  ASSERT_FALSE(pairs.empty());

  for (auto const& [from, to] : pairs) {
    float const crown_height = heights->get_height_at(from.x(), from.z());

    std::vector<QVector3D> staging;
    for (auto const& cell : cells) {
      if ((cell - from).length() > 10.0F) {
        continue;
      }
      bool clear = true;
      for (auto const& taken : staging) {
        if ((taken - cell).length() < 2.5F) {
          clear = false;
          break;
        }
      }
      if (clear) {
        staging.push_back(cell);
      }
      if (staging.size() == 6U) {
        break;
      }
    }
    if (staging.size() < 6U) {
      continue;
    }

    std::vector<EntityID> troops;
    troops.reserve(staging.size());
    for (auto const& spot : staging) {
      troops.push_back(spawn(spot));
    }
    trace(troops, 2.0);

    std::vector<QVector3D> const destinations(troops.size(), to);
    CommandService::move_units(m_session->world(), troops, destinations);
    auto const crossing = trace(troops, 45.0);

    EXPECT_GT(crossing.lowest, crown_height - 4.0F)
        << "six troops crossing one hilltop from (" << from.x() << ", " << from.z()
        << ") to (" << to.x() << ", " << to.z() << ") dropped to " << crossing.lowest
        << " m off a " << crown_height << " m crown";
    EXPECT_EQ(crossing.off_walkable_samples, 0);

    for (auto const troop : troops) {
      m_session->world().destroy_entity(troop);
    }
  }
}

struct Outcome {
  Engine::Core::MovementOrderState state{Engine::Core::MovementOrderState::Idle};
  bool arrived_short{false};
  float distance_to_goal{0.0F};
};

auto outcome_of(Game::Session::SessionContext& session,
                EntityID id,
                const QVector3D& goal) -> Outcome {
  Outcome result;
  auto& world = session.world();
  auto const* facts = world.try_get<Engine::Core::MovementFactsComponent>(id);
  if (facts != nullptr) {
    result.state = facts->progress.state;
    result.arrived_short = facts->progress.arrived_short;
  }
  if (auto const* transform = world.try_get<TransformComponent>(id)) {
    result.distance_to_goal =
        std::hypot(transform->position.x - goal.x(), transform->position.z - goal.z());
  }
  return result;
}

auto soldier_off_walkable_samples(Game::Session::SessionContext& session,
                                  const std::vector<EntityID>& troops) -> int {
  int off = 0;
  for (auto const troop : troops) {
    auto* entity = session.world().get_entity(troop);
    if (entity == nullptr) {
      continue;
    }
    for (auto const& anchor :
         Game::Systems::FormationCombat::soldier_spatial_anchors(*entity)) {
      auto const cell = NavGrid::world_to_grid(anchor.world_x, anchor.world_z);
      if (!session.terrain().is_walkable(cell.x, cell.y)) {
        ++off;
        static int printed = 0;
        if (printed < 40 && std::getenv("SOI_HILL_DIAG") != nullptr) {
          ++printed;
          auto const* transform = entity->get_component<TransformComponent>();
          auto const* facts =
              entity->get_component<Engine::Core::MovementFactsComponent>();
          auto const* layout =
              entity->get_component<Engine::Core::UnitTraversalLayoutStateComponent>();
          std::fprintf(
              stderr,
              "OFF unit=%llu root=(%.1f,%.1f) yaw=%.0f state=%s soldier=(%.1f,%.1f) "
              "local=(%.2f,%.2f) scale=%.2f target_scale=%.2f\n",
              static_cast<unsigned long long>(troop),
              transform->position.x,
              transform->position.z,
              transform->rotation.y,
              facts ? Engine::Core::movement_state_name(facts->progress.state) : "?",
              anchor.world_x,
              anchor.world_z,
              anchor.local_x,
              anchor.local_z,
              layout ? layout->lateral_scale : -1.0F,
              layout ? layout->target_lateral_scale : -1.0F);
        }
      }
    }
  }
  return off;
}

void report_soldier_slope_samples(const char* shape, int samples) {
  ::testing::Test::RecordProperty(std::string("soldier_slope_samples_") + shape,
                                  samples);
  if (samples > 0) {
    std::fprintf(stderr, "note: %s: %d soldier samples on the slope\n", shape, samples);
  }
}

TEST_F(HillTraversalTest, AGroupOrderedRoundAHillArrivesOnTheFarSide) {
  for (auto const& shape : k_shapes) {
    one_hill(shape.shape, shape.thickness, 2);
    std::vector<EntityID> troops;
    for (int i = 0; i < 8; ++i) {
      float const z = -10.5F + 3.0F * static_cast<float>(i);
      troops.push_back(spawn(QVector3D(-(k_hill_radius + 16.0F), 0.0F, z)));
      ASSERT_NE(troops.back(), 0U);
    }
    trace(troops, 2.0);

    QVector3D const goal(k_hill_radius + 16.0F, 0.0F, 0.0F);
    auto const plan =
        CommandService::plan_ground_move(m_session->world(), troops, goal);
    ASSERT_TRUE(plan.fully_placeable_for(troops)) << shape.name;
    CommandService::issue_ground_move(m_session->world(), troops, plan);

    int soldier_off = 0;
    Trace march;
    for (int window = 0; window < 30; ++window) {
      auto const slice = trace(troops, 5.0);
      march.off_walkable_samples += slice.off_walkable_samples;
      if (march.off_walkable_samples > 0 && march.first_off_walkable.isNull()) {
        march.first_off_walkable = slice.first_off_walkable;
      }
      soldier_off += soldier_off_walkable_samples(*m_session, troops);
    }

    for (std::size_t i = 0; i < troops.size(); ++i) {
      QVector3D slot = goal;
      for (auto const& member : plan.member_slots) {
        if (member.member == troops[i]) {
          slot = member.position;
        }
      }
      auto const result = outcome_of(*m_session, troops[i], slot);
      EXPECT_LT(result.distance_to_goal, 3.0F)
          << shape.name << ": troop " << i << " ended " << result.distance_to_goal
          << " m from its slot in state "
          << Engine::Core::movement_state_name(result.state)
          << (result.arrived_short ? " (short)" : "");
      EXPECT_EQ(result.state, Engine::Core::MovementOrderState::Arrived)
          << shape.name << ": troop " << i;
      EXPECT_FALSE(result.arrived_short) << shape.name << ": troop " << i;
    }
    EXPECT_EQ(march.off_walkable_samples, 0)
        << shape.name << ": a root stood on the slope near ("
        << march.first_off_walkable.x() << ", " << march.first_off_walkable.z() << ")";
    report_soldier_slope_samples(shape.name, soldier_off);
  }
}

TEST_F(HillTraversalTest, ARawGroupOrderRoundAHillArrivesOnTheFarSide) {
  for (auto const& shape : k_shapes) {
    one_hill(shape.shape, shape.thickness, 2);
    std::vector<EntityID> troops;
    for (int i = 0; i < 8; ++i) {
      float const z = -10.5F + 3.0F * static_cast<float>(i);
      troops.push_back(spawn(QVector3D(-(k_hill_radius + 16.0F), 0.0F, z)));
      ASSERT_NE(troops.back(), 0U);
    }
    trace(troops, 2.0);

    QVector3D const goal(k_hill_radius + 16.0F, 0.0F, 0.0F);
    std::vector<QVector3D> const goals(troops.size(), goal);
    CommandService::move_units(m_session->world(), troops, goals);

    int soldier_off = 0;
    Trace march;
    for (int window = 0; window < 30; ++window) {
      auto const slice = trace(troops, 5.0);
      march.off_walkable_samples += slice.off_walkable_samples;
      if (march.off_walkable_samples > 0 && march.first_off_walkable.isNull()) {
        march.first_off_walkable = slice.first_off_walkable;
      }
      soldier_off += soldier_off_walkable_samples(*m_session, troops);
    }

    for (std::size_t i = 0; i < troops.size(); ++i) {
      auto const result = outcome_of(*m_session, troops[i], goal);
      EXPECT_LT(result.distance_to_goal, 10.0F)
          << shape.name << ": troop " << i << " ended " << result.distance_to_goal
          << " m from the goal in state "
          << Engine::Core::movement_state_name(result.state)
          << (result.arrived_short ? " (short)" : "");
      EXPECT_EQ(result.state, Engine::Core::MovementOrderState::Arrived)
          << shape.name << ": troop " << i;
    }
    EXPECT_EQ(march.off_walkable_samples, 0)
        << shape.name << ": a root stood on the slope near ("
        << march.first_off_walkable.x() << ", " << march.first_off_walkable.z() << ")";
    report_soldier_slope_samples(shape.name, soldier_off);
  }
}

TEST_F(HillTraversalTest, AnOrderOntoTheSlopeNeverClimbsIt) {
  for (auto const& shape : k_shapes) {
    one_hill(shape.shape, shape.thickness, 1);
    auto const* heights = m_session->terrain().get_height_map();

    QVector3D slope;
    bool found = false;
    for (float x = k_hill_radius; x > 0.0F && !found; x -= 1.0F) {
      for (float z : {6.0F, -6.0F, 10.0F, -10.0F, 14.0F, -14.0F, 18.0F, -18.0F}) {
        auto const cell = NavGrid::world_to_grid(x, z);
        if (heights->get_height_at(x, z) > 1.5F &&
            !m_session->terrain().is_walkable(cell.x, cell.y)) {
          slope = QVector3D(x, 0.0F, z);
          found = true;
          break;
        }
      }
    }
    ASSERT_TRUE(found) << shape.name;

    std::vector<EntityID> troops;
    for (int i = 0; i < 4; ++i) {
      troops.push_back(spawn(QVector3D(k_hill_radius + 18.0F, 0.0F, -4.5F + 3.0F * i)));
    }
    trace(troops, 1.0);
    std::vector<QVector3D> const goals(troops.size(), slope);
    CommandService::move_units(m_session->world(), troops, goals);

    int soldier_off = 0;
    Trace climb;
    for (int window = 0; window < 24; ++window) {
      auto const slice = trace(troops, 5.0);
      climb.off_walkable_samples += slice.off_walkable_samples;
      if (climb.off_walkable_samples > 0 && climb.first_off_walkable.isNull()) {
        climb.first_off_walkable = slice.first_off_walkable;
      }
      soldier_off += soldier_off_walkable_samples(*m_session, troops);
    }
    EXPECT_EQ(climb.off_walkable_samples, 0)
        << shape.name << ": ordered onto slope (" << slope.x() << ", " << slope.z()
        << ") a root stood on it near (" << climb.first_off_walkable.x() << ", "
        << climb.first_off_walkable.z() << ")";
    report_soldier_slope_samples(shape.name, soldier_off);
    for (std::size_t i = 0; i < troops.size(); ++i) {
      auto const result = outcome_of(*m_session, troops[i], slope);
      EXPECT_TRUE(Engine::Core::is_terminal_movement_state(result.state))
          << shape.name << ": troop " << i << " still "
          << Engine::Core::movement_state_name(result.state) << " "
          << result.distance_to_goal << " m from the slope point (" << slope.x() << ", "
          << slope.z() << ") at (" << position_of(troops[i]).x() << ", "
          << position_of(troops[i]).z() << ")";
    }
  }
}

TEST_F(HillTraversalTest, AnOrderToTheFarSideOfEveryShippedHillArrives) {
  for (auto const* map_name : {"map_copper_canyons.json", "map_amber_delta.json"}) {
    Game::Map::MapDefinition map;
    QString error;
    ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
        QDir(QStringLiteral("assets/maps")).filePath(QString::fromUtf8(map_name)),
        map,
        &error))
        << error.toStdString();
    match(map);
    auto* pathfinder = NavGrid::get_pathfinder();
    ASSERT_NE(pathfinder, nullptr);

    int crossings_tried = 0;
    for (std::size_t feature_index = 0; feature_index < map.terrain.size();
         ++feature_index) {
      auto const& feature = map.terrain[feature_index];
      if (feature.type != Game::Map::TerrainType::Hill) {
        continue;
      }
      float const span =
          std::max({feature.radius, feature.width * 0.5F, feature.depth * 0.5F});
      if (span < 8.0F) {
        continue;
      }
      QVector3D const centre(feature.center_x, 0.0F, feature.center_z);
      for (float bearing : {0.0F, 1.2F, 2.4F}) {
        QVector3D const out(std::cos(bearing), 0.0F, std::sin(bearing));
        QVector3D const start =
            NavGrid::snap_to_walkable_ground(centre - out * (span + 10.0F), 6);
        QVector3D const goal =
            NavGrid::snap_to_walkable_ground(centre + out * (span + 10.0F), 6);
        auto const start_cell = NavGrid::world_to_grid(start.x(), start.z());
        auto const goal_cell = NavGrid::world_to_grid(goal.x(), goal.z());
        if (!pathfinder->is_walkable(start_cell.x, start_cell.y) ||
            !pathfinder->is_walkable(goal_cell.x, goal_cell.y) ||
            !pathfinder->can_reach(start_cell,
                                   goal_cell,
                                   Game::Systems::Pathfinding::Passability::Heavy) ||
            (goal - start).length() < 2.0F * span) {
          continue;
        }
        auto const* heights = m_session->terrain().get_height_map();
        if (heights->get_height_at(start.x(), start.z()) > 1.0F ||
            heights->get_height_at(goal.x(), goal.z()) > 1.0F) {
          continue;
        }
        ++crossings_tried;

        std::vector<EntityID> troops;
        for (int i = 0; i < 4; ++i) {
          QVector3D const spot = NavGrid::snap_to_walkable_ground(
              start + QVector3D(-out.z(), 0.0F, out.x()) * (-4.5F + 3.0F * i), 4);
          troops.push_back(spawn(spot));
          ASSERT_NE(troops.back(), 0U);
        }
        trace(troops, 1.0);
        auto const plan =
            CommandService::plan_ground_move(m_session->world(), troops, goal);
        CommandService::issue_ground_move(m_session->world(), troops, plan);
        auto const march = trace(troops, 240.0);

        for (std::size_t i = 0; i < troops.size(); ++i) {
          QVector3D slot = goal;
          for (auto const& member : plan.member_slots) {
            if (member.member == troops[i]) {
              slot = member.position;
            }
          }
          auto const result = outcome_of(*m_session, troops[i], slot);
          EXPECT_LT(result.distance_to_goal, 3.0F)
              << map_name << ": hill " << feature_index << " bearing " << bearing
              << ": troop " << i << " ended " << result.distance_to_goal
              << " m from its slot (" << slot.x() << ", " << slot.z() << ") by ("
              << goal.x() << ", " << goal.z() << ") in state "
              << Engine::Core::movement_state_name(result.state)
              << (result.arrived_short ? " (short)" : "");
        }
        EXPECT_EQ(march.off_walkable_samples, 0)
            << map_name << ": hill " << feature_index << " bearing " << bearing
            << " put a root on blocked ground near (" << march.first_off_walkable.x()
            << ", " << march.first_off_walkable.z() << ")";
        for (auto const troop : troops) {
          m_session->world().destroy_entity(troop);
        }
      }
    }
    EXPECT_GT(crossings_tried, 3) << map_name << ": no hill offered a crossing";
  }
}

TEST_F(HillTraversalTest, ALargeGroupQueuesThroughARampAndStillArrives) {
  Game::Map::MapDefinition map;
  map.grid.width = k_synthetic_grid;
  map.grid.height = k_synthetic_grid;
  map.grid.tile_size = 1.0F;
  map.coordSystem = Game::Map::CoordSystem::World;
  map.biome.procedural_boulders_enabled = false;
  map.biome.procedural_iron_ore_enabled = false;
  map.biome.procedural_trees_enabled = false;

  Game::Map::TerrainFeature ridge;
  ridge.type = Game::Map::TerrainType::Hill;
  ridge.center_x = 0.0F;
  ridge.center_z = 0.0F;
  ridge.radius = 20.0F;
  ridge.height = 6.0F;
  ridge.shape = Game::Map::HillShape::Corridor;
  ridge.width = 140.0F;
  ridge.depth = 24.0F;
  ridge.thickness = 24.0F;
  ridge.entrances.emplace_back(0.0F, 0.0F, -14.0F);
  ridge.entrances.emplace_back(0.0F, 0.0F, 14.0F);
  map.terrain.push_back(ridge);
  match(map);

  auto* pathfinder = NavGrid::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  ASSERT_TRUE(pathfinder->is_walkable(NavGrid::world_to_grid(0.0F, 0.0F).x,
                                      NavGrid::world_to_grid(0.0F, 0.0F).y))
      << "the ridge grew no walkable crown";
  ASSERT_FALSE(pathfinder->is_walkable(NavGrid::world_to_grid(30.0F, -9.0F).x,
                                       NavGrid::world_to_grid(30.0F, -9.0F).y))
      << "the ridge flank is not blocked";

  std::vector<EntityID> troops;
  for (int i = 0; i < 16; ++i) {
    float const x = -10.5F + 3.0F * static_cast<float>(i % 8);
    float const z = -34.0F - 4.0F * static_cast<float>(i / 8);
    troops.push_back(spawn(QVector3D(x, 0.0F, z)));
    ASSERT_NE(troops.back(), 0U);
  }
  trace(troops, 2.0);

  QVector3D const goal(0.0F, 0.0F, 34.0F);
  auto const plan = CommandService::plan_ground_move(m_session->world(), troops, goal);
  ASSERT_TRUE(plan.fully_placeable_for(troops));
  CommandService::issue_ground_move(m_session->world(), troops, plan);

  Trace march;
  for (int window = 0; window < 48; ++window) {
    auto const slice = trace(troops, 5.0);
    march.off_walkable_samples += slice.off_walkable_samples;
    if (march.off_walkable_samples > 0 && march.first_off_walkable.isNull()) {
      march.first_off_walkable = slice.first_off_walkable;
    }
  }

  int gave_up = 0;
  for (std::size_t i = 0; i < troops.size(); ++i) {
    QVector3D slot = goal;
    for (auto const& member : plan.member_slots) {
      if (member.member == troops[i]) {
        slot = member.position;
      }
    }
    auto const result = outcome_of(*m_session, troops[i], slot);
    if (result.state == Engine::Core::MovementOrderState::Unreachable) {
      ++gave_up;
    }
    EXPECT_LT(result.distance_to_goal, 4.0F)
        << "troop " << i << " ended " << result.distance_to_goal
        << " m from its slot in state "
        << Engine::Core::movement_state_name(result.state)
        << (result.arrived_short ? " (short)" : "") << " at ("
        << position_of(troops[i]).x() << ", " << position_of(troops[i]).z() << ")";
    EXPECT_EQ(result.state, Engine::Core::MovementOrderState::Arrived) << "troop " << i;
  }
  EXPECT_EQ(gave_up, 0) << gave_up << " troops gave their order up in the queue";
  EXPECT_EQ(march.off_walkable_samples, 0)
      << "a root stood on the ridge flank near (" << march.first_off_walkable.x()
      << ", " << march.first_off_walkable.z() << ")";
}

} // namespace
