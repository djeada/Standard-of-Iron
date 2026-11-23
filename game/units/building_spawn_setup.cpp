#include "building_spawn_setup.h"

#include <string>

#include "../core/component_structures.h"
#include "../core/entity.h"
#include "../visuals/building_asset_key.h"
#include "building_body.h"

namespace Game::Units {

auto add_building_renderable(Engine::Core::Entity& entity,
                             Game::Systems::NationID nation_id,
                             std::string_view building_type,
                             std::string_view visual_style)
    -> Engine::Core::RenderableComponent* {
  auto* renderable = entity.add_component<Engine::Core::RenderableComponent>();
  if (renderable == nullptr) {
    return nullptr;
  }

  renderable->visible = true;
  renderable->renderer_id =
      Game::Visuals::styled_building_asset_key(nation_id, building_type, visual_style);

  auto* building =
      Engine::Core::get_or_add_component<Engine::Core::BuildingComponent>(entity);
  if (building != nullptr) {
    building->original_nation_id = nation_id;
    building->visual_style = std::string(visual_style);
  }

  return renderable;
}

auto building_transform_scale(std::string_view building_type) -> QVector3D {
  auto const scale = building_spawn_scale(building_type);
  return {scale.horizontal, scale.vertical, scale.horizontal};
}

} // namespace Game::Units
