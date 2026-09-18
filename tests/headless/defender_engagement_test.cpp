#include <QString>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <string>
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
#include "game/systems/command_service.h"
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

using Engine::Core::EntityID;
using Engine::Core::TransformComponent;
using Engine::Core::UnitComponent;
using Game::Session::SessionContext;
using Game::Units::SpawnType;

constexpr int k_map_size = 128;
constexpr int k_player = 1;
constexpr int k_carthage = 2;
constexpr int k_sepulcher = 99;

constexpr double k_longest_tolerated_idle_seconds = 4.0;

struct IdleReport {
  double longest_idle_seconds = 0.0;
  std::string worst_unit;
  int defenders_left = 0;
  int attackers_left = 0;
};

class DefenderEngagementTest : public ::testing::Test {
protected:
  void SetUp() override {
    reset_shared_world_state();
    m_factory = std::make_shared<Game::Units::UnitFactoryRegistry>();
    Game::Units::register_built_in_units(*m_factory);

    m_session = std::make_unique<SessionContext>();
    m_session->world().set_presentation_enabled(false);
    m_scope = std::make_unique<Game::Session::ScopedSession>(*m_session);
    Game::Systems::NavGrid::initialize(k_map_size, k_map_size);
    Game::Map::MapTransformer::setFactoryRegistry(m_factory);

    auto& owners = m_session->owners();
    owners.register_owner_with_id(k_player, Game::Systems::OwnerType::Player, "player");
    owners.set_owner_team(k_player, 1);
    owners.register_owner_with_id(k_carthage, Game::Systems::OwnerType::AI, "carthage");
    owners.set_owner_team(k_carthage, 2);

    Game::Systems::initialize_default_content(m_session->nations());
    m_session->nations().set_player_nation(k_player,
                                           Game::Systems::NationID::RomanRepublic);
    m_session->nations().set_player_nation(k_carthage,
                                           Game::Systems::NationID::Carthage);

    m_session->terrain().initialize(empty_map());

    Game::Systems::register_runtime_systems(m_session->world());
    if (auto* ai = m_session->world().get_system<Game::Systems::AISystem>()) {
      ai->reinitialize();
    }
    owners.register_owner_with_id(
        k_sepulcher, Game::Systems::OwnerType::AI, "Iron Sepulcher shrine");
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

  static auto empty_map() -> Game::Map::MapDefinition {
    Game::Map::MapDefinition map_definition;
    map_definition.grid.width = k_map_size;
    map_definition.grid.height = k_map_size;
    map_definition.grid.tile_size = 1.0F;
    return map_definition;
  }

  auto spawn(SpawnType type, int owner_id, float x, float z) -> EntityID {
    Game::Units::SpawnParams params;
    params.position = QVector3D(x, 0.0F, z);
    params.player_id = owner_id;
    params.spawn_type = type;
    params.ai_controlled = owner_id != k_player;
    params.is_initial_spawn = false;
    params.nation_id = owner_id == k_sepulcher ? Game::Systems::NationID::IronSepulcher
                       : owner_id == k_carthage
                           ? Game::Systems::NationID::Carthage
                           : Game::Systems::NationID::RomanRepublic;
    auto unit = m_factory->create(type, m_session->world(), params);
    return unit ? unit->id() : 0;
  }

  auto wake_shrine_garrison() -> QVector3D {
    auto map_definition = empty_map();
    Game::Map::UndeadZone zone;
    zone.id = QStringLiteral("shrine_sentinels");
    zone.x = 64.0F;
    zone.z = 64.0F;
    zone.radius = 7.0F;
    zone.leash_radius = 12.0F;
    zone.owner_id = k_sepulcher;
    zone.team_id = k_sepulcher;
    zone.awaken_on = {QStringLiteral("unit_enters_radius")};
    Game::Map::UndeadWave wave;
    wave.trigger = QStringLiteral("initial");
    wave.units.push_back({SpawnType::SkeletonSwordsman, 2});
    wave.units.push_back({SpawnType::SkeletonArcher, 2});
    wave.units.push_back({SpawnType::GravePriest, 1});
    zone.waves.push_back(wave);
    map_definition.undead_zones.push_back(zone);

    auto* undead =
        m_session->world().get_system<Game::Systems::UndeadAwakeningSystem>();
    if (undead == nullptr) {
      return {};
    }
    undead->configure(map_definition);
    return undead->shrine_world_position(zone.id);
  }

  auto troops_of(int owner) -> std::vector<EntityID> {
    std::vector<EntityID> ids;
    for (auto* entity :
         m_session->world().collect_entities_with<Engine::Core::UnitComponent>()) {
      auto const* unit = entity->get_component<UnitComponent>();
      if (unit != nullptr && unit->owner_id == owner && unit->health > 0 &&
          Game::Units::is_troop_spawn(unit->spawn_type)) {
        ids.push_back(entity->get_id());
      }
    }
    return ids;
  }

  void step() {
    const double dt = m_session->clock().tick_seconds();
    m_session->clock().advance(dt);
    while (m_session->clock().consume_tick()) {
      m_session->world().update(static_cast<float>(dt));
    }
  }

  void run_for(double seconds) {
    const double dt = m_session->clock().tick_seconds();
    for (double elapsed = 0.0; elapsed < seconds; elapsed += dt) {
      step();
    }
  }

  auto distance(EntityID a, EntityID b) -> float {
    auto const* ta = m_session->world().try_get<TransformComponent>(a);
    auto const* tb = m_session->world().try_get<TransformComponent>(b);
    return std::hypot(ta->position.x - tb->position.x, ta->position.z - tb->position.z);
  }

  auto fighting(EntityID id) -> bool {
    auto& world = m_session->world();
    auto const* target = world.try_get<Engine::Core::AttackTargetComponent>(id);
    auto const* attack = world.try_get<Engine::Core::AttackComponent>(id);
    return (target != nullptr && target->target_id != 0) ||
           (attack != nullptr && attack->in_melee_lock);
  }

  void attack_move(const std::vector<EntityID>& ids, QVector3D target) {
    Game::Systems::CommandService::MoveOptions options;
    options.kind = Game::Systems::MoveOrderKind::AttackMove;
    std::vector<QVector3D> const targets(ids.size(), target);
    Game::Systems::CommandService::move_units(
        m_session->world(), ids, targets, options);
  }

  auto watch_defenders(int defender_owner, double seconds) -> IdleReport {
    IdleReport report;
    std::map<EntityID, double> idle_since;
    const double dt = m_session->clock().tick_seconds();
    for (double elapsed = 0.0; elapsed < seconds; elapsed += dt) {
      step();
      auto const defenders = troops_of(defender_owner);
      auto const attackers = troops_of(k_player);
      if ((defenders.empty() && elapsed > 5.0) || attackers.empty()) {
        break;
      }

      for (auto const defender : defenders) {
        auto const* unit = m_session->world().try_get<UnitComponent>(defender);
        float nearest = std::numeric_limits<float>::max();
        for (auto const attacker : attackers) {
          nearest = std::min(nearest, distance(defender, attacker));
        }
        bool const ally_fighting_nearby =
            std::any_of(defenders.begin(), defenders.end(), [&](EntityID ally) {
              return ally != defender && fighting(ally) &&
                     distance(ally, defender) <= unit->vision_range;
            });
        bool const in_the_fight = nearest <= unit->vision_range || ally_fighting_nearby;

        if (!in_the_fight || fighting(defender)) {
          idle_since.erase(defender);
          continue;
        }
        auto const [it, fresh] = idle_since.emplace(defender, elapsed);
        double const idle_for = elapsed - it->second;
        if (idle_for > report.longest_idle_seconds) {
          report.longest_idle_seconds = idle_for;
          std::ostringstream name;
          name << Game::Units::spawn_typeToQString(unit->spawn_type).toStdString()
               << " #" << defender << " (nearest enemy " << nearest << " m)";
          report.worst_unit = name.str();
        }
      }
    }
    report.defenders_left = static_cast<int>(troops_of(defender_owner).size());
    report.attackers_left = static_cast<int>(troops_of(k_player).size());
    return report;
  }

  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_factory;
  std::unique_ptr<SessionContext> m_session;
  std::unique_ptr<Game::Session::ScopedSession> m_scope;
};

TEST_F(DefenderEngagementTest, GuardiansBehindTheShrineJoinAFightOnItsFarSide) {
  QVector3D const anchor = wake_shrine_garrison();

  const std::vector<EntityID> army{
      spawn(SpawnType::Swordsman, k_player, anchor.x() - 9.0F, anchor.z()),
      spawn(SpawnType::Swordsman, k_player, anchor.x() - 9.0F, anchor.z() + 3.0F),
      spawn(SpawnType::Spearman, k_player, anchor.x() - 9.0F, anchor.z() - 3.0F),
      spawn(SpawnType::Archer, k_player, anchor.x() - 14.0F, anchor.z() + 1.5F),
      spawn(SpawnType::Archer, k_player, anchor.x() - 14.0F, anchor.z() - 1.5F),
  };
  attack_move({army[0], army[1], army[2]},
              QVector3D(anchor.x() - 5.0F, 0.0F, anchor.z()));

  auto const report = watch_defenders(k_sepulcher, 45.0);

  EXPECT_LE(report.longest_idle_seconds, k_longest_tolerated_idle_seconds)
      << report.worst_unit << " stood idle for " << report.longest_idle_seconds
      << " s while the shrine was being stormed";
}

TEST_F(DefenderEngagementTest, AGarrisonOutrangedByArchersStillAnswersTheVolleys) {
  QVector3D const anchor = wake_shrine_garrison();

  const EntityID scout =
      spawn(SpawnType::Archer, k_player, anchor.x() - 6.0F, anchor.z());
  run_for(0.5);
  m_session->world().try_get<TransformComponent>(scout)->position.x =
      anchor.x() - 15.0F;
  const std::vector<EntityID> army{
      scout,
      spawn(SpawnType::Archer, k_player, anchor.x() - 15.0F, anchor.z() + 3.0F),
  };
  attack_move(army, anchor);

  auto const report = watch_defenders(k_sepulcher, 30.0);

  EXPECT_LE(report.longest_idle_seconds, k_longest_tolerated_idle_seconds)
      << report.worst_unit << " stood idle for " << report.longest_idle_seconds
      << " s while archers shot up its garrison";
  EXPECT_EQ(report.attackers_left, 0)
      << "two archers shot at a woken sepulcher for 30 s and nobody came for them";
}

TEST_F(DefenderEngagementTest, AnAiCampRaidedByAColumnFightsWithEveryMan) {
  spawn(SpawnType::Barracks, k_carthage, 64.0F, 72.0F);
  spawn(SpawnType::Spearman, k_carthage, 60.0F, 64.0F);
  spawn(SpawnType::Spearman, k_carthage, 64.0F, 62.0F);
  spawn(SpawnType::Swordsman, k_carthage, 68.0F, 64.0F);
  spawn(SpawnType::Archer, k_carthage, 62.0F, 67.0F);
  spawn(SpawnType::Archer, k_carthage, 66.0F, 67.0F);
  spawn(SpawnType::Swordsman, k_carthage, 72.0F, 66.0F);
  run_for(1.0);

  const std::vector<EntityID> army{
      spawn(SpawnType::Swordsman, k_player, 40.0F, 64.0F),
      spawn(SpawnType::Swordsman, k_player, 40.0F, 60.0F),
      spawn(SpawnType::Spearman, k_player, 40.0F, 68.0F),
      spawn(SpawnType::MountedSwordsman, k_player, 38.0F, 56.0F),
  };
  attack_move(army, QVector3D(64.0F, 0.0F, 64.0F));

  auto const report = watch_defenders(k_carthage, 40.0);

  EXPECT_LE(report.longest_idle_seconds, k_longest_tolerated_idle_seconds)
      << report.worst_unit << " stood idle for " << report.longest_idle_seconds
      << " s while its camp was raided";
}

TEST_F(DefenderEngagementTest, AnAiCampShotAtFromRangeSendsMenAtTheArchers) {
  spawn(SpawnType::Barracks, k_carthage, 64.0F, 72.0F);
  spawn(SpawnType::Spearman, k_carthage, 60.0F, 64.0F);
  spawn(SpawnType::Spearman, k_carthage, 64.0F, 62.0F);
  spawn(SpawnType::Swordsman, k_carthage, 68.0F, 64.0F);
  spawn(SpawnType::Archer, k_carthage, 62.0F, 67.0F);
  spawn(SpawnType::Swordsman, k_carthage, 72.0F, 66.0F);
  run_for(1.0);

  const std::vector<EntityID> archers{
      spawn(SpawnType::Archer, k_player, 52.0F, 64.0F),
      spawn(SpawnType::Archer, k_player, 52.0F, 60.0F),
  };
  for (auto const archer : archers) {
    Engine::Core::get_or_add_component<Engine::Core::HoldModeComponent>(
        *m_session->world().get_entity(archer))
        ->active = true;
  }

  auto const report = watch_defenders(k_carthage, 30.0);

  EXPECT_LE(report.longest_idle_seconds, k_longest_tolerated_idle_seconds)
      << report.worst_unit << " stood idle for " << report.longest_idle_seconds
      << " s while archers shot into its camp";
  EXPECT_EQ(report.attackers_left, 0) << "the camp let two archers shoot it at will";
}

TEST_F(DefenderEngagementTest, AnIdleAiSoldierJoinsTheFightOfAnAllyItCanSee) {
  const EntityID fighter = spawn(SpawnType::Swordsman, k_carthage, 50.0F, 64.0F);
  const EntityID bystander = spawn(SpawnType::Swordsman, k_carthage, 38.0F, 64.0F);
  const EntityID raider = spawn(SpawnType::Swordsman, k_player, 57.0F, 64.0F);
  ASSERT_NE(fighter, 0U);
  ASSERT_NE(bystander, 0U);
  ASSERT_NE(raider, 0U);

  auto& world = m_session->world();
  auto const* bystander_unit = world.try_get<UnitComponent>(bystander);
  ASSERT_GT(distance(bystander, raider), bystander_unit->vision_range)
      << "the raider must be out of the bystander's own sight, or the case is vacuous";
  ASSERT_LE(distance(bystander, fighter), bystander_unit->vision_range);

  Engine::Core::get_or_add_component<Engine::Core::HoldModeComponent>(
      *world.get_entity(raider))
      ->active = true;
  auto* held = Engine::Core::get_or_add_component<Engine::Core::AttackTargetComponent>(
      *world.get_entity(fighter));
  held->target_id = raider;
  held->should_chase = false;

  run_for(1.5);

  auto const* joined = world.try_get<Engine::Core::AttackTargetComponent>(bystander);
  EXPECT_TRUE(joined != nullptr && joined->target_id == raider)
      << "an idle soldier watched the man beside it fight without joining in";
}

} // namespace
