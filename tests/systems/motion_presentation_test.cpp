#include <gtest/gtest.h>
#include <memory>

#include "core/component_core.h"
#include "core/system.h"
#include "core/world.h"
#include "game/core/component_gameplay.h"
#include "game/systems/route_follow_system.h"
#include "game/units/spawn_type.h"
#include "tests/support/movement_test_access.h"

namespace {

constexpr float k_step = 1.0F / 30.0F;

class ScriptedGlide : public Engine::Core::System {
public:
  float speed{0.0F};

  void update(Engine::Core::World* world, float delta_time) override {
    if (world == nullptr || speed == 0.0F) {
      return;
    }
    for (auto [id, transform] : world->view<Engine::Core::TransformComponent>()) {
      (void)id;
      transform.position.x += speed * delta_time;
    }
  }
};

auto add_walker(Engine::Core::World& world) -> Engine::Core::Entity* {
  auto* walker = world.create_entity();
  auto* transform = walker->add_component<Engine::Core::TransformComponent>();
  transform->position = {0.0F, 0.0F, 0.0F};
  auto* unit = walker->add_component<Engine::Core::UnitComponent>();
  unit->spawn_type = Game::Units::SpawnType::Civilian;
  unit->owner_id = 1;
  unit->health = 35;
  unit->max_health = 35;
  unit->speed = 2.3F;
  walker->add_component<Engine::Core::MovementComponent>();
  return walker;
}

auto motion_state(Engine::Core::Entity& entity)
    -> Engine::Core::MotionPresentationState {
  return entity.get_component<Engine::Core::MotionPresentationComponent>()->state;
}

TEST(MotionPresentationTest, AUnitThatCannotAdvanceStopsPresentingAWalk) {
  Engine::Core::World world;
  auto* walker = add_walker(world);
  auto* movement = walker->get_component<Engine::Core::MovementComponent>();
  movement->engage_manual_move(40.0F, 0.0F);
  movement->set_manual_velocity(1.7F, 0.0F);

  world.update(k_step);
  EXPECT_EQ(motion_state(*walker), Engine::Core::MotionPresentationState::Idle);

  for (int tick = 0; tick < 30; ++tick) {
    movement->set_manual_velocity(1.7F, 0.0F);
    world.update(k_step);
  }

  EXPECT_EQ(motion_state(*walker), Engine::Core::MotionPresentationState::Idle);
}

TEST(MotionPresentationTest, AUnitThatKeepsMovingKeepsWalking) {
  Engine::Core::World world;
  auto glide = std::make_unique<ScriptedGlide>();
  auto* glide_ref = glide.get();
  world.add_system(std::move(glide));
  auto* walker = add_walker(world);
  auto* movement = walker->get_component<Engine::Core::MovementComponent>();
  movement->engage_manual_move(40.0F, 0.0F);
  glide_ref->speed = 1.7F;

  for (int tick = 0; tick < 30; ++tick) {
    movement->set_manual_velocity(1.7F, 0.0F);
    world.update(k_step);
  }

  EXPECT_EQ(motion_state(*walker),
            Engine::Core::MotionPresentationState::ForcedDisplacement);
}

TEST(MotionPresentationTest, AcceptedMotorFactsOwnPresentedVelocityAndDisplacement) {
  Engine::Core::World world;
  auto glide = std::make_unique<ScriptedGlide>();
  auto* glide_ref = glide.get();
  world.add_system(std::move(glide));
  auto* walker = add_walker(world);
  auto* facts = walker->add_component<Engine::Core::MovementFactsComponent>();
  glide_ref->speed = 1.7F;
  facts->motor.valid = true;
  facts->motor.accepted_dx = glide_ref->speed * k_step;
  facts->motor.accepted_vx = glide_ref->speed;

  world.update(k_step);

  auto const* motion =
      walker->get_component<Engine::Core::MotionPresentationComponent>();
  ASSERT_NE(motion, nullptr);
  EXPECT_EQ(motion->state, Engine::Core::MotionPresentationState::Walk);
  EXPECT_FLOAT_EQ(motion->displacement_x, facts->motor.accepted_dx);
  EXPECT_FLOAT_EQ(motion->velocity_x, facts->motor.accepted_vx);
  EXPECT_FLOAT_EQ(motion->speed, facts->motor.accepted_vx);
  EXPECT_EQ(facts->direction_source,
            Engine::Core::MovementDirectionSource::AcceptedVelocity);
}

TEST(MotionPresentationTest, NonTranslatingOrderStatesRemainDistinct) {
  Engine::Core::World world;
  auto* walker = add_walker(world);
  auto* facts = walker->add_component<Engine::Core::MovementFactsComponent>();
  facts->motor.valid = true;
  facts->desired.valid = true;
  facts->desired.tangent_x = 1.0F;
  facts->desired.tangent_z = 0.0F;

  facts->progress.state = Engine::Core::MovementOrderState::Turning;
  world.update(k_step);
  auto const* motion =
      walker->get_component<Engine::Core::MotionPresentationComponent>();
  ASSERT_NE(motion, nullptr);
  EXPECT_EQ(motion->state, Engine::Core::MotionPresentationState::Turning);
  EXPECT_FLOAT_EQ(motion->direction_x, 1.0F);
  EXPECT_EQ(facts->direction_source,
            Engine::Core::MovementDirectionSource::RouteTangent);

  facts->progress.state = Engine::Core::MovementOrderState::Yielding;
  world.update(k_step);
  EXPECT_EQ(motion->state, Engine::Core::MotionPresentationState::Yielding);
  EXPECT_FALSE(motion->has_locomotion());

  facts->progress.state = Engine::Core::MovementOrderState::Recovering;
  world.update(k_step);
  EXPECT_EQ(motion->state, Engine::Core::MotionPresentationState::Recovering);
  EXPECT_FALSE(motion->has_locomotion());
}

TEST(MotionPresentationTest, AStalledUnitWalksAgainOnceItIsFreed) {
  Engine::Core::World world;
  auto glide = std::make_unique<ScriptedGlide>();
  auto* glide_ref = glide.get();
  world.add_system(std::move(glide));
  auto* walker = add_walker(world);
  auto* movement = walker->get_component<Engine::Core::MovementComponent>();
  movement->engage_manual_move(40.0F, 0.0F);

  for (int tick = 0; tick < 30; ++tick) {
    movement->set_manual_velocity(1.7F, 0.0F);
    world.update(k_step);
  }
  ASSERT_EQ(motion_state(*walker), Engine::Core::MotionPresentationState::Idle);

  glide_ref->speed = 1.7F;
  for (int tick = 0; tick < 4; ++tick) {
    movement->set_manual_velocity(1.7F, 0.0F);
    world.update(k_step);
  }

  EXPECT_EQ(motion_state(*walker),
            Engine::Core::MotionPresentationState::ForcedDisplacement);
}

TEST(MotionPresentationTest, AZeroLengthTickKeepsTheGaitItFound) {
  Engine::Core::World world;
  auto glide = std::make_unique<ScriptedGlide>();
  auto* glide_ref = glide.get();
  world.add_system(std::move(glide));
  auto* runner = add_walker(world);
  auto* facts = runner->add_component<Engine::Core::MovementFactsComponent>();
  auto* stamina = runner->add_component<Engine::Core::StaminaComponent>();
  stamina->run_requested = true;
  stamina->is_running = true;
  glide_ref->speed = 4.6F;
  facts->motor.valid = true;
  facts->motor.accepted_dx = glide_ref->speed * k_step;
  facts->motor.accepted_vx = glide_ref->speed;

  world.update(k_step);
  ASSERT_EQ(motion_state(*runner), Engine::Core::MotionPresentationState::Run);

  facts->motor.accepted_dx = 0.0F;
  facts->motor.accepted_vx = 0.0F;
  world.update(0.0F);
  EXPECT_EQ(motion_state(*runner), Engine::Core::MotionPresentationState::Run);
  auto const* motion =
      runner->get_component<Engine::Core::MotionPresentationComponent>();
  EXPECT_FALSE(motion->state_changed);
}

TEST(MotionPresentationTest, RunningOutpacesTheDeclaredWalkingPace) {
  Engine::Core::World world;
  auto* runner = add_walker(world);
  auto* unit = runner->get_component<Engine::Core::UnitComponent>();
  auto* movement = runner->get_component<Engine::Core::MovementComponent>();
  auto* stamina = runner->add_component<Engine::Core::StaminaComponent>();

  MovementTestAccess::set_declared_group_pace(*movement, unit->speed);
  float const walking =
      Game::Systems::formation_navigation_speed(*runner, *unit, stamina);
  stamina->is_running = true;
  float const running =
      Game::Systems::formation_navigation_speed(*runner, *unit, stamina);

  EXPECT_FLOAT_EQ(walking, unit->speed);
  EXPECT_FLOAT_EQ(running,
                  unit->speed * Engine::Core::StaminaComponent::k_run_speed_multiplier);

  MovementTestAccess::set_declared_group_pace(*movement, 1.5F);
  EXPECT_FLOAT_EQ(Game::Systems::formation_navigation_speed(*runner, *unit, stamina),
                  1.5F * Engine::Core::StaminaComponent::k_run_speed_multiplier);
}

} // namespace
