#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/default_content.h"
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

constexpr int k_attacker_owner = 1;
constexpr int k_defender_owner = 2;
constexpr float k_soldier_radius = 0.25F;
constexpr float k_visible_overlap = 0.05F;

struct DrawnRect {
  float min_x;
  float max_x;
  float min_z;
  float max_z;
};

struct Clipping {
  float soldier_seconds{0.0F};
  float worst_depth{0.0F};
  float closest_gap{1.0e9F};
  int samples{0};
};

auto overlap_depth(const DrawnRect& rect, float x, float z) -> float {
  float const nearest_x = std::clamp(x, rect.min_x, rect.max_x);
  float const nearest_z = std::clamp(z, rect.min_z, rect.max_z);
  bool const inside =
      x > rect.min_x && x < rect.max_x && z > rect.min_z && z < rect.max_z;
  if (inside) {
    float const to_edge =
        std::min({x - rect.min_x, rect.max_x - x, z - rect.min_z, rect.max_z - z});
    return k_soldier_radius + to_edge;
  }
  return k_soldier_radius - std::hypot(x - nearest_x, z - nearest_z);
}

struct ClearanceCase {
  SpawnType structure;
  SpawnType attacker;
  const char* name;
};

class StructureMeleeClearance : public ::testing::TestWithParam<ClearanceCase> {
protected:
  void SetUp() override {
    Game::Systems::NationRegistry::instance().clear();
    Game::Systems::initialize_default_content(
        Game::Systems::NationRegistry::instance());
    m_factory = std::make_shared<Game::Units::UnitFactoryRegistry>();
    Game::Units::register_built_in_units(*m_factory);

    Game::Map::MapDefinition map;
    map.grid.width = 64;
    map.grid.height = 64;
    map.grid.tile_size = 1.0F;
    map.biome.procedural_boulders_enabled = false;
    map.biome.procedural_iron_ore_enabled = false;
    map.biome.procedural_trees_enabled = false;
    m_session = std::make_unique<SessionContext>();
    m_session->world().set_presentation_enabled(true);
    m_scope = std::make_unique<Game::Session::ScopedSession>(*m_session);
    m_session->owners().register_owner_with_id(
        k_attacker_owner, Game::Systems::OwnerType::Player, "rome");
    m_session->owners().set_owner_team(k_attacker_owner, 1);
    m_session->owners().register_owner_with_id(
        k_defender_owner, Game::Systems::OwnerType::AI, "carthage");
    m_session->owners().set_owner_team(k_defender_owner, 2);
    Game::Systems::initialize_default_content(m_session->nations());
    Game::Systems::register_runtime_systems(m_session->world());
    m_session->terrain().initialize(map);
    NavGrid::initialize(map.grid.width, map.grid.height);
  }

  void TearDown() override {
    m_scope.reset();
    m_session.reset();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
  }

  auto spawn(SpawnType type, int owner, const QVector3D& position) -> EntityID {
    Game::Units::SpawnParams params;
    params.position = position;
    params.player_id = owner;
    params.spawn_type = type;
    params.nation_id = owner == k_attacker_owner
                           ? Game::Systems::NationID::RomanRepublic
                           : Game::Systems::NationID::Carthage;
    params.is_initial_spawn = true;
    auto unit = m_factory->create(type, m_session->world(), params);
    return unit ? unit->id() : 0;
  }

  static auto drawn_rect(EntityID structure) -> DrawnRect {
    auto const* footprint =
        Game::Systems::BuildingCollisionRegistry::instance().find_building(structure);
    if (footprint == nullptr) {
      ADD_FAILURE() << "structure was not registered";
      return {};
    }
    return {footprint->body_center_x - footprint->body_width * 0.5F,
            footprint->body_center_x + footprint->body_width * 0.5F,
            footprint->body_center_z - footprint->body_depth * 0.5F,
            footprint->body_center_z + footprint->body_depth * 0.5F};
  }

  auto measure(const std::vector<EntityID>& units,
               const DrawnRect& rect,
               double seconds) -> Clipping {
    Clipping clipping;
    double const step = m_session->clock().tick_seconds();
    for (double elapsed = 0.0; elapsed < seconds; elapsed += step) {
      m_session->clock().advance(step);
      while (m_session->clock().consume_tick()) {
        m_session->world().update(static_cast<float>(step));
        for (auto const id : units) {
          auto* entity = m_session->world().get_entity(id);
          auto const* presentation =
              entity != nullptr
                  ? entity
                        ->get_component<Engine::Core::FormationPresentationComponent>()
                  : nullptr;
          if (presentation == nullptr) {
            continue;
          }
          for (const auto& soldier : presentation->soldiers) {
            if (!soldier.alive || !soldier.world_motion_valid) {
              continue;
            }
            ++clipping.samples;
            float const depth = overlap_depth(rect, soldier.world_x, soldier.world_z);
            clipping.closest_gap =
                std::min(clipping.closest_gap, k_soldier_radius - depth);
            if (depth > k_visible_overlap) {
              clipping.soldier_seconds += static_cast<float>(step);
              clipping.worst_depth = std::max(clipping.worst_depth, depth);
            }
          }
        }
      }
    }
    return clipping;
  }

  static void
  order_attack(Engine::Core::World& world, EntityID attacker, EntityID target) {
    auto* entity = world.get_entity(attacker);
    ASSERT_NE(entity, nullptr);
    auto* attack = entity->add_component<Engine::Core::AttackTargetComponent>();
    attack->target_id = target;
    attack->should_chase = true;
  }

  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_factory;
  std::unique_ptr<SessionContext> m_session;
  std::unique_ptr<Game::Session::ScopedSession> m_scope;
};

TEST_P(StructureMeleeClearance, AttackersStandOutsideTheDrawnBuilding) {
  EntityID const structure =
      spawn(GetParam().structure, k_defender_owner, QVector3D(0.0F, 0.0F, 0.0F));
  ASSERT_NE(structure, 0U);
  NavGrid::get_pathfinder()->update_navigation_grid();
  EntityID const attacker =
      spawn(GetParam().attacker, k_attacker_owner, QVector3D(-16.0F, 0.0F, -3.0F));
  ASSERT_NE(attacker, 0U);

  order_attack(m_session->world(), attacker, structure);

  DrawnRect const rect = drawn_rect(structure);
  Clipping const clipping = measure({attacker}, rect, 18.0);
  EXPECT_GT(clipping.samples, 0);
  EXPECT_LT(clipping.soldier_seconds, 0.5F)
      << GetParam().name << ": men stood inside the drawn building (worst "
      << clipping.worst_depth << " m, closest " << clipping.closest_gap << " m)";
  EXPECT_LT(clipping.worst_depth, 0.15F) << GetParam().name;
}

TEST_P(StructureMeleeClearance, AMeleeBesideTheBuildingStaysOutOfIt) {
  EntityID const structure =
      spawn(GetParam().structure, k_defender_owner, QVector3D(0.0F, 0.0F, 0.0F));
  ASSERT_NE(structure, 0U);
  NavGrid::get_pathfinder()->update_navigation_grid();
  DrawnRect const rect = drawn_rect(structure);
  EntityID const defender = spawn(
      SpawnType::Spearman, k_defender_owner, QVector3D(0.0F, 0.0F, rect.min_z - 4.0F));
  EntityID const attacker = spawn(GetParam().attacker,
                                  k_attacker_owner,
                                  QVector3D(-4.0F, 0.0F, rect.min_z - 22.0F));
  ASSERT_NE(defender, 0U);
  ASSERT_NE(attacker, 0U);
  order_attack(m_session->world(), attacker, defender);

  Clipping const clipping = measure({attacker, defender}, rect, 18.0);
  EXPECT_GT(clipping.samples, 0);
  EXPECT_LT(clipping.soldier_seconds, 0.5F)
      << GetParam().name << ": the brawl shoved men into the drawn building (worst "
      << clipping.worst_depth << " m, closest " << clipping.closest_gap << " m)";
  EXPECT_LT(clipping.worst_depth, 0.15F) << GetParam().name;
}

INSTANTIATE_TEST_SUITE_P(
    Structures,
    StructureMeleeClearance,
    ::testing::Values(
        ClearanceCase{SpawnType::Barracks, SpawnType::Swordsman, "BarracksSwordsmen"},
        ClearanceCase{SpawnType::Barracks, SpawnType::Spearman, "BarracksSpearmen"},
        ClearanceCase{
            SpawnType::Barracks, SpawnType::MountedSwordsman, "BarracksRiders"},
        ClearanceCase{SpawnType::Home, SpawnType::Swordsman, "HomeSwordsmen"},
        ClearanceCase{SpawnType::Temple, SpawnType::Swordsman, "TempleSwordsmen"},
        ClearanceCase{SpawnType::Marketplace, SpawnType::Spearman, "MarketSpearmen"},
        ClearanceCase{SpawnType::DefenseTower, SpawnType::Swordsman, "TowerSwordsmen"},
        ClearanceCase{
            SpawnType::DefenseTower, SpawnType::MountedSwordsman, "TowerRiders"}),
    [](const testing::TestParamInfo<ClearanceCase>& info) {
      return std::string(info.param.name);
    });

} // namespace
