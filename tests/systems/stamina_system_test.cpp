#include "core/component.h"
#include "core/entity.h"
#include "core/world.h"
#include "systems/stamina_system.h"
#include "units/spawn_type.h"
#include <gtest/gtest.h>

using namespace Engine::Core;
using namespace Game::Systems;

class StaminaSystemTest : public ::testing::Test {
protected:
  void SetUp() override { world = std::make_unique<World>(); }

  void TearDown() override { world.reset(); }

  std::unique_ptr<World> world;
  StaminaSystem stamina_system;
};

TEST_F(StaminaSystemTest, StaminaDepletesWhileRunning) {
  auto *unit = world->create_entity();
  unit->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *unit_comp = unit->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  unit_comp->owner_id = 1;
  unit_comp->spawn_type = Game::Units::SpawnType::Archer;

  auto *movement = unit->add_component<MovementComponent>();
  movement->vx = 1.0F;
  movement->vz = 1.0F;

  auto *stamina = unit->add_component<StaminaComponent>();
  stamina->stamina = 100.0F;
  stamina->max_stamina = 100.0F;
  stamina->depletion_rate = 20.0F;
  stamina->run_requested = true;

  stamina_system.update(world.get(), 1.0F);

  EXPECT_TRUE(stamina->is_running);
  EXPECT_FLOAT_EQ(stamina->stamina, 80.0F);
}

TEST_F(StaminaSystemTest, StaminaRegeneratesWhenNotRunning) {
  auto *unit = world->create_entity();
  unit->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *unit_comp = unit->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  unit_comp->owner_id = 1;
  unit_comp->spawn_type = Game::Units::SpawnType::Archer;

  auto *movement = unit->add_component<MovementComponent>();
  movement->vx = 0.0F;
  movement->vz = 0.0F;

  auto *stamina = unit->add_component<StaminaComponent>();
  stamina->stamina = 50.0F;
  stamina->max_stamina = 100.0F;
  stamina->regen_rate = 10.0F;
  stamina->run_requested = false;

  stamina_system.update(world.get(), 1.0F);

  EXPECT_FALSE(stamina->is_running);
  EXPECT_FLOAT_EQ(stamina->stamina, 60.0F);
}

TEST_F(StaminaSystemTest, StaminaDoesNotExceedMax) {
  auto *unit = world->create_entity();
  unit->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *unit_comp = unit->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  unit_comp->owner_id = 1;
  unit_comp->spawn_type = Game::Units::SpawnType::Archer;

  auto *movement = unit->add_component<MovementComponent>();
  movement->vx = 0.0F;
  movement->vz = 0.0F;

  auto *stamina = unit->add_component<StaminaComponent>();
  stamina->stamina = 95.0F;
  stamina->max_stamina = 100.0F;
  stamina->regen_rate = 20.0F;
  stamina->run_requested = false;

  stamina_system.update(world.get(), 1.0F);

  EXPECT_FLOAT_EQ(stamina->stamina, 100.0F);
}

TEST_F(StaminaSystemTest, RunningStopsWhenStaminaDepleted) {
  auto *unit = world->create_entity();
  unit->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *unit_comp = unit->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  unit_comp->owner_id = 1;
  unit_comp->spawn_type = Game::Units::SpawnType::Archer;

  auto *movement = unit->add_component<MovementComponent>();
  movement->vx = 1.0F;
  movement->vz = 1.0F;

  auto *stamina = unit->add_component<StaminaComponent>();
  stamina->stamina = 5.0F;
  stamina->max_stamina = 100.0F;
  stamina->depletion_rate = 20.0F;
  stamina->run_requested = true;
  stamina->is_running = true;

  stamina_system.update(world.get(), 1.0F);

  EXPECT_FALSE(stamina->is_running);
  EXPECT_FLOAT_EQ(stamina->stamina, 0.0F);
}

TEST_F(StaminaSystemTest, CatapultsCannotRun) {
  auto *unit = world->create_entity();
  unit->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *unit_comp = unit->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  unit_comp->owner_id = 1;
  unit_comp->spawn_type = Game::Units::SpawnType::Catapult;

  auto *movement = unit->add_component<MovementComponent>();
  movement->vx = 1.0F;
  movement->vz = 1.0F;

  auto *stamina = unit->add_component<StaminaComponent>();
  stamina->stamina = 100.0F;
  stamina->max_stamina = 100.0F;
  stamina->run_requested = true;

  stamina_system.update(world.get(), 1.0F);

  EXPECT_FALSE(stamina->is_running);
  EXPECT_FALSE(stamina->run_requested);
}

TEST_F(StaminaSystemTest, BallistasCannotRun) {
  auto *unit = world->create_entity();
  unit->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *unit_comp = unit->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  unit_comp->owner_id = 1;
  unit_comp->spawn_type = Game::Units::SpawnType::Ballista;

  auto *movement = unit->add_component<MovementComponent>();
  movement->vx = 1.0F;
  movement->vz = 1.0F;

  auto *stamina = unit->add_component<StaminaComponent>();
  stamina->stamina = 100.0F;
  stamina->max_stamina = 100.0F;
  stamina->run_requested = true;

  stamina_system.update(world.get(), 1.0F);

  EXPECT_FALSE(stamina->is_running);
  EXPECT_FALSE(stamina->run_requested);
}

TEST_F(StaminaSystemTest, InfantryCanRun) {
  auto *unit = world->create_entity();
  unit->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *unit_comp = unit->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  unit_comp->owner_id = 1;
  unit_comp->spawn_type = Game::Units::SpawnType::Knight;

  auto *movement = unit->add_component<MovementComponent>();
  movement->vx = 1.0F;
  movement->vz = 1.0F;

  auto *stamina = unit->add_component<StaminaComponent>();
  stamina->stamina = 100.0F;
  stamina->max_stamina = 100.0F;
  stamina->run_requested = true;

  stamina_system.update(world.get(), 1.0F);

  EXPECT_TRUE(stamina->is_running);
}

TEST_F(StaminaSystemTest, CavalryCanRun) {
  auto *unit = world->create_entity();
  unit->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *unit_comp = unit->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  unit_comp->owner_id = 1;
  unit_comp->spawn_type = Game::Units::SpawnType::MountedKnight;

  auto *movement = unit->add_component<MovementComponent>();
  movement->vx = 1.0F;
  movement->vz = 1.0F;

  auto *stamina = unit->add_component<StaminaComponent>();
  stamina->stamina = 100.0F;
  stamina->max_stamina = 100.0F;
  stamina->run_requested = true;

  stamina_system.update(world.get(), 1.0F);

  EXPECT_TRUE(stamina->is_running);
}

TEST_F(StaminaSystemTest, NoRunningWhenStationary) {
  auto *unit = world->create_entity();
  unit->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *unit_comp = unit->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  unit_comp->owner_id = 1;
  unit_comp->spawn_type = Game::Units::SpawnType::Archer;

  auto *movement = unit->add_component<MovementComponent>();
  movement->vx = 0.0F;
  movement->vz = 0.0F;

  auto *stamina = unit->add_component<StaminaComponent>();
  stamina->stamina = 100.0F;
  stamina->max_stamina = 100.0F;
  stamina->run_requested = true;

  stamina_system.update(world.get(), 1.0F);

  EXPECT_FALSE(stamina->is_running);
  EXPECT_FLOAT_EQ(stamina->stamina, 100.0F);
}
