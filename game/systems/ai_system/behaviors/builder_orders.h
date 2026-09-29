#pragma once

#include <vector>

#include "../ai_types.h"
#include "builder_pool.h"
#include "builder_stall_watch.h"

namespace Game::Systems::AI {

void order_harvest(const AISnapshot& snapshot,
                   const AIContext& context,
                   ResourceType resource,
                   const SourNodes& sour,
                   BuilderPool& pool,
                   std::vector<AICommand>& out_commands);

void order_repairs(const AISnapshot& snapshot,
                   BuilderPool& pool,
                   int reserve,
                   std::vector<AICommand>& out_commands);

void order_field_work(const AISnapshot& snapshot,
                      const AIContext& context,
                      BuilderPool& pool,
                      int reserve,
                      std::vector<AICommand>& out_commands);

enum class ClearingOutcome {
  Unobstructed,
  Clearing,
  NoBuilder,
};

[[nodiscard]] auto
clear_node_in_the_way(const AISnapshot& snapshot,
                      const SourNodes& sour,
                      BuilderPool& pool,
                      const char* building_type,
                      float site_x,
                      float site_z,
                      std::vector<AICommand>& out_commands) -> ClearingOutcome;

[[nodiscard]] auto order_construction(const AISnapshot& snapshot,
                                      const AIContext& context,
                                      BuilderPool& pool,
                                      const char* building_type,
                                      float site_x,
                                      float site_z,
                                      float rotation_y,
                                      std::vector<AICommand>& out_commands) -> bool;

} // namespace Game::Systems::AI
