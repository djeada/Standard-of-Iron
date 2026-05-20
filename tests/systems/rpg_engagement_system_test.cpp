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
