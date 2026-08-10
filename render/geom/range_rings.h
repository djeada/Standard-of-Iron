#pragma once

#include <QVector3D>

#include <cstdint>
#include <span>

#include "game/systems/attack_range.h"

namespace Render::GL {
class Renderer;
class ResourceManager;

enum class RangeRingPattern : std::uint8_t {
  Solid,
  Dashed,
  Dotted,
  Ticked,
};

struct RangeRingSpec {
  QVector3D center{0.0F, 0.0F, 0.0F};
  float radius{0.0F};
  RangeRingPattern pattern{RangeRingPattern::Solid};
  bool focused{false};
  bool minimum{false};
};

[[nodiscard]] auto range_ring_pattern_for(Game::Systems::RangeWeaponClass weapon_class)
    -> RangeRingPattern;

void render_range_rings(Renderer* renderer,
                        ResourceManager* resources,
                        std::span<const RangeRingSpec> rings);

void render_attack_range_rings(Renderer* renderer,
                               ResourceManager* resources,
                               std::span<const Game::Systems::AttackRangeRing> rings);

} // namespace Render::GL
