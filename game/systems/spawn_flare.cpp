#include "spawn_flare.h"

#include <algorithm>
#include <cmath>

#include "../core/component_structures.h"
#include "../core/world.h"
#include "../units/troop_config.h"
#include "building_collision_registry.h"

namespace Game::Systems {

void attach_spawn_flare(Engine::Core::World& world,
                        Engine::Core::EntityID entity_id,
                        Game::Units::SpawnType spawn_type,
                        Engine::Core::SpawnFlareStyle style) {
  auto* entity = world.get_entity(entity_id);
  if (entity == nullptr) {
    return;
  }
  auto* effect = entity->add_component<Engine::Core::ProductionCompletionComponent>();
  effect->style = style;
  effect->duration = Engine::Core::ProductionCompletionComponent::duration_for(style);
  effect->remaining = effect->duration;
  effect->radius = std::max(
      1.0F, Game::Units::TroopConfig::instance().get_selection_ring_size(spawn_type));
  if (entity->has_component<Engine::Core::BuildingComponent>()) {
    const auto size = BuildingCollisionRegistry::get_building_size(spawn_type);
    effect->radius =
        std::max(effect->radius, 0.6F * std::hypot(size.width, size.depth));
  }
}

} // namespace Game::Systems
