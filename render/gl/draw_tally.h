#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace Render::GL {

struct DrawTally {
  static constexpr std::size_t k_slots = 16;
  static constexpr std::size_t k_other = k_slots - 1;

  std::size_t current_type{k_other};
  std::array<std::uint64_t, k_slots> triangles{};
  std::array<std::uint64_t, k_slots> instances{};

  static auto instance() noexcept -> DrawTally& {
    static DrawTally tally;
    return tally;
  }

  void reset() noexcept {
    current_type = k_other;
    triangles.fill(0);
    instances.fill(0);
  }

  void set_type(std::size_t type) noexcept {
    current_type = type < k_other ? type : k_other;
  }

  void note(std::size_t index_count, std::size_t instance_count = 1) noexcept {
    triangles[current_type] +=
        static_cast<std::uint64_t>(index_count / 3) * instance_count;
    instances[current_type] += instance_count;
  }
};

inline void tally_draw(std::size_t index_count,
                       std::size_t instance_count = 1) noexcept {
  DrawTally::instance().note(index_count, instance_count);
}

} // namespace Render::GL
