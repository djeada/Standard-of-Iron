

#include <gtest/gtest.h>
#include <set>
#include <utility>

#include "game/systems/ai_system/ai_commander_doctrine.h"
#include "game/systems/ai_system/ai_strategy.h"
#include "game/units/commander_catalog.h"

namespace {

using Game::Systems::AI::AIPosture;
using Game::Systems::AI::AIStrategy;
using Game::Systems::AI::doctrine_profile_for_troop;

TEST(AICommanderDoctrine, EveryCommanderAuthorsADoctrine) {
  const auto& definitions = Game::Units::all_commander_definitions();
  ASSERT_FALSE(definitions.empty());

  for (const auto& definition : definitions) {
    EXPECT_TRUE(definition.doctrine.is_authored())
        << definition.id << " has no AI doctrine";
    const auto profile = doctrine_profile_for_troop(definition.troop_type);
    ASSERT_TRUE(profile.has_value()) << definition.id;
    EXPECT_GE(profile->personality.aggression, 0.0F) << definition.id;
    EXPECT_LE(profile->personality.aggression, 1.0F) << definition.id;
    EXPECT_GE(profile->personality.defense, 0.0F) << definition.id;
    EXPECT_LE(profile->personality.defense, 1.0F) << definition.id;
    EXPECT_GE(profile->personality.harassment, 0.0F) << definition.id;
    EXPECT_LE(profile->personality.harassment, 1.0F) << definition.id;
  }
}

TEST(AICommanderDoctrine, DoctrineStringsParseToRealStrategies) {
  for (const auto& definition : Game::Units::all_commander_definitions()) {
    const auto profile = doctrine_profile_for_troop(definition.troop_type);
    ASSERT_TRUE(profile.has_value()) << definition.id;

    const auto round_tripped = Game::Systems::AI::AIStrategyFactory::parse_strategy(
        Game::Systems::AI::AIStrategyFactory::strategy_to_string(profile->strategy));
    EXPECT_EQ(round_tripped, profile->strategy) << definition.id;
    EXPECT_EQ(
        Game::Systems::AI::AIStrategyFactory::strategy_to_string(profile->strategy)
            .toStdString(),
        definition.doctrine.ai_strategy)
        << definition.id << " names a strategy the AI does not know";
  }
}

TEST(AICommanderDoctrine, CommandersDoNotAllPlayTheSameWay) {
  std::set<std::pair<int, int>> shapes;
  for (const auto& definition : Game::Units::all_commander_definitions()) {
    const auto profile = doctrine_profile_for_troop(definition.troop_type);
    ASSERT_TRUE(profile.has_value());
    shapes.emplace(static_cast<int>(profile->strategy),
                   static_cast<int>(profile->posture));
  }
  EXPECT_GE(shapes.size(), 4U)
      << "the roster collapsed onto too few doctrines to tell commanders apart";
}

TEST(AICommanderDoctrine, DelayerAndAggressorResolveToOpposedConfigs) {
  const auto delayer =
      doctrine_profile_for_troop(Game::Units::TroopType::RomanLegionOrganizer);
  const auto aggressor =
      doctrine_profile_for_troop(Game::Units::TroopType::RomanVeteranConsul);
  ASSERT_TRUE(delayer.has_value());
  ASSERT_TRUE(aggressor.has_value());

  EXPECT_EQ(delayer->strategy, AIStrategy::Defensive);
  EXPECT_EQ(delayer->posture, AIPosture::Garrison);
  EXPECT_EQ(aggressor->strategy, AIStrategy::Aggressive);
  EXPECT_EQ(aggressor->posture, AIPosture::Field);

  const auto delayer_config =
      Game::Systems::AI::AIStrategyFactory::create_config(*delayer);
  const auto aggressor_config =
      Game::Systems::AI::AIStrategyFactory::create_config(*aggressor);

  EXPECT_GT(aggressor_config.aggression_modifier, delayer_config.aggression_modifier);
  EXPECT_GT(delayer_config.defense_modifier, aggressor_config.defense_modifier);
  EXPECT_LT(aggressor_config.proactive_attack_size,
            delayer_config.proactive_attack_size)
      << "the aggressor should commit on a smaller assembled force";
}

TEST(AICommanderDoctrine, HarasserRaidsAndGarrisonDoesNot) {
  const auto harasser =
      doctrine_profile_for_troop(Game::Units::TroopType::CarthageBowCommander);
  const auto garrison =
      doctrine_profile_for_troop(Game::Units::TroopType::CarthageSpearCommander);
  ASSERT_TRUE(harasser.has_value());
  ASSERT_TRUE(garrison.has_value());

  const auto harasser_config =
      Game::Systems::AI::AIStrategyFactory::create_config(*harasser);
  const auto garrison_config =
      Game::Systems::AI::AIStrategyFactory::create_config(*garrison);

  EXPECT_GT(harasser_config.harass_units, 0);
  EXPECT_GT(harasser_config.harassment_range, 0.0F);
  EXPECT_EQ(garrison_config.posture, AIPosture::Garrison);
}

TEST(AICommanderDoctrine, NonCommanderTroopsCarryNoDoctrine) {
  EXPECT_FALSE(
      doctrine_profile_for_troop(Game::Units::TroopType::Swordsman).has_value());
  EXPECT_FALSE(doctrine_profile_for_troop(Game::Units::TroopType::Archer).has_value());
}

} // namespace
