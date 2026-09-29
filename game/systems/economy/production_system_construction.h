#pragma once

#include "production_system_shared_site.h"

namespace Engine::Core {
class World;
class Entity;
class BuilderProductionComponent;
class MovementComponent;
class TransformComponent;
class UnitComponent;
} // namespace Engine::Core

namespace Game::Systems::ProductionTasks {

enum class StructureOutcome {
  Skipped,
  NotRaised,
  Raised,
};

[[nodiscard]] auto
raise_structure(Engine::Core::World& world,
                Engine::Core::Entity& builder_entity,
                Engine::Core::BuilderProductionComponent& builder,
                const Engine::Core::TransformComponent& transform,
                const Engine::Core::UnitComponent& unit,
                Engine::Core::MovementComponent* movement,
                const FinishedSites& finishing_sites) -> StructureOutcome;

} // namespace Game::Systems::ProductionTasks
