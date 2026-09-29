#pragma once

namespace Engine::Core {
class World;
class Entity;
class BuilderProductionComponent;
} // namespace Engine::Core

namespace Game::Systems::ProductionTasks {

void drop_lost_wall_site(Engine::Core::World& world,
                         Engine::Core::BuilderProductionComponent& builder);

void assign_queued_wall_site_if_idle(Engine::Core::World& world,
                                     Engine::Core::Entity& builder_entity,
                                     Engine::Core::BuilderProductionComponent& builder);

[[nodiscard]] auto
skip_invalid_wall_site(Engine::Core::World* world,
                       Engine::Core::Entity* builder_entity,
                       Engine::Core::BuilderProductionComponent* builder) -> bool;

void publish_wall_progress(Engine::Core::World& world,
                           const Engine::Core::BuilderProductionComponent& builder);

} // namespace Game::Systems::ProductionTasks
