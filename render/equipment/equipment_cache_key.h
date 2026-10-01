#pragma once

#include <bit>
#include <cstdint>

namespace Render::GL {

[[nodiscard]] constexpr auto equipment_key(float value) noexcept -> std::int32_t {
  return std::bit_cast<std::int32_t>(value == 0.0F ? 0.0F : value);
}

} // namespace Render::GL
