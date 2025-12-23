#pragma once

#include "ai_types.h"
#include <QString>
#include <optional>

namespace Game::Systems::AI {

class AIStrategyFactory {
public:
  // Parse strategy from string (from JSON)
  static auto parse_strategy(const QString &strategy_str) -> AIStrategy;
  
  // Get strategy name for debugging
  static auto strategy_to_string(AIStrategy strategy) -> QString;
  
  // Create configuration based on strategy type
  static auto create_config(AIStrategy strategy) -> AIStrategyConfig;
  
  // Apply personality modifiers to strategy config
  static void apply_personality(AIStrategyConfig &config, 
                                float aggression, 
                                float defense, 
                                float harassment);
};

} // namespace Game::Systems::AI
