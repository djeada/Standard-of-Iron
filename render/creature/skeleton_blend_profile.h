#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>

namespace Render::Creature {

class BoneMask {
public:
  constexpr BoneMask() noexcept = default;
  constexpr explicit BoneMask(std::uint64_t bits) noexcept
      : m_bits(bits) {}

  [[nodiscard]] constexpr auto contains(std::size_t bone) const noexcept -> bool {
    return bone < 64U && ((m_bits >> bone) & 1ULL) != 0ULL;
  }

  [[nodiscard]] constexpr auto bits() const noexcept -> std::uint64_t { return m_bits; }

  [[nodiscard]] constexpr auto empty() const noexcept -> bool { return m_bits == 0ULL; }

  [[nodiscard]] static constexpr auto
  of(std::initializer_list<std::size_t> bones) -> BoneMask {
    std::uint64_t bits = 0ULL;
    for (const std::size_t bone : bones) {
      if (bone < 64U) {
        bits |= (1ULL << bone);
      }
    }
    return BoneMask{bits};
  }

private:
  std::uint64_t m_bits{0ULL};
};

struct SkeletonBlendProfile {
  BoneMask upper_body{};
};

} // namespace Render::Creature
