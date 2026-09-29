#pragma once

namespace Engine::Core {
class World;
class BuilderProductionComponent;
} // namespace Engine::Core

namespace Game::Systems::ProductionTasks {

void show_repair_in_progress(Engine::Core::World& world,
                             const Engine::Core::BuilderProductionComponent& builder);

[[nodiscard]] auto
apply_structure_repair_tick(Engine::Core::World* world,
                            Engine::Core::BuilderProductionComponent* builder) -> bool;

void finish_repair_task(Engine::Core::World& world,
                        Engine::Core::BuilderProductionComponent& builder,
                        int owner_id);

} // namespace Game::Systems::ProductionTasks
