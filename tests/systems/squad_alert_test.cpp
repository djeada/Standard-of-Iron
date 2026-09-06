#include <algorithm>
#include <gtest/gtest.h>
#include <vector>

#include "core/component_commander.h"
#include "core/entity.h"
#include "core/world.h"
#include "systems/building_collision_registry.h"
#include "systems/combat_system/combat_utils.h"
#include "systems/combat_system/damage_processor.h"
#include "systems/combat_system/threat_alert.h"
#include "systems/nav_grid.h"
#include "systems/owner_registry.h"
#include "tests/support/movement_test_access.h"
#include "units/spawn_type.h"

using namespace Engine::Core;
using namespace Game::Systems;

namespace {

class SquadAlertTest : public ::testing::Test {
protected:
  void SetUp() override {
    world = std::make_unique<World>();
    OwnerRegistry::instance().clear();
    BuildingCollisionRegistry::instance().clear();
    NavGrid::initialize(64, 64);
  }

  void TearDown() override { world.reset(); }

  auto make_soldier(float x, float z, int owner_id) -> Entity* {
    auto* entity = world->create_entity();
    entity->add_component<TransformComponent>(x, 0.0F, z);
    auto* unit = entity->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
    unit->owner_id = owner_id;
    unit->spawn_type = Game::Units::SpawnType::Knight;
    entity->add_component<AttackComponent>(2.0F, 12, 1.0F);
    entity->add_component<MovementComponent>();
    return entity;
  }

  static auto target_of(Entity* entity) -> EntityID {
    auto* target = entity->get_component<AttackTargetComponent>();
    return target == nullptr ? 0 : target->target_id;
  }

  std::unique_ptr<World> world;
};

TEST_F(SquadAlertTest, AllyAnsweredWhenTheDefenderWasAlreadyFighting) {
  auto* defender = make_soldier(0.0F, 0.0F, 1);
  auto* ally = make_soldier(4.0F, 0.0F, 1);
  auto* first_enemy = make_soldier(2.0F, 0.0F, 2);
  auto* second_enemy = make_soldier(1.0F, 0.0F, 2);

  auto* defender_target = defender->add_component<AttackTargetComponent>();
  defender_target->target_id = first_enemy->get_id();

  Combat::deal_damage(world.get(), defender, 5, second_enemy->get_id());

  EXPECT_NE(target_of(ally), 0U)
      << "nobody answered because the defender was already busy";
}

TEST_F(SquadAlertTest, AllyBusyAtFirstContactIsAskedAgainOnLaterHits) {
  auto* defender = make_soldier(0.0F, 0.0F, 1);
  auto* ally = make_soldier(4.0F, 0.0F, 1);
  auto* enemy = make_soldier(2.0F, 0.0F, 2);
  auto* distraction = make_soldier(6.0F, 0.0F, 2);

  auto* ally_target = ally->add_component<AttackTargetComponent>();
  ally_target->target_id = distraction->get_id();

  Combat::deal_damage(world.get(), defender, 5, enemy->get_id());

  ally_target->target_id = 0;
  distraction->get_component<UnitComponent>()->health = 0;
  Combat::tick_threat_alerts(world.get(), 1.0F);

  Combat::deal_damage(world.get(), defender, 5, enemy->get_id());

  EXPECT_EQ(target_of(ally), enemy->get_id())
      << "the freed ally was never asked a second time";
}

TEST_F(SquadAlertTest, AllyInsideTheFogRevealRadiusAnswers) {
  auto* defender = make_soldier(0.0F, 0.0F, 1);
  auto* ally = make_soldier(17.0F, 0.0F, 1);
  auto* enemy = make_soldier(2.0F, 0.0F, 2);

  Combat::deal_damage(world.get(), defender, 5, enemy->get_id());

  EXPECT_EQ(target_of(ally), enemy->get_id())
      << "an ally the defender can see did not answer";
}

TEST_F(SquadAlertTest, HelpIsCalledAtMostOncePerInterval) {
  auto* defender = make_soldier(0.0F, 0.0F, 1);
  auto* ally = make_soldier(4.0F, 0.0F, 1);
  auto* enemy = make_soldier(2.0F, 0.0F, 2);

  Combat::deal_damage(world.get(), defender, 5, enemy->get_id());
  ASSERT_EQ(target_of(ally), enemy->get_id());

  ally->get_component<AttackTargetComponent>()->target_id = 0;
  Combat::deal_damage(world.get(), defender, 5, enemy->get_id());

  EXPECT_EQ(target_of(ally), 0U)
      << "a second hit inside the interval rescanned the neighbourhood";
}

TEST_F(SquadAlertTest, ASightingPullsIdleNeighboursButNotMarchingOnes) {
  auto* scout = make_soldier(0.0F, 0.0F, 1);
  auto* idler = make_soldier(16.0F, 0.0F, 1);
  auto* marcher = make_soldier(15.0F, 0.0F, 1);
  auto* enemy = make_soldier(2.0F, 0.0F, 2);

  auto* marcher_move = marcher->get_component<MovementComponent>();
  MovementTestAccess::set_has_target(*marcher_move, true);
  MovementTestAccess::set_target_x(*marcher_move, 60.0F);

  Combat::note_threat(
      world.get(), scout, enemy, ThreatAlertComponent::Kind::EnemySighted);

  EXPECT_EQ(target_of(idler), enemy->get_id())
      << "an idle neighbour ignored a sighting";
  EXPECT_EQ(target_of(marcher), 0U) << "a sighting pulled a unit off its march order";
}

TEST_F(SquadAlertTest, ASightingSkipsAlliesThatCanSeeTheEnemyThemselves) {
  auto* scout = make_soldier(0.0F, 0.0F, 1);
  auto* neighbour = make_soldier(4.0F, 0.0F, 1);
  auto* enemy = make_soldier(2.0F, 0.0F, 2);

  Combat::note_threat(
      world.get(), scout, enemy, ThreatAlertComponent::Kind::EnemySighted);

  EXPECT_EQ(target_of(neighbour), 0U)
      << "a sighting was relayed to someone already looking at the enemy";
}

TEST_F(SquadAlertTest, ASightingCommitsFarFewerMenThanAnAttackDoes) {
  auto* scout = make_soldier(0.0F, 0.0F, 1);
  auto* enemy = make_soldier(2.0F, 0.0F, 2);
  std::vector<Entity*> squad;
  squad.reserve(8);
  for (int i = 0; i < 8; ++i) {
    squad.push_back(make_soldier(15.0F + (static_cast<float>(i) * 0.2F), 0.0F, 1));
  }

  auto const count_responders = [&squad, enemy]() {
    return std::count_if(squad.begin(), squad.end(), [enemy](Entity* member) {
      auto* target = member->get_component<AttackTargetComponent>();
      return target != nullptr && target->target_id == enemy->get_id();
    });
  };

  Combat::note_threat(
      world.get(), scout, enemy, ThreatAlertComponent::Kind::EnemySighted);
  EXPECT_EQ(count_responders(), 1)
      << "a lone sighting mobilised more than a token response";

  for (auto* member : squad) {
    if (auto* target = member->get_component<AttackTargetComponent>()) {
      target->target_id = 0;
    }
  }
  Combat::tick_threat_alerts(world.get(), 1.0F);

  Combat::note_threat(
      world.get(), scout, enemy, ThreatAlertComponent::Kind::UnderAttack);
  EXPECT_EQ(count_responders(), 3) << "the squad did not answer a man under attack";
}

TEST_F(SquadAlertTest, OneEnemyDrawsAMeasuredResponseNotTheWholeGarrison) {
  auto* enemy = make_soldier(2.0F, 0.0F, 2);
  std::vector<Entity*> garrison;
  garrison.reserve(8);
  for (int i = 0; i < 8; ++i) {
    garrison.push_back(make_soldier(4.0F + (static_cast<float>(i) * 0.5F), 0.0F, 1));
  }

  for (int hit = 0; hit < 10; ++hit) {
    Combat::deal_damage(world.get(), garrison.front(), 1, enemy->get_id());
    Combat::tick_threat_alerts(world.get(), 1.0F);
  }

  auto const responders =
      std::count_if(garrison.begin(), garrison.end(), [enemy](Entity* member) {
        auto* target = member->get_component<AttackTargetComponent>();
        return target != nullptr && target->target_id == enemy->get_id();
      });
  EXPECT_EQ(responders, 3) << "sustained fire from one man mobilised " << responders
                           << " defenders";
}

TEST_F(SquadAlertTest, ACommanderIsNeverDraggedInByAnAlert) {
  auto* defender = make_soldier(0.0F, 0.0F, 1);
  auto* commander = make_soldier(4.0F, 0.0F, 1);
  commander->add_component<CommanderComponent>();
  auto* enemy = make_soldier(2.0F, 0.0F, 2);

  Combat::deal_damage(world.get(), defender, 5, enemy->get_id());

  EXPECT_EQ(target_of(commander), 0U)
      << "a squad alert pulled the lord into a skirmish";
}

TEST_F(SquadAlertTest, BeingHitPullsAMarchingAllyOffItsOrder) {
  auto* defender = make_soldier(0.0F, 0.0F, 1);
  auto* marcher = make_soldier(4.0F, 0.0F, 1);
  auto* enemy = make_soldier(2.0F, 0.0F, 2);

  auto* marcher_move = marcher->get_component<MovementComponent>();
  MovementTestAccess::set_has_target(*marcher_move, true);
  MovementTestAccess::set_target_x(*marcher_move, 60.0F);

  Combat::deal_damage(world.get(), defender, 5, enemy->get_id());

  EXPECT_EQ(target_of(marcher), enemy->get_id())
      << "a marching ally ignored the man beside it being cut down";
}

TEST_F(SquadAlertTest, ABuildingUnderAttackCallsTheGarrison) {
  auto* barracks = world->create_entity();
  barracks->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* barracks_unit = barracks->add_component<UnitComponent>(2000, 2000, 0.0F, 12.0F);
  barracks_unit->owner_id = 1;
  barracks_unit->spawn_type = Game::Units::SpawnType::Barracks;
  barracks->add_component<BuildingComponent>();

  auto* garrison = make_soldier(6.0F, 0.0F, 1);
  auto* enemy = make_soldier(3.0F, 0.0F, 2);

  Combat::deal_damage(world.get(), barracks, 5, enemy->get_id());

  EXPECT_EQ(target_of(garrison), enemy->get_id()) << "nobody defended the building";
}

TEST_F(SquadAlertTest, WorkersDoNotAnswerAlerts) {
  for (auto const job :
       {Game::Units::SpawnType::Civilian, Game::Units::SpawnType::Builder}) {
    SetUp();
    auto* defender = make_soldier(0.0F, 0.0F, 1);
    auto* worker = make_soldier(4.0F, 0.0F, 1);
    worker->get_component<UnitComponent>()->spawn_type = job;
    auto* enemy = make_soldier(2.0F, 0.0F, 2);

    Combat::deal_damage(world.get(), defender, 5, enemy->get_id());

    EXPECT_EQ(target_of(worker), 0U) << "a worker was pulled off its job by an alert";
  }
}

TEST_F(SquadAlertTest, BeingHitEscalatesPastASightingRaisedTheSameSecond) {
  auto* defender = make_soldier(0.0F, 0.0F, 1);
  auto* marcher = make_soldier(4.0F, 0.0F, 1);
  auto* enemy = make_soldier(2.0F, 0.0F, 2);

  auto* marcher_move = marcher->get_component<MovementComponent>();
  MovementTestAccess::set_has_target(*marcher_move, true);
  MovementTestAccess::set_target_x(*marcher_move, 60.0F);

  Combat::note_threat(
      world.get(), defender, enemy, ThreatAlertComponent::Kind::EnemySighted);
  ASSERT_EQ(target_of(marcher), 0U);

  Combat::deal_damage(world.get(), defender, 5, enemy->get_id());

  EXPECT_EQ(target_of(marcher), enemy->get_id())
      << "a sighting one tick earlier muted the call for help";
}

TEST_F(SquadAlertTest, TheAlertRadiusTracksTheFogRevealRadius) {
  UnitComponent unit(100, 100, 1.0F, 20.0F);
  EXPECT_FLOAT_EQ(Combat::threat_alert_radius(&unit),
                  20.0F * Engine::Core::Defaults::k_vision_reveal_scale);

  UnitComponent short_sighted(100, 100, 1.0F, 4.0F);
  EXPECT_FLOAT_EQ(Combat::threat_alert_radius(&short_sighted),
                  Engine::Core::Defaults::k_unit_default_vision_range *
                      Engine::Core::Defaults::k_vision_reveal_scale);
}

} // namespace
