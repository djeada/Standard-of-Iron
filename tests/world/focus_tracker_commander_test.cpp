#include <algorithm>
#include <gtest/gtest.h>

#include "app/world/focus_tracker.h"
#include "game/core/component_combat.h"
#include "game/core/world.h"
#include "game/session/selection_service.h"
#include "game/session/session_context.h"

namespace {

using Engine::Core::Entity;
using Engine::Core::World;
using Game::Systems::TargetFocusRole;

constexpr int k_local_owner = 1;
constexpr int k_enemy_owner = 2;

auto spawn_unit(World& world, int owner_id, float x) -> Entity* {
  auto* entity = world.create_entity();
  entity->add_component<Engine::Core::TransformComponent>(x, 0.0F, 0.0F);
  auto* unit =
      entity->add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  unit->owner_id = owner_id;
  return entity;
}

auto has_role(const App::World::FocusTracker& tracker, TargetFocusRole role) -> bool {
  const auto& markers = tracker.markers();
  return std::any_of(markers.begin(), markers.end(), [role](const auto& marker) {
    return marker.role == role;
  });
}

auto has_locked_target(const App::World::FocusTracker& tracker) -> bool {
  return has_role(tracker, TargetFocusRole::LockedTarget);
}

class FocusTrackerCommanderTest : public ::testing::Test {
protected:
  void SetUp() override {
    session.world().set_presentation_enabled(false);
    auto* soldier = spawn_unit(world, k_local_owner, 0.0F);
    auto* enemy = spawn_unit(world, k_enemy_owner, 6.0F);
    soldier->add_component<Engine::Core::AttackTargetComponent>()->target_id =
        enemy->get_id();
    auto* attacker = spawn_unit(world, k_enemy_owner, -6.0F);
    attacker->add_component<Engine::Core::AttackTargetComponent>()->target_id =
        soldier->get_id();
    session.selection().select_unit(soldier->get_id());
  }

  auto inputs(bool commander_active) -> App::World::FocusInputs {
    return {.world = &world,
            .local_owner_id = k_local_owner,
            .commander_active = commander_active};
  }

  Game::Session::SessionContext session;
  World& world = session.world();
};

TEST_F(FocusTrackerCommanderTest, ArmyViewMarksTheOrderedAttackTarget) {
  App::World::FocusTracker tracker;
  tracker.sync_target_focus_markers(inputs(false));
  EXPECT_TRUE(has_locked_target(tracker));
}

TEST_F(FocusTrackerCommanderTest, CommanderModeHidesTheCrossedSwordsTargetMarker) {
  App::World::FocusTracker tracker;
  tracker.sync_target_focus_markers(inputs(true));
  EXPECT_FALSE(has_locked_target(tracker))
      << "the army's attack-target marker must not follow the player into "
         "commander mode";

  tracker.sync_target_focus_markers(inputs(false));
  EXPECT_TRUE(has_locked_target(tracker))
      << "leaving commander mode brings the marker back";
}

TEST_F(FocusTrackerCommanderTest, CommanderModeHidesTheIncomingAttackerRings) {
  App::World::FocusTracker tracker;
  tracker.sync_target_focus_markers(inputs(false));
  EXPECT_TRUE(has_role(tracker, TargetFocusRole::IncomingAttacker));

  tracker.sync_target_focus_markers(inputs(true));
  EXPECT_FALSE(has_role(tracker, TargetFocusRole::IncomingAttacker))
      << "a wave pressing the commander drew a red ring under every squad in "
         "the chase view";
}

} // namespace
