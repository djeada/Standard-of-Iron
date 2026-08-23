#include <cmath>
#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "core/movement_trace.h"
#include "core/movement_trace_analysis.h"

using Engine::Core::analyze_movement_trace;
using Engine::Core::MovementDirectionSource;
using Engine::Core::MovementFindingKind;
using Engine::Core::MovementGateThresholds;
using Engine::Core::MovementOrderState;
using Engine::Core::MovementSoldierSample;
using Engine::Core::MovementTrace;
using Engine::Core::MovementTraceManifest;
using Engine::Core::MovementTroopSample;
using Engine::Core::ScopedMovementTrace;
using Engine::Core::TraversalLayoutMode;

namespace {

constexpr float k_step = 1.0F / 60.0F;

auto manifest() -> MovementTraceManifest {
  MovementTraceManifest m;
  m.seed = 1234U;
  m.command_stream = "move(1, 10, 0)";
  m.map_id = "unit_test";
  m.fixed_step_seconds = k_step;
  m.presentation_cap_hz = 60;
  m.build_type = "Debug";
  m.commit = "deadbeef";
  m.graphics_preset = "High";
  m.unit_composition = "1x swordsman";
  m.scenario = "movement_trace_test";
  return m;
}

// A troop walking a straight 10 m line and arriving: the trace every gate is
// compared against.
auto healthy_run(std::uint32_t ticks = 120U) -> std::vector<MovementTroopSample> {
  std::vector<MovementTroopSample> samples;
  float x = 0.0F;
  float const speed = 2.0F;
  for (std::uint32_t tick = 0; tick < ticks; ++tick) {
    MovementTroopSample sample;
    sample.tick = tick;
    sample.entity_id = 7U;
    sample.command_sequence = 1U;
    sample.state = MovementOrderState::Following;
    sample.previous_root_x = x;
    x += speed * k_step;
    sample.root_x = x;
    sample.root_yaw = 90.0F;
    sample.previous_root_yaw = 90.0F;
    sample.accepted_vx = speed;
    sample.accepted_dx = speed * k_step;
    sample.route_advance = speed * k_step;
    sample.remaining_arclength = std::max(0.0F, 10.0F - x);
    sample.order_seconds = static_cast<float>(tick) * k_step;
    sample.presentation_valid = true;
    sample.presentation_state = 1U;
    sample.direction_source = MovementDirectionSource::AcceptedVelocity;
    sample.waypoint_index = 0U;
    sample.waypoint_count = 1U;
    sample.current_files = 3U;
    sample.soldier_body_radius = 0.25F;
    sample.corridor_half_width = 2.0F;
    samples.push_back(sample);
  }
  MovementTroopSample arrival = samples.back();
  arrival.tick = ticks;
  arrival.state = MovementOrderState::Arrived;
  arrival.accepted_vx = 0.0F;
  arrival.accepted_dx = 0.0F;
  arrival.route_advance = 0.0F;
  arrival.remaining_arclength = 0.0F;
  arrival.presentation_valid = true;
  arrival.presentation_state = 0U;
  arrival.direction_source = MovementDirectionSource::BodyForward;
  samples.push_back(arrival);
  return samples;
}

} // namespace

TEST(MovementTraceTest, DisabledSinkRecordsNothing) {
  auto& trace = MovementTrace::instance();
  trace.end_session();
  EXPECT_FALSE(trace.enabled());

  MovementTroopSample sample;
  sample.tick = 1U;
  sample.entity_id = 1U;
  trace.record(sample);
  EXPECT_EQ(trace.troop_sample_count(), 0U);
  EXPECT_TRUE(trace.troop_samples().empty());
}

TEST(MovementTraceTest, MemorySessionCapturesBothStreams) {
  ScopedMovementTrace const session(manifest());
  auto& trace = MovementTrace::instance();
  ASSERT_TRUE(trace.enabled());

  MovementTroopSample troop;
  troop.tick = 3U;
  troop.entity_id = 42U;
  trace.record(troop);

  MovementSoldierSample soldier;
  soldier.frame = 9U;
  soldier.troop_id = 42U;
  soldier.stable_slot = 2U;
  trace.record(soldier);

  EXPECT_EQ(trace.troop_sample_count(), 1U);
  EXPECT_EQ(trace.soldier_sample_count(), 1U);
  ASSERT_EQ(trace.troop_samples().size(), 1U);
  EXPECT_EQ(trace.troop_samples().front().entity_id, 42U);
  EXPECT_EQ(trace.manifest().seed, 1234U);
}

TEST(MovementTraceTest, TroopSampleSurvivesAJsonRoundTrip) {
  MovementTroopSample original;
  original.tick = 987U;
  original.entity_id = 55U;
  original.owner_id = 2;
  original.state = MovementOrderState::Repathing;
  original.root_x = -12.25F;
  original.root_z = 7.5F;
  original.root_yaw = -134.5F;
  original.accepted_vx = 1.75F;
  original.accepted_vz = -0.5F;
  original.remaining_arclength = 31.75F;
  original.waypoint_index = 4U;
  original.waypoint_count = 9U;
  original.repath_count = 3U;
  original.traversal_mode = TraversalLayoutMode::MarchingOrder;
  original.current_files = 2U;
  original.penetration_depth = 0.125F;
  original.soldier_body_radius = 0.3125F;
  original.has_contact = true;
  original.passing_side = -1;
  original.direction_source = MovementDirectionSource::RouteTangent;

  MovementTroopSample parsed;
  ASSERT_TRUE(
      Engine::Core::parse_troop_sample(Engine::Core::to_json(original), parsed));

  EXPECT_EQ(parsed.tick, original.tick);
  EXPECT_EQ(parsed.entity_id, original.entity_id);
  EXPECT_EQ(parsed.owner_id, original.owner_id);
  EXPECT_EQ(parsed.state, original.state);
  EXPECT_FLOAT_EQ(parsed.root_x, original.root_x);
  EXPECT_FLOAT_EQ(parsed.root_z, original.root_z);
  EXPECT_FLOAT_EQ(parsed.root_yaw, original.root_yaw);
  EXPECT_FLOAT_EQ(parsed.accepted_vx, original.accepted_vx);
  EXPECT_FLOAT_EQ(parsed.accepted_vz, original.accepted_vz);
  EXPECT_FLOAT_EQ(parsed.remaining_arclength, original.remaining_arclength);
  EXPECT_EQ(parsed.waypoint_index, original.waypoint_index);
  EXPECT_EQ(parsed.repath_count, original.repath_count);
  EXPECT_EQ(parsed.traversal_mode, original.traversal_mode);
  EXPECT_FLOAT_EQ(parsed.penetration_depth, original.penetration_depth);
  EXPECT_FLOAT_EQ(parsed.soldier_body_radius, original.soldier_body_radius);
  EXPECT_TRUE(parsed.has_contact);
  EXPECT_EQ(parsed.passing_side, -1);
  EXPECT_EQ(parsed.direction_source, original.direction_source);
}

TEST(MovementTraceTest, SoldierSampleSurvivesAJsonRoundTrip) {
  MovementSoldierSample original;
  original.frame = 4242U;
  original.troop_id = 11U;
  original.stable_slot = 17U;
  original.interpolation_alpha = 0.375F;
  original.body_root_x = 3.5F;
  original.body_root_z = -2.25F;
  original.ring_root_x = 3.5F;
  original.ring_root_z = -2.25F;
  original.alive = true;
  original.culled = false;
  original.has_final_anchor = false;
  original.gait_speed = 1.5F;

  MovementSoldierSample parsed;
  ASSERT_TRUE(
      Engine::Core::parse_soldier_sample(Engine::Core::to_json(original), parsed));
  EXPECT_EQ(parsed.frame, original.frame);
  EXPECT_EQ(parsed.troop_id, original.troop_id);
  EXPECT_EQ(parsed.stable_slot, original.stable_slot);
  EXPECT_FLOAT_EQ(parsed.interpolation_alpha, original.interpolation_alpha);
  EXPECT_FLOAT_EQ(parsed.body_root_x, original.body_root_x);
  EXPECT_FLOAT_EQ(parsed.ring_root_z, original.ring_root_z);
  EXPECT_FALSE(parsed.has_final_anchor);
  EXPECT_FLOAT_EQ(parsed.gait_speed, original.gait_speed);
}

TEST(MovementAnalysisTest, CleanRunProducesNoFindings) {
  MovementGateThresholds thresholds;
  thresholds.fixed_step_seconds = k_step;
  auto const analysis = analyze_movement_trace(healthy_run(), {}, thresholds);
  EXPECT_TRUE(analysis.passed()) << Engine::Core::format_movement_findings(analysis);
  ASSERT_EQ(analysis.entities.size(), 1U);
  EXPECT_TRUE(analysis.entities.front().reached_terminal);
}

TEST(MovementAnalysisTest, WalkingInPlaceIsAStallAndAGaitMismatch) {
  std::vector<MovementTroopSample> samples;
  for (std::uint32_t tick = 0; tick < 120U; ++tick) {
    MovementTroopSample sample;
    sample.tick = tick;
    sample.entity_id = 3U;
    sample.command_sequence = 1U;
    sample.state = MovementOrderState::Following;
    sample.remaining_arclength = 12.0F;
    sample.route_advance = 0.0F;
    // Past the launch allowance: this body has had time to accelerate and has
    // not moved.
    sample.order_seconds = 1.0F + static_cast<float>(tick) * k_step;
    sample.accepted_vx = 0.0F;
    sample.accepted_vz = 0.0F;
    // The renderer is told to walk while the motor accepted nothing.
    sample.presentation_valid = true;
    sample.presentation_state = 1U;
    sample.direction_source = MovementDirectionSource::DesiredVelocity;
    sample.desired_vx = 2.0F;
    samples.push_back(sample);
  }

  MovementGateThresholds thresholds;
  thresholds.fixed_step_seconds = k_step;
  auto const analysis = analyze_movement_trace(samples, {}, thresholds);

  EXPECT_FALSE(analysis.passed());
  EXPECT_GE(analysis.count(MovementFindingKind::ProgressStall), 1U);
  EXPECT_GE(analysis.count(MovementFindingKind::GaitWithoutMotion), 1U);
  EXPECT_GE(analysis.count(MovementFindingKind::DirectionSourceNotAccepted), 1U);
  EXPECT_GE(analysis.count(MovementFindingKind::MissingTerminalOutcome), 1U);
}

TEST(MovementAnalysisTest, AlternatingHeadingIsRejected) {
  std::vector<MovementTroopSample> samples;
  float x = 0.0F;
  for (std::uint32_t tick = 0; tick < 60U; ++tick) {
    MovementTroopSample sample;
    sample.tick = tick;
    sample.entity_id = 5U;
    sample.command_sequence = 1U;
    sample.state =
        (tick == 59U) ? MovementOrderState::Arrived : MovementOrderState::Following;
    sample.previous_root_x = x;
    x += 2.0F * k_step;
    sample.root_x = x;
    sample.route_advance = 2.0F * k_step;
    sample.remaining_arclength = std::max(0.0F, 4.0F - x);
    sample.order_seconds = static_cast<float>(tick) * k_step;
    sample.accepted_vx = 2.0F;
    sample.presentation_valid = true;
    sample.presentation_state = (tick == 59U) ? 0U : 1U;
    sample.direction_source = MovementDirectionSource::AcceptedVelocity;
    // Yaw flicks left and right every tick on a straight route.
    sample.root_yaw = (tick % 2U == 0U) ? 84.0F : 96.0F;
    samples.push_back(sample);
  }
  samples.back().accepted_vx = 0.0F;

  MovementGateThresholds thresholds;
  thresholds.fixed_step_seconds = k_step;
  auto const analysis = analyze_movement_trace(samples, {}, thresholds);
  EXPECT_GE(analysis.count(MovementFindingKind::HeadingOscillation), 1U);
}

TEST(MovementAnalysisTest, RingOffTheBodyAnchorIsRejectedAtTinyError) {
  std::vector<MovementSoldierSample> soldiers;
  for (std::uint32_t frame = 0; frame < 10U; ++frame) {
    MovementSoldierSample sample;
    sample.frame = frame;
    sample.troop_id = 2U;
    sample.stable_slot = 1U;
    sample.body_root_x = static_cast<float>(frame) * 0.02F;
    sample.body_root_z = 1.0F;
    sample.shadow_root_x = sample.body_root_x;
    sample.shadow_root_z = sample.body_root_z;
    sample.picking_root_x = sample.body_root_x;
    sample.picking_root_z = sample.body_root_z;
    // One millimetre of ring slip: invisible on screen, still a contract break.
    sample.ring_root_x = sample.body_root_x + 0.001F;
    sample.ring_root_z = sample.body_root_z;
    soldiers.push_back(sample);
  }

  MovementGateThresholds thresholds;
  thresholds.fixed_step_seconds = k_step;
  auto const analysis = analyze_movement_trace({}, soldiers, thresholds);
  EXPECT_GE(analysis.count(MovementFindingKind::MarkerAnchorMismatch), 10U);
  ASSERT_NE(analysis.worst_soldier(), nullptr);
  EXPECT_NEAR(analysis.worst_soldier()->max_marker_error, 0.001F, 1.0e-5F);
}

TEST(MovementAnalysisTest, SoldierSlotSnapIsRejected) {
  std::vector<MovementSoldierSample> soldiers;
  for (std::uint32_t frame = 0; frame < 6U; ++frame) {
    MovementSoldierSample sample;
    sample.frame = frame;
    sample.troop_id = 2U;
    sample.stable_slot = 4U;
    // The formation reflows into a column between frame 2 and 3.
    sample.body_root_x = frame < 3U ? 0.0F : 2.0F;
    sample.body_root_z = 0.0F;
    sample.shadow_root_x = sample.body_root_x;
    sample.picking_root_x = sample.body_root_x;
    sample.ring_root_x = sample.body_root_x;
    soldiers.push_back(sample);
  }

  MovementGateThresholds thresholds;
  thresholds.fixed_step_seconds = k_step;
  auto const analysis = analyze_movement_trace({}, soldiers, thresholds);
  EXPECT_EQ(analysis.count(MovementFindingKind::SoldierAnchorJump), 1U);
}

TEST(MovementAnalysisTest, SingleFileInAWideCorridorIsRejected) {
  auto samples = healthy_run(30U);
  for (auto& sample : samples) {
    sample.current_files = 1U;
  }
  MovementGateThresholds thresholds;
  thresholds.fixed_step_seconds = k_step;
  auto const analysis = analyze_movement_trace(samples, {}, thresholds);
  EXPECT_GE(analysis.count(MovementFindingKind::LayoutAspectRatio), 1U);
}

TEST(MovementAnalysisTest, DigestIsStableAndSensitiveToBehaviour) {
  auto const baseline = healthy_run();
  EXPECT_EQ(Engine::Core::movement_digest(baseline, {}),
            Engine::Core::movement_digest(baseline, {}));

  auto diverged = baseline;
  diverged[40].root_x += 0.5F;
  EXPECT_NE(Engine::Core::movement_digest(baseline, {}),
            Engine::Core::movement_digest(diverged, {}));

  // Presentation-only differences must not move the digest.
  auto presentation_only = baseline;
  for (auto& sample : presentation_only) {
    sample.locomotion_phase += 0.25F;
    sample.presentation_speed += 0.1F;
  }
  EXPECT_EQ(Engine::Core::movement_digest(baseline, {}),
            Engine::Core::movement_digest(presentation_only, {}));
}

TEST(MovementAnalysisTest, TimelineWindowNamesTheFailingTick) {
  auto samples = healthy_run();
  for (std::size_t index = 30; index < 60; ++index) {
    samples[index].order_seconds = 1.0F + static_cast<float>(index) * k_step;
    samples[index].route_advance = 0.0F;
    samples[index].accepted_vx = 0.0F;
    samples[index].remaining_arclength = samples[29].remaining_arclength;
  }

  MovementGateThresholds thresholds;
  thresholds.fixed_step_seconds = k_step;
  auto const analysis = analyze_movement_trace(samples, {}, thresholds);
  ASSERT_FALSE(analysis.passed());
  EXPECT_GE(analysis.first_failing_tick, 30U);

  auto const timeline = Engine::Core::format_movement_timeline(
      samples, 7U, analysis.first_failing_tick, 5U, 5U);
  EXPECT_NE(timeline.find("Following"), std::string::npos);
  EXPECT_FALSE(Engine::Core::format_movement_summary(analysis).empty());
}
