#pragma once

#include "core/entity_id.h"

namespace Engine::Core {
class World;
class BuilderProductionComponent;
class MovementComponent;
class TransformComponent;
} // namespace Engine::Core

namespace Game::Systems::ProductionTasks {

[[nodiscard]] auto distance_to_site_edge(
    const Engine::Core::BuilderProductionComponent& builder, float x, float z) -> float;

void reset_site_approach(Engine::Core::BuilderProductionComponent& builder);

void abandon_site_route(const Engine::Core::BuilderProductionComponent& builder,
                        Engine::Core::MovementComponent* movement);

struct SiteApproachActor {
  Engine::Core::EntityID id{0};
  int owner_id{0};
  Engine::Core::TransformComponent* transform{nullptr};
  Engine::Core::MovementComponent* movement{nullptr};
};

void advance_site_approach(Engine::Core::World& world,
                           const SiteApproachActor& actor,
                           Engine::Core::BuilderProductionComponent& builder,
                           float delta_time);

[[nodiscard]] auto
crew_at_posts(Engine::Core::World& world,
              Engine::Core::EntityID id,
              const Engine::Core::BuilderProductionComponent& builder) -> bool;

void settle_crew_at_posts(Engine::Core::World& world,
                          Engine::Core::EntityID id,
                          Engine::Core::BuilderProductionComponent& builder,
                          float delta_time);

} // namespace Game::Systems::ProductionTasks
