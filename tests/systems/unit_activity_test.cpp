#include <gtest/gtest.h>

#include "core/component.h"
#include "core/world.h"
#include "game/systems/builder_product_types.h"
#include "game/systems/unit_activity.h"
#include "tests/support/movement_test_access.h"

namespace {

using Game::Systems::ActivityKind;
using Game::Systems::ActivityState;
using Game::Systems::classify_unit_activity;

auto make_unit(Engine::Core::World& world) -> Engine::Core::Entity* {
  auto* entity = world.create_entity();
  entity->add_component<Engine::Core::UnitComponent>();
  entity->add_component<Engine::Core::TransformComponent>();
  return entity;
}

auto builder_at_work(Engine::Core::World& world,
                     std::string_view product) -> Engine::Core::Entity* {
  auto* entity = make_unit(world);
  auto* builder = entity->add_component<Engine::Core::BuilderProductionComponent>();
  builder->product_type = std::string(product);
  builder->has_construction_site = true;
  builder->at_construction_site = true;
  builder->in_progress = true;
  return entity;
}

TEST(UnitActivityTest, AnIdleUnitReportsNothingWorthShowing) {
  Engine::Core::World world;
  auto* entity = make_unit(world);

  const auto activity = classify_unit_activity(*entity);
  EXPECT_EQ(activity.kind, ActivityKind::Idle);
  EXPECT_FALSE(Game::Systems::activity_is_noteworthy(activity));
}

TEST(UnitActivityTest, EachHarvestProductMapsToItsOwnResourceActivity) {
  Engine::Core::World world;

  EXPECT_EQ(classify_unit_activity(
                *builder_at_work(world, Game::Systems::k_builder_product_cut_tree))
                .kind,
            ActivityKind::ChopWood);
  EXPECT_EQ(classify_unit_activity(
                *builder_at_work(world, Game::Systems::k_builder_product_collect_stone))
                .kind,
            ActivityKind::MineStone);
  EXPECT_EQ(
      classify_unit_activity(
          *builder_at_work(world, Game::Systems::k_builder_product_collect_iron_ore))
          .kind,
      ActivityKind::MineIron);
  EXPECT_EQ(classify_unit_activity(
                *builder_at_work(world, Game::Systems::k_builder_product_repair))
                .kind,
            ActivityKind::Repair);
  EXPECT_EQ(classify_unit_activity(
                *builder_at_work(world, Game::Systems::k_builder_product_dismantle))
                .kind,
            ActivityKind::Dismantle);
  EXPECT_EQ(classify_unit_activity(*builder_at_work(world, "barracks")).kind,
            ActivityKind::Construct);
}

TEST(UnitActivityTest, WorkOnlyReadsAsActiveOnceTheBuilderHasArrived) {
  Engine::Core::World world;
  auto* entity = make_unit(world);
  auto* builder = entity->add_component<Engine::Core::BuilderProductionComponent>();
  builder->product_type = std::string(Game::Systems::k_builder_product_cut_tree);
  builder->has_construction_site = true;
  builder->at_construction_site = false;

  auto activity = classify_unit_activity(*entity);
  EXPECT_EQ(activity.kind, ActivityKind::ChopWood);
  EXPECT_EQ(activity.state, ActivityState::Queued);

  builder->at_construction_site = true;
  builder->in_progress = true;
  activity = classify_unit_activity(*entity);
  EXPECT_EQ(activity.state, ActivityState::Active);
}

TEST(UnitActivityTest, AbandonedWorkAndLostTargetsReadDifferently) {
  Engine::Core::World world;

  auto* abandoned = builder_at_work(world, "barracks");
  abandoned->get_component<Engine::Core::BuilderProductionComponent>()->report_fault(
      Engine::Core::BuilderTaskFault::Interrupted);
  EXPECT_EQ(classify_unit_activity(*abandoned).state, ActivityState::Interrupted);

  auto* lost = builder_at_work(world, Game::Systems::k_builder_product_cut_tree);
  lost->get_component<Engine::Core::BuilderProductionComponent>()->report_fault(
      Engine::Core::BuilderTaskFault::TargetLost);
  const auto activity = classify_unit_activity(*lost);
  EXPECT_EQ(activity.state, ActivityState::Unavailable);
  // The work is still named so the player can see which order stalled.
  EXPECT_EQ(activity.kind, ActivityKind::ChopWood);
}

TEST(UnitActivityTest, AFaultThatHasTimedOutStopsBeingReported) {
  Engine::Core::World world;
  auto* entity = builder_at_work(world, "barracks");
  auto* builder = entity->get_component<Engine::Core::BuilderProductionComponent>();
  builder->report_fault(Engine::Core::BuilderTaskFault::Interrupted);
  builder->fault_display_remaining = 0.0F;

  EXPECT_EQ(classify_unit_activity(*entity).state, ActivityState::Active);
}

TEST(UnitActivityTest, QueuedWallSitesAreCounted) {
  Engine::Core::World world;
  auto* entity = builder_at_work(world, "wall_segment");
  auto* builder = entity->get_component<Engine::Core::BuilderProductionComponent>();
  builder->queued_construction_site_ids = {11, 12, 13};

  EXPECT_EQ(classify_unit_activity(*entity).queued_orders, 3);
}

TEST(UnitActivityTest, ACivilianCarryingItsLoadReportsDelivery) {
  Engine::Core::World world;
  auto* entity = make_unit(world);
  auto* delivery = entity->add_component<Engine::Core::CivilianDeliveryComponent>();
  delivery->target_barracks_id = 7;

  const auto activity = classify_unit_activity(*entity);
  EXPECT_EQ(activity.kind, ActivityKind::Deliver);
  EXPECT_EQ(activity.state, ActivityState::Queued);
}

TEST(UnitActivityTest, StancesAndOrdersAreToldApart) {
  Engine::Core::World world;

  auto* attacker = make_unit(world);
  attacker->add_component<Engine::Core::AttackTargetComponent>();
  EXPECT_EQ(classify_unit_activity(*attacker).kind, ActivityKind::Attack);

  auto* patroller = make_unit(world);
  patroller->add_component<Engine::Core::PatrolComponent>()->patrolling = true;
  EXPECT_EQ(classify_unit_activity(*patroller).kind, ActivityKind::Patrol);

  auto* guard = make_unit(world);
  guard->add_component<Engine::Core::GuardModeComponent>()->active = true;
  EXPECT_EQ(classify_unit_activity(*guard).kind, ActivityKind::Guard);

  auto* holder = make_unit(world);
  holder->add_component<Engine::Core::HoldModeComponent>()->active = true;
  EXPECT_EQ(classify_unit_activity(*holder).kind, ActivityKind::Hold);

  auto* healer = make_unit(world);
  healer->add_component<Engine::Core::HealerComponent>()->is_healing_active = true;
  EXPECT_EQ(classify_unit_activity(*healer).kind, ActivityKind::Heal);

  auto* barracks = make_unit(world);
  barracks->add_component<Engine::Core::ProductionComponent>()->in_progress = true;
  EXPECT_EQ(classify_unit_activity(*barracks).kind, ActivityKind::Train);
}

TEST(UnitActivityTest, AWedgedUnitReportsBlockedRatherThanMoving) {
  Engine::Core::World world;
  auto* entity = make_unit(world);
  auto* movement = entity->add_component<Engine::Core::MovementComponent>();
  MovementTestAccess::set_has_target(*movement, true);
  MovementTestAccess::set_target_x(*movement, 10.0F);
  MovementTestAccess::set_target_y(*movement, 10.0F);

  auto activity = classify_unit_activity(*entity);
  EXPECT_EQ(activity.kind, ActivityKind::Move);
  EXPECT_FALSE(Game::Systems::activity_is_noteworthy(activity))
      << "an ordinary march must not put a marker over every soldier";

  MovementTestAccess::set_stuck_time(*movement, 5.0F);
  activity = classify_unit_activity(*entity);
  EXPECT_EQ(activity.kind, ActivityKind::Blocked);
  EXPECT_EQ(activity.state, ActivityState::Unavailable);
  EXPECT_TRUE(Game::Systems::activity_is_noteworthy(activity));
}

TEST(UnitActivityTest, IdsRoundTripSoQmlAndCppNameTheSameThing) {
  for (const auto kind : {ActivityKind::Idle,
                          ActivityKind::Move,
                          ActivityKind::Attack,
                          ActivityKind::Patrol,
                          ActivityKind::Guard,
                          ActivityKind::Hold,
                          ActivityKind::Construct,
                          ActivityKind::Repair,
                          ActivityKind::Dismantle,
                          ActivityKind::ChopWood,
                          ActivityKind::MineStone,
                          ActivityKind::MineIron,
                          ActivityKind::Deliver,
                          ActivityKind::Heal,
                          ActivityKind::Train,
                          ActivityKind::Blocked}) {
    EXPECT_EQ(
        Game::Systems::activity_kind_from_id(Game::Systems::activity_kind_id(kind)),
        kind);
  }
  for (const auto state : {ActivityState::Active,
                           ActivityState::Queued,
                           ActivityState::Unavailable,
                           ActivityState::Interrupted}) {
    EXPECT_EQ(
        Game::Systems::activity_state_from_id(Game::Systems::activity_state_id(state)),
        state);
  }
  EXPECT_EQ(Game::Systems::activity_kind_from_id("not_a_thing"), ActivityKind::Idle);
}

} // namespace
