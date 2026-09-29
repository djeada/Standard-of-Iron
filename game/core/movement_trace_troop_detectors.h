#pragma once

#include <cstdint>
#include <vector>

#include "movement_trace_analysis_detail.h"

namespace Engine::Core::movement_analysis {

struct SampleFrame {
  const MovementTroopSample& sample;
  float dt{0.0F};
  float speed{0.0F};
  bool has_previous{false};
  bool new_order{false};
  bool active{false};
  bool locomotion{false};
};

struct DetectorEnv {
  const MovementGateThresholds& thresholds;
  FindingSink& sink;
  MovementEntitySummary& summary;
  float step{0.0F};

  void report(MovementFindingKind kind,
              const MovementTroopSample& sample,
              float magnitude,
              std::string detail) const {
    sink.add(kind, summary.entity_id, 0, sample.tick, magnitude, std::move(detail));
  }

  void report_run(OpenFinding& run,
                  MovementFindingKind kind,
                  const MovementTroopSample& sample,
                  float magnitude,
                  std::string detail) const {
    sink.extend(
        run, kind, summary.entity_id, 0, sample.tick, magnitude, std::move(detail));
  }
};

class StallDetector {
public:
  void on_new_order();
  void observe(const SampleFrame& frame, const DetectorEnv& env);

private:
  OpenFinding m_run;
  float m_stall_seconds{0.0F};
  float m_stall_advance{0.0F};
  float m_turning_seconds{0.0F};
};

class RouteRegressionDetector {
public:
  void on_new_order();
  void observe(const SampleFrame& frame, const DetectorEnv& env);

private:
  OpenFinding m_run;
  float m_min_remaining{0.0F};
  bool m_has_min_remaining{false};
  float m_regression_seconds{0.0F};
  std::uint64_t m_previous_revision{0};
};

class ObstructionDetector {
public:
  void observe(const SampleFrame& frame, const DetectorEnv& env);

private:
  void observe_recovery(const SampleFrame& frame, const DetectorEnv& env);
  void observe_local_block(const SampleFrame& frame, const DetectorEnv& env);
  void observe_rejected_steps(const SampleFrame& frame, const DetectorEnv& env);

  OpenFinding m_recovery_run;
  OpenFinding m_block_run;
  float m_recovering_seconds{0.0F};
  float m_blocked_seconds{0.0F};
  int m_blocked_streak{0};
};

class BodyContactDetector {
public:
  void observe(const SampleFrame& frame, const DetectorEnv& env);

private:
  OpenFinding m_penetration_run;
  OpenFinding m_overlap_run;
  float m_penetration_seconds{0.0F};
  float m_overlap_seconds{0.0F};
};

class RouteChurnDetector {
public:
  void on_new_order();
  void observe(const SampleFrame& frame, const DetectorEnv& env);

private:
  std::uint32_t m_repaths_in_order{0};
  std::uint32_t m_waypoint_regressions{0};
  std::uint32_t m_previous_waypoint_index{0};
  std::uint64_t m_previous_route_id{0};
};

class HeadingDetector {
public:
  void observe(const SampleFrame& frame, const DetectorEnv& env);

private:
  void
  observe_angular_rates(const SampleFrame& frame, const DetectorEnv& env, float delta);
  void
  observe_oscillation(const SampleFrame& frame, const DetectorEnv& env, float delta);

  float m_previous_yaw{0.0F};
  bool m_has_previous_yaw{false};
  bool m_previous_about_faced{false};
  float m_previous_angular_speed{0.0F};
  bool m_has_previous_angular_speed{false};
  float m_previous_heading_delta{0.0F};
  int m_heading_flip_run{0};
};

class DirectionReversalDetector {
public:
  void on_new_order();
  void observe(const SampleFrame& frame, const DetectorEnv& env);

private:
  std::vector<std::uint64_t> m_reversal_ticks;
  bool m_has_previous_direction{false};
  float m_previous_direction_degrees{0.0F};
};

class GaitDetector {
public:
  void observe(const SampleFrame& frame, const DetectorEnv& env);

private:
  OpenFinding m_without_motion_run;
  OpenFinding m_idle_moving_run;
  float m_stopped_seconds{0.0F};
  float m_moving_seconds{0.0F};
};

class ArrivalDetector {
public:
  void observe(const SampleFrame& frame, const DetectorEnv& env);

private:
  MovementOrderState m_previous_state{MovementOrderState::Idle};
  bool m_pending{false};
  float m_seconds{0.0F};
};

class LayoutDetector {
public:
  void observe(const SampleFrame& frame, const DetectorEnv& env);

private:
  void observe_mode_change(const SampleFrame& frame, const DetectorEnv& env);
  void observe_aspect_ratio(const SampleFrame& frame, const DetectorEnv& env);

  TraversalLayoutMode m_previous_mode{TraversalLayoutMode::Normal};
  bool m_has_previous_mode{false};
  std::uint32_t m_mode_changes{0};
  float m_mode_seconds{0.0F};
  std::uint32_t m_previous_portal{0};
};

class StarvationDetector {
public:
  void on_new_order() { m_active_seconds = 0.0F; }
  void observe(const SampleFrame& frame, const DetectorEnv& env);

private:
  float m_active_seconds{0.0F};
};

} // namespace Engine::Core::movement_analysis
