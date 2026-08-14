#pragma once

#include <QVector3D>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace Render {

struct RoleColorPalette {
  static constexpr std::size_t k_capacity = 32U;

  std::array<QVector3D, k_capacity> colors{};
  std::uint8_t count{0U};

  [[nodiscard]] auto view() const noexcept -> std::span<const QVector3D> {
    return {colors.data(), static_cast<std::size_t>(count)};
  }
};

} // namespace Render
