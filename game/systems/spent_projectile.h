#pragma once
#include <QVector3D>

#include <cstddef>

#include "projectile_kind.h"

namespace Game::Systems {

struct SpentProjectile {
  QVector3D position;
  QVector3D direction{0.0F, -1.0F, 0.0F};
  QVector3D color;
  ProjectileKind kind{ProjectileKind::Arrow};
  float roll_deg{0.0F};
  float scale{1.0F};
  float embed{0.0F};
  float age{0.0F};
  float lifetime{18.0F};
  bool ballista_bolt{false};
};

inline constexpr float k_spent_projectile_fade_seconds = 4.5F;
inline constexpr std::size_t k_max_spent_projectiles = 120;

[[nodiscard]] inline auto
spent_projectile_alpha(const SpentProjectile& spent) -> float {
  float const remaining = spent.lifetime - spent.age;
  if (remaining >= k_spent_projectile_fade_seconds) {
    return 1.0F;
  }
  return remaining <= 0.0F ? 0.0F : remaining / k_spent_projectile_fade_seconds;
}

} // namespace Game::Systems
