#include <array>
#include <cmath>
#include <gtest/gtest.h>
#include <memory>
#include <numbers>

#include "core/component.h"
#include "core/entity.h"
#include "core/world.h"
#include "systems/building_collision_registry.h"
#include "systems/combat_actions/combat_action_definition.h"
#include "systems/combat_actions/weapon_trace.h"
#include "systems/combat_actions/weapon_trace_sampling.h"
#include "systems/navigation/nav_grid.h"
#include "systems/owner_registry.h"

namespace {

using Engine::Core::AttackComponent;
using Engine::Core::CommanderComponent;
using Engine::Core::Entity;
using Engine::Core::EntityID;
using Engine::Core::TransformComponent;
using Engine::Core::UnitComponent;
using Engine::Core::World;
using namespace Game::Systems::CombatActions;
using Game::Systems::NavGrid;
using Game::Systems::OwnerRegistry;

class WeaponTraceTest : public ::testing::Test {
protected:
  void SetUp() override {
    world = std::make_unique<World>();
    OwnerRegistry::instance().clear();
    NavGrid::initialize(64, 64);
  }

  void TearDown() override { world.reset(); }

  auto make_commander(float x, float z) -> Entity* {
    auto* commander = world->create_entity();
    commander->add_component<TransformComponent>(x, 0.0F, z);
    auto* unit = commander->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
    unit->owner_id = 1;
    unit->render_individuals_per_unit_override = 1;
    commander->add_component<CommanderComponent>()->fpv_controlled = true;
    auto* attack = commander->add_component<AttackComponent>();
    attack->can_melee = true;
    attack->can_ranged = false;
    attack->preferred_mode = AttackComponent::CombatMode::Melee;
    attack->current_mode = AttackComponent::CombatMode::Melee;
    attack->melee_range = 1.8F;
    attack->melee_damage = 10;
    return commander;
  }

  auto make_enemy(float x, float z) -> Entity* {
    auto* enemy = world->create_entity();
    enemy->add_component<TransformComponent>(x, 0.0F, z);
    auto* unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
    unit->render_individuals_per_unit_override = 1;
    unit->owner_id = 2;
    return enemy;
  }

  static auto sword() -> const CombatActionDefinition& {
    return *find_combat_action_definition(CombatActionId::RpgSwordSlashLeft);
  }

  std::unique_ptr<World> world;
};

TEST_F(WeaponTraceTest, TheAttackerFrameFollowsTheEntityYaw) {
  auto* commander = make_commander(2.0F, 3.0F);
  commander->get_component<TransformComponent>()->rotation.y = 90.0F;

  auto const frame = attacker_frame(*commander);
  ASSERT_TRUE(frame.valid);
  auto const ahead = to_world(frame, {0.0F, 0.5F, 1.0F});
  EXPECT_NEAR(ahead.x(), 3.0F, 1.0e-4F);
  EXPECT_NEAR(ahead.y(), 0.5F, 1.0e-4F);
  EXPECT_NEAR(ahead.z(), 3.0F, 1.0e-4F);
}

TEST_F(WeaponTraceTest, NormalizedOrFallsBackForDegenerateVectors) {
  auto const fallback = normalized_or({0.0F, 0.0F, 0.0F}, {0.0F, 2.0F, 0.0F});
  EXPECT_NEAR(fallback.y(), 1.0F, 1.0e-5F);
  auto const both_zero = normalized_or({0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 0.0F});
  EXPECT_NEAR(both_zero.z(), 1.0F, 1.0e-5F);
  EXPECT_NEAR(
      normalized_or({3.0F, 0.0F, 4.0F}, {0.0F, 1.0F, 0.0F}).length(), 1.0F, 1.0e-5F);
}

TEST_F(WeaponTraceTest, NoSegmentBeforeOrAfterTheTraceWindow) {
  auto* commander = make_commander(0.0F, 0.0F);
  EXPECT_FALSE(sample_authored_weapon_trace_segment(
                   *commander,
                   sword(),
                   {.previous_normalized_time = 0.0F, .current_normalized_time = 0.05F})
                   .valid);
  EXPECT_FALSE(sample_authored_weapon_trace_segment(
                   *commander,
                   sword(),
                   {.previous_normalized_time = 0.98F, .current_normalized_time = 1.0F})
                   .valid);
}

TEST_F(WeaponTraceTest, SegmentInsideTheWindowSitsInFrontOfTheAttacker) {
  auto* commander = make_commander(0.0F, 0.0F);
  auto const segment = sample_authored_weapon_trace_segment(
      *commander,
      sword(),
      {.previous_normalized_time = 0.44F, .current_normalized_time = 0.52F});
  ASSERT_TRUE(segment.valid);
  EXPECT_NE(segment.source, WeaponTraceSegmentSource::None);
  EXPECT_GE(segment.radius, 0.04F);
  EXPECT_GT((segment.current_tip - segment.current_base).length(), 0.1F);
}

TEST_F(WeaponTraceTest, SamplingTheSameSpanTwiceIsIdentical) {
  auto* commander = make_commander(0.0F, 0.0F);
  WeaponTraceTimeSpan const span{.previous_normalized_time = 0.44F,
                                 .current_normalized_time = 0.52F};
  auto const a = sample_authored_weapon_trace_segment(*commander, sword(), span);
  auto const b = sample_authored_weapon_trace_segment(*commander, sword(), span);
  ASSERT_TRUE(a.valid);
  EXPECT_EQ(a.current_tip, b.current_tip);
  EXPECT_EQ(a.previous_base, b.previous_base);
}

TEST_F(WeaponTraceTest, AnInvertedSpanCollapsesToTheCurrentInstant) {
  auto* commander = make_commander(0.0F, 0.0F);
  auto const segment = sample_authored_weapon_trace_segment(
      *commander,
      sword(),
      {.previous_normalized_time = 0.52F, .current_normalized_time = 0.44F});
  ASSERT_TRUE(segment.valid);
  EXPECT_EQ(segment.previous_tip, segment.current_tip);
  EXPECT_EQ(segment.previous_base, segment.current_base);
}

TEST_F(WeaponTraceTest, StaticContactFindsAnEnemyInFrontOnly) {
  auto* commander = make_commander(0.0F, 0.0F);
  auto* front = make_enemy(0.0F, 1.2F);
  auto* behind = make_enemy(0.0F, -1.2F);

  auto const contact = find_weapon_trace_contact(*world, *commander, sword());
  EXPECT_EQ(contact.attacker_id, commander->get_id());
  EXPECT_EQ(contact.target_id, front->get_id());
  EXPECT_GT(contact.local_forward, 0.0F);

  std::array<EntityID, 1> const ignored{front->get_id()};
  auto const rest =
      find_weapon_trace_contact(*world, *commander, sword(), behind->get_id(), ignored);
  EXPECT_EQ(rest.target_id, 0U);
}

TEST_F(WeaponTraceTest, TheNearerEnemyWinsAndAHintCanOverrideIt) {
  auto* commander = make_commander(0.0F, 0.0F);
  auto* near_enemy = make_enemy(0.0F, 1.0F);
  auto* far_enemy = make_enemy(0.0F, 1.03F);

  auto const plain = find_weapon_trace_contact(*world, *commander, sword());
  EXPECT_EQ(plain.target_id, near_enemy->get_id());

  auto const hinted =
      find_weapon_trace_contact(*world, *commander, sword(), far_enemy->get_id());
  EXPECT_EQ(hinted.target_id, far_enemy->get_id());
}

TEST_F(WeaponTraceTest, IgnoredSoldierSlotsAreSkipped) {
  auto* commander = make_commander(0.0F, 0.0F);
  auto* enemy = make_enemy(0.0F, 1.2F);

  auto const first = find_weapon_trace_contact(*world, *commander, sword());
  ASSERT_EQ(first.target_id, enemy->get_id());

  std::array<WeaponTraceIgnoredTarget, 1> const ignored_slots{
      WeaponTraceIgnoredTarget{first.target_id, first.target_soldier_slot}};
  auto const second = find_weapon_trace_contact(
      *world,
      *commander,
      sword(),
      0,
      {},
      std::span<const WeaponTraceIgnoredTarget>{ignored_slots});
  EXPECT_EQ(second.target_id, 0U);
}

TEST_F(WeaponTraceTest, AnAttackerWithoutAUnitFindsNothing) {
  auto* bare = world->create_entity();
  bare->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  make_enemy(0.0F, 1.0F);

  auto const contact = find_weapon_trace_contact(*world, *bare, sword());
  EXPECT_EQ(contact.attacker_id, bare->get_id());
  EXPECT_EQ(contact.target_id, 0U);
}

TEST_F(WeaponTraceTest, TheSweptSearchAgreesWithTheStaticOneOutsideTheWindow) {
  auto* commander = make_commander(0.0F, 0.0F);
  auto* enemy = make_enemy(0.0F, 1.2F);

  auto const swept = find_weapon_trace_contact(
      *world,
      *commander,
      sword(),
      {.previous_normalized_time = 0.0F, .current_normalized_time = 0.02F});
  EXPECT_EQ(swept.target_id, enemy->get_id());
}

TEST_F(WeaponTraceTest, ALongSweptSpanIsSlicedAndStillFindsTheEnemy) {
  auto* commander = make_commander(0.0F, 0.0F);
  auto* enemy = make_enemy(0.0F, 1.2F);

  auto const contact = find_weapon_trace_contact(
      *world,
      *commander,
      sword(),
      {.previous_normalized_time = 0.0F, .current_normalized_time = 0.5F});
  EXPECT_EQ(contact.target_id, enemy->get_id());
}

TEST_F(WeaponTraceTest, TheSweptContactReportsTipSpeedInsideTheWindow) {
  auto* commander = make_commander(0.0F, 0.0F);
  auto* enemy = make_enemy(0.0F, 1.2F);
  WeaponTraceTimeSpan const span{.previous_normalized_time = 0.44F,
                                 .current_normalized_time = 0.46F};

  auto const segment = sample_authored_weapon_trace_segment(*commander, sword(), span);
  ASSERT_TRUE(segment.valid);
  auto const contact = find_weapon_trace_contact(*world, *commander, sword(), span);
  if (contact.target_id != 0) {
    EXPECT_EQ(contact.target_id, enemy->get_id());
    EXPECT_GE(contact.contact_speed, 0.0F);
    EXPECT_TRUE(std::isfinite(contact.contact_speed));
  }
}

} // namespace
