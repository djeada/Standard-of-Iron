#include "building_spawn_setup.h"

#include "../core/component_structures.h"
#include "../core/entity.h"
#include "../visuals/building_asset_key.h"

namespace Game::Units {

auto add_building_renderable(Engine::Core::Entity& entity,
                             Game::Systems::NationID nation_id,
                             std::string_view building_type)
    -> Engine::Core::RenderableComponent* {
  auto* renderable = entity.add_component<Engine::Core::RenderableComponent>();
  if (renderable == nullptr) {
    return nullptr;
  }

  renderable->visible = true;
  renderable->renderer_id = Game::Visuals::building_asset_key(nation_id, building_type);

  auto* building =
      Engine::Core::get_or_add_component<Engine::Core::BuildingComponent>(entity);
  if (building != nullptr) {
    building->original_nation_id = nation_id;
  }

  return renderable;
}

} // namespace Game::Units
