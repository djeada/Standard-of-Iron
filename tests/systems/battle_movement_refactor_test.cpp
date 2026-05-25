#include <gtest/gtest.h>

#include "core/component.h"
#include "core/entity.h"
#include "core/world.h"
#include "systems/command_service.h"
#include "systems/engagement_slot_system.h"
#include "systems/local_avoidance_system.h"
#include "systems/target_commitment_system.h"
#include "systems/cohort_system.h"

using namespace Engine::Core;
using namespace Game::Systems;

class LocalAvoidanceTest : public ::testing::Test {
protected:
  void SetUp() override {
    world = std::make_unique<World>();
    CommandService::initialize(64, 64);
  }

  void TearDown() override { world.reset(); }

  std::unique_ptr<World> world;
};

TEST_F(LocalAvoidanceTest, LargeGroupMaintainsSpacing) {
  // Create a group of 10 units at the same position.
  std::vector<Entity*> units;
  for (int i = 0; i < 10; ++i) {
    auto* entity = world->create_entity();
    entity->add_component<TransformComponent>(5.0F, 0.0F, 5.0F);
    auto* unit = entity->add_component<UnitComponent>(100, 100, 2.0F, 12.0F);
    unit->owner_id = 1;
    auto* movement = entity->add_component<MovementComponent>();
    movement->has_target = true;
    movement->target_x = 20.0F;
    movement->target_y = 5.0F;
    movement->vx = 1.0F;
    movement->vz = 0.0F;
    units.push_back(entity);
  }

  LocalAvoidanceSystem system;

  // Run several ticks of avoidance.
  for (int tick = 0; tick < 20; ++tick) {
    system.update(world.get(), 0.1F);
  }

  // Check that units have spread out - minimum pair distance > 0.
  float min_dist_sq = 1e9F;
  for (std::size_t i = 0; i < units.size(); ++i) {
    auto* ti = units[i]->get_component<TransformComponent>();
    for (std::size_t j = i + 1; j < units.size(); ++j) {
      auto* tj = units[j]->get_component<TransformComponent>();
      float dx = ti->position.x - tj->position.x;
      float dz = ti->position.z - tj->position.z;
      min_dist_sq = std::min(min_dist_sq, dx * dx + dz * dz);
    }
  }

  // Units should have separated from identical positions.
  EXPECT_GT(min_dist_sq, 0.01F);
}

TEST_F(LocalAvoidanceTest, StationaryUnitsNotDisplaced) {
  // Stationary units (no target) should not be moved by avoidance.
  auto* stationary = world->create_entity();
  stationary->add_component<TransformComponent>(10.0F, 0.0F, 10.0F);
  auto* unit = stationary->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  unit->owner_id = 1;
  stationary->add_component<MovementComponent>(); // no target

  auto* mover = world->create_entity();
  mover->add_component<TransformComponent>(10.1F, 0.0F, 10.1F);
  auto* mover_unit = mover->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  mover_unit->owner_id = 1;
  auto* movement = mover->add_component<MovementComponent>();
  movement->has_target = true;
  movement->vx = 1.0F;

  auto* transform = stationary->get_component<TransformComponent>();
  float const orig_x = transform->position.x;
  float const orig_z = transform->position.z;

  LocalAvoidanceSystem system;
  system.update(world.get(), 0.1F);

  // Stationary unit should not move.
  EXPECT_FLOAT_EQ(transform->position.x, orig_x);
  EXPECT_FLOAT_EQ(transform->position.z, orig_z);
}

TEST_F(LocalAvoidanceTest, DiagnosticsReported) {
  auto* entity = world->create_entity();
  entity->add_component<TransformComponent>(5.0F, 0.0F, 5.0F);
  auto* unit = entity->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  unit->owner_id = 1;
  auto* movement = entity->add_component<MovementComponent>();
  movement->has_target = true;

  LocalAvoidanceSystem system;
  system.update(world.get(), 0.016F);

  EXPECT_GE(system.diagnostics().units_processed, 1U);
}

// --- Engagement Slot Tests ---

class EngagementSlotTest : public ::testing::Test {
protected:
  void SetUp() override {
    world = std::make_unique<World>();
    CommandService::initialize(64, 64);
  }

  void TearDown() override { world.reset(); }

  std::unique_ptr<World> world;
};

TEST_F(EngagementSlotTest, MeleeAttackersGetDistinctSlots) {
  // Create a target.
  auto* target = world->create_entity();
  target->add_component<TransformComponent>(10.0F, 0.0F, 10.0F);
  auto* target_unit = target->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  target_unit->owner_id = 2;

  // Create multiple attackers targeting the same entity.
  std::vector<Entity*> attackers;
  for (int i = 0; i < 5; ++i) {
    auto* attacker = world->create_entity();
    attacker->add_component<TransformComponent>(
        8.0F + static_cast<float>(i) * 0.5F, 0.0F, 10.0F);
    auto* unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
    unit->owner_id = 1;
    auto* atk = attacker->add_component<AttackComponent>(2.0F, 10, 1.0F);
    atk->in_melee_lock = true;
    atk->current_mode = AttackComponent::CombatMode::Melee;
    auto* attack_target = attacker->add_component<AttackTargetComponent>();
    attack_target->target_id = target->get_id();
    attackers.push_back(attacker);
  }

  EngagementSlotSystem system;
  system.update(world.get(), 0.016F);

  // Each attacker should have a unique slot index.
  std::set<std::uint8_t> slot_indices;
  for (auto* attacker : attackers) {
    auto* slot = attacker->get_component<EngagementSlotComponent>();
    ASSERT_NE(slot, nullptr);
    EXPECT_TRUE(slot->valid);
    EXPECT_EQ(slot->target_id, target->get_id());
    slot_indices.insert(slot->slot_index);
  }

  // All slots should be distinct.
  EXPECT_EQ(slot_indices.size(), attackers.size());
}

TEST_F(EngagementSlotTest, OverflowHandled) {
  auto* target = world->create_entity();
  target->add_component<TransformComponent>(10.0F, 0.0F, 10.0F);
  auto* target_unit = target->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  target_unit->owner_id = 2;

  // Create more attackers than max slots.
  for (int i = 0; i < 12; ++i) {
    auto* attacker = world->create_entity();
    attacker->add_component<TransformComponent>(8.0F, 0.0F, 10.0F);
    auto* unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
    unit->owner_id = 1;
    auto* atk = attacker->add_component<AttackComponent>(2.0F, 10, 1.0F);
    atk->in_melee_lock = true;
    atk->current_mode = AttackComponent::CombatMode::Melee;
    auto* attack_target = attacker->add_component<AttackTargetComponent>();
    attack_target->target_id = target->get_id();
  }

  EngagementSlotSystem system;
  system.update(world.get(), 0.016F);

  EXPECT_GT(system.diagnostics().overflow_redirects, 0U);
}

// --- Target Commitment Tests ---

class TargetCommitmentTest : public ::testing::Test {
protected:
  void SetUp() override {
    world = std::make_unique<World>();
    CommandService::initialize(64, 64);
  }

  void TearDown() override { world.reset(); }

  std::unique_ptr<World> world;
};

TEST_F(TargetCommitmentTest, CannotSwitchDuringWindUp) {
  auto* enemy1 = world->create_entity();
  enemy1->add_component<TransformComponent>(2.0F, 0.0F, 0.0F);
  auto* enemy1_unit = enemy1->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy1_unit->owner_id = 2;

  auto* enemy2 = world->create_entity();
  enemy2->add_component<TransformComponent>(3.0F, 0.0F, 0.0F);
  auto* enemy2_unit = enemy2->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy2_unit->owner_id = 2;

  auto* attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  unit->owner_id = 1;
  auto* atk = attacker->add_component<AttackComponent>(2.0F, 10, 1.0F);
  atk->in_melee_lock = true;
  auto* attack_target = attacker->add_component<AttackTargetComponent>();
  attack_target->target_id = enemy1->get_id();

  // Set combat state to WindUp.
  auto* combat_state = attacker->add_component<CombatStateComponent>();
  combat_state->animation_state = CombatAnimationState::WindUp;

  TargetCommitmentSystem system;

  // First tick: establishes commitment.
  system.update(world.get(), 0.016F);

  // Try to switch target.
  attack_target->target_id = enemy2->get_id();
  system.update(world.get(), 0.016F);

  // Should be blocked - target remains enemy1.
  EXPECT_EQ(attack_target->target_id, enemy1->get_id());
  EXPECT_GT(system.diagnostics().switches_blocked, 0U);
}

TEST_F(TargetCommitmentTest, CanSwitchAfterRecovery) {
  auto* enemy1 = world->create_entity();
  enemy1->add_component<TransformComponent>(2.0F, 0.0F, 0.0F);
  auto* enemy1_unit = enemy1->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy1_unit->owner_id = 2;

  auto* enemy2 = world->create_entity();
  enemy2->add_component<TransformComponent>(3.0F, 0.0F, 0.0F);
  auto* enemy2_unit = enemy2->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy2_unit->owner_id = 2;

  auto* attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  unit->owner_id = 1;
  auto* atk = attacker->add_component<AttackComponent>(2.0F, 10, 1.0F);
  atk->in_melee_lock = true;
  auto* attack_target = attacker->add_component<AttackTargetComponent>();
  attack_target->target_id = enemy1->get_id();

  auto* combat_state = attacker->add_component<CombatStateComponent>();
  combat_state->animation_state = CombatAnimationState::Idle;

  TargetCommitmentSystem system;
  system.update(world.get(), 0.016F);

  // Exhaust cooldown.
  for (int i = 0; i < 60; ++i) {
    system.update(world.get(), 0.016F);
  }

  // Now switch target.
  attack_target->target_id = enemy2->get_id();
  system.update(world.get(), 0.016F);

  EXPECT_EQ(attack_target->target_id, enemy2->get_id());
}

TEST_F(TargetCommitmentTest, ForcedReleaseOnTargetDeath) {
  auto* enemy = world->create_entity();
  enemy->add_component<TransformComponent>(2.0F, 0.0F, 0.0F);
  auto* enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;

  auto* attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  unit->owner_id = 1;
  auto* atk = attacker->add_component<AttackComponent>(2.0F, 10, 1.0F);
  atk->in_melee_lock = true;
  auto* attack_target = attacker->add_component<AttackTargetComponent>();
  attack_target->target_id = enemy->get_id();

  auto* combat_state = attacker->add_component<CombatStateComponent>();
  combat_state->animation_state = CombatAnimationState::Strike;

  TargetCommitmentSystem system;
  system.update(world.get(), 0.016F);

  // Kill the target.
  enemy_unit->health = 0;
  system.update(world.get(), 0.016F);

  auto* commitment =
      attacker->get_component<TargetCommitmentComponent>();
  ASSERT_NE(commitment, nullptr);
  EXPECT_EQ(commitment->committed_target_id, static_cast<EntityID>(0));
  EXPECT_GT(system.diagnostics().forced_releases, 0U);
}

// --- Cohort System Tests ---

class CohortSystemTest : public ::testing::Test {
protected:
  void SetUp() override {
    world = std::make_unique<World>();
    CommandService::initialize(64, 64);
  }

  void TearDown() override { world.reset(); }

  std::unique_ptr<World> world;
};

TEST_F(CohortSystemTest, NearbyUnitsFormCohort) {
  // Create a group of nearby idle units.
  for (int i = 0; i < 4; ++i) {
    auto* entity = world->create_entity();
    entity->add_component<TransformComponent>(
        10.0F + static_cast<float>(i), 0.0F, 10.0F);
    auto* unit = entity->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
    unit->owner_id = 1;
    entity->add_component<MovementComponent>();
  }

  CohortSystem system;
  system.update(world.get(), 0.016F);

  EXPECT_GE(system.diagnostics().cohorts_formed, 1U);
  EXPECT_GE(system.diagnostics().units_in_cohorts, 4U);
}

TEST_F(CohortSystemTest, CohortActivatesWhenOneDetectsEnemy) {
  std::vector<Entity*> units;
  for (int i = 0; i < 4; ++i) {
    auto* entity = world->create_entity();
    entity->add_component<TransformComponent>(
        10.0F + static_cast<float>(i), 0.0F, 10.0F);
    auto* unit = entity->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
    unit->owner_id = 1;
    entity->add_component<MovementComponent>();
    units.push_back(entity);
  }

  CohortSystem system;
  // First tick forms cohorts.
  system.update(world.get(), 0.016F);

  // Give one unit an attack target (simulating enemy detection).
  auto* attack_target = units[0]->add_component<AttackTargetComponent>();
  attack_target->target_id = 999; // doesn't matter if entity exists

  // Second tick propagates activation.
  system.update(world.get(), 0.016F);

  // All units in the cohort should be activated.
  for (auto* entity : units) {
    auto* membership =
        entity->get_component<CohortMembershipComponent>();
    ASSERT_NE(membership, nullptr);
    EXPECT_TRUE(membership->cohort_activated);
  }
}

// --- Elephant Knockback Cooldown Tests ---

TEST(ElephantKnockbackCooldownTest, CooldownPreventsRepeatedKnockback) {
  ElephantKnockbackCooldownComponent comp;

  EXPECT_FALSE(comp.is_on_cooldown(42));

  comp.add_cooldown(42);
  EXPECT_TRUE(comp.is_on_cooldown(42));

  // After ticking past cooldown duration.
  comp.tick(ElephantKnockbackCooldownComponent::k_knockback_cooldown + 0.1F);
  EXPECT_FALSE(comp.is_on_cooldown(42));
}

TEST(ElephantKnockbackCooldownTest, MultipleVictimsTracked) {
  ElephantKnockbackCooldownComponent comp;

  comp.add_cooldown(1);
  comp.add_cooldown(2);
  comp.add_cooldown(3);

  EXPECT_TRUE(comp.is_on_cooldown(1));
  EXPECT_TRUE(comp.is_on_cooldown(2));
  EXPECT_TRUE(comp.is_on_cooldown(3));
  EXPECT_FALSE(comp.is_on_cooldown(4));
}

// --- Movement Intent Component Tests ---

TEST(MovementIntentTest, DefaultValues) {
  MovementIntentComponent intent;
  EXPECT_FLOAT_EQ(intent.desired_vx, 0.0F);
  EXPECT_FLOAT_EQ(intent.desired_vz, 0.0F);
  EXPECT_FLOAT_EQ(intent.knockback_dx, 0.0F);
  EXPECT_FLOAT_EQ(intent.knockback_dz, 0.0F);
  EXPECT_FALSE(intent.has_facing_request);
  EXPECT_EQ(intent.priority, 0);
}
