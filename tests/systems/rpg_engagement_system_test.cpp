#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <vector>

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

TEST_F(RpgEngagementSystemTest, PressureIsLimitedByRoomNotByNamedRoles) {
  auto* commander = create_unit(world, 0.0F, 0.0F, 1);
  auto* commander_transform = commander->get_component<TransformComponent>();
  ASSERT_NE(commander_transform, nullptr);
  commander_transform->rotation.y = 0.0F;

  create_unit(world, 0.0F, 2.0F, 2);
  create_unit(world, -1.8F, 1.4F, 2);
  create_unit(world, 1.8F, 1.4F, 2);
  create_unit(world, -0.8F, 2.4F, 2);
  create_unit(world, 0.8F, 2.4F, 2);
  create_unit(world, 0.0F, 4.0F, 2);

  Game::Systems::RpgCombat::refresh_commander_engagement(&world, commander->get_id());

  auto* engagement = commander->get_component<RpgEngagementComponent>();
  ASSERT_NE(engagement, nullptr);
  ASSERT_EQ(engagement->engagement_slots.size(), 6U);

  int pressing = 0;
  for (auto const& slot : engagement->engagement_slots) {
    if (slot.pressing) {
      ++pressing;
      EXPECT_FALSE(slot.obstructed)
          << "a fighter with a friendly in the way cannot press";
    }
  }
  EXPECT_EQ(pressing, engagement->pressing_count());
  EXPECT_GT(pressing, 0);
  EXPECT_LE(pressing, 4) << "the crowd limits itself; it does not queue behind "
                            "a fixed cast of attackers";
  EXPECT_LT(pressing, static_cast<int>(engagement->engagement_slots.size()));
}

TEST_F(RpgEngagementSystemTest, AFighterBehindAFriendDoesNotPress) {
  auto* commander = create_unit(world, 0.0F, 0.0F, 1);
  commander->get_component<TransformComponent>()->rotation.y = 0.0F;

  create_unit(world, 0.0F, 1.8F, 2);
  auto* screened = create_unit(world, 0.0F, 3.2F, 2);

  Game::Systems::RpgCombat::refresh_commander_engagement(&world, commander->get_id());

  auto* engagement = commander->get_component<RpgEngagementComponent>();
  ASSERT_NE(engagement, nullptr);
  for (auto const& slot : engagement->engagement_slots) {
    if (slot.entity_id == screened->get_id()) {
      EXPECT_TRUE(slot.obstructed);
      EXPECT_FALSE(slot.pressing);
    }
  }
}

TEST_F(RpgEngagementSystemTest, WaitingFightersDoNotShareOneRadius) {
  auto* commander = create_unit(world, 0.0F, 0.0F, 1);
  commander->get_component<TransformComponent>()->rotation.y = 0.0F;

  std::vector<Entity*> crowd;
  for (int i = 0; i < 6; ++i) {
    float const angle = static_cast<float>(i) * 1.05F;
    crowd.push_back(
        create_unit(world,
                    std::sin(angle) * (2.6F + (0.25F * static_cast<float>(i))),
                    std::cos(angle) * (2.6F + (0.25F * static_cast<float>(i))),
                    2));
    crowd.back()->get_component<UnitComponent>()->render_individuals_per_unit_override =
        1;
  }

  for (int tick = 0; tick < 40; ++tick) {
    Game::Systems::RpgCombat::tick_rpg_combat(&world, commander->get_id(), 0.05F);
  }

  std::vector<float> distances;
  for (auto* fighter : crowd) {
    auto const* transform = fighter->get_component<TransformComponent>();
    distances.push_back(std::hypot(transform->position.x, transform->position.z));
  }
  auto const [min_it, max_it] = std::minmax_element(distances.begin(), distances.end());
  EXPECT_GT(*max_it - *min_it, 0.35F)
      << "every fighter settled on the same radius, which is a visible circle";
}

TEST_F(RpgEngagementSystemTest, EnemiesTurnAtABodysRateNotInstantly) {
  auto* commander = create_unit(world, 0.0F, 0.0F, 1);
  auto* enemy = create_unit(world, 0.0F, 2.0F, 2);
  enemy->get_component<UnitComponent>()->render_individuals_per_unit_override = 1;

  auto* enemy_transform = enemy->get_component<TransformComponent>();
  enemy_transform->rotation.y = 0.0F;
  enemy_transform->desired_yaw = 0.0F;
  enemy_transform->has_desired_yaw = true;

  Game::Systems::RpgCombat::tick_rpg_combat(&world, commander->get_id(), 0.016F);

  ASSERT_TRUE(enemy_transform->has_desired_yaw);
  float const turned =
      std::abs(std::fmod(enemy_transform->desired_yaw + 540.0F, 360.0F) - 180.0F);
  EXPECT_GT(turned, 0.0F) << "the body did not begin to come round at all";
  EXPECT_LT(turned, 10.0F)
      << "the body snapped a half turn in one frame instead of turning at a rate";
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
  EXPECT_TRUE(engagement->engagement_slots.front().pressing);
  EXPECT_EQ(engagement->pressing_count(), 1);
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

TEST_F(RpgEngagementSystemTest, AFighterAlreadyPressingKeepsTheFront) {
  auto* commander = create_unit(world, 0.0F, 0.0F, 1);
  commander->get_component<TransformComponent>()->rotation.y = 0.0F;
  auto* incumbent = create_unit(world, 0.0F, 2.0F, 2);

  Game::Systems::RpgCombat::refresh_commander_engagement(&world, commander->get_id());
  auto* engagement = commander->get_component<RpgEngagementComponent>();
  ASSERT_NE(engagement, nullptr);
  ASSERT_TRUE(engagement->is_pressing(incumbent->get_id()));

  create_unit(world, 0.4F, 1.2F, 2);
  Game::Systems::RpgCombat::refresh_commander_engagement(&world, commander->get_id());

  EXPECT_TRUE(engagement->is_pressing(incumbent->get_id()))
      << "pressure that changes hands every frame reads as flicker, not as a fight";
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
