#include "core/component.h"
#include "core/entity.h"
#include "core/world.h"
#include "systems/idle_behavior_system.h"
#include "units/spawn_type.h"
#include <gtest/gtest.h>

using namespace Engine::Core;
using namespace Game::Systems;

class IdleBehaviorSystemTest : public ::testing::Test {
protected:
  void SetUp() override { world = std::make_unique<World>(); }

  void TearDown() override { world.reset(); }

  std::unique_ptr<World> world;
  IdleBehaviorSystem idle_system;
};

TEST_F(IdleBehaviorSystemTest, IdleUnitAccumulatesIdleTime) {
  auto *unit = world->create_entity();
  unit->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *unit_comp = unit->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  unit_comp->owner_id = 1;
  unit_comp->spawn_type = Game::Units::SpawnType::Archer;

  auto *movement = unit->add_component<MovementComponent>();
  movement->vx = 0.0F;
  movement->vz = 0.0F;
  movement->has_target = false;

  auto *idle = unit->add_component<IdleBehaviorComponent>();
  idle->idle_time = 0.0F;

  idle_system.update(world.get(), 1.0F);

  EXPECT_TRUE(idle->is_idle);
  EXPECT_FLOAT_EQ(idle->idle_time, 1.0F);
}

TEST_F(IdleBehaviorSystemTest, MovingUnitIsNotIdle) {
  auto *unit = world->create_entity();
  unit->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *unit_comp = unit->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  unit_comp->owner_id = 1;

  auto *movement = unit->add_component<MovementComponent>();
  movement->vx = 1.0F;
  movement->vz = 1.0F;

  auto *idle = unit->add_component<IdleBehaviorComponent>();
  idle->is_idle = true;
  idle->idle_time = 5.0F;

  idle_system.update(world.get(), 1.0F);

  EXPECT_FALSE(idle->is_idle);
  EXPECT_FLOAT_EQ(idle->idle_time, 0.0F);
}

TEST_F(IdleBehaviorSystemTest, UnitWithTargetIsNotIdle) {
  auto *unit = world->create_entity();
  unit->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *unit_comp = unit->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  unit_comp->owner_id = 1;

  auto *movement = unit->add_component<MovementComponent>();
  movement->vx = 0.0F;
  movement->vz = 0.0F;
  movement->has_target = true;

  auto *idle = unit->add_component<IdleBehaviorComponent>();

  idle_system.update(world.get(), 1.0F);

  EXPECT_FALSE(idle->is_idle);
}

TEST_F(IdleBehaviorSystemTest, UnitInCombatIsNotIdle) {
  auto *unit = world->create_entity();
  unit->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *unit_comp = unit->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  unit_comp->owner_id = 1;

  auto *movement = unit->add_component<MovementComponent>();
  movement->vx = 0.0F;
  movement->vz = 0.0F;
  movement->has_target = false;

  auto *attack_target = unit->add_component<AttackTargetComponent>();
  attack_target->target_id = 999;

  auto *idle = unit->add_component<IdleBehaviorComponent>();

  idle_system.update(world.get(), 1.0F);

  EXPECT_FALSE(idle->is_idle);
}

TEST_F(IdleBehaviorSystemTest, DeadUnitInterruptsIdle) {
  auto *unit = world->create_entity();
  unit->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *unit_comp = unit->add_component<UnitComponent>(0, 100, 1.0F, 12.0F);
  unit_comp->owner_id = 1;

  auto *idle = unit->add_component<IdleBehaviorComponent>();
  idle->is_idle = true;
  idle->idle_time = 10.0F;
  idle->ambient_idle_active = true;

  idle_system.update(world.get(), 1.0F);

  EXPECT_FALSE(idle->is_idle);
  EXPECT_FALSE(idle->ambient_idle_active);
}

TEST_F(IdleBehaviorSystemTest, MicroIdleTimerUpdates) {
  auto *unit = world->create_entity();
  unit->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *unit_comp = unit->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  unit_comp->owner_id = 1;

  auto *movement = unit->add_component<MovementComponent>();
  movement->vx = 0.0F;
  movement->vz = 0.0F;
  movement->has_target = false;

  auto *idle = unit->add_component<IdleBehaviorComponent>();
  idle->micro_idles_enabled = true;
  idle->micro_idle_timer = 0.0F;
  idle->micro_idle_interval = 2.0F;

  idle_system.update(world.get(), 0.5F);

  EXPECT_GT(idle->micro_idle_timer, 0.0F);
}

TEST_F(IdleBehaviorSystemTest, DisabledMicroIdlesRemainNone) {
  auto *unit = world->create_entity();
  unit->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *unit_comp = unit->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  unit_comp->owner_id = 1;

  auto *movement = unit->add_component<MovementComponent>();
  movement->vx = 0.0F;
  movement->vz = 0.0F;
  movement->has_target = false;

  auto *idle = unit->add_component<IdleBehaviorComponent>();
  idle->micro_idles_enabled = false;
  idle->micro_idle_timer = 10.0F;

  idle_system.update(world.get(), 1.0F);

  EXPECT_EQ(idle->current_micro_idle, IdleAnimationType::None);
}

TEST_F(IdleBehaviorSystemTest, InterruptClearsAllIdleStates) {
  IdleBehaviorComponent idle;
  idle.is_idle = true;
  idle.idle_time = 10.0F;
  idle.ambient_idle_active = true;
  idle.group_idle_active = true;
  idle.group_partner_id = 123;
  idle.current_micro_idle = IdleAnimationType::WeightShift;
  idle.current_ambient_idle = IdleAnimationType::CheckWeapon;

  idle.interrupt();

  EXPECT_FLOAT_EQ(idle.idle_time, 0.0F);
  EXPECT_FALSE(idle.ambient_idle_active);
  EXPECT_FALSE(idle.group_idle_active);
  EXPECT_EQ(idle.group_partner_id, 0);
  EXPECT_EQ(idle.current_micro_idle, IdleAnimationType::None);
  EXPECT_EQ(idle.current_ambient_idle, IdleAnimationType::None);
}

TEST_F(IdleBehaviorSystemTest, RandomOffsetInitialization) {
  IdleBehaviorComponent idle1;
  IdleBehaviorComponent idle2;

  idle1.initialize_random_offset(100);
  idle2.initialize_random_offset(200);

  // Different entity IDs should produce different offsets
  EXPECT_NE(idle1.random_offset, idle2.random_offset);
  EXPECT_NE(idle1.personality_seed, idle2.personality_seed);
}

TEST_F(IdleBehaviorSystemTest, IsPerformingIdleAnimationCheck) {
  IdleBehaviorComponent idle;

  EXPECT_FALSE(idle.is_performing_idle_animation());

  idle.current_micro_idle = IdleAnimationType::Breathing;
  EXPECT_TRUE(idle.is_performing_idle_animation());

  idle.current_micro_idle = IdleAnimationType::None;
  idle.ambient_idle_active = true;
  EXPECT_TRUE(idle.is_performing_idle_animation());

  idle.ambient_idle_active = false;
  idle.group_idle_active = true;
  EXPECT_TRUE(idle.is_performing_idle_animation());
}

TEST_F(IdleBehaviorSystemTest, AmbientIdleCooldownDecreases) {
  auto *unit = world->create_entity();
  unit->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *unit_comp = unit->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  unit_comp->owner_id = 1;

  auto *movement = unit->add_component<MovementComponent>();
  movement->vx = 0.0F;
  movement->vz = 0.0F;
  movement->has_target = false;

  auto *idle = unit->add_component<IdleBehaviorComponent>();
  idle->ambient_idles_enabled = true;
  idle->ambient_idle_cooldown = 5.0F;

  idle_system.update(world.get(), 1.0F);

  EXPECT_FLOAT_EQ(idle->ambient_idle_cooldown, 4.0F);
}

TEST_F(IdleBehaviorSystemTest, NullWorldDoesNotCrash) {
  // Should not crash when world is null
  idle_system.update(nullptr, 1.0F);
  SUCCEED();
}

TEST_F(IdleBehaviorSystemTest, EntityWithoutTransformIsSkipped) {
  auto *unit = world->create_entity();
  auto *unit_comp = unit->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  unit_comp->owner_id = 1;

  auto *idle = unit->add_component<IdleBehaviorComponent>();

  // Should not crash when transform is missing
  idle_system.update(world.get(), 1.0F);
  EXPECT_FALSE(idle->is_idle);
}
