#pragma once

namespace Engine::Core {
class World;
class Entity;
class BuilderProductionComponent;
class MovementComponent;
class TransformComponent;
} // namespace Engine::Core

namespace Game::Systems::ProductionTasks {

[[nodiscard]] auto refresh_food_task(Engine::Core::World& world,
                                     Engine::Core::Entity& worker,
                                     Engine::Core::BuilderProductionComponent& builder,
                                     const Engine::Core::TransformComponent* transform,
                                     Engine::Core::MovementComponent* movement) -> bool;

void complete_food_task(Engine::Core::World& world,
                        Engine::Core::Entity& worker,
                        Engine::Core::BuilderProductionComponent& builder);

} // namespace Game::Systems::ProductionTasks
