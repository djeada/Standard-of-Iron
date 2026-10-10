#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <gtest/gtest.h>
#include <map>
#include <memory>
#include <vector>

#include "game/command/command.h"
#include "game/command/command_queue.h"
#include "game/core/component.h"
#include "game/core/event_manager.h"
#include "game/core/world.h"
#include "game/formation/army_formation_registry.h"
#include "game/formation/formation_frame.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/default_content.h"
#include "game/systems/formation_combat_geometry.h"
#include "game/systems/nation_registry.h"
#include "game/systems/navigation/nav_grid.h"
#include "game/systems/navigation/pathfinding.h"
#include "game/systems/owner_registry.h"
#include "game/systems/runtime_system_registry.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"

namespace {

using Engine::Core::EntityID;
using Game::Formation::ArmyFormation;
using Game::Formation::ArmyFormationIntent;
using Game::Formation::ArmyFormationRegistry;
using Game::Formation::BattleBand;
using Game::Session::SessionContext;
using Game::Systems::NavGrid;
using Game::Systems::NationID;
using Game::Units::SpawnType;

constexpr int k_rome = 1;
constexpr int k_carthage = 2;
constexpr int k_map = 160;

class BattleOrderBehaviourTest : public ::testing::Test {
protected:
  void SetUp() override {
    reset_shared_state();
    Game::Systems::NationRegistry::instance().clear();
    Game::Systems::initialize_default_content(
        Game::Systems::NationRegistry::instance());
    m_factory = std::make_shared<Game::Units::UnitFactoryRegistry>();
    Game::Units::register_built_in_units(*m_factory);
  }

  void TearDown() override {
    m_scope.reset();
    m_session.reset();
    reset_shared_state();
    Game::Systems::NationRegistry::instance().clear();
  }

  static void reset_shared_state() {
    Engine::Core::EventManager::instance().clear_all_subscriptions();
    Game::Map::TerrainService::instance().clear();
    Game::Formation::ArmyFormationRegistry::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Systems::FormationCombat::invalidate_layout_cache();
  }

  void open() {
    Game::Map::MapDefinition map;
    map.grid.width = k_map;
    map.grid.height = k_map;
    map.grid.tile_size = 1.0F;
    map.coordSystem = Game::Map::CoordSystem::World;
    map.biome.procedural_boulders_enabled = false;
    map.biome.procedural_iron_ore_enabled = false;
    map.biome.procedural_trees_enabled = false;

    m_scope.reset();
    m_session.reset();
    reset_shared_state();
    m_session = std::make_unique<SessionContext>();
    m_session->world().set_presentation_enabled(true);
    m_scope = std::make_unique<Game::Session::ScopedSession>(*m_session);
    auto& owners = m_session->owners();
    owners.register_owner_with_id(k_rome, Game::Systems::OwnerType::Player, "rome");
    owners.set_owner_team(k_rome, 1);
    owners.register_owner_with_id(k_carthage, Game::Systems::OwnerType::Player, "carthage");
    owners.set_owner_team(k_carthage, 2);
    Game::Systems::initialize_default_content(m_session->nations());
    m_session->nations().set_player_nation(k_rome, NationID::RomanRepublic);
    m_session->nations().set_player_nation(k_carthage, NationID::Carthage);
    Game::Systems::register_runtime_systems(m_session->world());
    m_session->terrain().initialize(map);
    NavGrid::initialize(map.grid.width, map.grid.height);
    auto* pathfinder = NavGrid::get_pathfinder();
    ASSERT_NE(pathfinder, nullptr);
    pathfinder->update_navigation_grid();
  }

  auto spawn(SpawnType type, int owner, const QVector3D& position, float yaw) -> EntityID {
    Game::Units::SpawnParams params;
    params.position = position;
    params.player_id = owner;
    params.spawn_type = type;
    params.rotation_y = yaw;
    params.ai_controlled = false;
    params.nation_id = owner == k_rome ? NationID::RomanRepublic : NationID::Carthage;
    auto unit = m_factory->create(type, m_session->world(), params);
    return unit ? unit->id() : 0;
  }

  auto row(SpawnType type,
           int owner,
           int count,
           const QVector3D& centre,
           float pitch,
           float yaw) -> std::vector<EntityID> {
    std::vector<EntityID> units;
    for (int i = 0; i < count; ++i) {
      float const x = (static_cast<float>(i) - static_cast<float>(count - 1) * 0.5F) * pitch;
      units.push_back(spawn(type, owner, centre + QVector3D(x, 0.0F, 0.0F), yaw));
    }
    return units;
  }

  void deploy(const std::vector<EntityID>& units,
              int owner,
              const QVector3D& anchor,
              float facing,
              ArmyFormationIntent intent) {
    Game::Command::DeployFormation order;
    order.units = units;
    order.anchor = anchor;
    order.facing = facing;
    order.intent = intent;
    order.spacing = 1.5F;
    Game::Command::submit(
        m_session->world(), Game::Command::Source::LocalPlayer, owner, std::move(order));
  }

  void attack(const std::vector<EntityID>& units, int owner, EntityID target) {
    Game::Command::AttackTarget order;
    order.units = units;
    order.target = target;
    order.should_chase = true;
    Game::Command::submit(
        m_session->world(), Game::Command::Source::LocalPlayer, owner, std::move(order));
  }

  template <typename Fn> void run(double seconds, Fn&& each_tick) {
    double const step = m_session->clock().tick_seconds();
    for (double elapsed = 0.0; elapsed < seconds - 1e-9; elapsed += step) {
      m_session->clock().advance(step);
      while (m_session->clock().consume_tick()) {
        m_session->world().update(static_cast<float>(step));
        each_tick();
      }
    }
  }

  void run(double seconds) {
    run(seconds, [] {});
  }

  auto registry() -> ArmyFormationRegistry& {
    return ArmyFormationRegistry::for_world(m_session->world());
  }

  auto formation_of(EntityID member) -> const ArmyFormation* {
    return registry().find(registry().group_of(member));
  }

  auto position_of(EntityID id) -> QVector3D {
    const auto* transform =
        m_session->world().try_get<Engine::Core::TransformComponent>(id);
    return transform == nullptr
               ? QVector3D()
               : QVector3D(transform->position.x, 0.0F, transform->position.z);
  }

  auto alive(EntityID id) -> bool {
    const auto* unit = m_session->world().try_get<Engine::Core::UnitComponent>(id);
    return unit != nullptr && unit->health > 0;
  }

  auto health_of(const std::vector<EntityID>& units) -> int {
    int total = 0;
    for (auto const id : units) {
      if (const auto* unit = m_session->world().try_get<Engine::Core::UnitComponent>(id)) {
        total += std::max(0, unit->health);
      }
    }
    return total;
  }

  void form_up(const std::vector<EntityID>& units, double limit) {
    double waited = 0.0;
    while (waited < limit) {
      run(1.0);
      waited += 1.0;
      const auto* formation = formation_of(units.front());
      if (formation != nullptr && formation->is_formed()) {
        return;
      }
    }
  }

  struct CrescentRun {
    float centre_yield{0.0F};
    float wing_wheel{0.0F};
    bool saw_yielding{false};
    float centre_retreat{0.0F};
    float wing_drift_while_holding{0.0F};
    std::vector<QVector3D> final_positions;
  };

  auto live_formation(const std::vector<EntityID>& members) -> const ArmyFormation* {
    for (auto const id : members) {
      if (alive(id)) {
        if (const auto* formation = formation_of(id)) {
          return formation;
        }
      }
    }
    return nullptr;
  }

  auto run_crescent(double seconds) -> CrescentRun {
    open();
    auto const carthage =
        row(SpawnType::Swordsman, k_carthage, 8, QVector3D(0.0F, 0.0F, -20.0F), 8.0F, 0.0F);
    deploy(carthage,
           k_carthage,
           QVector3D(0.0F, 0.0F, -20.0F),
           0.0F,
           ArmyFormationIntent::ConvexCrescent);
    form_up(carthage, 60.0);
    const auto* formation = formation_of(carthage.front());
    EXPECT_NE(formation, nullptr);
    if (formation == nullptr) {
      return {};
    }
    EXPECT_EQ(formation->intent, ArmyFormationIntent::ConvexCrescent);
    QVector3D const forward = Game::Formation::planning::rotate_offset(
        QVector3D(0.0F, 0.0F, 1.0F), formation->facing);

    std::vector<EntityID> centre;
    std::vector<EntityID> wings;
    for (const auto& slot : formation->slot_list) {
      if (slot.band == BattleBand::CrescentCentre) {
        centre.push_back(slot.occupant);
      } else if (slot.band == BattleBand::CrescentWing) {
        wings.push_back(slot.occupant);
      }
    }
    EXPECT_FALSE(centre.empty());
    EXPECT_FALSE(wings.empty());
    if (centre.empty()) {
      return {};
    }
    std::map<EntityID, QVector3D> start;
    for (auto const id : carthage) {
      start[id] = position_of(id);
    }

    auto const romans =
        row(SpawnType::Swordsman, k_rome, 6, QVector3D(0.0F, 0.0F, 18.0F), 6.0F, 180.0F);
    for (std::size_t i = 0; i < romans.size(); ++i) {
      attack({romans[i]}, k_rome, centre[i % centre.size()]);
    }

    CrescentRun result;
    std::map<EntityID, float> forward_at_contact;
    run(seconds, [&] {
      const auto* live = live_formation(carthage);
      if (live == nullptr) {
        return;
      }
      auto const& state = live->manoeuvre;
      if (state.yielding && !result.saw_yielding) {
        for (auto const id : centre) {
          forward_at_contact[id] = QVector3D::dotProduct(position_of(id), forward);
        }
      }
      result.saw_yielding = result.saw_yielding || state.yielding;
      result.centre_yield = std::max(result.centre_yield, state.centre_yield);
      result.wing_wheel = std::max(result.wing_wheel, state.wing_wheel);
      // While the centre has given less than half its ground the fight is all
      // at the centre; the wings must stand.
      if (state.centre_yield < 0.5F) {
        for (auto const id : wings) {
          if (alive(id)) {
            result.wing_drift_while_holding =
                std::max(result.wing_drift_while_holding,
                         (position_of(id) - start[id]).length());
          }
        }
      }
    });
    float retreat = 0.0F;
    int counted = 0;
    for (auto const& [id, at_contact] : forward_at_contact) {
      if (!alive(id)) {
        continue;
      }
      retreat += at_contact - QVector3D::dotProduct(position_of(id), forward);
      ++counted;
    }
    result.centre_retreat = counted > 0 ? retreat / static_cast<float>(counted) : 0.0F;
    for (auto const id : carthage) {
      result.final_positions.push_back(position_of(id));
    }
    for (auto const id : romans) {
      result.final_positions.push_back(position_of(id));
    }
    return result;
  }

  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_factory;
  std::unique_ptr<SessionContext> m_session;
  std::unique_ptr<Game::Session::ScopedSession> m_scope;
};

TEST_F(BattleOrderBehaviourTest, CrescentCentreGivesGroundWhileTheWingsHold) {
  auto const result = run_crescent(80.0);
  EXPECT_TRUE(result.saw_yielding);
  EXPECT_GT(result.centre_yield, 0.7F);
  // The pressed centre stepped back toward its own lines...
  EXPECT_GT(result.centre_retreat, 3.0F);
  // ...while the wings stood...
  EXPECT_LT(result.wing_drift_while_holding, 1.5F);
  // ...until, given ground enough, they wheeled in.
  EXPECT_GT(result.wing_wheel, 0.5F);
}

TEST_F(BattleOrderBehaviourTest, CrescentFightIsDeterministicTakeToTake) {
  auto const first = run_crescent(25.0);
  auto const second = run_crescent(25.0);
  ASSERT_EQ(first.final_positions.size(), second.final_positions.size());
  for (std::size_t i = 0; i < first.final_positions.size(); ++i) {
    EXPECT_EQ(std::memcmp(&first.final_positions[i],
                          &second.final_positions[i],
                          sizeof(QVector3D)),
              0)
        << i;
  }
  EXPECT_EQ(first.centre_yield, second.centre_yield);
}

TEST_F(BattleOrderBehaviourTest, ElephantsRunTheOpenLanesOfATriplexAcies) {
  open();
  // The legion faces south (toward -z); the elephant comes from the south.
  auto const legion =
      row(SpawnType::Swordsman, k_rome, 9, QVector3D(0.0F, 0.0F, 30.0F), 8.0F, 180.0F);
  deploy(legion, k_rome, QVector3D(0.0F, 0.0F, 30.0F), 180.0F,
         ArmyFormationIntent::TriplexAcies);
  form_up(legion, 60.0);
  const auto* formation = formation_of(legion.front());
  ASSERT_NE(formation, nullptr);
  ASSERT_EQ(formation->intent, ArmyFormationIntent::TriplexAcies);

  EntityID target = 0;
  for (const auto& slot : formation->slot_list) {
    if (slot.band == BattleBand::Hastati &&
        (target == 0 || std::abs(slot.world_position.x()) <
                            std::abs(position_of(target).x()))) {
      target = slot.occupant;
    }
  }
  ASSERT_NE(target, 0U);

  auto const elephant =
      spawn(SpawnType::Elephant, k_carthage, QVector3D(3.0F, 0.0F, -40.0F), 0.0F);
  ASSERT_NE(elephant, 0U);
  int const legion_health = health_of(legion);
  attack({elephant}, k_carthage, target);

  float rear_z = -1000.0F;
  bool lanes_opened = false;
  bool ran_through = false;
  bool ran_a_lane = false;
  run(45.0, [&] {
    const auto* live = formation_of(legion.front());
    if (live == nullptr) {
      return;
    }
    lanes_opened = lanes_opened || live->manoeuvre.lanes_opened;
    if (const auto* beast =
            m_session->world().try_get<Engine::Core::ElephantComponent>(elephant)) {
      ran_a_lane = ran_a_lane || beast->lane_running;
    }
    for (const auto& slot : live->slot_list) {
      if (slot.band == BattleBand::Triarii) {
        rear_z = std::max(rear_z, slot.world_position.z() + slot.half_depth);
      }
    }
    if (alive(elephant) && rear_z > -1000.0F && position_of(elephant).z() > rear_z + 1.0F) {
      ran_through = true;
    }
  });

  EXPECT_TRUE(lanes_opened);
  EXPECT_TRUE(ran_a_lane);
  EXPECT_TRUE(ran_through) << "elephant ended at z=" << position_of(elephant).z()
                           << " rear line at z=" << rear_z;
  // The lanes absorbed the charge: the maniples lost little.
  EXPECT_GT(health_of(legion), legion_health * 9 / 10);
}

} // namespace
