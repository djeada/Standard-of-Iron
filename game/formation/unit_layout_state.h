#pragma once

#include <cstdint>

namespace Game::Formation {

enum class UnitLayoutState : std::uint8_t {
  Normal,
  Defensive,
  Attacking,
  Braced,
  Marching,
  Routing,
  Disrupted,
  Working
};

} // namespace Game::Formation
