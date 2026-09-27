#pragma once

namespace Engine::Core {
class Entity;
}

namespace Game::Systems::DuelSpacing {

[[nodiscard]] auto is_duel_body(const Engine::Core::Entity& entity) -> bool;

struct Standoff {

  float preferred{0.0F};

  float minimum{0.0F};
};

inline constexpr float k_commander_min_separation = 1.15F;
inline constexpr float k_commander_preferred_separation = 1.35F;

[[nodiscard]] auto standoff_between(const Engine::Core::Entity& self,
                                    const Engine::Core::Entity& opponent) -> Standoff;

} // namespace Game::Systems::DuelSpacing
