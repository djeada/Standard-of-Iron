#pragma once

#include <cstdint>
#include <vector>

namespace Game::Audio {

inline constexpr int k_decoded_output_channels = 2;

struct DecodedTrack {
  std::vector<std::int16_t> pcm;
  unsigned frames = 0;
  unsigned channels = k_decoded_output_channels;
};

} // namespace Game::Audio
