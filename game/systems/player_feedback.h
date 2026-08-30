#pragma once

#include "../core/entity_id.h"
#include "resource_types.h"

namespace Engine::Core {
class ProductionComponent;
class UnitComponent;
} // namespace Engine::Core

namespace Game::Systems {

void grant_resource(int owner_id,
                    Engine::Core::EntityID anchor,
                    ResourceType type,
                    int amount);

void grant_harvested_resource(int owner_id,
                              Engine::Core::EntityID anchor,
                              ResourceType type,
                              int amount);

void grant_resources(int owner_id,
                     Engine::Core::EntityID anchor,
                     const ResourceAmounts& amounts);

void grant_resources_at(
    int owner_id, float x, float y, float z, const ResourceAmounts& amounts);

void spend_resources(int owner_id,
                     Engine::Core::EntityID anchor,
                     const ResourceAmounts& cost);

void spend_resources_at(
    int owner_id, float x, float y, float z, const ResourceAmounts& cost);

void trade_resources(int owner_id,
                     Engine::Core::EntityID anchor,
                     ResourceType spent_type,
                     int spent_amount,
                     ResourceType gained_type,
                     int gained_amount);

auto grant_manpower(int owner_id,
                    Engine::Core::EntityID anchor,
                    Engine::Core::ProductionComponent& production,
                    int amount,
                    int ceiling) -> int;

auto spend_manpower(int owner_id,
                    Engine::Core::EntityID anchor,
                    Engine::Core::ProductionComponent& production,
                    int amount) -> int;

auto restore_health(int owner_id,
                    Engine::Core::EntityID target,
                    Engine::Core::UnitComponent& unit,
                    int amount,
                    int ceiling) -> int;

} // namespace Game::Systems
