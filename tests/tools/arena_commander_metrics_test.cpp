#include <algorithm>
#include <functional>
#include <gtest/gtest.h>
#include <vector>

#include "app/commander/commander_presentation_trace.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_type.h"
#include "tools/arena/arena_scenario.h"

namespace {

using Expect = Arena::ArenaExpectationKind;

auto make_host(Engine::Core::World& world) -> Arena::ArenaScenarioHost {
  Arena::ArenaScenarioHost host;
  host.spawn_unit = [&world](const Arena::ArenaScenarioGroup& group,
                             const QVector3D& position) {
    auto* entity = world.create_entity();
    entity->add_component<Engine::Core::TransformComponent>(
        position.x(), position.y(), position.z());
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    unit->owner_id = group.owner_id;
    unit->spawn_type = Game::Units::spawn_typeFromTroopType(group.troop_type);
    unit->health = 100;
    unit->max_health = 100;
    entity->add_component<Engine::Core::MovementComponent>();
    entity->add_component<Engine::Core::AttackComponent>();
    if (Game::Units::is_commander_troop(group.troop_type)) {
      entity->add_component<Engine::Core::CommanderComponent>();
    }
    return entity->get_id();
  };
  host.find_unit = [](Engine::Core::EntityID) -> Game::Units::Unit* {
    return nullptr;
  };
  host.set_camera = [](const auto&, const auto&) {
  };
  host.set_force_full_creature_lod = [](bool) {
  };
  host.configure_rpg_commander = [&world](Engine::Core::EntityID entity_id) {
    auto* entity = world.get_entity(entity_id);
    if (entity == nullptr) {
      return;
    }
    if (auto* rpg =
            Engine::Core::get_or_add_component<Engine::Core::RpgHealthComponent>(
                entity)) {
      rpg->active = true;
    }
    Engine::Core::get_or_add_component<Engine::Core::CommanderGuardComponent>(entity);
  };
  return host;
}

auto commander_definition(std::vector<Arena::ArenaExpectation> expectations)
    -> Arena::ArenaScenarioDefinition {
  Arena::ArenaScenarioDefinition scenario;
  scenario.id = QStringLiteral("commander_metrics_test");
  scenario.label = QStringLiteral("Commander Metrics Test");
  scenario.duration_seconds = 1.0F;
  scenario.rpg_mode = true;
  scenario.rpg_commander_group = QStringLiteral("hero");
  scenario.groups = {{QStringLiteral("hero"),
                      Game::Units::TroopType::RomanFieldCommander,
                      Game::Systems::NationID::RomanRepublic,
                      1,
                      1,
                      1,
                      QVector3D(0.0F, 0.0F, 0.0F),
                      QVector3D(0.0F, 0.0F, 1.0F)}};
  scenario.expectations = std::move(expectations);
  return scenario;
}

auto run_with_trace(
    const std::vector<Arena::ArenaExpectation>& expectations,
    const std::function<void(int, App::Core::CommanderPresentationTrace&)>& shape)
    -> Arena::ArenaScenarioReport {
  Engine::Core::World world;
  auto scenario = commander_definition(expectations);
  Arena::ArenaScenarioRunner runner(world, make_host(world), scenario);
  EXPECT_TRUE(runner.start());

  constexpr float k_dt = 1.0F / 60.0F;
  App::Core::CommanderPresentationTrace trace;
  trace.valid = true;
  trace.camera.valid = true;
  trace.camera.dt = k_dt;
  trace.motor.dt = k_dt;

  for (int frame = 0; frame < 60 && !runner.finished(); ++frame) {
    ++trace.sequence;
    trace.time_seconds = static_cast<float>(frame) * k_dt;
    shape(frame, trace);
    runner.observe_commander_presentation(trace);
    runner.observe_rendered_frame(1.0);
    runner.update(k_dt);
  }
  while (!runner.finished()) {
    runner.update(k_dt);
  }
  return runner.report();
}

auto has_code(const Arena::ArenaScenarioReport& report, const QString& code) -> bool {
  return std::any_of(report.issues.begin(),
                     report.issues.end(),
                     [&code](auto const& issue) { return issue.code == code; });
}

auto metric(Expect kind,
            float threshold = 0.0F,
            float distance = 0.0F) -> Arena::ArenaExpectation {
  Arena::ArenaExpectation result;
  result.kind = kind;
  result.group = QStringLiteral("hero");
  result.threshold = threshold;
  result.distance = distance;
  return result;
}

} // namespace

TEST(ArenaCommanderMetricsTest, SteadyBoomPassesTheContinuityContract) {
  auto const report =
      run_with_trace({metric(Expect::CommanderBoomIsContinuous, 0.35F)},
                     [](int frame, App::Core::CommanderPresentationTrace& trace) {
                       trace.camera.boom_resolved =
                           3.4F + 0.001F * static_cast<float>(frame);
                     });
  EXPECT_FALSE(has_code(report, QStringLiteral("commander_boom_discontinuity")));
  EXPECT_FALSE(has_code(report, QStringLiteral("commander_boom_pumping")));
}

TEST(ArenaCommanderMetricsTest, ImmediateRetractionIsAllowedButPoppingBackOutIsNot) {
  auto const retract =
      run_with_trace({metric(Expect::CommanderBoomIsContinuous, 0.35F)},
                     [](int frame, App::Core::CommanderPresentationTrace& trace) {
                       trace.camera.boom_resolved = frame < 30 ? 3.4F : 0.7F;
                     });
  EXPECT_FALSE(has_code(retract, QStringLiteral("commander_boom_discontinuity")))
      << "retracting to avoid penetration is the camera doing its job";

  auto const extend =
      run_with_trace({metric(Expect::CommanderBoomIsContinuous, 0.35F)},
                     [](int frame, App::Core::CommanderPresentationTrace& trace) {
                       trace.camera.boom_resolved = frame < 30 ? 0.7F : 3.4F;
                     });
  EXPECT_TRUE(has_code(extend, QStringLiteral("commander_boom_discontinuity")))
      << "releasing the boom in one frame is a visible pop";
}

TEST(ArenaCommanderMetricsTest, AlternatingBoomUnderOneObstructionIsPumping) {
  auto const report =
      run_with_trace({metric(Expect::CommanderBoomIsContinuous, 0.35F, 2.0F)},
                     [](int frame, App::Core::CommanderPresentationTrace& trace) {
                       trace.camera.building_blocked_fraction = 0.4F;
                       trace.camera.boom_resolved = (frame % 2 == 0) ? 2.0F : 2.2F;
                     });
  EXPECT_TRUE(has_code(report, QStringLiteral("commander_boom_pumping")));
}

TEST(ArenaCommanderMetricsTest, ViewThatTurnsWithoutLookInputIsUncommanded) {
  auto const quiet =
      run_with_trace({metric(Expect::NoUncommandedViewRotation, 0.05F)},
                     [](int, App::Core::CommanderPresentationTrace& trace) {
                       trace.camera.yaw_velocity = 0.0F;
                       trace.camera.pitch_velocity = 0.0F;
                     });
  EXPECT_FALSE(has_code(quiet, QStringLiteral("commander_view_rotated_uncommanded")));

  auto const drifting =
      run_with_trace({metric(Expect::NoUncommandedViewRotation, 0.05F)},
                     [](int, App::Core::CommanderPresentationTrace& trace) {
                       trace.camera.yaw_velocity = 12.0F;
                     });
  EXPECT_TRUE(has_code(drifting, QStringLiteral("commander_view_rotated_uncommanded")));
}

TEST(ArenaCommanderMetricsTest, TurningUnderLookInputOrALockIsNotUncommanded) {
  auto const steered =
      run_with_trace({metric(Expect::NoUncommandedViewRotation, 0.05F)},
                     [](int, App::Core::CommanderPresentationTrace& trace) {
                       trace.camera.yaw_velocity = 90.0F;
                       trace.input.look_delta_yaw = 1.5F;
                     });
  EXPECT_FALSE(has_code(steered, QStringLiteral("commander_view_rotated_uncommanded")));

  auto const locked =
      run_with_trace({metric(Expect::NoUncommandedViewRotation, 0.05F)},
                     [](int, App::Core::CommanderPresentationTrace& trace) {
                       trace.camera.yaw_velocity = 90.0F;
                       trace.combat.locked_target_id = 7;
                     });
  EXPECT_FALSE(has_code(locked, QStringLiteral("commander_view_rotated_uncommanded")))
      << "a declared lock is allowed to steer the view";
}

TEST(ArenaCommanderMetricsTest, MotorCorrectionAboveTheBudgetIsReported) {
  auto const gentle =
      run_with_trace({metric(Expect::CommanderMotorCorrectionWithin, 0.08F)},
                     [](int, App::Core::CommanderPresentationTrace& trace) {
                       trace.motor.separation_push = 0.02F;
                     });
  EXPECT_FALSE(has_code(gentle, QStringLiteral("commander_motor_correction")));

  auto const shoved =
      run_with_trace({metric(Expect::CommanderMotorCorrectionWithin, 0.08F)},
                     [](int frame, App::Core::CommanderPresentationTrace& trace) {
                       trace.motor.separation_push = frame == 20 ? 0.4F : 0.0F;
                       trace.motor.displacement_source =
                           App::Core::CommanderDisplacementSource::BodySeparation;
                     });
  EXPECT_TRUE(has_code(shoved, QStringLiteral("commander_motor_correction")));

  auto const snapped =
      run_with_trace({metric(Expect::CommanderMotorCorrectionWithin, 0.08F)},
                     [](int frame, App::Core::CommanderPresentationTrace& trace) {
                       trace.motor.snap_back_distance = frame == 30 ? 1.2F : 0.0F;
                       trace.motor.displacement_source =
                           App::Core::CommanderDisplacementSource::JumpRecovery;
                     });
  EXPECT_TRUE(has_code(snapped, QStringLiteral("commander_motor_correction")))
      << "a landing that restores the last walkable position is a correction";
}

TEST(ArenaCommanderMetricsTest, SpeedDiscontinuityIsReportedInAccelerationTerms) {
  auto const smooth =
      run_with_trace({metric(Expect::CommanderSpeedIsContinuous, 4.0F)},
                     [](int frame, App::Core::CommanderPresentationTrace& trace) {
                       float const speed =
                           std::min(2.5F, 0.05F * static_cast<float>(frame));
                       trace.motor.actual_velocity = QVector3D(speed, 0.0F, 0.0F);
                     });
  EXPECT_FALSE(has_code(smooth, QStringLiteral("commander_speed_discontinuity")));

  auto const jolted =
      run_with_trace({metric(Expect::CommanderSpeedIsContinuous, 4.0F)},
                     [](int frame, App::Core::CommanderPresentationTrace& trace) {
                       trace.motor.actual_velocity =
                           QVector3D(frame == 25 ? 6.5F : 0.0F, 0.0F, 0.0F);
                     });
  EXPECT_TRUE(has_code(jolted, QStringLiteral("commander_speed_discontinuity")));
}

TEST(ArenaCommanderMetricsTest, EveryAttackEdgeMustBeConsumedOrDropped) {
  auto const accounted =
      run_with_trace({metric(Expect::CommanderInputEdgesAllConsumed)},
                     [](int frame, App::Core::CommanderPresentationTrace& trace) {
                       if (frame == 10) {
                         trace.input.primary_press_sequence = 1;
                         trace.input.primary_consumed_sequence = 1;
                       }
                     });
  EXPECT_FALSE(
      has_code(accounted, QStringLiteral("commander_attack_edge_unaccounted")));
  EXPECT_FALSE(has_code(accounted, QStringLiteral("commander_attack_edge_dropped")));

  auto const vanished =
      run_with_trace({metric(Expect::CommanderInputEdgesAllConsumed)},
                     [](int frame, App::Core::CommanderPresentationTrace& trace) {
                       if (frame == 10) {
                         trace.input.primary_press_sequence = 3;
                         trace.input.primary_consumed_sequence = 1;
                       }
                     });
  EXPECT_TRUE(has_code(vanished, QStringLiteral("commander_attack_edge_unaccounted")));

  auto const dropped =
      run_with_trace({metric(Expect::CommanderInputEdgesAllConsumed)},
                     [](int frame, App::Core::CommanderPresentationTrace& trace) {
                       if (frame == 10) {
                         trace.input.primary_press_sequence = 2;
                         trace.input.primary_consumed_sequence = 1;
                         trace.input.primary_dropped_sequence = 1;
                       }
                     });
  EXPECT_FALSE(has_code(dropped, QStringLiteral("commander_attack_edge_unaccounted")));
  EXPECT_TRUE(has_code(dropped, QStringLiteral("commander_attack_edge_dropped")))
      << "a press that reaches no consumer is a lost input, not an accounting detail";
}

TEST(ArenaCommanderMetricsTest, DodgeRequestsMustResolveToConsumedOrRefused) {
  auto const report =
      run_with_trace({metric(Expect::CommanderInputEdgesAllConsumed)},
                     [](int frame, App::Core::CommanderPresentationTrace& trace) {
                       if (frame == 10) {
                         trace.input.dodge_request_sequence = 2;
                         trace.input.dodge_consumed_sequence = 1;
                       }
                     });
  EXPECT_TRUE(has_code(report, QStringLiteral("commander_dodge_edge_unaccounted")));
}

TEST(ArenaCommanderMetricsTest, MissingTraceIsReportedRatherThanPassingSilently) {
  Engine::Core::World world;
  auto scenario =
      commander_definition({metric(Expect::CommanderInputEdgesAllConsumed)});
  Arena::ArenaScenarioRunner runner(world, make_host(world), scenario);
  ASSERT_TRUE(runner.start());
  while (!runner.finished()) {
    runner.observe_rendered_frame(1.0);
    runner.update(1.0F / 60.0F);
  }
  EXPECT_TRUE(has_code(runner.report(), QStringLiteral("commander_input_not_traced")))
      << "an expectation with no data behind it must not report as satisfied";
}

TEST(ArenaCommanderMetricsTest, OneActionMayLandOnlyItsAuthoredNumberOfContacts) {
  auto const single =
      run_with_trace({metric(Expect::CommanderContactCountAtMost, 1.0F)},
                     [](int frame, App::Core::CommanderPresentationTrace& trace) {
                       trace.combat.action_running = frame > 10 && frame < 40;
                       trace.combat.action_hit_count = frame > 20 && frame < 40 ? 1 : 0;
                     });
  EXPECT_FALSE(has_code(single, QStringLiteral("commander_contact_multiplicity")));

  auto const doubled =
      run_with_trace({metric(Expect::CommanderContactCountAtMost, 1.0F)},
                     [](int frame, App::Core::CommanderPresentationTrace& trace) {
                       trace.combat.action_running = frame > 10 && frame < 40;
                       trace.combat.action_hit_count = frame > 20 && frame < 40 ? 2 : 0;
                     });
  EXPECT_TRUE(has_code(doubled, QStringLiteral("commander_contact_multiplicity")));
}
