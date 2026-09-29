#pragma once

#include <unordered_set>

#include "ai_types.h"

namespace Game::Systems::AI {

struct FriendlyTally {
  float total_health_ratio = 0.0F;

  const EntitySnapshot* primary_barracks = nullptr;
};

void reset_context_counters(const AISnapshot& snapshot, AIContext& ctx);

void expire_attack_records(const AISnapshot& snapshot,
                           AIContext& ctx,
                           const std::unordered_set<Engine::Core::EntityID>& alive_ids);

[[nodiscard]] auto
tally_friendly_units(const AISnapshot& snapshot,
                     AIContext& ctx,
                     Engine::Core::EntityID previous_primary_barracks) -> FriendlyTally;

[[nodiscard]] auto count_siege_engines(const AISnapshot& snapshot) -> int;

void count_neutral_barracks(const AISnapshot& snapshot, AIContext& ctx);

void count_nearby_threats(const AISnapshot& snapshot, AIContext& ctx);

} // namespace Game::Systems::AI
