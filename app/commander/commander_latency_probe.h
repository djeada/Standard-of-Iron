#pragma once

#include <string>
#include <vector>

namespace App::Core {

struct CommanderLatencySample {
  double input_seconds{-1.0};
  double camera_seconds{-1.0};
  double pose_seconds{-1.0};
  double movement_seconds{-1.0};
  double simulation_seconds{-1.0};
  double attack_start_seconds{-1.0};
  double guard_start_seconds{-1.0};
  double dodge_start_seconds{-1.0};

  [[nodiscard]] auto has_input() const -> bool { return input_seconds >= 0.0; }
};

struct CommanderLatencyBudget {
  double frame_seconds{1.0 / 60.0};
  double tick_seconds{1.0 / 60.0};

  double camera_frames{1.0};

  double pose_frames{1.0};
  double pose_ticks{1.0};
  double movement_frames{1.0};
  double movement_ticks{1.0};

  double simulation_ticks{1.0};

  double slack_seconds{1.0e-6};
};

struct CommanderLatencyVerdict {
  bool passed{true};
  std::vector<std::string> failures;
};

class CommanderLatencyProbe {
public:
  void reset();
  void set_time(double seconds) { m_now = seconds; }
  [[nodiscard]] auto now() const -> double { return m_now; }

  void note_input();
  void note_camera_response();
  void note_pose_response();
  void note_movement_response();
  void note_simulation_response();
  void note_attack_start();
  void note_guard_start();
  void note_dodge_start();

  [[nodiscard]] auto sample() const -> const CommanderLatencySample& {
    return m_sample;
  }

private:
  void stamp(double& slot);

  CommanderLatencySample m_sample;
  double m_now{0.0};
};

[[nodiscard]] auto
evaluate(const CommanderLatencySample& sample,
         const CommanderLatencyBudget& budget) -> CommanderLatencyVerdict;

[[nodiscard]] auto describe(const CommanderLatencySample& sample) -> std::string;

} // namespace App::Core
