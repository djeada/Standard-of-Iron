#pragma once

#include "game/core/component_gameplay.h"
#include "game/systems/builder_product_types.h"

namespace Game::Systems {

[[nodiscard]] inline auto
builder_is_idle(const Engine::Core::BuilderProductionComponent* builder) -> bool {
  if (builder == nullptr) {
    return true;
  }
  const bool busy = builder->in_progress || builder->has_construction_site ||
                    builder->has_task_target || builder->structure_task_entity_id != 0;
  if (busy && !builder->product_type.empty()) {
    return false;
  }
  return !builder->has_gather_order && !builder->auto_gather;
}

} // namespace Game::Systems
