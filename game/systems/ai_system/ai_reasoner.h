#pragma once

#include "ai_types.h"

namespace Game::Systems::AI {

class AIReasoner {
public:
  AIReasoner() = default;
  ~AIReasoner() = default;

  AIReasoner(const AIReasoner &) = delete;
  auto operator=(const AIReasoner &) -> AIReasoner & = delete;

  static void update_context(const AISnapshot &snapshot, AIContext &ctx);

  static void update_state_machine(const AISnapshot &snapshot, AIContext &ctx,
                                   float delta_time);

  static void validate_state(AIContext &ctx);
};

} 
