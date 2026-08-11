#include <gtest/gtest.h>

#include "core/component.h"
#include "core/world.h"
#include "systems/owner_registry.h"
#include "systems/rpg_combat_system/rpg_combat_processor.h"

using namespace Engine::Core;

namespace {

auto create_unit(World& world, float x, float z, int owner_id) -> Entity* {
  auto* entity = world.create_entity();
  entity->add_component<TransformComponent>(x, 0.0F, z);
  auto* unit = entity->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  unit->owner_id = owner_id;
  return entity;
}

} // namespace

class RpgEngagementSystemTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::OwnerRegistry::instance().clear();
    Game::Systems::OwnerRegistry::instance().register_owner_with_id(
        1, Game::Systems::OwnerType::Player, "Player");
    Game::Systems::OwnerRegistry::instance().register_owner_with_id(
        2, Game::Systems::OwnerType::AI, "Enemy");
  }

  World world;
};

TEST_F(RpgEngagementSystemTest, AssignsOnlyFrontAndTwoSideThreatsAroundCommander) {
  auto* commander = create_unit(world, 0.0F, 0.0F, 1);
  auto* commander_transform = commander->get_component<TransformComponent>();
  ASSERT_NE(commander_transform, nullptr);
  commander_transform->rotation.y = 0.0F;

  auto* front = create_unit(world, 0.0F, 2.0F, 2);
  auto* left = create_unit(world, -1.8F, 1.4F, 2);
  auto* right = create_unit(world, 1.8F, 1.4F, 2);
  create_unit(world, -0.8F, 2.4F, 2);
  create_unit(world, 0.8F, 2.4F, 2);
  create_unit(world, 0.0F, 4.0F, 2);

  Game::Systems::RpgCombat::refresh_commander_engagement(&world, commander->get_id());

  auto* engagement = commander->get_component<RpgEngagementComponent>();
  ASSERT_NE(engagement, nullptr);
  EXPECT_EQ(engagement->front_attacker_id, front->get_id());
  EXPECT_EQ(engagement->left_threat_id, left->get_id());
  EXPECT_EQ(engagement->right_threat_id, right->get_id());
  EXPECT_EQ(engagement->active_attackers, 3);
  ASSERT_EQ(engagement->engagement_slots.size(), 6U);

  int active_slots = 0;
  int support_slots = 0;
  for (auto const& slot : engagement->engagement_slots) {
    if (slot.role == RpgEngagementRole::Support) {
      ++support_slots;
    } else {
      ++active_slots;
    }
  }
  EXPECT_EQ(active_slots, 3);
  EXPECT_EQ(support_slots, 3);
}

TEST_F(RpgEngagementSystemTest, IgnoresAlliesDeadUnitsAndUnitsOutsideRing) {
  auto* commander = create_unit(world, 0.0F, 0.0F, 1);
  auto* commander_transform = commander->get_component<TransformComponent>();
  ASSERT_NE(commander_transform, nullptr);
  commander_transform->rotation.y = 0.0F;

  auto* enemy = create_unit(world, 0.0F, 2.0F, 2);
  create_unit(world, 0.0F, 2.2F, 1);
  create_unit(world, 0.0F, 8.0F, 2);
  auto* dead_enemy = create_unit(world, 1.0F, 1.0F, 2);
  dead_enemy->get_component<UnitComponent>()->health = 0;

  Game::Systems::RpgCombat::refresh_commander_engagement(&world, commander->get_id());

  auto* engagement = commander->get_component<RpgEngagementComponent>();
  ASSERT_NE(engagement, nullptr);
  ASSERT_EQ(engagement->engagement_slots.size(), 1U);
  EXPECT_EQ(engagement->engagement_slots.front().entity_id, enemy->get_id());
  EXPECT_EQ(engagement->front_attacker_id, enemy->get_id());
  EXPECT_EQ(engagement->active_attackers, 1);
}

TEST_F(RpgEngagementSystemTest, EmptyRingHasNoFightContext) {
  auto* commander = create_unit(world, 0.0F, 0.0F, 1);
  create_unit(world, 0.0F, 20.0F, 2);

  Game::Systems::RpgCombat::refresh_commander_engagement(&world, commander->get_id());

  auto* engagement = commander->get_component<RpgEngagementComponent>();
  ASSERT_NE(engagement, nullptr);
  EXPECT_EQ(engagement->fight_context, FightContext::None);
}

TEST_F(RpgEngagementSystemTest, SingleBodyOpponentsMakeADuel) {
  auto* commander = create_unit(world, 0.0F, 0.0F, 1);
  auto* champion = create_unit(world, 0.0F, 2.0F, 2);
  champion->get_component<UnitComponent>()->render_individuals_per_unit_override = 1;

  Game::Systems::RpgCombat::refresh_commander_engagement(&world, commander->get_id());

  auto* engagement = commander->get_component<RpgEngagementComponent>();
  ASSERT_NE(engagement, nullptr);
  EXPECT_EQ(engagement->fight_context, FightContext::Duel);
}

TEST_F(RpgEngagementSystemTest, FormationOpponentInRingMakesASkirmish) {
  auto* commander = create_unit(world, 0.0F, 0.0F, 1);
  create_unit(world, 0.0F, 2.0F, 2);
  auto* formation = create_unit(world, 1.5F, 2.0F, 2);
  formation->get_component<UnitComponent>()->render_individuals_per_unit_override = 9;

  Game::Systems::RpgCombat::refresh_commander_engagement(&world, commander->get_id());

  auto* engagement = commander->get_component<RpgEngagementComponent>();
  ASSERT_NE(engagement, nullptr);
  EXPECT_EQ(engagement->fight_context, FightContext::Skirmish);
}

TEST_F(RpgEngagementSystemTest, FrontAttackerRoleIsSticky) {
  auto* commander = create_unit(world, 0.0F, 0.0F, 1);
  commander->get_component<TransformComponent>()->rotation.y = 0.0F;
  auto* incumbent = create_unit(world, 0.0F, 2.0F, 2);

  Game::Systems::RpgCombat::refresh_commander_engagement(&world, commander->get_id());
  auto* engagement = commander->get_component<RpgEngagementComponent>();
  ASSERT_NE(engagement, nullptr);
  ASSERT_EQ(engagement->front_attacker_id, incumbent->get_id());

  create_unit(world, 0.4F, 1.2F, 2);
  Game::Systems::RpgCombat::refresh_commander_engagement(&world, commander->get_id());

  EXPECT_EQ(engagement->front_attacker_id, incumbent->get_id());
}

TEST_F(RpgEngagementSystemTest, FrontAttackerReassignsWhenIncumbentLeavesSector) {
  auto* commander = create_unit(world, 0.0F, 0.0F, 1);
  commander->get_component<TransformComponent>()->rotation.y = 0.0F;
  auto* incumbent = create_unit(world, 0.0F, 2.0F, 2);
  auto* challenger = create_unit(world, 0.2F, 1.5F, 2);

  Game::Systems::RpgCombat::refresh_commander_engagement(&world, commander->get_id());
  auto* engagement = commander->get_component<RpgEngagementComponent>();
  ASSERT_NE(engagement, nullptr);

  auto* first_front = world.get_entity(engagement->front_attacker_id);
  ASSERT_NE(first_front, nullptr);
  auto* moved = first_front->get_component<TransformComponent>();
  moved->position.x = 0.0F;
  moved->position.z = -2.5F;

  Game::Systems::RpgCombat::refresh_commander_engagement(&world, commander->get_id());
  EXPECT_NE(engagement->front_attacker_id, first_front->get_id());
  EXPECT_NE(engagement->front_attacker_id, 0U);
  (void)incumbent;
  (void)challenger;
}

TEST_F(RpgEngagementSystemTest, FormationOpponentsAreNeverDraggedByTheRing) {
  auto* commander = create_unit(world, 0.0F, 0.0F, 1);
  commander->get_component<TransformComponent>()->rotation.y = 0.0F;

  auto* formation = create_unit(world, -2.0F, 1.0F, 2);
  formation->get_component<UnitComponent>()->render_individuals_per_unit_override = 9;
  auto* formation_tf = formation->get_component<TransformComponent>();
  formation_tf->rotation.y = 123.0F;

  auto* single = create_unit(world, 0.0F, 2.0F, 2);
  single->get_component<UnitComponent>()->render_individuals_per_unit_override = 1;
  float const engage_distance =
      Game::Systems::RpgCombat::ideal_engage_distance(*single, *commander);
  single->get_component<TransformComponent>()->position.z = engage_distance;

  Game::Systems::RpgCombat::tick_rpg_combat(&world, commander->get_id(), 0.016F);

  EXPECT_FLOAT_EQ(formation_tf->rotation.y, 123.0F);
  EXPECT_FALSE(formation_tf->has_desired_yaw);
  auto* formation_movement = formation->get_component<MovementComponent>();
  EXPECT_TRUE(formation_movement == nullptr || !formation_movement->get_has_target());

  auto* single_tf = single->get_component<TransformComponent>();
  EXPECT_TRUE(single_tf->has_desired_yaw);
}

TEST_F(RpgEngagementSystemTest, IdealEngageDistanceScalesWithWeaponReach) {
  auto* commander = create_unit(world, 0.0F, 0.0F, 1);

  auto* swordsman = create_unit(world, 0.0F, 3.0F, 2);
  auto* sword_attack = swordsman->add_component<AttackComponent>();
  sword_attack->current_mode = AttackComponent::CombatMode::Melee;
  sword_attack->melee_range = 1.6F;

  auto* spearman = create_unit(world, 1.0F, 3.0F, 2);
  auto* spear_attack = spearman->add_component<AttackComponent>();
  spear_attack->current_mode = AttackComponent::CombatMode::Melee;
  spear_attack->melee_range = 2.5F;

  float const sword_distance =
      Game::Systems::RpgCombat::ideal_engage_distance(*swordsman, *commander);
  float const spear_distance =
      Game::Systems::RpgCombat::ideal_engage_distance(*spearman, *commander);

  EXPECT_GT(spear_distance, sword_distance);
  EXPECT_GT(spear_distance - sword_distance, 0.8F);

  float const commander_radius = 0.5F;
  EXPECT_LE(sword_distance, sword_attack->melee_range + commander_radius);
  EXPECT_LE(spear_distance, spear_attack->melee_range + commander_radius);
}
