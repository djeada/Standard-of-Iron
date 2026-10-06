#pragma once

#include "../core/component.h"
#include "../core/entity.h"
#include "nature_ai.h"
#include "wildlife_census.h"
#include "wildlife_group.h"

namespace Engine::Core {
class World;
}

namespace Game::Wildlife {

[[nodiscard]] auto
resolve_prey(Engine::Core::World& world,
             Engine::Core::EntityID entity_id,
             float hunter_x,
             float hunter_z,
             const Engine::Core::Entity* hunter = nullptr) -> PreyRef;

class WildlifePredation {
public:
  explicit WildlifePredation(WildlifeStats& stats);

  auto begin_bite(Engine::Core::Entity& entity,
                  Engine::Core::WildlifeComponent& wildlife,
                  const PreyRef& prey,
                  float hunter_x,
                  float hunter_z) -> bool;

  void try_contact_bite(Engine::Core::World& world,
                        const AnimalRef& animal,
                        Engine::Core::WildlifeComponent& wildlife);

  void advance_bite(Engine::Core::World& world,
                    const AnimalRef& animal,
                    Engine::Core::WildlifeComponent& wildlife,
                    float delta_time);

private:
  void land_bite(Engine::Core::World& world,
                 const AnimalRef& animal,
                 const Engine::Core::WildlifeComponent& wildlife);
  void hurt_prey(Engine::Core::World& world,
                 const AnimalRef& animal,
                 const PreyRef& prey,
                 int melee_damage);

  WildlifeStats& m_stats;
};

} // namespace Game::Wildlife
