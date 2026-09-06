

#include <cmath>
#include <cstdio>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "app/commander/commander_control_controller.h"
#include "app/commander/commander_motor.h"
#include "game/core/component_gameplay.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/systems/body_profile.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/command_service.h"
#include "game/systems/movement_pipeline.h"
#include "game/systems/movement_system.h"
#include "game/systems/nav_grid.h"
#include "game/systems/pathfinding.h"
#include "game/systems/walkability.h"
#include "scene/camera.h"

namespace {

constexpr float k_dt = 1.0F / 60.0F;

class CommanderSharedTraversalTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::NavGrid::initialize(96, 96);
  }

  void TearDown() override {
    Game::Map::TerrainService::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
  }

  static auto spawn_body(Engine::Core::World& world,
                         float x,
                         float z,
                         int owner_id) -> Engine::Core::Entity* {
    auto* entity = world.create_entity();
    EXPECT_NE(entity, nullptr);
    entity->add_component<Engine::Core::TransformComponent>(x, 0.0F, z);
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    entity->add_component<Engine::Core::MovementComponent>();
    unit->health = 100;
    unit->max_health = 100;
    unit->owner_id = owner_id;
    unit->speed = 2.15F;
    unit->spawn_type = Game::Units::SpawnType::Knight;
    return entity;
  }

  static auto spawn_squad(Engine::Core::World& world,
                          float x,
                          float z,
                          int owner_id) -> Engine::Core::Entity* {
    auto* entity = spawn_body(world, x, z, owner_id);
    entity->get_component<Engine::Core::UnitComponent>()
        ->render_individuals_per_unit_override = 8;
    return entity;
  }

  static auto spawn_commander(Engine::Core::World& world,
                              float x,
                              float z,
                              bool direct_control = true) -> Engine::Core::Entity* {
    auto* entity = spawn_body(world, x, z, 1);
    entity->add_component<Engine::Core::CommanderComponent>()->fpv_controlled =
        direct_control;
    return entity;
  }

  static auto walk_forward(Engine::Core::World& world,
                           Engine::Core::Entity& commander,
                           float seconds) -> float {
    auto* transform = commander.get_component<Engine::Core::TransformComponent>();
    float const start_z = transform->position.z;
    CommanderControlController controller;
    Render::GL::Camera camera;
    auto const frames = static_cast<int>(seconds / k_dt);
    for (int frame = 0; frame < frames; ++frame) {
      controller.input().forward = true;
      EXPECT_TRUE(controller.update(world, commander.get_id(), 1, camera, k_dt));
      world.update(k_dt);
    }
    return transform->position.z - start_z;
  }
};

TEST_F(CommanderSharedTraversalTest, TheCommanderMotorAsksTheSharedWalkabilityLayer) {
  Engine::Core::World world;
  auto* commander = spawn_commander(world, 0.0F, -6.0F);

  auto& registry = Game::Systems::BuildingCollisionRegistry::instance();
  registry.register_building(9001U, "home", 0.0F, 0.0F, 2, 0.0F);
  Game::Systems::Walkability::refresh();

  auto const profile = Game::Systems::body_profile_for(*commander);
  int sampled = 0;
  for (float x = -6.0F; x <= 6.0F; x += 0.5F) {
    for (float z = -6.0F; z <= 6.0F; z += 0.5F) {
      bool const shared =
          Game::Systems::Walkability::can_stand(QVector3D(x, 0.0F, z), profile);
      bool const direct_control =
          App::Core::CommanderMotor::is_walkable_at(*commander, x, z);
      ASSERT_EQ(shared, direct_control)
          << "direct control disagreed with the shared walkability layer at (" << x
          << ", " << z << ")";
      ++sampled;
    }
  }
  EXPECT_GT(sampled, 0);
}

TEST_F(CommanderSharedTraversalTest, SwitchingControlModeDoesNotChangeTraversal) {
  Engine::Core::World world;
  auto* commander = spawn_commander(world, 0.0F, -6.0F);
  auto* commander_data = commander->get_component<Engine::Core::CommanderComponent>();

  auto& registry = Game::Systems::BuildingCollisionRegistry::instance();
  registry.register_building(9001U, "home", 0.0F, 0.0F, 2, 0.0F);
  Game::Systems::Walkability::refresh();

  commander_data->fpv_controlled = false;
  auto const rts_profile = Game::Systems::body_profile_for(*commander);
  commander_data->fpv_controlled = true;
  auto const rpg_profile = Game::Systems::body_profile_for(*commander);

  EXPECT_FLOAT_EQ(rts_profile.radius, rpg_profile.radius);
  EXPECT_EQ(rts_profile.passability, rpg_profile.passability);
  EXPECT_EQ(rts_profile.stops_at_building_facade, rpg_profile.stops_at_building_facade);

  for (float x = -6.0F; x <= 6.0F; x += 0.5F) {
    for (float z = -6.0F; z <= 6.0F; z += 0.5F) {
      commander_data->fpv_controlled = false;
      bool const as_rts = App::Core::CommanderMotor::is_walkable_at(*commander, x, z);
      commander_data->fpv_controlled = true;
      bool const as_rpg = App::Core::CommanderMotor::is_walkable_at(*commander, x, z);
      ASSERT_EQ(as_rts, as_rpg)
          << "control mode changed traversability at (" << x << ", " << z << ")";
    }
  }
}

TEST_F(CommanderSharedTraversalTest, DirectControlSteeringEntersTheSharedPipeline) {
  Engine::Core::World world;
  auto* commander = spawn_commander(world, 0.0F, 0.0F);
  world.add_system(std::make_unique<Game::Systems::MovementPipeline>());

  CommanderControlController controller;
  Render::GL::Camera camera;
  for (int frame = 0; frame < 30; ++frame) {
    controller.input().forward = true;
    ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, k_dt));
    world.update(k_dt);
  }

  auto const* facts = commander->get_component<Engine::Core::MovementFactsComponent>();
  ASSERT_NE(facts, nullptr);
  EXPECT_TRUE(facts->desired.valid)
      << "direct control steering never reached the shared movement facts, so the "
         "shared avoidance and contact stages cannot see the commander at all";
  EXPECT_EQ(facts->desired.source, Engine::Core::DesiredMotionSource::DirectControl);
  EXPECT_GT(std::hypot(facts->desired.velocity_x, facts->desired.velocity_z), 0.1F);
}

TEST_F(CommanderSharedTraversalTest, AStandingCommanderPublishesNoSteeringIntent) {
  Engine::Core::World world;
  auto* commander = spawn_commander(world, 0.0F, 0.0F);
  world.add_system(std::make_unique<Game::Systems::MovementPipeline>());

  CommanderControlController controller;
  Render::GL::Camera camera;
  for (int frame = 0; frame < 30; ++frame) {
    ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, k_dt));
    world.update(k_dt);
  }

  auto const* facts = commander->get_component<Engine::Core::MovementFactsComponent>();
  ASSERT_NE(facts, nullptr);
  EXPECT_FALSE(facts->desired.valid)
      << "a commander who is not walking must anchor the pair so friendly traffic "
         "flows around him instead of shoving him";
}

TEST_F(CommanderSharedTraversalTest, DynamicBodiesAreNotWrittenIntoTheNavigationGrid) {
  Engine::Core::World world;
  auto* pathfinder = Game::Systems::NavGrid::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);

  std::vector<Engine::Core::Entity*> crowd;
  for (float x = -2.0F; x <= 2.0F; x += 1.0F) {
    for (float z = -2.0F; z <= 2.0F; z += 1.0F) {
      crowd.push_back(spawn_squad(world, x, z, 1));
    }
  }
  crowd.push_back(spawn_body(world, 3.0F, 0.0F, 2));
  auto* commander = spawn_commander(world, 0.0F, 0.0F);
  crowd.push_back(commander);

  world.add_system(std::make_unique<Game::Systems::MovementPipeline>());
  world.update(k_dt);
  Game::Systems::Walkability::refresh();

  auto const profile = Game::Systems::body_profile_for(*commander);
  for (auto* body : crowd) {
    auto const* transform = body->get_component<Engine::Core::TransformComponent>();
    ASSERT_NE(transform, nullptr);
    EXPECT_TRUE(Game::Systems::Walkability::can_stand(
        QVector3D(transform->position.x, 0.0F, transform->position.z), profile))
        << "a dynamic body at (" << transform->position.x << ", "
        << transform->position.z << ") turned its own cell into a blocker";
    EXPECT_TRUE(App::Core::CommanderMotor::is_walkable_at(
        *commander, transform->position.x, transform->position.z));
  }
}

TEST_F(CommanderSharedTraversalTest,
       ADenseFriendlyFormationDoesNotWallOffTheCommander) {
  constexpr float k_seconds = 6.0F;

  float open_ground = 0.0F;
  {
    Engine::Core::World world;
    auto* commander = spawn_commander(world, 0.0F, -5.0F);
    world.add_system(std::make_unique<Game::Systems::MovementPipeline>());
    open_ground = walk_forward(world, *commander, k_seconds);
  }
  ASSERT_GT(open_ground, 5.0F) << "the open-ground reference walk did not happen";

  Engine::Core::World world;
  auto* commander = spawn_commander(world, 0.0F, -5.0F);
  for (float x = -3.0F; x <= 3.0F; x += 3.0F) {
    for (float z = -2.0F; z <= 2.0F; z += 2.0F) {
      spawn_squad(world, x, z, 1);
    }
  }
  world.add_system(std::make_unique<Game::Systems::MovementPipeline>());
  float const through_crowd = walk_forward(world, *commander, k_seconds);

  RecordProperty("open_ground_metres", static_cast<int>(open_ground * 1000.0F));
  RecordProperty("through_crowd_metres", static_cast<int>(through_crowd * 1000.0F));
  std::fprintf(stderr,
               "[crowd] open=%.2fm crowd=%.2fm ratio=%.2f\n",
               open_ground,
               through_crowd,
               through_crowd / open_ground);
  EXPECT_GT(through_crowd, open_ground * 0.6F)
      << "the commander advanced " << through_crowd << " m through friendly traffic "
      << "against " << open_ground << " m in the open; dense friendly bodies are "
      << "acting as a navigation wall";
}

TEST_F(CommanderSharedTraversalTest, ACommanderStandingAmongFriendliesIsNotEjected) {
  Engine::Core::World world;
  auto* commander = spawn_commander(world, 0.0F, 0.0F);
  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  for (float x = -1.0F; x <= 1.0F; x += 1.0F) {
    for (float z = -1.0F; z <= 1.0F; z += 1.0F) {
      if (x == 0.0F && z == 0.0F) {
        continue;
      }
      spawn_body(world, x, z, 1);
    }
  }
  world.add_system(std::make_unique<Game::Systems::MovementPipeline>());

  CommanderControlController controller;
  Render::GL::Camera camera;
  for (int frame = 0; frame < 300; ++frame) {
    ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, k_dt));
    world.update(k_dt);
  }

  float const drift = std::hypot(transform->position.x, transform->position.z);
  EXPECT_LT(drift, 1.0F) << "a commander standing still was pushed " << drift
                         << " m out of his own ranks";
}

TEST_F(CommanderSharedTraversalTest, AStaticBlockerIsRespectedIdenticallyInBothModes) {

  auto approach = [](bool direct_control) {
    Engine::Core::World world;
    auto* commander = spawn_commander(world, 0.0F, -6.0F, direct_control);
    auto* transform = commander->get_component<Engine::Core::TransformComponent>();
    auto& registry = Game::Systems::BuildingCollisionRegistry::instance();
    registry.register_building(9001U, "home", 0.0F, 0.0F, 2, 0.0F);
    Game::Systems::Walkability::refresh();
    world.add_system(std::make_unique<Game::Systems::MovementPipeline>());

    if (direct_control) {
      walk_forward(world, *commander, 8.0F);
    } else {
      Game::Systems::CommandService::move_units(
          world, {commander->get_id()}, {QVector3D(0.0F, 0.0F, 0.0F)});
      for (int frame = 0; frame < 480; ++frame) {
        world.update(k_dt);
      }
    }
    struct Outcome {
      float z{0.0F};
      bool legal_as_rts{false};
      bool legal_as_rpg{false};
    };
    auto* commander_data = commander->get_component<Engine::Core::CommanderComponent>();
    Outcome outcome;
    outcome.z = transform->position.z;
    commander_data->fpv_controlled = false;
    outcome.legal_as_rts = App::Core::CommanderMotor::is_walkable_at(
        *commander, transform->position.x, transform->position.z);
    commander_data->fpv_controlled = true;
    outcome.legal_as_rpg = App::Core::CommanderMotor::is_walkable_at(
        *commander, transform->position.x, transform->position.z);
    registry.clear();
    return outcome;
  };

  auto const rpg = approach(true);
  auto const rts = approach(false);

  EXPECT_TRUE(rpg.legal_as_rts)
      << "direct control stopped at z=" << rpg.z << ", which RTS movement would refuse";
  EXPECT_TRUE(rpg.legal_as_rpg);
  EXPECT_TRUE(rts.legal_as_rts);
  EXPECT_TRUE(rts.legal_as_rpg) << "an RTS move order stopped at z=" << rts.z
                                << ", which direct control would refuse";
  EXPECT_GE(rpg.z, rts.z - 0.05F)
      << "direct control stopped further from the house (z=" << rpg.z
      << ") than an RTS order did (z=" << rts.z
      << "); the modes disagree about the same static blocker";
  EXPECT_LT(rpg.z, 0.0F) << "the commander walked into the house";
}

TEST_F(CommanderSharedTraversalTest, ModeSwitchingMidApproachChangesNothing) {
  Engine::Core::World world;
  auto* commander = spawn_commander(world, 0.0F, -6.0F);
  auto* commander_data = commander->get_component<Engine::Core::CommanderComponent>();
  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  auto& registry = Game::Systems::BuildingCollisionRegistry::instance();
  registry.register_building(9001U, "home", 0.0F, 0.0F, 2, 0.0F);
  Game::Systems::Walkability::refresh();
  world.add_system(std::make_unique<Game::Systems::MovementPipeline>());

  CommanderControlController controller;
  Render::GL::Camera camera;
  auto tick = [&](int frames) {
    for (int frame = 0; frame < frames; ++frame) {
      controller.input().forward = true;
      ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, k_dt));
      world.update(k_dt);
    }
  };

  tick(180);
  float const before_switch_z = transform->position.z;

  commander_data->fpv_controlled = false;
  world.update(k_dt);
  float const as_rts_z = transform->position.z;
  EXPECT_NEAR(as_rts_z, before_switch_z, 0.05F)
      << "leaving direct control moved the body by itself";

  commander_data->fpv_controlled = true;
  world.update(k_dt);
  EXPECT_NEAR(transform->position.z, as_rts_z, 0.05F)
      << "re-entering direct control moved the body by itself";

  tick(300);
  EXPECT_TRUE(App::Core::CommanderMotor::is_walkable_at(
      *commander, transform->position.x, transform->position.z));
  EXPECT_LT(transform->position.z, 0.0F) << "the commander walked into the house";

  registry.clear();
}

} // namespace
