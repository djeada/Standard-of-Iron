#pragma once

#include <string>

#include "core/component_economy.h"
#include "systems/resource_types.h"

namespace Engine::Core {
class World;
class Entity;
class BuilderProductionComponent;
} // namespace Engine::Core

namespace Game::Systems::ProductionTasks {

[[nodiscard]] auto is_wall_network_product(const std::string& product_type) -> bool;

void clear_builder_task_target(Engine::Core::World& world,
                               Engine::Core::BuilderProductionComponent* builder,
                               bool release_tree = true);

void load_onto_hauler(
    Engine::Core::Entity* worker,
    ResourceType resource_type,
    int amount,
    Engine::Core::CarriedFoodForm food_form = Engine::Core::CarriedFoodForm::Grain);

void complete_harvest_task(Engine::Core::World& world,
                           Engine::Core::Entity& worker,
                           Engine::Core::BuilderProductionComponent& builder);

} // namespace Game::Systems::ProductionTasks
