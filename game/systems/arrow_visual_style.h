#pragma once
#include <cstdint>

namespace Game::Systems {

enum class ArrowVisualStyle : std::uint8_t {
  Focused,
  Volley,
  Marker,
  Javelin,

  Aimed,

  Commander,
  CommanderSignature,
};

} // namespace Game::Systems
