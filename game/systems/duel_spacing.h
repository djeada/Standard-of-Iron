#pragma once

namespace Engine::Core {
class Entity;
}

namespace Game::Systems::DuelSpacing {

// Two single bodies fighting one another -- commanders, champions, any unit
// that is not a formation, a horse, an elephant, a building or an animal --
// keep a sword's reach between them. Formations are deliberately left out:
// their melee blob overlaps on purpose.
[[nodiscard]] auto is_duel_body(const Engine::Core::Entity& entity) -> bool;

struct Standoff {
  // Centre-to-centre distance the pair settles at between exchanges.
  float preferred{0.0F};
  // Centre-to-centre distance nothing may push them inside: not footwork,
  // not a strike lunge, not a walk in on the lock.
  float minimum{0.0F};
};

// When either side is a commander the floor rises so the two figures, drawn
// larger than a line soldier, never read as standing inside each other.
inline constexpr float k_commander_min_separation = 1.15F;
inline constexpr float k_commander_preferred_separation = 1.35F;

[[nodiscard]] auto standoff_between(const Engine::Core::Entity& self,
                                    const Engine::Core::Entity& opponent) -> Standoff;

} // namespace Game::Systems::DuelSpacing
