#pragma once

#include <utility>
#include <vector>

#include "core/entity_id.h"

namespace Engine::Core {
class World;
class BuilderProductionComponent;
} // namespace Engine::Core

namespace Game::Systems::ProductionTasks {

[[nodiscard]] auto
raises_shared_site(const Engine::Core::BuilderProductionComponent& builder) -> bool;

[[nodiscard]] auto
site_progress(const Engine::Core::BuilderProductionComponent& builder) -> float;

[[nodiscard]] auto crew_gather_pace(const Engine::Core::World& world,
                                    Engine::Core::EntityID id) -> float;

using FinishedSites =
    std::vector<std::pair<Engine::Core::EntityID, std::vector<Engine::Core::EntityID>>>;

[[nodiscard]] auto advance_shared_sites(Engine::Core::World& world,
                                        float delta_time) -> FinishedSites;

} // namespace Game::Systems::ProductionTasks
