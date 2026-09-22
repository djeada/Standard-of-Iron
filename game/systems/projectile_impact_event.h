#pragma once
#include <QVector3D>

#include <cstdint>

#include "projectile_kind.h"

namespace Game::Systems {

struct ProjectileImpactEvent {
  std::uint64_t sequence{0};
  QVector3D position;
  QVector3D incoming_direction;
  QVector3D color;
  ProjectileKind kind{ProjectileKind::Arrow};
  float age{0.0F};
  float lifetime{0.65F};
  float scale{1.0F};
  bool ballista_bolt{false};

  bool aimed_shot{false};
  bool hit_target{false};
  bool damage_applied{false};
  std::uint64_t attacker_id{0};
  std::uint64_t target_id{0};
};

} // namespace Game::Systems
