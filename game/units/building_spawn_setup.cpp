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

auto building_transform_scale(std::string_view building_type) -> QVector3D {
  constexpr float k_home_factor = 1.5F;
  constexpr float k_temple_factor = 1.5F;
  constexpr float k_marketplace_factor = 1.5F;
  constexpr float k_farm_factor = 5.1F;
  if (building_type == "barracks") {
    return {1.8F, 1.2F, 1.8F};
  }
  if (building_type == "home") {
    return {1.2F * k_home_factor, 1.0F * k_home_factor, 1.2F * k_home_factor};
  }
  if (building_type == "temple") {
    return {1.3F * k_temple_factor, 1.15F * k_temple_factor, 1.3F * k_temple_factor};
  }
  if (building_type == "marketplace") {
    return {1.3F * k_marketplace_factor,
            1.0F * k_marketplace_factor,
            1.3F * k_marketplace_factor};
  }
  if (building_type == "farm") {
    return {1.4F * k_farm_factor, 1.0F * k_farm_factor, 1.4F * k_farm_factor};
  }
  if (building_type == "defense_tower") {
    return {1.0F, 2.0F, 1.0F};
  }
  if (building_type == "wall_gate") {
    return {1.5F, 1.5F, 1.5F};
  }
  return {1.0F, 1.0F, 1.0F};
}

} // namespace Game::Units
