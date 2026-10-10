#include <gtest/gtest.h>
#include <initializer_list>
#include <utility>

#include "game/formation/army_formation_types.h"
#include "game/systems/ai_system/ai_formation.h"
#include "game/systems/ai_system/ai_types.h"
#include "game/systems/nation_registry.h"
#include "game/units/spawn_type.h"

namespace {

using Game::Formation::ArmyFormationIntent;
using Game::Systems::NationID;
using Game::Units::SpawnType;

auto army(std::initializer_list<std::pair<SpawnType, int>> makeup)
    -> Game::Systems::AI::AISnapshot {
  Game::Systems::AI::AISnapshot snapshot;
  for (auto const& [type, count] : makeup) {
    for (int i = 0; i < count; ++i) {
      Game::Systems::AI::EntitySnapshot unit;
      unit.spawn_type = type;
      snapshot.friendly_units.push_back(unit);
    }
  }
  return snapshot;
}

auto pick(NationID nation_id,
          const Game::Systems::AI::AISnapshot& snapshot) -> ArmyFormationIntent {
  Game::Systems::Nation nation;
  nation.id = nation_id;
  Game::Systems::AI::AIContext context;
  context.nation = &nation;
  context.strategy_config.personality.aggression = 0.5F;
  return Game::Systems::AI::select_ai_intent(snapshot, context, false, false);
}

TEST(AiBattleOrderIntentTest, RomeFormsTheTriplexAciesWithEnoughInfantry) {
  EXPECT_EQ(pick(NationID::RomanRepublic,
                 army({{SpawnType::Swordsman, 4}, {SpawnType::Spearman, 2}})),
            ArmyFormationIntent::TriplexAcies);
  EXPECT_EQ(pick(NationID::RomanRepublic, army({{SpawnType::Swordsman, 3}})),
            ArmyFormationIntent::FactionDefault);
}

TEST(AiBattleOrderIntentTest, CarthageScreensWithElephantsOrFormsTheCrescent) {
  EXPECT_EQ(pick(NationID::Carthage,
                 army({{SpawnType::Elephant, 2}, {SpawnType::Swordsman, 3}})),
            ArmyFormationIntent::ElephantScreen);
  EXPECT_EQ(pick(NationID::Carthage,
                 army({{SpawnType::MountedSwordsman, 2},
                       {SpawnType::Swordsman, 2},
                       {SpawnType::Spearman, 2}})),
            ArmyFormationIntent::ConvexCrescent);
  EXPECT_EQ(pick(NationID::Carthage, army({{SpawnType::Swordsman, 6}})),
            ArmyFormationIntent::FactionDefault);
}

TEST(AiBattleOrderIntentTest, PostureAndSiegeStillComeFirst) {
  Game::Systems::Nation nation;
  nation.id = NationID::RomanRepublic;
  Game::Systems::AI::AIContext context;
  context.nation = &nation;
  auto const snapshot = army({{SpawnType::Swordsman, 8}});
  EXPECT_EQ(Game::Systems::AI::select_ai_intent(snapshot, context, true, false),
            ArmyFormationIntent::Defensive);
  EXPECT_EQ(Game::Systems::AI::select_ai_intent(snapshot, context, false, true),
            ArmyFormationIntent::SiegeEscort);
}

} // namespace
