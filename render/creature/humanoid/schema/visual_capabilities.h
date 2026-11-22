#pragma once

#include <cstdint>

namespace Render::Humanoid {

enum class HumanoidCapability : std::uint32_t {
  LeftHandShield = 1U << 0U,
  TwoHandedWeapon = 1U << 1U,
  FacialHair = 1U << 2U,
  MountedRider = 1U << 3U,
  ConstructionTools = 1U << 4U,
};

class HumanoidCapabilities {
public:
  constexpr HumanoidCapabilities() noexcept = default;
  constexpr explicit HumanoidCapabilities(std::uint32_t bits) noexcept
      : m_bits(bits) {}

  [[nodiscard]] constexpr auto
  contains(HumanoidCapability capability) const noexcept -> bool {
    return (m_bits & static_cast<std::uint32_t>(capability)) != 0U;
  }

  constexpr void add(HumanoidCapability capability) noexcept {
    m_bits |= static_cast<std::uint32_t>(capability);
  }

  [[nodiscard]] constexpr auto bits() const noexcept -> std::uint32_t { return m_bits; }

private:
  std::uint32_t m_bits{0U};
};

} // namespace Render::Humanoid
