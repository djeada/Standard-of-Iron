#include <cmath>
#include <gtest/gtest.h>
#include <set>
#include <utility>
#include <vector>

#include "core/component.h"
#include "core/entity.h"
#include "core/world.h"
#include "systems/body_contact_system.h"
#include "systems/cohort_system.h"
#include "systems/combat_system/elephant_special_processor.h"
#include "systems/engagement_slot_system.h"
#include "systems/local_avoidance_system.h"
#include "systems/nav_grid.h"
#include "systems/target_commitment_system.h"
#include "tests/support/movement_test_access.h"

using namespace Engine::Core;
using namespace Game::Systems;

class LocalAvoidanceTest : public ::testing::Test {
protected:
  void SetUp() override {
    world = std::make_unique<World>();
    NavGrid::initialize(64, 64);
  }

  void TearDown() override { world.reset(); }

  std::unique_ptr<World> world;
};

TEST_F(LocalAvoidanceTest, LargeGroupProducesSteeringWithoutTeleportingUnits) {

  std::vector<Entity*> units;
  constexpr int k_crowded_unit_count = 256;
  for (int i = 0; i < k_crowded_unit_count; ++i) {
    auto* entity = world->create_entity();

    entity->add_component<TransformComponent>(5.0F + static_cast<float>(i % 16) * 0.8F,
                                              0.0F,
                                              5.0F + static_cast<float>(i / 16) * 0.8F);
    auto* unit = entity->add_component<UnitComponent>(100, 100, 2.0F, 12.0F);
    unit->owner_id = 1;
    auto* movement = entity->add_component<MovementComponent>();
    MovementTestAccess::set_has_target(*movement, true);
    MovementTestAccess::set_target_x(*movement, 20.0F);
    MovementTestAccess::set_target_y(*movement, 5.0F);
    MovementTestAccess::set_vx(*movement, 1.0F);
    MovementTestAccess::set_vz(*movement, 0.0F);

    auto* facts = entity->add_component<MovementFactsComponent>();
    facts->desired.valid = true;
    facts->desired.velocity_x = 1.0F;
    facts->desired.velocity_z = 0.0F;
    units.push_back(entity);
  }

  LocalAvoidanceSystem system;

  std::vector<std::pair<float, float>> original_positions;
  original_positions.reserve(units.size());
  for (auto* entity : units) {
    auto* transform = entity->get_component<TransformComponent>();
    original_positions.emplace_back(transform->position.x, transform->position.z);
  }

  system.update(world.get(), 0.1F);

  for (std::size_t i = 0; i < units.size(); ++i) {
    auto* transform = units[i]->get_component<TransformComponent>();
    EXPECT_FLOAT_EQ(transform->position.x, original_positions[i].first);
    EXPECT_FLOAT_EQ(transform->position.z, original_positions[i].second);
  }

  EXPECT_TRUE(std::any_of(units.begin(), units.end(), [](const Entity* entity) {
    const auto* facts = entity->get_component<MovementFactsComponent>();
    return facts != nullptr && facts->steering.valid &&
           (std::abs(facts->steering.correction_x) > 0.001F ||
            std::abs(facts->steering.correction_z) > 0.001F);
  }));
  for (const auto* entity : units) {
    const auto* facts = entity->get_component<MovementFactsComponent>();
    ASSERT_NE(facts, nullptr);
    ASSERT_TRUE(facts->steering.valid);
    const float steered =
        std::hypot(facts->steering.velocity_x, facts->steering.velocity_z);
    EXPECT_GE(steered,
              Game::Systems::LocalAvoidanceSystem::k_min_speed_fraction - 1.0e-3F)
        << "traffic brought a body to a standstill";
    EXPECT_LE(steered, 1.0F + 1.0e-3F) << "steering handed out free speed";
  }
  for (const auto* entity : units) {
    const auto* movement = entity->get_component<MovementComponent>();
    ASSERT_NE(movement, nullptr);
    EXPECT_FLOAT_EQ(movement->get_vx(), 1.0F);
    EXPECT_FLOAT_EQ(movement->get_vz(), 0.0F);
  }
  EXPECT_GE(system.diagnostics().units_processed, 1U);
  EXPECT_GT(system.diagnostics().units_steered, 0U);
  EXPECT_GT(system.diagnostics().average_neighbors_checked, 0U)
      << "the shared spatial index returned no neighbours at all";
}

TEST_F(LocalAvoidanceTest, ContactPassPullsStackedBodiesApart) {
  auto under_way = [](Entity* entity) {
    auto* facts = entity->add_component<MovementFactsComponent>();
    facts->desired.valid = true;
    facts->desired.velocity_x = 1.0F;
  };

  auto* left = world->create_entity();
  left->add_component<TransformComponent>(5.0F, 0.0F, 5.0F);
  left->add_component<UnitComponent>(100, 100, 2.0F, 12.0F)->owner_id = 1;
  left->add_component<MovementComponent>();
  under_way(left);

  auto* right = world->create_entity();
  right->add_component<TransformComponent>(5.05F, 0.0F, 5.0F);
  right->add_component<UnitComponent>(100, 100, 2.0F, 12.0F)->owner_id = 1;
  right->add_component<MovementComponent>();
  under_way(right);

  const auto* left_transform = left->get_component<TransformComponent>();
  const auto* right_transform = right->get_component<TransformComponent>();
  const float before =
      std::hypot(left_transform->position.x - right_transform->position.x,
                 left_transform->position.z - right_transform->position.z);

  BodyContactSystem contact;
  contact.update(world.get(), 1.0F / 60.0F);

  const float after =
      std::hypot(left_transform->position.x - right_transform->position.x,
                 left_transform->position.z - right_transform->position.z);

  EXPECT_GT(after, before) << "stacked bodies were left inside one another";
  EXPECT_GT(contact.diagnostics().pairs_resolved, 0U);
  EXPECT_GT(contact.diagnostics().deepest_overlap, 0.0F);
}

TEST_F(LocalAvoidanceTest, ContactPassDoesNotShoveAHoldingBody) {
  auto* holder = world->create_entity();
  holder->add_component<TransformComponent>(8.0F, 0.0F, 8.0F);
  holder->add_component<UnitComponent>(100, 100, 2.0F, 12.0F)->owner_id = 1;
  holder->add_component<MovementComponent>();
  holder->add_component<HoldModeComponent>()->active = true;
  auto* holder_facts = holder->add_component<MovementFactsComponent>();
  holder_facts->desired.valid = true;

  auto* walker = world->create_entity();
  walker->add_component<TransformComponent>(8.05F, 0.0F, 8.0F);
  walker->add_component<UnitComponent>(100, 100, 2.0F, 12.0F)->owner_id = 1;
  walker->add_component<MovementComponent>();
  auto* walker_facts = walker->add_component<MovementFactsComponent>();
  walker_facts->desired.valid = true;
  walker_facts->desired.velocity_x = 1.0F;

  const auto* holder_transform = holder->get_component<TransformComponent>();
  const float holder_x = holder_transform->position.x;
  const float holder_z = holder_transform->position.z;

  BodyContactSystem contact;
  contact.update(world.get(), 1.0F / 60.0F);

  EXPECT_FLOAT_EQ(holder_transform->position.x, holder_x);
  EXPECT_FLOAT_EQ(holder_transform->position.z, holder_z);
  EXPECT_GT(std::abs(walker->get_component<TransformComponent>()->position.x - 8.05F),
            0.0F);
}

TEST_F(LocalAvoidanceTest, StationaryUnitsNotDisplaced) {

  auto* stationary = world->create_entity();
  stationary->add_component<TransformComponent>(10.0F, 0.0F, 10.0F);
  auto* unit = stationary->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  unit->owner_id = 1;
  stationary->add_component<MovementComponent>();

  auto* mover = world->create_entity();
  mover->add_component<TransformComponent>(10.1F, 0.0F, 10.1F);
  auto* mover_unit = mover->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  mover_unit->owner_id = 1;
  auto* movement = mover->add_component<MovementComponent>();
  MovementTestAccess::set_has_target(*movement, true);
  MovementTestAccess::set_vx(*movement, 1.0F);

  auto* transform = stationary->get_component<TransformComponent>();
  float const orig_x = transform->position.x;
  float const orig_z = transform->position.z;

  LocalAvoidanceSystem system;
  system.update(world.get(), 0.1F);

  EXPECT_FLOAT_EQ(transform->position.x, orig_x);
  EXPECT_FLOAT_EQ(transform->position.z, orig_z);
}

TEST_F(LocalAvoidanceTest, DiagnosticsReported) {
  auto* entity = world->create_entity();
  entity->add_component<TransformComponent>(5.0F, 0.0F, 5.0F);
  auto* unit = entity->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  unit->owner_id = 1;
  auto* movement = entity->add_component<MovementComponent>();
  MovementTestAccess::set_has_target(*movement, true);

  LocalAvoidanceSystem system;
  system.update(world.get(), 0.016F);

  EXPECT_GE(system.diagnostics().units_processed, 1U);
}

class EngagementSlotTest : public ::testing::Test {
protected:
  void SetUp() override {
    world = std::make_unique<World>();
    NavGrid::initialize(64, 64);
  }

  void TearDown() override { world.reset(); }

  std::unique_ptr<World> world;
};

TEST_F(EngagementSlotTest, MeleeAttackersGetDistinctSlots) {

  auto* target = world->create_entity();
  target->add_component<TransformComponent>(10.0F, 0.0F, 10.0F);
  auto* target_unit = target->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  target_unit->owner_id = 2;

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

  std::set<std::uint8_t> slot_indices;
  for (auto* attacker : attackers) {
    auto* slot = attacker->get_component<EngagementSlotComponent>();
    ASSERT_NE(slot, nullptr);
    EXPECT_TRUE(slot->valid);
    EXPECT_EQ(slot->target_id, target->get_id());
    slot_indices.insert(slot->slot_index);
  }

  EXPECT_EQ(slot_indices.size(), attackers.size());
}

TEST_F(EngagementSlotTest, OverflowHandled) {
  auto* target = world->create_entity();
  target->add_component<TransformComponent>(10.0F, 0.0F, 10.0F);
  auto* target_unit = target->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  target_unit->owner_id = 2;

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

class TargetCommitmentTest : public ::testing::Test {
protected:
  void SetUp() override {
    world = std::make_unique<World>();
    NavGrid::initialize(64, 64);
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

  auto* combat_state = attacker->add_component<CombatStateComponent>();
  combat_state->animation_state = CombatAnimationState::WindUp;

  TargetCommitmentSystem system;

  system.update(world.get(), 0.016F);

  attack_target->target_id = enemy2->get_id();
  system.update(world.get(), 0.016F);

  EXPECT_EQ(attack_target->target_id, enemy1->get_id());
  EXPECT_GT(system.diagnostics().switches_blocked, 0U);
}

TEST_F(TargetCommitmentTest, BlockedSwitchReleasesConflictingReciprocalLock) {
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
  atk->melee_lock_target_id = enemy1->get_id();
  auto* attack_target = attacker->add_component<AttackTargetComponent>();
  attack_target->target_id = enemy1->get_id();

  TargetCommitmentSystem system;
  system.update(world.get(), 0.016F);

  attack_target->target_id = enemy2->get_id();
  attack_target->should_chase = false;
  atk->in_melee_lock = true;
  atk->melee_lock_target_id = enemy2->get_id();
  system.update(world.get(), 0.0F);

  EXPECT_EQ(attack_target->target_id, enemy1->get_id());
  EXPECT_TRUE(attack_target->should_chase);
  EXPECT_FALSE(atk->in_melee_lock);
  EXPECT_EQ(atk->melee_lock_target_id, static_cast<EntityID>(0));
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

  for (int i = 0; i < 60; ++i) {
    system.update(world.get(), 0.016F);
  }

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

  enemy_unit->health = 0;
  system.update(world.get(), 0.016F);

  auto* commitment = attacker->get_component<TargetCommitmentComponent>();
  ASSERT_NE(commitment, nullptr);
  EXPECT_EQ(commitment->committed_target_id, static_cast<EntityID>(0));
  EXPECT_GT(system.diagnostics().forced_releases, 0U);
}

class CohortSystemTest : public ::testing::Test {
protected:
  void SetUp() override {
    world = std::make_unique<World>();
    NavGrid::initialize(64, 64);
  }

  void TearDown() override { world.reset(); }

  std::unique_ptr<World> world;
};

TEST_F(CohortSystemTest, NearbyUnitsFormCohort) {

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

  system.update(world.get(), 0.016F);

  auto* attack_target = units[0]->add_component<AttackTargetComponent>();
  attack_target->target_id = 999;

  system.update(world.get(), 0.016F);

  for (auto* entity : units) {
    auto* membership = entity->get_component<CohortMembershipComponent>();
    ASSERT_NE(membership, nullptr);
    EXPECT_TRUE(membership->cohort_activated);
  }
}

TEST(ElephantKnockbackCooldownTest, CooldownPreventsRepeatedKnockback) {
  ElephantKnockbackCooldownComponent comp;

  EXPECT_FALSE(comp.is_on_cooldown(42));

  comp.add_cooldown(42);
  EXPECT_TRUE(comp.is_on_cooldown(42));

  Game::Systems::Combat::expire_knockback_cooldowns(
      comp, ElephantKnockbackCooldownComponent::k_knockback_cooldown + 0.1F);
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

TEST(MovementIntentTest, DefaultValues) {
  MovementIntentComponent const intent;
  EXPECT_FLOAT_EQ(intent.desired_vx, 0.0F);
  EXPECT_FLOAT_EQ(intent.desired_vz, 0.0F);
  EXPECT_FLOAT_EQ(intent.knockback_dx, 0.0F);
  EXPECT_FLOAT_EQ(intent.knockback_dz, 0.0F);
  EXPECT_FALSE(intent.has_facing_request);
  EXPECT_EQ(intent.priority, 0);
}
