#pragma once

#include "ai_types.h"
#include <QString>
#include <optional>

namespace Game::Systems::AI {

class AIStrategyFactory {
public:
  static auto parse_strategy(const QString &strategy_str) -> AIStrategy;

  static auto strategy_to_string(AIStrategy strategy) -> QString;

  static auto create_config(AIStrategy strategy) -> AIStrategyConfig;

  static void apply_personality(AIStrategyConfig &config, float aggression,
                                float defense, float harassment);
};

} 
