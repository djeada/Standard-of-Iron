#include "simulation_timing.h"

namespace Engine::Core::Timing {

auto combat_state_update() noexcept -> MicrosecondAccumulator& {
  static MicrosecondAccumulator accumulator;
  return accumulator;
}

} // namespace Engine::Core::Timing
