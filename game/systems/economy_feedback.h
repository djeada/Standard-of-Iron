#pragma once

#include "../core/entity_id.h"
#include "resource_types.h"

namespace Game::Systems {

void publish_resource_feedback(int owner_id,
                               Engine::Core::EntityID anchor,
                               ResourceType type,
                               int amount);

void publish_resource_feedback_at(
    int owner_id, float x, float y, float z, ResourceType type, int amount);

void publish_resource_bundle(int owner_id,
                             Engine::Core::EntityID anchor,
                             const ResourceAmounts& amounts,
                             int sign);

void publish_resource_bundle_at(
    int owner_id, float x, float y, float z, const ResourceAmounts& amounts, int sign);

void publish_trade_feedback(int owner_id,
                            Engine::Core::EntityID anchor,
                            ResourceType spent_type,
                            int spent_amount,
                            ResourceType gained_type,
                            int gained_amount);

void publish_population_feedback(int owner_id,
                                 Engine::Core::EntityID anchor,
                                 int amount);

} // namespace Game::Systems
