#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include "core/entity_id.h"

namespace Engine::Core {
class World;
class BuilderProductionComponent;
enum class BuilderTaskFault : std::uint8_t;
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

auto refund_abandoned_site(Engine::Core::World& world,
                           Engine::Core::EntityID crew) -> bool;

void abandon_paid_site(Engine::Core::World& world,
                       Engine::Core::EntityID crew,
                       int owner_id,
                       Engine::Core::BuilderTaskFault fault);

[[nodiscard]] auto advance_shared_sites(Engine::Core::World& world,
                                        float delta_time) -> FinishedSites;

} // namespace Game::Systems::ProductionTasks
