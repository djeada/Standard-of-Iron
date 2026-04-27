#pragma once

#include <cstdint>

namespace Render::Creature::Pipeline {

struct BpatPlayback {
  std::uint16_t clip_id{0U};
  std::uint16_t frame_in_clip{0U};
};

inline constexpr std::uint16_t kInvalidBpatClip = 0xFFFFu;

} // namespace Render::Creature::Pipeline
