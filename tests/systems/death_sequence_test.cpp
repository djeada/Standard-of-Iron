#include <gtest/gtest.h>
#include <memory>

#include "core/component.h"
#include "core/death_sequence.h"
#include "core/entity.h"
#include "core/world.h"
#include "systems/cleanup_system.h"
#include "systems/combat_system/damage_application.h"

using Engine::Core::DeathAnimationComponent;
using Engine::Core::DeathSequenceProfile;
using Engine::Core::DeathSequenceState;
using Engine::Core::DeathSequenceTiming;
using Engine::Core::RenderableComponent;
using Engine::Core::SoldierCasualtyAnimationComponent;
using Engine::Core::TransformComponent;
using Engine::Core::UnitComponent;
using Engine::Core::WildlifeComponent;

namespace {

auto timing(float fall, float hold, float sink) -> DeathSequenceTiming {
  DeathSequenceTiming t{};
  t.state_duration = fall;
  t.dead_hold_duration = hold;
  t.sink_duration = sink;
  return t;
}

} // namespace

TEST(DeathSequence, WalksDyingThenSettledThenSinkingBeforeExpiring) {
  DeathAnimationComponent death{};
  Engine::Core::apply_death_sequence_timing(death, timing(1.0F, 2.0F, 0.5F));

  EXPECT_FALSE(Engine::Core::advance_death_sequence(death, 0.5F));
  EXPECT_EQ(death.state, DeathSequenceState::Dying);
  EXPECT_FLOAT_EQ(Engine::Core::death_sink_progress(death), 0.0F);

  EXPECT_FALSE(Engine::Core::advance_death_sequence(death, 0.5F));
  EXPECT_EQ(death.state, DeathSequenceState::DeadHold);
  EXPECT_TRUE(Engine::Core::death_sequence_is_settled(death));

  EXPECT_FALSE(Engine::Core::advance_death_sequence(death, 2.0F));
  EXPECT_EQ(death.state, DeathSequenceState::Sinking);
  EXPECT_FLOAT_EQ(Engine::Core::death_sink_progress(death), 0.0F);

  EXPECT_FALSE(Engine::Core::advance_death_sequence(death, 0.25F));
  EXPECT_FLOAT_EQ(Engine::Core::death_sink_progress(death), 0.5F);

  EXPECT_TRUE(Engine::Core::advance_death_sequence(death, 0.25F));
  EXPECT_FLOAT_EQ(Engine::Core::death_sink_progress(death), 1.0F);
}

TEST(DeathSequence, CasualtyEntriesShareTheSameLifecycle) {
  SoldierCasualtyAnimationComponent::Entry entry{};
  Engine::Core::apply_death_sequence_timing(entry, timing(0.2F, 0.2F, 0.2F));
  EXPECT_EQ(entry.state, DeathSequenceState::Dying);
  EXPECT_FALSE(Engine::Core::advance_death_sequence(entry, 0.2F));
  EXPECT_EQ(entry.state, DeathSequenceState::DeadHold);
  EXPECT_FALSE(Engine::Core::advance_death_sequence(entry, 0.2F));
  EXPECT_EQ(entry.state, DeathSequenceState::Sinking);
  EXPECT_TRUE(Engine::Core::advance_death_sequence(entry, 0.2F));
}

TEST(DeathSequence, ElapsedTimeKeepsCountingAcrossEveryStage) {
  SoldierCasualtyAnimationComponent::Entry entry{};
  Engine::Core::apply_death_sequence_timing(entry, timing(1.0F, 2.0F, 0.5F));
  float previous = -1.0F;
  for (int step = 0; step < 70; ++step) {
    float const elapsed = Engine::Core::death_sequence_elapsed(entry);
    EXPECT_GT(elapsed, previous)
        << "step " << step << " state " << static_cast<int>(entry.state);
    previous = elapsed;
    if (Engine::Core::advance_death_sequence(entry, 0.05F)) {
      break;
    }
  }
  EXPECT_EQ(entry.state, DeathSequenceState::Sinking);
  EXPECT_NEAR(Engine::Core::death_sequence_elapsed(entry), 3.5F, 0.06F);
}

TEST(DeathSequence, BeginSinkingLeavesAnAlreadySinkingBodyAlone) {
  DeathAnimationComponent death{};
  Engine::Core::apply_death_sequence_timing(death, timing(0.1F, 0.1F, 1.0F));
  Engine::Core::begin_death_sinking(death);
  EXPECT_EQ(death.state, DeathSequenceState::Sinking);
  (void)Engine::Core::advance_death_sequence(death, 0.5F);
  Engine::Core::begin_death_sinking(death);
  EXPECT_FLOAT_EQ(death.state_time, 0.5F);
}

TEST(DeathSequence, DefaultsKeepABodyOnTheFieldLongerThanItsFall) {
  DeathAnimationComponent const death{};
  EXPECT_GT(death.dead_hold_duration, death.state_duration);
  EXPECT_GT(death.sink_duration, 0.0F);
  SoldierCasualtyAnimationComponent::Entry const entry{};
  EXPECT_FLOAT_EQ(entry.dead_hold_duration, death.dead_hold_duration);
  EXPECT_FLOAT_EQ(entry.sink_duration, death.sink_duration);
}

class DeathSequenceWorldTest : public ::testing::Test {
protected:
  std::unique_ptr<Engine::Core::World> world = std::make_unique<Engine::Core::World>();

  auto add_corpse(float hold_age) -> Engine::Core::EntityID {
    auto* entity = world->create_entity();
    entity->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
    entity->add_component<RenderableComponent>();
    entity->add_component<UnitComponent>(0, 100, 1.0F, 12.0F);
    auto* death = entity->add_component<DeathAnimationComponent>();
    death->state = DeathSequenceState::DeadHold;
    death->state_time = hold_age;
    death->dead_hold_duration = 100.0F;
    death->sink_duration = 100.0F;
    return entity->get_id();
  }
};

TEST_F(DeathSequenceWorldTest, TheOldestSettledBodiesSinkWhenTheFieldIsOverBudget) {
  auto const budget = static_cast<std::size_t>(Engine::Core::Defaults::k_corpse_budget);
  std::vector<Engine::Core::EntityID> ids;
  for (std::size_t index = 0; index < budget + 3U; ++index) {
    ids.push_back(add_corpse(static_cast<float>(index)));
  }

  Game::Systems::CleanupSystem cleanup;
  cleanup.update(world.get(), 0.01F);

  std::size_t sinking = 0;
  for (std::size_t index = 0; index < ids.size(); ++index) {
    auto const* death =
        world->get_entity(ids[index])->get_component<DeathAnimationComponent>();
    ASSERT_NE(death, nullptr);
    bool const should_sink = index >= budget;
    EXPECT_EQ(death->state == DeathSequenceState::Sinking, should_sink)
        << "corpse " << index << " of " << ids.size();
    sinking += death->state == DeathSequenceState::Sinking ? 1U : 0U;
  }
  EXPECT_EQ(sinking, 3U);
}

TEST_F(DeathSequenceWorldTest, FormationCasualtiesCountTowardsTheBudget) {
  auto const budget = static_cast<std::size_t>(Engine::Core::Defaults::k_corpse_budget);
  auto* squad = world->create_entity();
  squad->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  squad->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  auto* casualties = squad->add_component<SoldierCasualtyAnimationComponent>();
  for (std::size_t index = 0; index < budget; ++index) {
    SoldierCasualtyAnimationComponent::Entry entry{};
    entry.slot_index = static_cast<std::uint16_t>(index);
    entry.state = DeathSequenceState::DeadHold;
    entry.state_time = 50.0F;
    entry.dead_hold_duration = 100.0F;
    entry.sink_duration = 100.0F;
    casualties->entries.push_back(entry);
  }
  auto const fresh = add_corpse(0.0F);

  Game::Systems::CleanupSystem cleanup;
  cleanup.update(world.get(), 0.01F);

  EXPECT_EQ(world->get_entity(fresh)->get_component<DeathAnimationComponent>()->state,
            DeathSequenceState::DeadHold);
  EXPECT_EQ(std::count_if(casualties->entries.begin(),
                          casualties->entries.end(),
                          [](auto const& entry) {
                            return entry.state == DeathSequenceState::Sinking;
                          }),
            1);
}

TEST_F(DeathSequenceWorldTest, BeginDeathSequenceAuthorsTheSameTimingForEveryKillPath) {
  auto* soldier = world->create_entity();
  soldier->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* soldier_unit = soldier->add_component<UnitComponent>(40, 100, 1.0F, 12.0F);
  Game::Systems::Combat::begin_death_sequence(soldier, nullptr);
  auto const* soldier_death = soldier->get_component<DeathAnimationComponent>();
  ASSERT_NE(soldier_death, nullptr);
  EXPECT_EQ(soldier_unit->health, 0);
  EXPECT_EQ(soldier_death->profile, DeathSequenceProfile::Infantry);
  EXPECT_EQ(soldier_death->state, DeathSequenceState::Dying);
  EXPECT_GT(soldier_death->state_duration, 0.9F);
  EXPECT_FLOAT_EQ(soldier_death->dead_hold_duration,
                  Engine::Core::Defaults::k_corpse_hold_duration);

  auto* sheep = world->create_entity();
  sheep->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  sheep->add_component<UnitComponent>(10, 10, 1.0F, 0.0F);
  sheep->add_component<WildlifeComponent>();
  Game::Systems::Combat::begin_death_sequence(sheep, nullptr);
  auto const* sheep_death = sheep->get_component<DeathAnimationComponent>();
  ASSERT_NE(sheep_death, nullptr);
  EXPECT_EQ(sheep_death->profile, DeathSequenceProfile::Horse);
  EXPECT_FLOAT_EQ(sheep_death->state_duration, 1.2F);
  EXPECT_GT(sheep_death->sink_duration, 0.0F);
}
