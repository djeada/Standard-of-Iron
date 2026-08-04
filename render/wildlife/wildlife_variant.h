#pragma once

#include <QVector3D>

#include <array>
#include <cstddef>
#include <cstdint>

namespace Render::GL {

inline constexpr std::size_t k_wildlife_role_capacity = 12U;

struct WildlifeVariant {
  std::array<QVector3D, k_wildlife_role_capacity> roles{};
  std::uint8_t role_count{0U};
};

} // namespace Render::GL
