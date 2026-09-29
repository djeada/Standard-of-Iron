#include <algorithm>
#include <cmath>
#include <map>

#include "movement_trace_troop_detectors.h"

namespace Engine::Core::movement_analysis {

namespace {

using TroopSamples = std::vector<const MovementTroopSample*>;

struct EntityDetectors {
  StallDetector stall;
  RouteRegressionDetector regression;
  ObstructionDetector obstruction;
  BodyContactDetector contact;
  RouteChurnDetector churn;
  HeadingDetector heading;
  DirectionReversalDetector reversal;
  GaitDetector gait;
  ArrivalDetector arrival;
  LayoutDetector layout;
  StarvationDetector starvation;

  void begin_order() {
    stall.on_new_order();
    regression.on_new_order();
    churn.on_new_order();
    reversal.on_new_order();
    starvation.on_new_order();
  }

  void observe(const SampleFrame& frame, const DetectorEnv& env) {
    stall.observe(frame, env);
    regression.observe(frame, env);
    obstruction.observe(frame, env);
    contact.observe(frame, env);
    churn.observe(frame, env);
    heading.observe(frame, env);
    reversal.observe(frame, env);
    gait.observe(frame, env);
    arrival.observe(frame, env);
    layout.observe(frame, env);
    starvation.observe(frame, env);
  }
};

void record_summary(MovementEntitySummary& summary, const MovementTroopSample& sample) {
  summary.last_tick = sample.tick;
  ++summary.ticks;
  summary.travelled += std::hypot(sample.accepted_dx, sample.accepted_dz);
  summary.blocked_steps = std::max(summary.blocked_steps, sample.blocked_steps);
  summary.repaths = std::max(summary.repaths, sample.repath_count);
  summary.final_state = sample.state;
}

void finish_entity(MovementEntitySummary& summary,
                   const MovementGateThresholds& thresholds,
                   FindingSink& sink) {
  bool const ended_while_stalled =
      summary.max_stall_seconds > thresholds.progress_stall_window_seconds;
  if (thresholds.require_terminal_outcomes &&
      is_active_movement_state(summary.final_state) && ended_while_stalled) {
    sink.add(MovementFindingKind::MissingTerminalOutcome,
             summary.entity_id,
             0,
             summary.last_tick,
             0.0F,
             text("trace ends with the order still %s",
                  movement_state_name(summary.final_state)));
  }
  summary.reached_terminal = is_terminal_movement_state(summary.final_state) ||
                             summary.final_state == MovementOrderState::Idle;
}

void walk_entity(EntityID entity_id,
                 const TroopSamples& samples,
                 const MovementGateThresholds& thresholds,
                 MovementAnalysis& analysis,
                 FindingSink& sink) {
  float const step = std::max(1.0e-4F, thresholds.fixed_step_seconds);
  MovementEntitySummary summary;
  summary.entity_id = entity_id;
  summary.first_tick = samples.front()->tick;
  std::size_t const findings_before = sink.finding_count();

  DetectorEnv const env{thresholds, sink, summary, step};
  EntityDetectors detectors;
  bool has_previous = false;
  std::uint64_t previous_tick = 0;
  std::uint64_t command_sequence = 0;

  for (auto const* sample_ptr : samples) {
    auto const& sample = *sample_ptr;
    float const dt =
        has_previous
            ? std::max(step, static_cast<float>(sample.tick - previous_tick) * step)
            : step;
    record_summary(summary, sample);

    bool const new_order = sample.command_sequence != command_sequence;
    if (new_order) {
      command_sequence = sample.command_sequence;
      detectors.begin_order();
    }

    SampleFrame const frame{sample,
                            dt,
                            accepted_speed(sample),
                            has_previous,
                            new_order,
                            is_active_movement_state(sample.state),
                            state_claims_travel(sample.presentation_state)};
    detectors.observe(frame, env);

    previous_tick = sample.tick;
    has_previous = true;
  }

  finish_entity(summary, thresholds, sink);
  summary.findings = static_cast<std::uint32_t>(sink.finding_count() - findings_before);
  analysis.entities.push_back(summary);
}

} // namespace

void analyze_troops(const std::vector<MovementTroopSample>& troops,
                    const MovementGateThresholds& thresholds,
                    MovementAnalysis& analysis,
                    FindingSink& sink) {
  std::map<std::pair<std::uint64_t, EntityID>, TroopSamples> by_entity;
  for (auto const& sample : troops) {
    by_entity[{sample.session_id, sample.entity_id}].push_back(&sample);
  }

  for (auto& [key, samples] : by_entity) {
    std::stable_sort(
        samples.begin(),
        samples.end(),
        [](const MovementTroopSample* lhs, const MovementTroopSample* rhs) {
          return lhs->tick < rhs->tick;
        });
    walk_entity(key.second, samples, thresholds, analysis, sink);
  }
}

} // namespace Engine::Core::movement_analysis
