#pragma once

#include <algorithm>
#include <array>
#include <cstddef>

namespace Animation {

template <typename Key, std::size_t N, typename Channel>
[[nodiscard]] auto sample_pose_channel(const std::array<Key, N>& keys,
                                       float phase,
                                       Channel channel) -> float {
  static_assert(N >= 2);
  phase = std::clamp(phase, keys.front().phase, keys.back().phase);
  std::size_t end = 1;
  while (end + 1 < N && phase > keys[end].phase) {
    ++end;
  }
  auto tangent = [&](std::size_t i) {
    if (i == 0 || i + 1 == N) {
      return 0.0F;
    }
    float const before = keys[i].phase - keys[i - 1].phase;
    float const after = keys[i + 1].phase - keys[i].phase;
    float const a = (channel(keys[i]) - channel(keys[i - 1])) / before;
    float const b = (channel(keys[i + 1]) - channel(keys[i])) / after;
    if (a * b <= 0.0F) {
      return 0.0F;
    }
    float const w1 = 2.0F * after + before;
    float const w2 = after + 2.0F * before;
    return (w1 + w2) / (w1 / a + w2 / b);
  };
  float const span = keys[end].phase - keys[end - 1].phase;
  float const t = (phase - keys[end - 1].phase) / span;
  float const t2 = t * t;
  float const t3 = t2 * t;
  return (2.0F * t3 - 3.0F * t2 + 1.0F) * channel(keys[end - 1]) +
         (t3 - 2.0F * t2 + t) * span * tangent(end - 1) +
         (-2.0F * t3 + 3.0F * t2) * channel(keys[end]) +
         (t3 - t2) * span * tangent(end);
}

} // namespace Animation
