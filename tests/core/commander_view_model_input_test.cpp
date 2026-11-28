#include <cmath>
#include <gtest/gtest.h>
#include <mutex>

#include "app/commander/commander_control_controller.h"
#include "app/core/client_context.h"
#include "app/input/cursor_mode.h"
#include "app/viewmodels/camera_view_model.h"
#include "app/viewmodels/commander_view_model.h"
#include "app/viewmodels/placement_view_model.h"
#include "game/accessibility/motion_settings.h"
#include "game/core/component_commander.h"
#include "game/core/event_manager.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/combat_actions/combat_action_definition.h"
#include "game/systems/navigation/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/runtime_system_registry.h"
#include "scene/camera.h"

namespace {

class StubClientHost : public App::Core::ClientHost {
public:
  void ensure_initialized() override { ++initialize_calls; }

  auto lock_frame() -> std::unique_lock<std::recursive_mutex> override {
    return std::unique_lock<std::recursive_mutex>(m_frame_mutex);
  }

  void set_cursor_mode(CursorMode mode) override { cursor_mode = mode; }

  int initialize_calls{0};
  CursorMode cursor_mode{CursorMode::Normal};

private:
  std::recursive_mutex m_frame_mutex;
};

class CommanderViewModelInputTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::NavGrid::initialize(32, 32);

    m_context.world = &m_world;
    m_context.local_owner_id = 1;
    m_context.commander_camera = &m_commander_camera;
    m_context.active_camera = &m_commander_camera;

    m_camera_view_model =
        std::make_unique<App::ViewModels::CameraViewModel>(m_context, m_host);
    m_placement_view_model =
        std::make_unique<App::ViewModels::PlacementViewModel>(m_context, m_host);
    m_commander = std::make_unique<App::ViewModels::CommanderViewModel>(
        m_context, m_host, *m_camera_view_model, *m_placement_view_model);
  }

  void TearDown() override {
    m_commander.reset();
    m_placement_view_model.reset();
    m_camera_view_model.reset();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
  }

  auto spawn_commander(float x, float z) -> Engine::Core::Entity* {
    return spawn_commander(m_world, x, z);
  }

  static auto spawn_commander(Engine::Core::World& world,
                              float x,
                              float z) -> Engine::Core::Entity* {
    auto* entity = world.create_entity();
    if (entity == nullptr) {
      return nullptr;
    }
    entity->add_component<Engine::Core::TransformComponent>(x, 0.0F, z);
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    entity->add_component<Engine::Core::MovementComponent>();
    entity->add_component<Engine::Core::CommanderComponent>();
    if (unit == nullptr) {
      return nullptr;
    }
    unit->health = 100;
    unit->max_health = 100;
    unit->owner_id = 1;
    unit->speed = 3.0F;
    unit->spawn_type = Game::Units::SpawnType::Swordsman;
    return entity;
  }

  auto spawn_enemy(float x, float z) -> Engine::Core::Entity* {
    auto* entity = m_world.create_entity();
    if (entity == nullptr) {
      return nullptr;
    }
    entity->add_component<Engine::Core::TransformComponent>(x, 0.0F, z);
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    if (unit == nullptr) {
      return nullptr;
    }
    unit->health = 100;
    unit->max_health = 100;
    unit->owner_id = 2;
    unit->speed = 3.0F;
    unit->spawn_type = Game::Units::SpawnType::Swordsman;
    return entity;
  }

  static auto attack_is_running(Engine::Core::Entity& commander) -> bool {
    auto const* action =
        commander.get_component<Engine::Core::RpgCommanderActionComponent>();
    return action != nullptr && action->action_running;
  }

  Engine::Core::World m_world;
  App::Core::ClientContext m_context;
  StubClientHost m_host;
  Render::GL::Camera m_commander_camera;
  std::unique_ptr<App::ViewModels::CameraViewModel> m_camera_view_model;
  std::unique_ptr<App::ViewModels::PlacementViewModel> m_placement_view_model;
  std::unique_ptr<App::ViewModels::CommanderViewModel> m_commander;
};

constexpr float k_tick = 1.0F / 60.0F;

TEST_F(CommanderViewModelInputTest, EnteringCommanderModeTakesControlOfTheCommander) {
  auto* commander = spawn_commander(0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);

  ASSERT_TRUE(m_commander->available());
  m_commander->enter_mode();

  EXPECT_TRUE(m_commander->active());
  EXPECT_EQ(m_commander->controlled_commander_id(), commander->get_id());
  EXPECT_GT(m_host.initialize_calls, 0);
}

TEST_F(CommanderViewModelInputTest,
       MissionEntryShowsTheViewAtOnceAndTakesControlOnTheFirstTick) {
  Game::Session::SessionContext session;
  const Game::Session::ScopedSession scope(session);
  Engine::Core::World& world = session.world();
  Game::Systems::register_runtime_systems(world);
  session.owners().register_owner_with_id(1, Game::Systems::OwnerType::Player);
  m_context.world = &world;

  auto* commander = spawn_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);
  commander->get_component<Engine::Core::TransformComponent>()->rotation.y = 110.0F;
  auto const* data = commander->get_component<Engine::Core::CommanderComponent>();

  m_commander->enter_mission_mode();

  EXPECT_TRUE(m_commander->active())
      << "the commander view must be up while the load screen still covers it";
  EXPECT_FALSE(data->fpv_controlled) << "no tick has run yet";

  for (int tick = 0; tick < 8 && !data->fpv_controlled; ++tick) {
    session.advance(session.clock().tick_seconds(),
                    1,
                    Game::Session::SessionContext::OverloadPolicy::DiscardBacklog,
                    [&](float seconds) {
                      m_commander->apply_logged_actions();
                      world.update(seconds);
                    });
  }

  EXPECT_TRUE(data->fpv_controlled)
      << "the logged order must still hand the commander to the player";
  EXPECT_TRUE(m_commander->active());
  m_context.world = &m_world;
}

TEST_F(CommanderViewModelInputTest, APressAloneDoesNotReachTheWorld) {
  auto* commander = spawn_commander(0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(spawn_enemy(0.0F, 1.2F), nullptr);

  m_commander->enter_mode();
  ASSERT_TRUE(m_commander->active());
  ASSERT_FALSE(attack_is_running(*commander));

  m_commander->primary_action_down();

  EXPECT_EQ(m_commander->input_edges().primary_press_sequence, 1U);
  EXPECT_EQ(m_commander->input_edges().primary_consumed_sequence, 0U);
  EXPECT_FALSE(attack_is_running(*commander))
      << "the GUI layer records an input edge; simulation is the sole consumer";
}

TEST_F(CommanderViewModelInputTest, MissionEntryHandsOffToArmyViewAtTheCommander) {
  Render::GL::Camera army_camera;
  const QVector3D original_target(-8.0F, 0.0F, 4.0F);
  const QVector3D offset(0.0F, 12.0F, -6.0F);
  army_camera.look_at(original_target + offset, original_target, {0.0F, 1.0F, 0.0F});
  m_context.rts_camera = &army_camera;
  auto* commander = spawn_commander(0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);
  m_commander->enter_mission_mode();
  ASSERT_TRUE(m_commander->active());
  auto* body = commander->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(body, nullptr);
  body->position.x = 5.0F;
  body->position.z = 7.0F;
  m_commander->exit_mode();

  const QVector3D commander_position(5.0F, 0.0F, 7.0F);
  EXPECT_EQ(army_camera.get_target(), commander_position);
  EXPECT_EQ(army_camera.get_position(), commander_position + offset);
}

TEST_F(CommanderViewModelInputTest, SavedDirectControlRestoresViewAndClearsButtons) {
  ASSERT_NE(spawn_commander(0.0F, 0.0F), nullptr);
  const QJsonObject saved{{"active", true},
                          {"yaw", 117.0},
                          {"pitch", -17.0},
                          {"close_camera", true},
                          {"bow", false}};
  m_commander->restore_view_state(saved);
  ASSERT_TRUE(m_commander->active());
  EXPECT_EQ(m_commander->capture_view_state(), saved);
  m_commander->primary_action_down();
  EXPECT_EQ(m_commander->capture_view_state(), saved);

  m_commander->exit_mode();
  m_commander->restore_view_state({{"active", false}});
  EXPECT_FALSE(m_commander->active());
  m_commander->restore_view_state(saved);
  EXPECT_TRUE(m_commander->active());
  EXPECT_EQ(m_commander->capture_view_state(), saved);
  m_commander->update_control_mode(k_tick);
  EXPECT_FALSE(
      attack_is_running(*m_world.get_entity(m_commander->controlled_commander_id())));
}

TEST_F(CommanderViewModelInputTest, FocusLossCancelsAHeldDrawWithoutLoosingAnArrow) {
  using namespace Engine::Core;
  auto* commander = spawn_commander(0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);
  m_commander->enter_mode();
  ASSERT_TRUE(m_commander->active());
  auto* aim = commander->add_component<RpgCommanderAimComponent>();
  aim->stance = FpvWeaponStance::Bow;
  aim->stance_resolved = true;
  aim->draw_stage = BowDrawStage::FullDraw;
  aim->draw_held = true;
  auto* action = commander->get_component<RpgCommanderActionComponent>();
  ASSERT_NE(action, nullptr);
  action->action_running = true;
  action->combat_action_id = static_cast<std::uint8_t>(
      Game::Systems::CombatActions::CombatActionId::RpgBowShot);

  m_commander->release_input();
  EXPECT_EQ(aim->draw_stage, BowDrawStage::FullDraw);
  m_commander->update_control_mode(k_tick);
  EXPECT_EQ(aim->draw_stage, BowDrawStage::None);
  EXPECT_FALSE(action->action_running);
}

TEST_F(CommanderViewModelInputTest,
       OnePhysicalAttackPressProducesOneConsumedAttackEdge) {
  auto* commander = spawn_commander(0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(spawn_enemy(0.0F, 1.2F), nullptr);

  m_commander->enter_mode();
  ASSERT_TRUE(m_commander->active());

  m_commander->primary_action_down();
  m_commander->primary_action_up();
  m_commander->update_control_mode(k_tick);

  auto const& edges = m_commander->input_edges();
  EXPECT_EQ(edges.primary_press_sequence, 1U);
  EXPECT_EQ(edges.primary_release_sequence, 1U);
  EXPECT_EQ(edges.primary_consumed_sequence, 1U);
  EXPECT_EQ(edges.primary_dropped_sequence, 0U);

  for (int tick = 0; tick < 8; ++tick) {
    m_commander->update_control_mode(k_tick);
  }
  EXPECT_EQ(m_commander->input_edges().primary_consumed_sequence, 1U)
      << "a released press may not be consumed a second time";
}

TEST_F(CommanderViewModelInputTest,
       PressAndReleaseBetweenSimulationTicksIsConsumedOnce) {
  auto* commander = spawn_commander(0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);

  m_commander->enter_mode();
  ASSERT_TRUE(m_commander->active());

  for (int press = 0; press < 3; ++press) {
    m_commander->primary_action_down();
    m_commander->primary_action_up();
    m_commander->update_control_mode(k_tick);
    for (int settle = 0; settle < 40; ++settle) {
      m_commander->update_control_mode(k_tick);
    }
  }

  auto const& edges = m_commander->input_edges();
  EXPECT_EQ(edges.primary_press_sequence, 3U);
  EXPECT_EQ(edges.primary_consumed_sequence + edges.primary_dropped_sequence, 3U);
  EXPECT_EQ(edges.primary_dropped_sequence, 0U)
      << "a press that a tick can still see must not be dropped";
}

TEST_F(CommanderViewModelInputTest, ReleasingGuardDoesNotWriteTheWorldOutsideATick) {
  auto* commander = spawn_commander(0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);

  m_commander->enter_mode();
  ASSERT_TRUE(m_commander->active());

  m_commander->secondary_action_down();
  m_commander->update_control_mode(k_tick);
  auto* guard = commander->get_component<Engine::Core::CommanderGuardComponent>();
  ASSERT_NE(guard, nullptr);
  ASSERT_TRUE(guard->active);

  m_commander->secondary_action_up();
  EXPECT_TRUE(guard->active)
      << "the release is an input edge; the tick lowers the guard";

  m_commander->update_control_mode(k_tick);
  EXPECT_FALSE(guard->active);
}

TEST_F(CommanderViewModelInputTest, FocusLossClearsEveryHeldCommanderAction) {
  auto* commander = spawn_commander(0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);
  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);

  m_commander->enter_mode();
  ASSERT_TRUE(m_commander->active());

  m_commander->key_down(Qt::Key_W);
  m_commander->key_down(Qt::Key_Shift);
  m_commander->primary_action_down();
  m_commander->secondary_action_down();
  m_commander->dodge();

  m_commander->sample_frame_intent();
  ASSERT_TRUE(m_commander->frame_intent().has_move());
  ASSERT_TRUE(m_commander->frame_intent().attack_held);
  ASSERT_TRUE(m_commander->frame_intent().guard);

  m_commander->release_input();

  m_commander->sample_frame_intent();
  auto const& intent = m_commander->frame_intent();
  EXPECT_FALSE(intent.has_move());
  EXPECT_FALSE(intent.run);
  EXPECT_FALSE(intent.attack_held);
  EXPECT_FALSE(intent.guard);
  EXPECT_FALSE(intent.dodge_pressed);
  EXPECT_FALSE(intent.jump_pressed);

  auto const& edges = m_commander->input_edges();
  EXPECT_EQ(edges.primary_release_sequence, 1U);
  EXPECT_EQ(edges.guard_release_sequence, 1U);
  EXPECT_EQ(edges.primary_consumed_sequence + edges.primary_dropped_sequence, 1U);
  EXPECT_EQ(edges.dodge_consumed_sequence + edges.dodge_refused_sequence, 1U);

  float const start_z = transform->position.z;
  for (int tick = 0; tick < 30; ++tick) {
    m_commander->update_control_mode(k_tick);
  }
  EXPECT_NEAR(transform->position.z, start_z, 1.0e-3F)
      << "input released on focus loss must not leave the commander walking";
}

TEST_F(CommanderViewModelInputTest, TakingControlDropsTheCameraInsteadOfCuttingToIt) {
  auto* commander = spawn_commander(0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);
  Render::GL::Camera army_camera;
  const QVector3D army_eye(30.0F, 48.0F, -26.0F);
  army_camera.look_at(
      army_eye, QVector3D(0.0F, 0.0F, 0.0F), QVector3D(0.0F, 1.0F, 0.0F));
  m_context.rts_camera = &army_camera;

  m_commander->enter_mode();
  ASSERT_TRUE(m_commander->active());
  EXPECT_LT((m_commander_camera.get_position() - army_eye).length(), 0.5F)
      << "the first frame of commander control is still the army view";

  float seconds = 0.0F;
  float previous_height = m_commander_camera.get_position().y();
  while (seconds < 1.0F) {
    m_commander->update_camera_presentation(k_tick);
    float const height = m_commander_camera.get_position().y();
    EXPECT_LE(height, previous_height + 0.05F);
    previous_height = height;
    seconds += k_tick;
  }
  EXPECT_LT(m_commander_camera.get_position().y(), 6.0F)
      << "a second later the lens is at the commander's shoulder";
  EXPECT_LT(std::hypot(m_commander_camera.get_position().x(),
                       m_commander_camera.get_position().z()),
            8.0F);
  m_context.rts_camera = nullptr;
}

TEST_F(CommanderViewModelInputTest, AWonBattleHoldsTheCommandersViewBeforeTheVerdict) {
  auto* commander = spawn_commander(0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);
  m_commander->enter_mode();
  ASSERT_TRUE(m_commander->active());
  m_commander->update_control_mode(k_tick);
  m_commander->update_camera_presentation(k_tick);
  QVector3D const eye_before = m_commander_camera.get_position();

  m_commander->conclude_for_outcome(QStringLiteral("victory"));
  EXPECT_TRUE(m_commander->active())
      << "the verdict used to throw the player out of the commander's view on the "
         "frame the battle ended";
  EXPECT_EQ(m_commander->finale(), QStringLiteral("victory"));
  EXPECT_LT(m_commander->time_effect_scale(k_tick, false), 1.0F)
      << "the closing beat plays in slow motion";

  m_commander->key_down(Qt::Key_W);
  m_commander->update_control_mode(k_tick);
  auto const* transform = commander->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);
  EXPECT_NEAR(transform->position.z, 0.0F, 1.0e-3F);
  m_commander->toggle_mode();
  EXPECT_TRUE(m_commander->active()) << "the beat cannot be skipped into the army view";

  float seconds = 0.0F;
  while (seconds < App::Core::CommanderFinale::k_duration_seconds + 0.2F) {
    m_commander->update_camera_presentation(k_tick);
    seconds += k_tick;
  }
  EXPECT_GT((m_commander_camera.get_position() - eye_before).length(), 1.0F)
      << "the camera has to have made its turn";

  m_commander->finish_finale();
  EXPECT_FALSE(m_commander->active());
  EXPECT_TRUE(m_commander->finale().isEmpty());
  EXPECT_FLOAT_EQ(m_commander->time_effect_scale(k_tick, false), 1.0F);
}

TEST_F(CommanderViewModelInputTest, TheCommandersDeathIsWatchedNotCutAwayFrom) {
  auto* commander = spawn_commander(3.0F, -2.0F);
  ASSERT_NE(commander, nullptr);
  m_commander->enter_mode();
  ASSERT_TRUE(m_commander->active());
  m_commander->update_control_mode(k_tick);
  m_commander->update_camera_presentation(k_tick);

  auto* unit = commander->get_component<Engine::Core::UnitComponent>();
  ASSERT_NE(unit, nullptr);
  unit->health = 0;
  m_commander->controlled_commander_fell();

  EXPECT_TRUE(m_commander->active());
  EXPECT_EQ(m_commander->finale(), QStringLiteral("fallen"));

  for (int tick = 0; tick < 30; ++tick) {
    m_commander->update_control_mode(k_tick);
    m_commander->update_camera_presentation(k_tick);
  }
  EXPECT_TRUE(m_commander->active());

  m_commander->conclude_for_outcome(QStringLiteral("defeat"));
  EXPECT_EQ(m_commander->finale(), QStringLiteral("fallen"));

  QVector3D const target = m_commander_camera.get_target();
  EXPECT_LT(std::hypot(target.x() - 3.0F, target.z() + 2.0F), 4.0F)
      << "the lens stays with the fallen commander";

  m_commander->finish_finale();
  EXPECT_FALSE(m_commander->active());
}

TEST_F(CommanderViewModelInputTest, LeavingByAnyOtherRouteEndsTheClosingBeat) {
  auto* commander = spawn_commander(0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);
  m_commander->enter_mode();
  m_commander->conclude_for_outcome(QStringLiteral("defeat"));
  ASSERT_EQ(m_commander->finale(), QStringLiteral("defeat"));

  m_commander->exit_mode();
  EXPECT_TRUE(m_commander->finale().isEmpty());
  EXPECT_FALSE(m_commander->active());
  m_commander->finish_finale();
  EXPECT_FALSE(m_commander->active());
}

TEST_F(CommanderViewModelInputTest, ReducedMotionSkipsTheClosingBeat) {
  auto* commander = spawn_commander(0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);
  m_commander->enter_mode();
  Game::Accessibility::MotionSettings::set_reduced_motion(true);
  m_commander->conclude_for_outcome(QStringLiteral("victory"));
  Game::Accessibility::MotionSettings::set_reduced_motion(false);
  EXPECT_TRUE(m_commander->finale().isEmpty());
  EXPECT_FALSE(m_commander->active())
      << "with reduced motion the verdict follows at once, as it always did";
}

TEST_F(CommanderViewModelInputTest, LandingAHitNeverSlowsTheSimulationClock) {
  constexpr float k_tick = 1.0F / 60.0F;
  auto* commander = spawn_commander(0.0F, 0.0F);
  auto* enemy = spawn_enemy(0.0F, 1.4F);
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(enemy, nullptr);
  m_commander->enter_mode();
  ASSERT_TRUE(m_commander->active());

  auto* commander_data = commander->get_component<Engine::Core::CommanderComponent>();
  ASSERT_NE(commander_data, nullptr);

  EXPECT_FLOAT_EQ(m_commander->time_effect_scale(k_tick, false), 1.0F);

  commander_data->just_struck_enemy = true;
  m_commander->update_control_mode(k_tick);

  for (int tick = 0; tick < 30; ++tick) {
    EXPECT_FLOAT_EQ(m_commander->time_effect_scale(k_tick, false), 1.0F)
        << "a landed hit must not scale the simulation clock at tick " << tick;
  }
  EXPECT_TRUE(commander_data->just_struck_enemy)
      << "the hit flag belongs to the combo chain; the view model must not eat it";
}

TEST_F(CommanderViewModelInputTest, TheFightAroundTheCommanderStillShowsItsNumbers) {
  using HitRouting = App::ViewModels::CommanderViewModel::HitRouting;
  auto* commander = spawn_commander(0.0F, 0.0F);
  auto* near_enemy = spawn_enemy(0.0F, 12.0F);
  auto* far_enemy = spawn_enemy(0.0F, 90.0F);
  auto* soldier = spawn_commander(4.0F, 10.0F);
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(near_enemy, nullptr);
  ASSERT_NE(far_enemy, nullptr);
  ASSERT_NE(soldier, nullptr);
  soldier->remove_component<Engine::Core::CommanderComponent>();
  m_commander->enter_mode();
  ASSERT_EQ(m_commander->controlled_commander_id(), commander->get_id());

  const auto hit = [](Engine::Core::Entity* attacker, Engine::Core::Entity* target) {
    return Engine::Core::CombatHitEvent(attacker->get_id(),
                                        target->get_id(),
                                        12,
                                        Game::Units::SpawnType::Swordsman,
                                        false);
  };

  EXPECT_EQ(m_commander->classify_hit(hit(commander, near_enemy)),
            HitRouting::CommanderBurst);
  EXPECT_EQ(m_commander->classify_hit(hit(near_enemy, soldier)), HitRouting::Rts)
      << "a soldier wounded beside the commander shows the red badge";
  EXPECT_EQ(m_commander->classify_hit(hit(soldier, near_enemy)), HitRouting::Rts)
      << "a blow his men land beside him shows the orange badge";
  EXPECT_EQ(m_commander->classify_hit(hit(soldier, far_enemy)), HitRouting::Suppressed)
      << "a fight far off the commander's view stays quiet";
}

TEST_F(CommanderViewModelInputTest, ADeadCommanderLeavesTheModeAndTheCursorBehind) {
  constexpr float k_tick = 1.0F / 60.0F;
  auto* commander = spawn_commander(0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);
  m_commander->enter_mode();
  ASSERT_TRUE(m_commander->active());

  auto* unit = commander->get_component<Engine::Core::UnitComponent>();
  ASSERT_NE(unit, nullptr);
  unit->health = 0;

  m_commander->update_control_mode(k_tick);

  EXPECT_FALSE(m_commander->active())
      << "a dead commander must not keep direct control alive";
  EXPECT_EQ(m_host.cursor_mode, CursorMode::Normal)
      << "death must hand the cursor back";

  auto const* commander_data =
      commander->get_component<Engine::Core::CommanderComponent>();
  ASSERT_NE(commander_data, nullptr);
  EXPECT_FALSE(commander_data->fpv_controlled);
}

TEST_F(CommanderViewModelInputTest, AVanishedCommanderCannotLeaveTheModeActive) {
  constexpr float k_tick = 1.0F / 60.0F;
  auto* commander = spawn_commander(0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);
  auto const commander_id = commander->get_id();
  m_commander->enter_mode();
  ASSERT_TRUE(m_commander->active());

  m_world.destroy_entity(commander_id);
  m_commander->update_control_mode(k_tick);

  EXPECT_FALSE(m_commander->active())
      << "pending removal must not leave a live camera and input pointer";
  EXPECT_EQ(m_host.cursor_mode, CursorMode::Normal);
}

} // namespace
