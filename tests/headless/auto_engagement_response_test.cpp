#include <cmath>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

#include "game/core/component.h"
#include "game/core/event_manager.h"
#include "game/core/ownership_constants.h"
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
#include "game/systems/combat_system/engagement_trace.h"
#include "game/systems/command_service.h"
#include "game/systems/default_content.h"
#include "game/systems/formation_combat_geometry.h"
#include "game/systems/nation_registry.h"
#include "game/systems/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/runtime_system_registry.h"
#include "game/systems/troop_count_registry.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"

namespace {

using Engine::Core::AttackComponent;
using Engine::Core::AttackTargetComponent;
using Engine::Core::EntityID;
using Engine::Core::TransformComponent;
using Engine::Core::UnitComponent;
using Engine::Core::WildlifeComponent;
using Game::Session::SessionContext;
using Game::Systems::Combat::EngagementTrace;

constexpr int k_map_size = 96;
constexpr int k_player = 1;
constexpr int k_enemy = 2;

class AutoEngagementResponseTest : public ::testing::Test {
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
    owners.register_owner_with_id(k_enemy, Game::Systems::OwnerType::AI, "enemy");
    owners.set_owner_team(k_enemy, 2);

    Game::Systems::initialize_default_content(m_session->nations());
    m_session->nations().set_player_nation(k_player,
                                           Game::Systems::NationID::RomanRepublic);
    m_session->nations().set_player_nation(k_enemy, Game::Systems::NationID::Carthage);

    Game::Map::MapDefinition map_definition;
    map_definition.grid.width = k_map_size;
    map_definition.grid.height = k_map_size;
    map_definition.grid.tile_size = 1.0F;
    m_session->terrain().initialize(map_definition);

    Game::Systems::register_runtime_systems(m_session->world());
    if (auto* ai = m_session->world().get_system<Game::Systems::AISystem>()) {
      ai->reinitialize();
    }

    EngagementTrace::instance().set_enabled(true);
    EngagementTrace::instance().clear();
  }

  void TearDown() override {
    EngagementTrace::instance().set_enabled(false);
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
    params.ai_controlled = owner_id == k_enemy;
    params.is_initial_spawn = false;
    params.nation_id = owner_id == k_enemy ? Game::Systems::NationID::Carthage
                                           : Game::Systems::NationID::RomanRepublic;
    auto unit = m_factory->create(type, m_session->world(), params);
    return unit ? unit->id() : 0;
  }

  auto spawn_hostile_wolf(float x, float z) -> EntityID {
    auto* entity = m_session->world().create_entity();
    entity->add_component<TransformComponent>(x, 0.0F, z);
    auto* unit = entity->add_component<UnitComponent>(60, 60, 1.6F, 12.0F);
    unit->owner_id = Game::Core::NEUTRAL_OWNER_ID;
    unit->spawn_type = Game::Units::SpawnType::Wolf;
    auto* attack = entity->add_component<AttackComponent>();
    attack->can_melee = true;
    attack->range = 1.4F;
    attack->melee_range = 1.4F;
    attack->damage = 6;
    auto* wildlife = entity->add_component<WildlifeComponent>();
    wildlife->species = Game::Wildlife::Species::Wolf;
    wildlife->hostile_timer = 30.0F;
    return entity->get_id();
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

  [[nodiscard]] auto entity(EntityID id) -> Engine::Core::Entity* {
    return m_session->world().get_entity(id);
  }

  [[nodiscard]] auto target_of(EntityID id) -> EntityID {
    auto* found = entity(id);
    if (found == nullptr) {
      return 0;
    }
    auto const* target = found->get_component<AttackTargetComponent>();
    return target == nullptr ? 0 : target->target_id;
  }

  [[nodiscard]] auto engaged(EntityID id) -> bool {
    auto* found = entity(id);
    if (found == nullptr) {
      return false;
    }
    if (target_of(id) != 0) {
      return true;
    }
    auto const* attack = found->get_component<AttackComponent>();
    return attack != nullptr && attack->in_melee_lock;
  }

  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_factory;
  std::unique_ptr<SessionContext> m_session;
  std::unique_ptr<Game::Session::ScopedSession> m_scope;
};

TEST_F(AutoEngagementResponseTest, SwordsmenGoToTheAidOfAnAllyBittenByWolves) {
  const EntityID victim = spawn(Game::Units::SpawnType::Civilian, k_player, 0.0F, 0.0F);
  const EntityID wolf = spawn_hostile_wolf(1.2F, 0.0F);
  ASSERT_NE(victim, 0U);
  ASSERT_NE(wolf, 0U);

  const std::vector<EntityID> escort{
      spawn(Game::Units::SpawnType::Knight, k_player, 4.0F, 0.0F),
      spawn(Game::Units::SpawnType::Knight, k_player, 4.0F, 3.0F),
  };
  for (auto const swordsman : escort) {
    ASSERT_NE(swordsman, 0U);
  }

  auto& world = m_session->world();
  Game::Systems::Combat::deal_damage(&world, world.get_entity(victim), 10, wolf);

  std::vector<bool> answered(escort.size(), false);
  const double step = m_session->clock().tick_seconds();
  for (double elapsed = 0.0; elapsed < 1.0; elapsed += step) {
    run_for(step);
    for (std::size_t index = 0; index < escort.size(); ++index) {
      answered[index] = answered[index] || target_of(escort[index]) == wolf;
    }
  }

  auto const* wolf_unit =
      entity(wolf) != nullptr ? entity(wolf)->get_component<UnitComponent>() : nullptr;
  bool const wolf_is_down = wolf_unit == nullptr || wolf_unit->health <= 0;
  for (std::size_t index = 0; index < escort.size(); ++index) {
    EXPECT_TRUE(answered[index] && (target_of(escort[index]) == wolf || wolf_is_down))
        << "swordsman " << escort[index] << " watched a wolf maul the man beside him"
        << "; wolf " << (wolf_is_down ? "down" : "alive") << ", his target "
        << target_of(escort[index]);
  }
}

TEST_F(AutoEngagementResponseTest, InfantryUnderArrowFireDoesNotStandAndTakeIt) {
  const EntityID legionary =
      spawn(Game::Units::SpawnType::Knight, k_player, 0.0F, 0.0F);
  const EntityID archer = spawn(Game::Units::SpawnType::Archer, k_enemy, 9.0F, 0.0F);
  ASSERT_NE(legionary, 0U);
  ASSERT_NE(archer, 0U);

  auto& world = m_session->world();
  Game::Systems::Combat::deal_damage(&world, world.get_entity(legionary), 12, archer);

  run_for(2.0);

  EXPECT_EQ(target_of(legionary), archer)
      << "infantry with no orders absorbed arrows without answering";
}

TEST_F(AutoEngagementResponseTest, VillageDefendersAnswerArchersInsideTheirWalls) {
  const EntityID home = spawn(Game::Units::SpawnType::Home, k_enemy, 0.0F, 0.0F);
  ASSERT_NE(home, 0U);

  const std::vector<EntityID> garrison{
      spawn(Game::Units::SpawnType::Spearman, k_enemy, -3.0F, 0.0F),
      spawn(Game::Units::SpawnType::Spearman, k_enemy, 0.0F, -3.0F),
  };
  for (auto const defender : garrison) {
    ASSERT_NE(defender, 0U);
  }

  const EntityID raider = spawn(Game::Units::SpawnType::Archer, k_player, 6.0F, 0.0F);
  ASSERT_NE(raider, 0U);

  run_for(4.0);

  for (auto const defender : garrison) {
    EXPECT_TRUE(engaged(defender))
        << "defender " << defender
        << " let an archer shoot up the village without reacting";
  }
}

TEST_F(AutoEngagementResponseTest, ArchersOpenFireOnlyOnceAThreatIsInWeaponRange) {
  const EntityID archer = spawn(Game::Units::SpawnType::Archer, k_player, 0.0F, 0.0F);
  ASSERT_NE(archer, 0U);

  auto* archer_entity = entity(archer);
  ASSERT_NE(archer_entity, nullptr);
  auto const* attack = archer_entity->get_component<AttackComponent>();
  ASSERT_NE(attack, nullptr);
  float const weapon_range = attack->range;

  const EntityID distant =
      spawn(Game::Units::SpawnType::Knight, k_enemy, weapon_range + 12.0F, 0.0F);
  ASSERT_NE(distant, 0U);

  auto* distant_entity = entity(distant);
  ASSERT_NE(distant_entity, nullptr);

  distant_entity->remove_component<Engine::Core::MovementComponent>();

  run_for(1.5);
  EXPECT_EQ(target_of(archer), 0U)
      << "an archer drew on a target well outside its weapon range";

  auto* distant_transform = distant_entity->get_component<TransformComponent>();
  ASSERT_NE(distant_transform, nullptr);
  distant_transform->position.x = weapon_range * 0.5F;

  run_for(1.5);
  EXPECT_EQ(target_of(archer), distant)
      << "an archer with a clear shot inside its range never took it";
}

TEST_F(AutoEngagementResponseTest, APlayerMoveOrderOverridesAnAutomaticTarget) {
  const EntityID legionary =
      spawn(Game::Units::SpawnType::Knight, k_player, 0.0F, 0.0F);

  const EntityID raider = spawn(Game::Units::SpawnType::Knight, k_enemy, 12.0F, 0.0F);
  ASSERT_NE(legionary, 0U);
  ASSERT_NE(raider, 0U);

  run_for(0.5);
  ASSERT_EQ(target_of(legionary), raider)
      << "the legionary never auto-acquired, so there is no engagement to break";

  const QVector3D destination(-24.0F, 0.0F, -18.0F);
  auto const distance_to_destination = [this, legionary, destination]() {
    auto const& mine = entity(legionary)->get_component<TransformComponent>()->position;
    float const dx = destination.x() - mine.x;
    float const dz = destination.z() - mine.z;
    return std::sqrt((dx * dx) + (dz * dz));
  };
  float const started_at = distance_to_destination();

  Game::Systems::CommandService::move_unit(m_session->world(), legionary, destination);

  EXPECT_EQ(target_of(legionary), 0U)
      << "a move order left the automatic target in place";

  run_for(3.0);

  EXPECT_EQ(target_of(legionary), 0U)
      << "auto-engagement re-acquired behind an explicit player move order";
  EXPECT_LT(distance_to_destination(), started_at - 1.0F)
      << "the legionary stayed with its automatic target instead of marching";
}

TEST_F(AutoEngagementResponseTest, AnAutomaticTargetIsReplacedByAPlayerAttackOrder) {
  const EntityID legionary =
      spawn(Game::Units::SpawnType::Knight, k_player, 0.0F, 0.0F);
  const EntityID near_raider =
      spawn(Game::Units::SpawnType::Knight, k_enemy, 10.0F, 0.0F);
  const EntityID chosen_raider =
      spawn(Game::Units::SpawnType::Knight, k_enemy, 0.0F, 13.0F);
  ASSERT_NE(legionary, 0U);
  ASSERT_NE(near_raider, 0U);
  ASSERT_NE(chosen_raider, 0U);

  run_for(0.5);
  ASSERT_EQ(target_of(legionary), near_raider);

  Game::Systems::CommandService::attack_target(
      m_session->world(), {legionary}, chosen_raider);

  EXPECT_EQ(target_of(legionary), chosen_raider)
      << "the player could not redirect a unit off its automatic target";

  auto* legionary_entity = entity(legionary);
  ASSERT_NE(legionary_entity, nullptr);
  auto const* target = legionary_entity->get_component<AttackTargetComponent>();
  ASSERT_NE(target, nullptr);
  EXPECT_TRUE(target->is_player_command)
      << "a player attack order was recorded as an automatic engagement";
}

TEST_F(AutoEngagementResponseTest, TheTraceNamesTheCandidateTheTargetAndTheReason) {
  const EntityID legionary =
      spawn(Game::Units::SpawnType::Knight, k_player, 0.0F, 0.0F);
  const EntityID raider = spawn(Game::Units::SpawnType::Knight, k_enemy, 5.0F, 0.0F);
  ASSERT_NE(legionary, 0U);
  ASSERT_NE(raider, 0U);

  run_for(1.5);

  auto const* record = EngagementTrace::instance().find(legionary);
  ASSERT_NE(record, nullptr) << "auto-engagement recorded no decision at all";
  EXPECT_EQ(record->target_id, raider) << "the trace does not name the chosen target";
  EXPECT_EQ(record->outcome, Game::Systems::Combat::EngagementOutcome::HoldingTarget);
  EXPECT_EQ(record->source, Game::Systems::Combat::CommandSource::Auto)
      << "an automatic engagement was attributed to a player order";

  const EntityID builder = spawn(Game::Units::SpawnType::Builder, k_player, 1.0F, 4.0F);
  ASSERT_NE(builder, 0U);
  run_for(0.5);

  auto const* refusal = EngagementTrace::instance().find(builder);
  ASSERT_NE(refusal, nullptr) << "no decision was recorded for the builder";
  EXPECT_EQ(refusal->outcome, Game::Systems::Combat::EngagementOutcome::NoCombatRole)
      << "the trace does not say why the builder stayed out of the fight";
}

TEST_F(AutoEngagementResponseTest, AnAiUnitThatPicksItsOwnFightReadsAsAutomatic) {

  const EntityID defender =
      spawn(Game::Units::SpawnType::Spearman, k_enemy, 0.0F, 0.0F);
  const EntityID raider = spawn(Game::Units::SpawnType::Knight, k_player, 6.0F, 0.0F);
  ASSERT_NE(defender, 0U);
  ASSERT_NE(raider, 0U);

  auto* defender_entity = entity(defender);
  ASSERT_NE(defender_entity, nullptr);
  ASSERT_TRUE(defender_entity->has_component<Engine::Core::AIControlledComponent>())
      << "this test is only meaningful for an AI-owned unit";

  run_for(1.0);

  ASSERT_EQ(target_of(defender), raider) << "the AI defender never acquired";

  auto const* record = EngagementTrace::instance().find(defender);
  ASSERT_NE(record, nullptr);
  EXPECT_EQ(record->source, Game::Systems::Combat::CommandSource::Auto)
      << "an AI unit that picked its own fight was reported as acting on an order; "
         "the command source has to say who chose the target, not who owns the unit";

  Game::Systems::CommandService::attack_target(m_session->world(), {defender}, raider);

  EXPECT_EQ(Game::Systems::Combat::command_source_of(defender_entity),
            Game::Systems::Combat::CommandSource::AIOrder)
      << "an ordered AI engagement has to read as an AI order";
}

TEST_F(AutoEngagementResponseTest, ANoncombatantNeverPicksItsOwnFight) {
  const EntityID builder = spawn(Game::Units::SpawnType::Builder, k_player, 0.0F, 0.0F);
  const EntityID raider = spawn(Game::Units::SpawnType::Knight, k_enemy, 4.0F, 0.0F);
  ASSERT_NE(builder, 0U);
  ASSERT_NE(raider, 0U);

  run_for(2.0);

  auto const* builder_attack = entity(builder)->get_component<AttackComponent>();
  bool const locked_by_raider = builder_attack != nullptr &&
                                builder_attack->in_melee_lock &&
                                builder_attack->melee_lock_target_id == raider;
  if (locked_by_raider) {
    EXPECT_EQ(target_of(builder), raider) << "a locked builder fought someone else";
  } else {
    EXPECT_EQ(target_of(builder), 0U) << "a builder went looking for a fight";
  }
  auto const* record = EngagementTrace::instance().find(builder);
  ASSERT_NE(record, nullptr);
  EXPECT_EQ(record->outcome, Game::Systems::Combat::EngagementOutcome::NoCombatRole)
      << "auto-engagement acquired a target for a builder";
}

} // namespace
