#include "ai_strategy.h"
#include <algorithm>

namespace Game::Systems::AI {

auto AIStrategyFactory::parse_strategy(const QString &strategy_str)
    -> AIStrategy {
  QString lower = strategy_str.toLower();

  if (lower == "aggressive") {
    return AIStrategy::Aggressive;
  }
  if (lower == "defensive") {
    return AIStrategy::Defensive;
  }
  if (lower == "expansionist") {
    return AIStrategy::Expansionist;
  }
  if (lower == "economic") {
    return AIStrategy::Economic;
  }
  if (lower == "harasser" || lower == "harassment") {
    return AIStrategy::Harasser;
  }
  if (lower == "rusher" || lower == "rush") {
    return AIStrategy::Rusher;
  }

  return AIStrategy::Balanced;
}

auto AIStrategyFactory::strategy_to_string(AIStrategy strategy) -> QString {
  switch (strategy) {
  case AIStrategy::Aggressive:
    return "Aggressive";
  case AIStrategy::Defensive:
    return "Defensive";
  case AIStrategy::Expansionist:
    return "Expansionist";
  case AIStrategy::Economic:
    return "Economic";
  case AIStrategy::Harasser:
    return "Harasser";
  case AIStrategy::Rusher:
    return "Rusher";
  case AIStrategy::Balanced:
  default:
    return "Balanced";
  }
}

auto AIStrategyFactory::create_config(AIStrategy strategy) -> AIStrategyConfig {
  AIStrategyConfig config;
  config.strategy = strategy;

  switch (strategy) {
  case AIStrategy::Aggressive:

    config.aggression_modifier = 1.5F;
    config.defense_modifier = 0.7F;
    config.expansion_priority = 0.8F;
    config.production_rate_modifier = 1.2F;
    config.min_attack_force = 0.6F;
    config.retreat_threshold = 0.15F;
    config.harassment_range = 0.0F;
    break;

  case AIStrategy::Defensive:

    config.aggression_modifier = 0.5F;
    config.defense_modifier = 1.8F;
    config.expansion_priority = 0.5F;
    config.production_rate_modifier = 1.3F;
    config.min_attack_force = 1.5F;
    config.retreat_threshold = 0.40F;
    config.harassment_range = 0.0F;
    break;

  case AIStrategy::Expansionist:

    config.aggression_modifier = 0.8F;
    config.defense_modifier = 1.0F;
    config.expansion_priority = 2.0F;
    config.production_rate_modifier = 1.1F;
    config.min_attack_force = 0.8F;
    config.retreat_threshold = 0.30F;
    config.harassment_range = 0.0F;
    break;

  case AIStrategy::Economic:

    config.aggression_modifier = 0.6F;
    config.defense_modifier = 1.2F;
    config.expansion_priority = 1.0F;
    config.production_rate_modifier = 1.5F;
    config.min_attack_force = 1.8F;
    config.retreat_threshold = 0.35F;
    config.harassment_range = 0.0F;
    break;

  case AIStrategy::Harasser:

    config.aggression_modifier = 1.3F;
    config.defense_modifier = 0.8F;
    config.expansion_priority = 0.7F;
    config.production_rate_modifier = 1.0F;
    config.min_attack_force = 0.4F;
    config.retreat_threshold = 0.50F;
    config.harassment_range = 60.0F;
    break;

  case AIStrategy::Rusher:

    config.aggression_modifier = 2.0F;
    config.defense_modifier = 0.5F;
    config.expansion_priority = 0.3F;
    config.production_rate_modifier = 0.9F;
    config.min_attack_force = 0.3F;
    config.retreat_threshold = 0.10F;
    config.harassment_range = 0.0F;
    break;

  case AIStrategy::Balanced:
  default:

    config.aggression_modifier = 1.0F;
    config.defense_modifier = 1.0F;
    config.expansion_priority = 1.0F;
    config.production_rate_modifier = 1.0F;
    config.min_attack_force = 1.0F;
    config.retreat_threshold = 0.25F;
    config.harassment_range = 0.0F;
    break;
  }

  return config;
}

void AIStrategyFactory::apply_personality(AIStrategyConfig &config,
                                          float aggression, float defense,
                                          float harassment) {

  float aggression_factor = (aggression - 0.5F) * 2.0F;
  float defense_factor = (defense - 0.5F) * 2.0F;
  float harassment_factor = (harassment - 0.5F) * 2.0F;

  config.aggression_modifier *= (1.0F + aggression_factor * 0.3F);
  config.min_attack_force *= (1.0F - aggression_factor * 0.2F);

  config.defense_modifier *= (1.0F + defense_factor * 0.3F);
  config.retreat_threshold *= (1.0F + defense_factor * 0.2F);

  if (harassment > 0.6F) {
    config.harassment_range += harassment_factor * 30.0F;
  }

  config.aggression_modifier =
      std::max(0.3F, std::min(3.0F, config.aggression_modifier));
  config.defense_modifier =
      std::max(0.3F, std::min(3.0F, config.defense_modifier));
  config.min_attack_force =
      std::max(0.2F, std::min(2.5F, config.min_attack_force));
  config.retreat_threshold =
      std::max(0.05F, std::min(0.60F, config.retreat_threshold));
  config.harassment_range =
      std::max(0.0F, std::min(100.0F, config.harassment_range));
}

} // namespace Game::Systems::AI
