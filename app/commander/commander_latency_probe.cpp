#include "app/commander/commander_latency_probe.h"

#include <sstream>
#include <utility>

namespace App::Core {
namespace {

void check(CommanderLatencyVerdict& verdict,
           const char* label,
           double input_seconds,
           double response_seconds,
           double budget_seconds,
           double slack_seconds) {
  std::ostringstream message;
  if (response_seconds < 0.0) {
    message << label << " never responded";
    verdict.passed = false;
    verdict.failures.push_back(message.str());
    return;
  }
  const double latency = response_seconds - input_seconds;
  if (latency <= budget_seconds + slack_seconds) {
    return;
  }
  message << label << " took " << (latency * 1000.0) << " ms, budget is "
          << (budget_seconds * 1000.0) << " ms";
  verdict.passed = false;
  verdict.failures.push_back(message.str());
}

} // namespace

void CommanderLatencyProbe::reset() {
  m_sample = {};
  m_now = 0.0;
}

void CommanderLatencyProbe::stamp(double& slot) {
  if (slot >= 0.0) {
    return;
  }
  slot = m_now;
}

void CommanderLatencyProbe::note_input() {
  stamp(m_sample.input_seconds);
}

void CommanderLatencyProbe::note_camera_response() {
  if (!m_sample.has_input()) {
    return;
  }
  stamp(m_sample.camera_seconds);
}

void CommanderLatencyProbe::note_pose_response() {
  if (!m_sample.has_input()) {
    return;
  }
  stamp(m_sample.pose_seconds);
}

void CommanderLatencyProbe::note_movement_response() {
  if (!m_sample.has_input()) {
    return;
  }
  stamp(m_sample.movement_seconds);
}

void CommanderLatencyProbe::note_simulation_response() {
  if (!m_sample.has_input()) {
    return;
  }
  stamp(m_sample.simulation_seconds);
}

void CommanderLatencyProbe::note_attack_start() {
  if (!m_sample.has_input()) {
    return;
  }
  stamp(m_sample.attack_start_seconds);
}

void CommanderLatencyProbe::note_guard_start() {
  if (!m_sample.has_input()) {
    return;
  }
  stamp(m_sample.guard_start_seconds);
}

void CommanderLatencyProbe::note_dodge_start() {
  if (!m_sample.has_input()) {
    return;
  }
  stamp(m_sample.dodge_start_seconds);
}

auto evaluate(const CommanderLatencySample& sample,
              const CommanderLatencyBudget& budget) -> CommanderLatencyVerdict {
  CommanderLatencyVerdict verdict;
  if (!sample.has_input()) {
    verdict.passed = false;
    verdict.failures.emplace_back("no commander input was recorded");
    return verdict;
  }

  const double frame = budget.frame_seconds;
  const double tick = budget.tick_seconds;

  check(verdict,
        "camera response",
        sample.input_seconds,
        sample.camera_seconds,
        budget.camera_frames * frame,
        budget.slack_seconds);

  const double pose_budget = (budget.pose_frames * frame) + (budget.pose_ticks * tick);
  if (sample.pose_seconds >= 0.0) {
    check(verdict,
          "pose acknowledgement",
          sample.input_seconds,
          sample.pose_seconds,
          pose_budget,
          budget.slack_seconds);
  }

  const double movement_budget =
      (budget.movement_frames * frame) + (budget.movement_ticks * tick);
  if (sample.movement_seconds >= 0.0) {
    check(verdict,
          "movement presentation",
          sample.input_seconds,
          sample.movement_seconds,
          movement_budget,
          budget.slack_seconds);
  }

  check(verdict,
        "simulation response",
        sample.input_seconds,
        sample.simulation_seconds,
        budget.simulation_ticks * tick,
        budget.slack_seconds);

  for (const auto& [label, stamp_seconds] :
       {std::pair<const char*, double>{"attack start", sample.attack_start_seconds},
        std::pair<const char*, double>{"guard start", sample.guard_start_seconds},
        std::pair<const char*, double>{"dodge start", sample.dodge_start_seconds}}) {
    if (stamp_seconds < 0.0) {
      continue;
    }
    check(verdict,
          label,
          sample.input_seconds,
          stamp_seconds,
          budget.simulation_ticks * tick,
          budget.slack_seconds);
  }

  return verdict;
}

auto describe(const CommanderLatencySample& sample) -> std::string {
  std::ostringstream out;
  out << "input=" << sample.input_seconds << " camera=" << sample.camera_seconds
      << " pose=" << sample.pose_seconds << " movement=" << sample.movement_seconds
      << " simulation=" << sample.simulation_seconds
      << " attack=" << sample.attack_start_seconds
      << " guard=" << sample.guard_start_seconds
      << " dodge=" << sample.dodge_start_seconds;
  return out.str();
}

} // namespace App::Core
