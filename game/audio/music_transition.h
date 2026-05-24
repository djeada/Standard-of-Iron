#pragma once

#include <cstdint>

namespace Game::Audio {

enum class MusicTransition : std::uint8_t {
  Immediate,
  Crossfade
};

} // namespace Game::Audio
