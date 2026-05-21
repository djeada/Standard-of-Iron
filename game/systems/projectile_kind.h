#pragma once

#include <cstdint>

namespace Game::Systems {

enum class ProjectileKind : std::uint8_t {
  Arrow = 0,
  Fireball,
  CursedArrow,
  Stone
};

[[nodiscard]] constexpr auto
is_cast_projectile_kind(ProjectileKind kind) noexcept -> bool {
  switch (kind) {
  case ProjectileKind::Fireball:
    return true;
  case ProjectileKind::Arrow:
  case ProjectileKind::CursedArrow:
  case ProjectileKind::Stone:
    return false;
  }
  return false;
}

} // namespace Game::Systems
