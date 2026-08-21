#include <gtest/gtest.h>
#include <vector>

#include "game/core/component.h"
#include "game/core/deferred_mutations.h"
#include "game/core/entity.h"
#include "game/core/system.h"
#include "game/core/system_schedule.h"
#include "game/core/world.h"

namespace {

using Engine::Core::AttackComponent;
using Engine::Core::component_type_id;
using Engine::Core::DeferredMutations;
using Engine::Core::EntityID;
using Engine::Core::MovementComponent;
using Engine::Core::plan_phase_batches;
using Engine::Core::SystemAccess;
using Engine::Core::SystemPhase;
using Engine::Core::TransformComponent;
using Engine::Core::UnitComponent;
using Engine::Core::World;

auto access(std::vector<Engine::Core::ComponentTypeId> reads,
            std::vector<Engine::Core::ComponentTypeId> writes) -> SystemAccess {
  return SystemAccess{
      .reads = std::move(reads), .writes = std::move(writes), .exclusive = false};
}

TEST(SystemAccessTest, TwoReadersOfTheSameComponentDoNotCollide) {
  const auto reader_a = access({component_type_id<TransformComponent>()}, {});
  const auto reader_b = access({component_type_id<TransformComponent>()}, {});
  EXPECT_FALSE(reader_a.conflicts_with(reader_b));
}

TEST(SystemAccessTest, AWriterCollidesWithAnyoneTouchingWhatItWrites) {
  const auto writer = access({}, {component_type_id<TransformComponent>()});
  const auto reader = access({component_type_id<TransformComponent>()}, {});
  const auto other_writer = access({}, {component_type_id<TransformComponent>()});
  const auto unrelated = access({component_type_id<UnitComponent>()},
                                {component_type_id<AttackComponent>()});

  EXPECT_TRUE(writer.conflicts_with(reader));
  EXPECT_TRUE(reader.conflicts_with(writer));
  EXPECT_TRUE(writer.conflicts_with(other_writer));
  EXPECT_FALSE(writer.conflicts_with(unrelated));
}

TEST(SystemAccessTest, AnUndeclaredSystemCollidesWithEverything) {
  const SystemAccess undeclared = SystemAccess::everything();
  const auto declared = access({component_type_id<UnitComponent>()}, {});

  EXPECT_TRUE(undeclared.conflicts_with(declared));
  EXPECT_TRUE(declared.conflicts_with(undeclared));
  EXPECT_TRUE(undeclared.conflicts_with(SystemAccess::everything()));
}

TEST(SystemScheduleTest, GroupsIndependentSystemsAndSplitsCollidingOnes) {
  const std::vector<SystemAccess> systems{
      access({component_type_id<TransformComponent>()}, {}),
      access({component_type_id<UnitComponent>()}, {}),
      access({}, {component_type_id<TransformComponent>()}),
      access({component_type_id<AttackComponent>()}, {}),
  };

  const auto batches = plan_phase_batches(systems);

  ASSERT_EQ(batches.size(), 2U);
  EXPECT_EQ(batches[0], (std::vector<std::size_t>{0, 1}));
  EXPECT_EQ(batches[1], (std::vector<std::size_t>{2, 3}));
}

TEST(SystemScheduleTest, UndeclaredSystemsEachGetTheirOwnBatch) {
  const std::vector<SystemAccess> systems{SystemAccess::everything(),
                                          SystemAccess::everything(),
                                          SystemAccess::everything()};

  const auto batches = plan_phase_batches(systems);

  ASSERT_EQ(batches.size(), 3U);
  for (const auto& batch : batches) {
    EXPECT_EQ(batch.size(), 1U);
  }
}

TEST(SystemScheduleTest, BatchesRunInRegistrationOrder) {
  const std::vector<SystemAccess> systems{
      access({component_type_id<TransformComponent>()}, {}),
      access({}, {component_type_id<TransformComponent>()}),
      access({component_type_id<TransformComponent>()}, {}),
  };

  const auto batches = plan_phase_batches(systems);

  std::vector<std::size_t> flattened;
  for (const auto& batch : batches) {
    flattened.insert(flattened.end(), batch.begin(), batch.end());
  }
  EXPECT_EQ(flattened, (std::vector<std::size_t>{0, 1, 2}));
}

TEST(SystemPhaseTest, EveryPhaseHasAName) {
  for (std::uint8_t raw = 0; raw < static_cast<std::uint8_t>(SystemPhase::_Count);
       ++raw) {
    const auto phase = static_cast<SystemPhase>(raw);
    EXPECT_STRNE(Engine::Core::phase_name(phase), "?");
  }
}

auto spawn_unit(World& world) -> EntityID {
  auto* entity = world.create_entity();
  entity->add_component<TransformComponent>();
  entity->add_component<UnitComponent>();
  return entity->get_id();
}

TEST(DeferredMutationsTest, NothingHappensUntilTheBarrier) {
  World world;
  const EntityID id = spawn_unit(world);

  DeferredMutations pending;
  pending.add_component<MovementComponent>(id);
  pending.destroy_entity(id);

  EXPECT_EQ(pending.pending(), 2U);
  EXPECT_TRUE(world.is_alive(id));
  EXPECT_FALSE(world.get_entity(id)->has_component<MovementComponent>());

  pending.apply(world);

  EXPECT_TRUE(pending.empty());
  EXPECT_FALSE(world.is_alive(id));
}

TEST(DeferredMutationsTest, AppliesInTheOrderRecorded) {
  World world;
  const EntityID id = spawn_unit(world);

  DeferredMutations pending;
  pending.add_component<MovementComponent>(id);
  pending.remove_component<MovementComponent>(id);
  pending.add_component<AttackComponent>(id);
  pending.apply(world);

  auto* entity = world.get_entity(id);
  ASSERT_NE(entity, nullptr);
  EXPECT_FALSE(entity->has_component<MovementComponent>());
  EXPECT_TRUE(entity->has_component<AttackComponent>());
}

TEST(DeferredMutationsTest, ForwardsConstructorArguments) {
  World world;
  const EntityID id = spawn_unit(world);

  DeferredMutations pending;
  pending.add_component<AttackComponent>(id, 7.5F, 3.0F, 0.5F);
  pending.apply(world);

  const auto* attack = world.get_entity(id)->get_component<AttackComponent>();
  ASSERT_NE(attack, nullptr);
  EXPECT_FLOAT_EQ(attack->range, 7.5F);
}

TEST(DeferredMutationsTest, SurvivesAnEntityThatDiedBeforeTheBarrier) {
  World world;
  const EntityID id = spawn_unit(world);

  DeferredMutations pending;
  pending.add_component<MovementComponent>(id);
  world.destroy_entity(id);

  pending.apply(world);
  EXPECT_FALSE(world.is_alive(id));
}

TEST(DeferredMutationsTest, AChangeRecordedByAChangeWaitsForTheNextBarrier) {
  World world;
  const EntityID id = spawn_unit(world);

  DeferredMutations pending;
  pending.run([&pending, id](World&) { pending.destroy_entity(id); });

  pending.apply(world);
  EXPECT_TRUE(world.is_alive(id));
  EXPECT_EQ(pending.pending(), 1U);

  pending.apply(world);
  EXPECT_FALSE(world.is_alive(id));
}

TEST(DeferredMutationsTest, LetsASystemRestructureTheWorldWhileWalkingAView) {
  World world;
  std::vector<EntityID> ids;
  for (int i = 0; i < 8; ++i) {
    ids.push_back(spawn_unit(world));
  }

  DeferredMutations pending;
  for (auto [id, transform, unit] : world.view<TransformComponent, UnitComponent>()) {
    (void)transform;
    (void)unit;
    pending.destroy_entity(id);
  }
  pending.apply(world);

  for (const EntityID id : ids) {
    EXPECT_FALSE(world.is_alive(id));
  }
}

} // namespace
