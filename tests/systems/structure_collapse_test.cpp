#include <cmath>
#include <gtest/gtest.h>
#include <memory>
#include <numbers>

#include "game/core/component.h"
#include "game/core/death_sequence.h"
#include "game/core/world.h"
#include "game/session/session_context.h"
#include "game/systems/cleanup_system.h"
#include "game/systems/combat_system/damage_application.h"
#include "game/systems/production_system.h"

namespace {

using Engine::Core::DeathAnimationComponent;
using Engine::Core::DeathSequenceProfile;
using Engine::Core::DeathSequenceState;
using Engine::Core::PendingRemovalComponent;
using Engine::Core::RenderableComponent;
using Engine::Core::TransformComponent;
using Engine::Core::UnitComponent;
using Game::Units::SpawnType;

constexpr int k_owner = 1;
constexpr int k_enemy = 2;

class StructureCollapseTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_session = std::make_unique<Game::Session::SessionContext>();
  }

  [[nodiscard]] auto world() -> Engine::Core::World& { return m_session->world(); }

  auto add_structure(SpawnType type,
                     float x = 0.0F,
                     float z = 0.0F) -> Engine::Core::Entity* {
    auto* entity = world().create_entity();
    entity->add_component<TransformComponent>(x, 0.0F, z);
    entity->add_component<RenderableComponent>();
    entity->add_component<Engine::Core::BuildingComponent>();
    auto* unit = entity->add_component<UnitComponent>(400, 400, 0.0F, 0.0F);
    unit->owner_id = k_owner;
    unit->spawn_type = type;
    return entity;
  }

  auto add_attacker(float x, float z) -> Engine::Core::Entity* {
    auto* entity = world().create_entity();
    entity->add_component<TransformComponent>(x, 0.0F, z);
    auto* unit = entity->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
    unit->owner_id = k_enemy;
    unit->spawn_type = SpawnType::Swordsman;
    return entity;
  }

  void destroy(Engine::Core::Entity* structure,
               Engine::Core::Entity* attacker = nullptr) {
    Game::Systems::Combat::apply_unit_damage(
        &world(), structure, 100000, attacker != nullptr ? attacker->get_id() : 0U);
  }

  void run_cleanup(float seconds, float step = 0.1F) {
    for (float t = 0.0F; t < seconds; t += step) {
      m_cleanup.update(&world(), step);
    }
  }

  std::unique_ptr<Game::Session::SessionContext> m_session;
  Game::Systems::CleanupSystem m_cleanup;
};

TEST_F(StructureCollapseTest, ADestroyedStructureCollapsesInsteadOfVanishing) {
  auto* barracks = add_structure(SpawnType::Barracks);
  destroy(barracks);

  auto const* death = barracks->get_component<DeathAnimationComponent>();
  ASSERT_NE(death, nullptr) << "a structure at 0 HP must play its collapse";
  EXPECT_EQ(death->profile, DeathSequenceProfile::Structure);
  EXPECT_EQ(death->state, DeathSequenceState::Dying);
  EXPECT_FLOAT_EQ(death->state_duration, Engine::Core::k_structure_collapse_duration);
  EXPECT_FALSE(barracks->has_component<PendingRemovalComponent>());
  EXPECT_TRUE(barracks->get_component<RenderableComponent>()->visible)
      << "the ruin stays drawn while it comes down";
  EXPECT_FALSE(Engine::Core::is_live_entity(*barracks));
}

TEST_F(StructureCollapseTest, TheRuinIsRemovedOnlyAfterItHasSunk) {
  auto* home = add_structure(SpawnType::Home);
  auto const id = home->get_id();
  destroy(home);

  float const lifetime = Engine::Core::k_structure_collapse_duration +
                         Engine::Core::k_structure_rubble_hold_duration +
                         Engine::Core::k_structure_rubble_sink_duration;
  run_cleanup(lifetime - 0.5F);
  ASSERT_NE(world().get_entity(id), nullptr) << "the rubble must linger";
  EXPECT_EQ(world().get_entity(id)->get_component<DeathAnimationComponent>()->state,
            DeathSequenceState::Sinking);

  run_cleanup(1.0F);
  EXPECT_EQ(world().get_entity(id), nullptr)
      << "once sunk the ruin leaves through the normal removal path";
}

TEST_F(StructureCollapseTest, RuinsDoNotCountAgainstTheCorpseBudget) {
  auto* tower = add_structure(SpawnType::DefenseTower);
  destroy(tower);
  run_cleanup(Engine::Core::k_structure_collapse_duration + 0.2F);

  auto const budget = static_cast<std::size_t>(Engine::Core::Defaults::k_corpse_budget);
  for (std::size_t i = 0; i < budget + 5U; ++i) {
    auto* corpse = world().create_entity();
    corpse->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
    corpse->add_component<UnitComponent>(0, 100, 1.0F, 12.0F);
    auto* death = corpse->add_component<DeathAnimationComponent>();
    death->state = DeathSequenceState::DeadHold;
    death->state_time = 0.0F;
    death->dead_hold_duration = 100.0F;
  }
  run_cleanup(0.1F);

  EXPECT_EQ(tower->get_component<DeathAnimationComponent>()->state,
            DeathSequenceState::DeadHold)
      << "a battle's worth of bodies must not bury the rubble early";
}

TEST_F(StructureCollapseTest, TheRuinFallsAwayFromWhoeverBroughtItDown) {
  auto* wall = add_structure(SpawnType::WallSegment, 0.0F, 0.0F);
  auto* attacker = add_attacker(0.0F, -5.0F);
  destroy(wall, attacker);

  auto const* death = wall->get_component<DeathAnimationComponent>();
  ASSERT_NE(death, nullptr);
  float const heading = static_cast<float>(death->sequence_variant) *
                        (2.0F * std::numbers::pi_v<float>) / 256.0F;
  EXPECT_NEAR(std::cos(heading), 1.0F, 0.02F)
      << "attacked from -z, the ruin should fall toward +z";
}

TEST_F(StructureCollapseTest, ACollapsingBarracksStopsProducing) {
  auto* barracks = add_structure(SpawnType::Barracks);
  auto* production = barracks->add_component<Engine::Core::ProductionComponent>();
  production->in_progress = true;
  production->time_remaining = 5.0F;
  destroy(barracks);

  Game::Systems::ProductionSystem system;
  system.update(&world(), 1.0F);

  EXPECT_FLOAT_EQ(production->time_remaining, 5.0F)
      << "rubble must not keep training troops";
}

} // namespace
