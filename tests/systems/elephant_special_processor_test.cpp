#include <gtest/gtest.h>

#include "core/component.h"
#include "core/world.h"
#include "systems/combat_system/combat_utils.h"
#include "systems/combat_system/elephant_special_processor.h"
#include "systems/owner_registry.h"
#include "units/spawn_type.h"

using namespace Engine::Core;
using namespace Game::Systems;

namespace {

auto add_unit(World& world,
              Game::Units::SpawnType spawn_type,
              int owner_id,
              float x,
              float z,
              int health = 100) -> Entity* {
  auto* entity = world.create_entity();
  entity->add_component<TransformComponent>(x, 0.0F, z);
  auto* unit = entity->add_component<UnitComponent>(health, health, 1.0F, 12.0F);
  unit->spawn_type = spawn_type;
  unit->owner_id = owner_id;
  entity->add_component<MovementComponent>();
  return entity;
}

auto add_elephant(World& world,
                  int owner_id,
                  float x = 0.0F,
                  float z = 0.0F) -> Entity* {
  auto* elephant =
      add_unit(world, Game::Units::SpawnType::Elephant, owner_id, x, z, 300);
  auto* elephant_comp = elephant->add_component<ElephantComponent>();
  elephant_comp->trample_radius = 2.5F;
  elephant_comp->trample_damage = 40;

  auto* motion = elephant->add_component<MotionPresentationComponent>();
  motion->set_state(MotionPresentationState::Run);
  return elephant;
}

} // namespace

class ElephantSpecialProcessorTest : public ::testing::Test {
protected:
  void SetUp() override {
    OwnerRegistry::instance().clear();
    world = std::make_unique<World>();
  }

  void TearDown() override { world.reset(); }

  void update(float delta_time) {
    auto query_context = Combat::build_combat_query_context(world.get());
    Combat::process_elephant_specials(world.get(), query_context, delta_time);
  }

  std::unique_ptr<World> world;
};

TEST_F(ElephantSpecialProcessorTest, PanickedElephantDoesNotDamageOwnTroops) {
  auto* elephant = add_elephant(*world, 1);
  auto* panic = elephant->add_component<ElephantPanicComponent>();
  ASSERT_NE(panic, nullptr);
  panic->duration = 5.0F;

  auto* friendly = add_unit(*world, Game::Units::SpawnType::Spearman, 1, 1.0F, 0.0F);
  auto* friendly_unit = friendly->get_component<UnitComponent>();
  ASSERT_NE(friendly_unit, nullptr);

  update(1.0F);

  EXPECT_EQ(friendly_unit->health, friendly_unit->max_health);
}

TEST_F(ElephantSpecialProcessorTest,
       PanickedElephantStillDamagesEnemiesInTrampleRadius) {
  auto* elephant = add_elephant(*world, 1);
  auto* panic = elephant->add_component<ElephantPanicComponent>();
  ASSERT_NE(panic, nullptr);
  panic->duration = 5.0F;

  auto* enemy = add_unit(*world, Game::Units::SpawnType::Spearman, 2, 1.0F, 0.0F);
  auto* enemy_unit = enemy->get_component<UnitComponent>();
  ASSERT_NE(enemy_unit, nullptr);

  update(1.0F);

  EXPECT_LT(enemy_unit->health, enemy_unit->max_health);
}

TEST_F(ElephantSpecialProcessorTest, PanicDoesNotCreateHiddenRandomMoveTarget) {
  auto* elephant = add_elephant(*world, 1);
  auto* panic = elephant->add_component<ElephantPanicComponent>();
  ASSERT_NE(panic, nullptr);
  panic->duration = 5.0F;

  auto* movement = elephant->get_component<MovementComponent>();
  ASSERT_NE(movement, nullptr);
  movement->has_target = false;
  movement->target_x = 0.0F;
  movement->target_y = 0.0F;

  update(3.0F);

  EXPECT_FALSE(movement->has_target);
  EXPECT_FLOAT_EQ(movement->target_x, 0.0F);
  EXPECT_FLOAT_EQ(movement->target_y, 0.0F);
}
