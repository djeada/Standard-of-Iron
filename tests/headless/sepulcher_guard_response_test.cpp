

#include <QString>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "game/core/component.h"
#include "game/core/event_manager.h"
#include "game/core/world.h"
#include "game/formation/army_formation_registry.h"
#include "game/map/map_definition.h"
#include "game/map/map_transformer.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "game/systems/ai_system.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/combat_system/damage_processor.h"
#include "game/systems/default_content.h"
#include "game/systems/formation_combat_geometry.h"
#include "game/systems/nation_registry.h"
#include "game/systems/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/runtime_system_registry.h"
#include "game/systems/troop_count_registry.h"
#include "game/systems/undead_awakening_system.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"

namespace {

using Engine::Core::AttackTargetComponent;
using Engine::Core::EntityID;
using Engine::Core::TransformComponent;
using Engine::Core::UnitComponent;
using Game::Session::SessionContext;

constexpr int k_map_size = 96;
constexpr int k_player = 1;

constexpr int k_sepulcher = 99;

class SepulcherGuardResponseTest : public ::testing::Test {
protected:
  void SetUp() override {
    reset_shared_world_state();
    Game::Systems::NavGrid::initialize(k_map_size, k_map_size);
    m_factory = std::make_shared<Game::Units::UnitFactoryRegistry>();
    Game::Units::register_built_in_units(*m_factory);
    Game::Map::MapTransformer::setFactoryRegistry(m_factory);

    m_session = std::make_unique<SessionContext>();
    m_session->world().set_presentation_enabled(false);
    m_scope = std::make_unique<Game::Session::ScopedSession>(*m_session);
    Game::Map::MapTransformer::setFactoryRegistry(m_factory);

    auto& owners = m_session->owners();
    owners.register_owner_with_id(k_player, Game::Systems::OwnerType::Player, "player");
    owners.set_owner_team(k_player, 1);

    Game::Systems::initialize_default_content(m_session->nations());
    m_session->nations().set_player_nation(k_player,
                                           Game::Systems::NationID::RomanRepublic);

    Game::Map::MapDefinition map_definition;
    map_definition.grid.width = k_map_size;
    map_definition.grid.height = k_map_size;
    map_definition.grid.tile_size = 1.0F;
    m_session->terrain().initialize(map_definition);

    Game::Systems::register_runtime_systems(m_session->world());

    if (auto* ai = m_session->world().get_system<Game::Systems::AISystem>()) {
      ai->reinitialize();
    }
    owners.register_owner_with_id(
        k_sepulcher, Game::Systems::OwnerType::AI, "Iron Sepulcher ruins_guard");
    owners.set_owner_team(k_sepulcher, k_sepulcher);
    m_session->nations().set_player_nation(k_sepulcher,
                                           Game::Systems::NationID::IronSepulcher);
  }

  void TearDown() override {
    m_scope.reset();
    m_session.reset();
    Game::Map::MapTransformer::setFactoryRegistry(nullptr);
    reset_shared_world_state();
  }

  static void reset_shared_world_state() {
    Engine::Core::EventManager::instance().clear_all_subscriptions();
    Game::Map::TerrainService::instance().clear();
    Game::Formation::ArmyFormationRegistry::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Systems::TroopCountRegistry::instance().clear();
    Game::Systems::PlayerResourceRegistry::instance().clear();
    Game::Systems::FormationCombat::invalidate_layout_cache();
  }

  auto spawn(Game::Units::SpawnType type, int owner_id, float x, float z) -> EntityID {
    Game::Units::SpawnParams params;
    params.position = QVector3D(x, 0.0F, z);
    params.player_id = owner_id;
    params.spawn_type = type;
    params.ai_controlled = owner_id == k_sepulcher;
    params.is_initial_spawn = false;
    params.nation_id = owner_id == k_sepulcher ? Game::Systems::NationID::IronSepulcher
                                               : Game::Systems::NationID::RomanRepublic;
    auto unit = m_factory->create(type, m_session->world(), params);
    return unit ? unit->id() : 0;
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

  [[nodiscard]] auto engaged(EntityID id) const -> bool {
    auto* entity = m_session->world().get_entity(id);
    if (entity == nullptr) {
      return false;
    }
    auto const* target = entity->get_component<AttackTargetComponent>();
    if (target != nullptr && target->target_id != 0) {
      return true;
    }
    auto const* attack = entity->get_component<Engine::Core::AttackComponent>();
    return attack != nullptr && attack->in_melee_lock;
  }

  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_factory;
  std::unique_ptr<SessionContext> m_session;
  std::unique_ptr<Game::Session::ScopedSession> m_scope;
};

TEST_F(SepulcherGuardResponseTest, AWokenWatchDoesNotWaitToBeKilledOneByOne) {

  const std::vector<EntityID> watch{
      spawn(Game::Units::SpawnType::SkeletonSwordsman, k_sepulcher, 40.0F, 40.0F),
      spawn(Game::Units::SpawnType::SkeletonSwordsman, k_sepulcher, 43.0F, 41.0F),
      spawn(Game::Units::SpawnType::GravePriest, k_sepulcher, 41.5F, 43.0F),
  };
  for (auto const guard : watch) {
    ASSERT_NE(guard, 0U);
  }

  const EntityID legionary =
      spawn(Game::Units::SpawnType::Swordsman, k_player, 45.0F, 40.0F);
  ASSERT_NE(legionary, 0U);

  run_for(3.0);

  for (auto const guard : watch) {
    EXPECT_TRUE(engaged(guard))
        << "guardian " << guard << " watched a legionary walk through the ruins";
  }
}

TEST_F(SepulcherGuardResponseTest, HittingOneGuardianBringsTheRestOfTheWatch) {
  const EntityID struck =
      spawn(Game::Units::SpawnType::SkeletonSwordsman, k_sepulcher, 40.0F, 40.0F);
  const EntityID neighbour =
      spawn(Game::Units::SpawnType::SkeletonSwordsman, k_sepulcher, 46.0F, 40.0F);
  ASSERT_NE(struck, 0U);
  ASSERT_NE(neighbour, 0U);

  const EntityID archer = spawn(Game::Units::SpawnType::Archer, k_player, 62.0F, 40.0F);
  ASSERT_NE(archer, 0U);

  auto& world = m_session->world();
  Game::Systems::Combat::deal_damage(&world, world.get_entity(struck), 40, archer);

  run_for(2.0);

  EXPECT_TRUE(engaged(struck)) << "the guardian being shot never turned around";
  EXPECT_TRUE(engaged(neighbour))
      << "a guardian six metres away ignored its neighbour being shot to pieces";
}

class SepulcherLeashTest : public SepulcherGuardResponseTest {
protected:
  static constexpr float k_zone_grid = 30.0F;
  static constexpr float k_leash = 10.0F;

  auto wake_zone() -> std::vector<EntityID> {
    Game::Map::MapDefinition map_definition;
    map_definition.grid.width = k_map_size;
    map_definition.grid.height = k_map_size;
    map_definition.grid.tile_size = 1.0F;

    Game::Map::UndeadZone zone;
    zone.id = QStringLiteral("ruins_watch");
    zone.x = k_zone_grid;
    zone.z = k_zone_grid;
    zone.radius = 6.0F;
    zone.leash_radius = k_leash;
    zone.owner_id = k_sepulcher;
    zone.team_id = k_sepulcher;
    zone.awaken_on = {QStringLiteral("unit_enters_radius")};
    Game::Map::UndeadWave wave;
    wave.trigger = QStringLiteral("initial");
    wave.units.push_back({Game::Units::SpawnType::SkeletonSwordsman, 2});
    zone.waves.push_back(wave);
    map_definition.undead_zones.push_back(zone);

    m_undead = m_session->world().get_system<Game::Systems::UndeadAwakeningSystem>();
    if (m_undead == nullptr) {
      return {};
    }
    m_undead->configure(map_definition);
    m_anchor = m_undead->shrine_world_position(zone.id);

    m_legionary = spawn(Game::Units::SpawnType::Swordsman,
                        k_player,
                        m_anchor.x() + 2.0F,
                        m_anchor.z() + 2.0F);
    run_for(0.5);

    std::vector<EntityID> guardians;
    for (auto* entity :
         m_session->world().collect_entities_with<Engine::Core::UnitComponent>()) {
      auto const* unit = entity->get_component<Engine::Core::UnitComponent>();
      if (unit != nullptr && unit->owner_id == k_sepulcher && unit->health > 0 &&
          Game::Units::is_troop_spawn(unit->spawn_type)) {
        guardians.push_back(entity->get_id());
      }
    }
    return guardians;
  }

  [[nodiscard]] auto distance_from_anchor(EntityID id) const -> float {
    auto const* transform = m_session->world().try_get<TransformComponent>(id);
    if (transform == nullptr) {
      return 0.0F;
    }
    return std::hypot(transform->position.x - m_anchor.x(),
                      transform->position.z - m_anchor.z());
  }

  Game::Systems::UndeadAwakeningSystem* m_undead = nullptr;
  QVector3D m_anchor;
  EntityID m_legionary = 0;
};

TEST_F(SepulcherLeashTest, GuardiansDoNotChasePreyBeyondTheLeash) {
  const auto guardians = wake_zone();
  ASSERT_EQ(guardians.size(), 2U);
  ASSERT_NE(m_legionary, 0U);

  auto& world = m_session->world();
  auto* prey = world.try_get<TransformComponent>(m_legionary);
  ASSERT_NE(prey, nullptr);
  prey->position.x = m_anchor.x() + 30.0F;
  prey->position.z = m_anchor.z();

  for (auto const guardian : guardians) {
    auto* chase = Engine::Core::get_or_add_component<AttackTargetComponent>(
        *world.get_entity(guardian));
    ASSERT_NE(chase, nullptr);
    chase->target_id = m_legionary;
    chase->should_chase = true;
  }

  run_for(6.0);

  for (auto const guardian : guardians) {
    auto const* target = world.try_get<AttackTargetComponent>(guardian);
    EXPECT_TRUE(target == nullptr || target->target_id != m_legionary)
        << "guardian " << guardian << " is still hunting prey far past the leash";
    EXPECT_LE(distance_from_anchor(guardian), k_leash + 3.0F)
        << "guardian " << guardian << " left its ruins";
  }
}

TEST_F(SepulcherLeashTest, AStrayedGuardianWalksBackToItsPost) {
  const auto guardians = wake_zone();
  ASSERT_EQ(guardians.size(), 2U);

  auto& world = m_session->world();
  const EntityID strayed = guardians.front();
  auto* transform = world.try_get<TransformComponent>(strayed);
  ASSERT_NE(transform, nullptr);
  transform->position.x = m_anchor.x() + 22.0F;
  transform->position.z = m_anchor.z();
  const float start = distance_from_anchor(strayed);

  run_for(10.0);

  EXPECT_LT(distance_from_anchor(strayed), start - 4.0F)
      << "a guardian dropped outside the leash never came home";
}

} // namespace
