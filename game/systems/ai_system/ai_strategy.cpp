#include "ai_strategy.h"

#include <algorithm>

namespace Game::Systems::AI {

namespace {

void clamp_style_targets(AIStrategyConfig& config) {
  config.target_builder_count = std::clamp(config.target_builder_count, 1, 8);
  config.base_home_target = std::clamp(config.base_home_target, 2, 12);
  config.desired_barracks_count = std::clamp(config.desired_barracks_count, 1, 6);
  config.desired_defense_tower_count =
      std::clamp(config.desired_defense_tower_count, 0, 10);
  config.desired_catapult_count = std::clamp(config.desired_catapult_count, 0, 5);
  config.desired_assembly_size = std::clamp(config.desired_assembly_size, 2, 12);
  config.reactive_attack_size = std::clamp(config.reactive_attack_size, 1, 12);
  config.proactive_attack_size = std::clamp(config.proactive_attack_size, 2, 14);
  config.reserve_units = std::clamp(config.reserve_units, 0, 6);
  config.harass_units = std::clamp(config.harass_units, 0, 4);
  config.desired_outpost_barracks_count =
      std::clamp(config.desired_outpost_barracks_count, 0, 2);
  config.outpost_home_target = std::clamp(config.outpost_home_target, 0, 2);
  config.assembly_radius = std::clamp(config.assembly_radius, 6.0F, 20.0F);
  config.gather_spacing = std::clamp(config.gather_spacing, 1.0F, 3.0F);
  config.attack_formation_spacing =
      std::clamp(config.attack_formation_spacing, 1.2F, 4.0F);
  config.scouting_distance = std::clamp(config.scouting_distance, 20.0F, 120.0F);
  config.reserve_hold_radius = std::clamp(config.reserve_hold_radius, 4.0F, 18.0F);
  config.expansion_site_distance =
      std::clamp(config.expansion_site_distance, 18.0F, 40.0F);
  config.proactive_attack_size =
      std::max(config.proactive_attack_size, config.reactive_attack_size);
}

} // namespace

auto AIStrategyFactory::parse_strategy(const QString& strategy_str) -> AIStrategy {
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
    config.target_builder_count = 2;
    config.base_home_target = 2;
    config.desired_barracks_count = 3;
    config.desired_defense_tower_count = 1;
    config.desired_catapult_count = 1;
    config.desired_assembly_size = 4;
    config.reactive_attack_size = 2;
    config.proactive_attack_size = 4;
    config.reserve_units = 0;
    config.harass_units = 1;
    config.desired_outpost_barracks_count = 1;
    config.outpost_home_target = 1;
    config.assembly_radius = 10.0F;
    config.gather_spacing = 1.4F;
    config.attack_formation_spacing = 2.2F;
    config.scouting_distance = 48.0F;
    config.reserve_hold_radius = 6.5F;
    config.expansion_site_distance = 32.0F;
    break;

  case AIStrategy::Defensive:

    config.aggression_modifier = 0.5F;
    config.defense_modifier = 1.8F;
    config.expansion_priority = 0.5F;
    config.production_rate_modifier = 1.3F;
    config.min_attack_force = 1.5F;
    config.retreat_threshold = 0.40F;
    config.harassment_range = 0.0F;
    config.target_builder_count = 4;
    config.base_home_target = 3;
    config.desired_barracks_count = 1;
    config.desired_defense_tower_count = 3;
    config.desired_catapult_count = 1;
    config.desired_assembly_size = 6;
    config.reactive_attack_size = 3;
    config.proactive_attack_size = 7;
    config.reserve_units = 2;
    config.harass_units = 0;
    config.desired_outpost_barracks_count = 0;
    config.outpost_home_target = 0;
    config.assembly_radius = 14.0F;
    config.gather_spacing = 1.8F;
    config.attack_formation_spacing = 3.0F;
    config.scouting_distance = 36.0F;
    config.reserve_hold_radius = 11.5F;
    config.expansion_site_distance = 22.0F;
    break;

  case AIStrategy::Expansionist:

    config.aggression_modifier = 0.8F;
    config.defense_modifier = 1.0F;
    config.expansion_priority = 2.0F;
    config.production_rate_modifier = 1.1F;
    config.min_attack_force = 0.8F;
    config.retreat_threshold = 0.30F;
    config.harassment_range = 0.0F;
    config.target_builder_count = 4;
    config.base_home_target = 3;
    config.desired_barracks_count = 2;
    config.desired_defense_tower_count = 1;
    config.desired_catapult_count = 0;
    config.desired_assembly_size = 5;
    config.reactive_attack_size = 2;
    config.proactive_attack_size = 5;
    config.reserve_units = 1;
    config.harass_units = 1;
    config.desired_outpost_barracks_count = 2;
    config.outpost_home_target = 1;
    config.assembly_radius = 13.0F;
    config.gather_spacing = 1.6F;
    config.attack_formation_spacing = 2.5F;
    config.scouting_distance = 44.0F;
    config.reserve_hold_radius = 8.5F;
    config.expansion_site_distance = 34.0F;
    break;

  case AIStrategy::Economic:

    config.aggression_modifier = 0.6F;
    config.defense_modifier = 1.2F;
    config.expansion_priority = 1.0F;
    config.production_rate_modifier = 1.5F;
    config.min_attack_force = 1.8F;
    config.retreat_threshold = 0.35F;
    config.harassment_range = 0.0F;
    config.target_builder_count = 5;
    config.base_home_target = 4;
    config.desired_barracks_count = 2;
    config.desired_defense_tower_count = 1;
    config.desired_catapult_count = 1;
    config.desired_assembly_size = 7;
    config.reactive_attack_size = 3;
    config.proactive_attack_size = 8;
    config.reserve_units = 2;
    config.harass_units = 1;
    config.desired_outpost_barracks_count = 1;
    config.outpost_home_target = 1;
    config.assembly_radius = 15.0F;
    config.gather_spacing = 1.8F;
    config.attack_formation_spacing = 2.8F;
    config.scouting_distance = 40.0F;
    config.reserve_hold_radius = 10.0F;
    config.expansion_site_distance = 28.0F;
    break;

  case AIStrategy::Harasser:

    config.aggression_modifier = 1.3F;
    config.defense_modifier = 0.8F;
    config.expansion_priority = 0.7F;
    config.production_rate_modifier = 1.0F;
    config.min_attack_force = 0.4F;
    config.retreat_threshold = 0.50F;
    config.harassment_range = 60.0F;
    config.target_builder_count = 3;
    config.base_home_target = 2;
    config.desired_barracks_count = 2;
    config.desired_defense_tower_count = 1;
    config.desired_catapult_count = 0;
    config.desired_assembly_size = 3;
    config.reactive_attack_size = 1;
    config.proactive_attack_size = 3;
    config.reserve_units = 0;
    config.harass_units = 2;
    config.desired_outpost_barracks_count = 1;
    config.outpost_home_target = 0;
    config.assembly_radius = 9.0F;
    config.gather_spacing = 1.3F;
    config.attack_formation_spacing = 1.9F;
    config.scouting_distance = 60.0F;
    config.reserve_hold_radius = 5.5F;
    config.expansion_site_distance = 30.0F;
    break;

  case AIStrategy::Rusher:

    config.aggression_modifier = 2.0F;
    config.defense_modifier = 0.5F;
    config.expansion_priority = 0.3F;
    config.production_rate_modifier = 0.9F;
    config.min_attack_force = 0.3F;
    config.retreat_threshold = 0.10F;
    config.harassment_range = 0.0F;
    config.target_builder_count = 2;
    config.base_home_target = 2;
    config.desired_barracks_count = 2;
    config.desired_defense_tower_count = 0;
    config.desired_catapult_count = 0;
    config.desired_assembly_size = 3;
    config.reactive_attack_size = 1;
    config.proactive_attack_size = 3;
    config.reserve_units = 0;
    config.harass_units = 1;
    config.desired_outpost_barracks_count = 0;
    config.outpost_home_target = 0;
    config.assembly_radius = 9.0F;
    config.gather_spacing = 1.2F;
    config.attack_formation_spacing = 1.8F;
    config.scouting_distance = 52.0F;
    config.reserve_hold_radius = 5.0F;
    config.expansion_site_distance = 24.0F;
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
    config.target_builder_count = 3;
    config.base_home_target = 2;
    config.desired_barracks_count = 2;
    config.desired_defense_tower_count = 1;
    config.desired_catapult_count = 1;
    config.desired_assembly_size = 5;
    config.reactive_attack_size = 2;
    config.proactive_attack_size = 5;
    config.reserve_units = 1;
    config.harass_units = 0;
    config.desired_outpost_barracks_count = 1;
    config.outpost_home_target = 1;
    config.assembly_radius = 12.0F;
    config.gather_spacing = 1.5F;
    config.attack_formation_spacing = 2.5F;
    config.scouting_distance = 40.0F;
    config.reserve_hold_radius = 8.0F;
    config.expansion_site_distance = 28.0F;
    break;
  }

  clamp_style_targets(config);
  return config;
}

void AIStrategyFactory::apply_personality(AIStrategyConfig& config,
                                          float aggression,
                                          float defense,
                                          float harassment) {
  config.personality.aggression = aggression;
  config.personality.defense = defense;
  config.personality.harassment = harassment;

  float aggression_factor = (aggression - 0.5F) * 2.0F;
  float defense_factor = (defense - 0.5F) * 2.0F;
  float harassment_factor = (harassment - 0.5F) * 2.0F;

  config.aggression_modifier *= (1.0F + aggression_factor * 0.3F);
  config.defense_modifier *= (1.0F + defense_factor * 0.3F);
  config.retreat_threshold *= (1.0F + defense_factor * 0.2F);

  if (harassment > 0.6F) {
    config.harassment_range += harassment_factor * 30.0F;
  }

  if (aggression > 0.65F) {
    config.desired_barracks_count += 1;
    config.desired_assembly_size -= 1;
    config.reactive_attack_size = std::max(1, config.reactive_attack_size - 1);
    config.proactive_attack_size = std::max(2, config.proactive_attack_size - 1);
    config.desired_outpost_barracks_count += 1;
    config.attack_formation_spacing -= 0.2F;
    config.reserve_hold_radius -= 0.8F;
    config.expansion_site_distance += 4.0F;
  }
  if (defense > 0.65F) {
    config.target_builder_count += 1;
    config.desired_defense_tower_count += 1;
    config.desired_assembly_size += 1;
    config.assembly_radius += 1.5F;
    config.reactive_attack_size += 1;
    config.proactive_attack_size += 1;
    config.reserve_units += 1;
    config.desired_outpost_barracks_count =
        std::max(0, config.desired_outpost_barracks_count - 1);
    config.attack_formation_spacing += 0.3F;
    config.reserve_hold_radius += 1.5F;
    config.expansion_site_distance -= 2.0F;
  }
  if (harassment > 0.65F) {
    config.desired_assembly_size -= 1;
    config.gather_spacing -= 0.1F;
    config.reactive_attack_size = std::max(1, config.reactive_attack_size - 1);
    config.harass_units += 1;
    config.scouting_distance += 10.0F;
    config.attack_formation_spacing -= 0.2F;
    config.reserve_hold_radius -= 0.5F;
    config.expansion_site_distance += 4.0F;
  } else if (harassment < 0.35F) {
    config.harass_units = 0;
  }

  config.aggression_modifier =
      std::max(0.3F, std::min(3.0F, config.aggression_modifier));
  config.defense_modifier = std::max(0.3F, std::min(3.0F, config.defense_modifier));
  config.retreat_threshold = std::max(0.05F, std::min(0.60F, config.retreat_threshold));
  config.harassment_range = std::max(0.0F, std::min(100.0F, config.harassment_range));
  clamp_style_targets(config);
}

void AIStrategyFactory::apply_difficulty(AIStrategyConfig& config,
                                         const QString& difficulty) {
  const QString normalized = difficulty.trimmed().toLower();
  config.difficulty.level = normalized.isEmpty() ? QStringLiteral("normal") : normalized;

  config.difficulty.update_interval_multiplier = 1.0F;
  config.difficulty.production_rate_multiplier = 1.0F;
  config.difficulty.scouting_distance_multiplier = 1.0F;

  if (normalized == "easy") {
    config.difficulty.update_interval_multiplier = 1.35F;
    config.difficulty.production_rate_multiplier = 0.80F;
    config.difficulty.scouting_distance_multiplier = 0.90F;
  } else if (normalized == "hard") {
    config.difficulty.update_interval_multiplier = 0.90F;
    config.difficulty.production_rate_multiplier = 1.10F;
    config.difficulty.scouting_distance_multiplier = 1.10F;
  } else if (normalized == "very_hard") {
    config.difficulty.update_interval_multiplier = 0.75F;
    config.difficulty.production_rate_multiplier = 1.20F;
    config.difficulty.scouting_distance_multiplier = 1.20F;
  }
}

} // namespace Game::Systems::AI
