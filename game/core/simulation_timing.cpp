#include "simulation_timing.h"

namespace Engine::Core::Timing {

auto combat_state_update() noexcept -> MicrosecondAccumulator& {
  static MicrosecondAccumulator accumulator;
  return accumulator;
}

auto commander_motor() noexcept -> MicrosecondAccumulator& {
  static MicrosecondAccumulator accumulator;
  return accumulator;
}

auto commander_targeting() noexcept -> MicrosecondAccumulator& {
  static MicrosecondAccumulator accumulator;
  return accumulator;
}

auto commander_weapon_trace() noexcept -> MicrosecondAccumulator& {
  static MicrosecondAccumulator accumulator;
  return accumulator;
}

auto commander_engagement() noexcept -> MicrosecondAccumulator& {
  static MicrosecondAccumulator accumulator;
  return accumulator;
}

auto commander_camera() noexcept -> MicrosecondAccumulator& {
  static MicrosecondAccumulator accumulator;
  return accumulator;
}

auto sample_and_reset_rpg_costs() noexcept -> RpgCostSample {
  RpgCostSample sample;
  sample.motor_us = commander_motor().value();
  sample.targeting_us = commander_targeting().value();
  sample.weapon_trace_us = commander_weapon_trace().value();
  sample.engagement_us = commander_engagement().value();
  sample.camera_us = commander_camera().value();
  commander_motor().reset();
  commander_targeting().reset();
  commander_weapon_trace().reset();
  commander_engagement().reset();
  commander_camera().reset();
  return sample;
}

} // namespace Engine::Core::Timing
