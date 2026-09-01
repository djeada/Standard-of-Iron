

#include <QDir>
#include <QString>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/map/map_loader.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/command_service.h"
#include "game/systems/default_content.h"
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

} // namespace
