#pragma once

#include <QString>

#include "ai_types.h"

namespace Game::Systems::AI::AIStrategyFactory {

auto parse_strategy(const QString& strategy_str) -> AIStrategy;

auto parse_posture(const QString& posture_str,
                   AIPosture fallback = AIPosture::Field) -> AIPosture;

auto strategy_to_string(AIStrategy strategy) -> QString;

auto posture_to_string(AIPosture posture) -> QString;

auto state_to_string(AIState state) -> QString;

auto create_config(AIStrategy strategy) -> AIStrategyConfig;

auto create_config(const AIPlayerProfile& profile) -> AIStrategyConfig;

void apply_personality(AIStrategyConfig& config,
                       float aggression,
                       float defense,
                       float harassment);

void apply_difficulty(AIStrategyConfig& config, const QString& difficulty);

} // namespace Game::Systems::AI::AIStrategyFactory
