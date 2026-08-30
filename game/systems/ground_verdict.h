#pragma once

#include <cstdint>

namespace Game::Systems {

enum class GroundVerdict : std::uint8_t {
  Clear,

  Occupied,

  Impassable,

  Water,

  Uneven,

  OffMap,
};

} // namespace Game::Systems
