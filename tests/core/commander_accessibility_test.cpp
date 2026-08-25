#include <cmath>
#include <gtest/gtest.h>

#include "app/commander/commander_control_controller.h"
#include "game/accessibility/commander_input_settings.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/nav_grid.h"

namespace {

namespace CommanderInput = Game::Accessibility::CommanderInput;

class CommanderAccessibilityTest : public ::testing::Test {
protected:
  void SetUp() override {
    CommanderInput::reset_to_defaults();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::NavGrid::initialize(32, 32);
  }

  void TearDown() override {
    CommanderInput::reset_to_defaults();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
  }

  static auto make_commander(Engine::Core::World& world) -> Engine::Core::Entity* {
    auto* entity = world.create_entity();
    if (entity == nullptr) {
      return nullptr;
    }
    entity->add_component<Engine::Core::TransformComponent>(0.0F, 0.0F, 0.0F);
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    entity->add_component<Engine::Core::MovementComponent>();
    auto* commander = entity->add_component<Engine::Core::CommanderComponent>();
    if (unit == nullptr || commander == nullptr) {
      return nullptr;
    }
    unit->health = 100;
    unit->max_health = 100;
    unit->owner_id = 1;
    unit->speed = 3.0F;
    unit->spawn_type = Game::Units::SpawnType::Knight;
    commander->fpv_controlled = true;
    return entity;
  }
};

TEST_F(CommanderAccessibilityTest, LookSensitivityScalesEachAxisIndependently) {
  CommanderControlController baseline;
  baseline.set_view_yaw(0.0F);
  baseline.set_view_pitch(0.0F);
  baseline.mouse_move(10.0, 10.0);
  float const base_yaw = baseline.view_yaw();
  float const base_pitch = baseline.view_pitch();
  ASSERT_GT(base_yaw, 0.0F);
  ASSERT_LT(base_pitch, 0.0F);

  CommanderInput::set_look_sensitivity_x(2.0F);
  CommanderControlController faster_x;
  faster_x.set_view_yaw(0.0F);
  faster_x.set_view_pitch(0.0F);
  faster_x.mouse_move(10.0, 10.0);
  EXPECT_NEAR(faster_x.view_yaw(), base_yaw * 2.0F, 1.0e-3F);
  EXPECT_NEAR(faster_x.view_pitch(), base_pitch, 1.0e-3F);

  CommanderInput::set_look_sensitivity_x(1.0F);
  CommanderInput::set_look_sensitivity_y(0.5F);
  CommanderControlController slower_y;
  slower_y.set_view_yaw(0.0F);
  slower_y.set_view_pitch(0.0F);
  slower_y.mouse_move(10.0, 10.0);
  EXPECT_NEAR(slower_y.view_yaw(), base_yaw, 1.0e-3F);
  EXPECT_NEAR(slower_y.view_pitch(), base_pitch * 0.5F, 1.0e-3F);
}

TEST_F(CommanderAccessibilityTest, InvertLookYFlipsOnlyThePitchAxis) {
  CommanderControlController baseline;
  baseline.set_view_yaw(0.0F);
  baseline.set_view_pitch(0.0F);
  baseline.mouse_move(10.0, 10.0);

  CommanderInput::set_invert_look_y(true);
  CommanderControlController inverted;
  inverted.set_view_yaw(0.0F);
  inverted.set_view_pitch(0.0F);
  inverted.mouse_move(10.0, 10.0);

  EXPECT_NEAR(inverted.view_yaw(), baseline.view_yaw(), 1.0e-3F);
  EXPECT_NEAR(inverted.view_pitch(), -baseline.view_pitch(), 1.0e-3F);
}

TEST_F(CommanderAccessibilityTest, GuardCanBeHeldOrToggled) {
  constexpr float k_dt = 1.0F / 60.0F;

  {
    Engine::Core::World world;
    auto* commander = make_commander(world);
    ASSERT_NE(commander, nullptr);
    CommanderControlController held;
    held.secondary_action_down();
    ASSERT_TRUE(held.update_simulation(world, commander->get_id(), 1, k_dt));
    auto const* guard =
        commander->get_component<Engine::Core::CommanderGuardComponent>();
    ASSERT_NE(guard, nullptr);
    EXPECT_TRUE(guard->active);

    held.secondary_action_up();
    ASSERT_TRUE(held.update_simulation(world, commander->get_id(), 1, k_dt));
    EXPECT_FALSE(guard->active);
  }

  CommanderInput::set_guard_is_toggle(true);
  {
    Engine::Core::World world;
    auto* commander = make_commander(world);
    ASSERT_NE(commander, nullptr);
    CommanderControlController toggled;
    toggled.secondary_action_down();
    toggled.secondary_action_up();
    ASSERT_TRUE(toggled.update_simulation(world, commander->get_id(), 1, k_dt));
    auto const* guard =
        commander->get_component<Engine::Core::CommanderGuardComponent>();
    ASSERT_NE(guard, nullptr);
    EXPECT_TRUE(guard->active) << "a released toggle press must leave the guard up";

    toggled.secondary_action_down();
    toggled.secondary_action_up();
    ASSERT_TRUE(toggled.update_simulation(world, commander->get_id(), 1, k_dt));
    EXPECT_FALSE(guard->active) << "the second toggle press must drop the guard";
  }
}

TEST_F(CommanderAccessibilityTest, SettingsClampToTheirAuthoredRange) {
  CommanderInput::set_look_sensitivity_x(100.0F);
  EXPECT_LE(CommanderInput::look_sensitivity_x(), 4.0F);
  CommanderInput::set_look_sensitivity_y(0.0F);
  EXPECT_GE(CommanderInput::look_sensitivity_y(), 0.2F);
  CommanderInput::set_field_of_view_scale(10.0F);
  EXPECT_LE(CommanderInput::field_of_view_scale(), 1.35F);
  CommanderInput::set_field_of_view_scale(0.0F);
  EXPECT_GE(CommanderInput::field_of_view_scale(), 0.75F);
}

} // namespace
