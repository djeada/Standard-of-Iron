#include <QString>

#include <algorithm>
#include <gtest/gtest.h>
#include <numeric>

#include "tools/arena/arena_scenario.h"
#include "tools/arena/matchup_short.h"
#include "tools/arena/promo_spec.h"

namespace {

using Arena::ArenaExpectation;
using Arena::ArenaExpectationKind;
using Arena::ArenaScenarioDefinition;
using Arena::ArenaScenarioGroup;
using Arena::Matchup::Matchup;
using Nation = Game::Systems::NationID;
using Troop = Game::Units::TroopType;

auto parsed(const char* text) -> Matchup {
  QString error;
  auto matchup = Arena::Matchup::parse(QString::fromLatin1(text), &error);
  EXPECT_TRUE(matchup.has_value()) << error.toStdString();
  return matchup.value_or(Matchup{});
}

auto unit_total(const ArenaScenarioDefinition& scenario, int owner_id) -> int {
  return std::accumulate(scenario.groups.begin(),
                         scenario.groups.end(),
                         0,
                         [owner_id](int total, const ArenaScenarioGroup& group) {
                           return group.owner_id == owner_id ? total + group.count
                                                             : total;
                         });
}

TEST(ArenaMatchupShortTest, ACountAndAUnitEitherSideOfVs) {
  const auto matchup = parsed("8 swordsman vs 6 archer");
  EXPECT_EQ(matchup.attacker.count, 8);
  EXPECT_EQ(matchup.attacker.troop, Troop::Swordsman);
  EXPECT_EQ(matchup.defender.count, 6);
  EXPECT_EQ(matchup.defender.troop, Troop::Archer);
}

TEST(ArenaMatchupShortTest, ANationCanLeadOrTrailTheUnit) {
  const auto leading = parsed("2 carthage swordsman vs 5 iron sepulcher swordsman");
  EXPECT_EQ(leading.attacker.count, 2);
  EXPECT_EQ(leading.attacker.nation, Nation::Carthage);
  EXPECT_EQ(leading.attacker.troop, Troop::Swordsman);
  EXPECT_EQ(leading.defender.count, 5);
  EXPECT_EQ(leading.defender.nation, Nation::IronSepulcher);
  EXPECT_EQ(leading.defender.troop, Troop::SkeletonSwordsman)
      << "the nation decides which troop it actually fields";

  const auto trailing = parsed("swordman carthage 2 units vs swordsman rome 5 units");
  EXPECT_EQ(trailing.attacker.count, 2);
  EXPECT_EQ(trailing.attacker.nation, Nation::Carthage);
  EXPECT_EQ(trailing.attacker.troop, Troop::Swordsman);
  EXPECT_EQ(trailing.defender.count, 5);
  EXPECT_EQ(trailing.defender.nation, Nation::RomanRepublic);
}

TEST(ArenaMatchupShortTest, ANationFieldsItsOwnTroops) {
  const auto undead = parsed("2 carthage swordsman vs 5 iron sepulcher swordsman");
  EXPECT_EQ(undead.attacker.troop, Troop::Swordsman);
  EXPECT_EQ(undead.defender.troop, Troop::SkeletonSwordsman)
      << "the Iron Sepulcher has no swordsman; asking for one used to spawn the "
         "generic unit, which renders as a Roman and puts no undead in the shot";
  EXPECT_EQ(parsed("3 iron sepulcher archer vs 3 roman archer").attacker.troop,
            Troop::SkeletonArcher);
  EXPECT_EQ(parsed("3 iron sepulcher healer vs 3 roman healer").attacker.troop,
            Troop::GravePriest);

  const auto living = parsed("4 carthage skeleton swordsman vs 4 roman swordsman");
  EXPECT_EQ(living.attacker.troop, Troop::Swordsman)
      << "and a living power never fields the dead";
}

TEST(ArenaMatchupShortTest, TwoUnnamedSidesStillFieldDifferentPowers) {
  const auto matchup = parsed("3 swordsman vs 3 swordsman");
  EXPECT_NE(matchup.attacker.nation, matchup.defender.nation)
      << "one nation in two team colours makes the sides look identical";
}

TEST(ArenaMatchupShortTest, PluralsAndSpacingAreAccepted) {
  const auto plural = parsed("8 swordsmen VS 6 archers");
  EXPECT_EQ(plural.attacker.troop, Troop::Swordsman);
  EXPECT_EQ(plural.defender.troop, Troop::Archer);

  const auto spaced = parsed("6 horse archer v 6 spearmen");
  EXPECT_EQ(spaced.attacker.troop, Troop::HorseArcher);
  EXPECT_EQ(spaced.defender.troop, Troop::Spearman);
}

TEST(ArenaMatchupShortTest, NonsenseIsRefusedWithAReadableReason) {
  QString error;
  EXPECT_FALSE(
      Arena::Matchup::parse(QStringLiteral("swordsman vs archer"), &error).has_value());
  EXPECT_FALSE(error.isEmpty());

  EXPECT_FALSE(
      Arena::Matchup::parse(QStringLiteral("8 catapults"), &error).has_value());
  EXPECT_FALSE(Arena::Matchup::parse(QStringLiteral("8 swordsman vs 6 wizard"), &error)
                   .has_value());
  EXPECT_TRUE(error.contains(QStringLiteral("wizard")))
      << "the message has to name the unit that was not understood";
  EXPECT_FALSE(Arena::Matchup::parse(QStringLiteral("0 swordsman vs 1 archer"), &error)
                   .has_value());
  EXPECT_FALSE(
      Arena::Matchup::parse(QStringLiteral("900 swordsman vs 1 archer"), &error)
          .has_value());
}

TEST(ArenaMatchupShortTest, TheGeneratedScenarioPassesTheArenaValidator) {
  const auto scenario = Arena::Matchup::build_scenario(
      parsed("2 carthage swordsman vs 5 iron sepulcher swordsman"));

  const auto errors = Arena::validate_scenario(scenario);
  ASSERT_TRUE(errors.empty())
      << "a generated matchup has to satisfy the same validator an authored "
         "scenario does; first failure: "
      << (errors.empty()
              ? std::string{}
              : (errors.front().field + QStringLiteral(": ") + errors.front().message)
                    .toStdString());

  EXPECT_EQ(unit_total(scenario, 1), 2);
  EXPECT_EQ(unit_total(scenario, 2), 5);
  ASSERT_EQ(scenario.battle_sides.size(), 2U)
      << "the closing report needs two tracked sides to name a winner";
  EXPECT_EQ(scenario.battle_sides[0].owner_id, 1);
  EXPECT_EQ(scenario.battle_sides[1].owner_id, 2);
  EXPECT_TRUE(std::any_of(scenario.expectations.begin(),
                          scenario.expectations.end(),
                          [](const ArenaExpectation& expectation) {
                            return expectation.kind ==
                                   ArenaExpectationKind::BattleReachesDecision;
                          }))
      << "without this the short runs its full duration instead of ending on the "
         "kill";

  for (const auto& group : scenario.groups) {
    EXPECT_EQ(group.individuals_per_unit, 0)
        << "a unit fields the headcount and layout its troop profile defines; "
           "overriding it makes the recording show something the game never does";
    EXPECT_EQ(group.nation_id,
              group.owner_id == 1 ? Nation::Carthage : Nation::IronSepulcher);
  }

  const bool sides_are_apart = std::all_of(
      scenario.groups.begin(), scenario.groups.end(), [](const auto& group) {
        return group.owner_id == 1 ? group.origin.x() < 0.0F : group.origin.x() > 0.0F;
      });
  EXPECT_TRUE(sides_are_apart) << "the two blocks start on opposite sides of the lens";
}

TEST(ArenaMatchupShortTest, TheGeneratedSpecIsAShortThatPassesTheReelChecks) {
  const auto matchup = parsed("2 carthage swordsman vs 5 iron sepulcher swordsman");
  const auto spec = Arena::Matchup::build_spec(matchup);

  EXPECT_EQ(spec.width, 1080);
  EXPECT_EQ(spec.height, 1920) << "Shorts are vertical 9:16";
  EXPECT_EQ(spec.width % 2, 0);
  EXPECT_EQ(spec.height % 2, 0) << "an odd dimension will not encode to h264";
  ASSERT_EQ(spec.shots.size(), 1U);

  const auto& shot = spec.shots.front();
  EXPECT_TRUE(spec.audio)
      << "a matchup short carries the game's own audio, not silence";
  EXPECT_FALSE(spec.music_track.isEmpty())
      << "the ambient music system supplies no bed for a recording, so the reel "
         "names one itself";
  EXPECT_FALSE(spec.report_sound_decided.isEmpty());
  EXPECT_FALSE(spec.report_sound_undecided.isEmpty());
  EXPECT_NE(spec.report_sound_decided, spec.report_sound_undecided)
      << "a decided battle and one that ran out of clock end differently";
  EXPECT_TRUE(shot.gameplay_ui)
      << "the floating numbers and mode indicators are the point of the short";
  EXPECT_GT(shot.report_card_seconds, 0.0F) << "the short closes on who won";
  EXPECT_EQ(shot.scenario, QString::fromLatin1(Arena::Matchup::k_scenario_id));

  const float clip_seconds =
      (shot.duration_seconds * shot.slow_motion) + shot.report_card_seconds;
  EXPECT_LE(clip_seconds, 180.0F) << "a Short has to stay inside the Shorts limit";

  EXPECT_TRUE(Arena::Promo::motion_violations(spec).empty())
      << "a generated matchup must not trip the camera-motion checks an authored "
         "reel is held to";

  EXPECT_EQ(shot.focus.mode, Arena::Promo::FocusMode::GroupPair)
      << "a melee walks, so a lens locked on the deployment centre loses it -- "
         "and a battle centre is mass-weighted, so five units against two drag "
         "the frame off the smaller army";
  EXPECT_FALSE(shot.focus.group.isEmpty());
  EXPECT_FALSE(shot.focus.second_group.isEmpty());
  EXPECT_NE(shot.focus.group, shot.focus.second_group);
  EXPECT_GE(shot.focus.smoothing, 1.5F)
      << "and a lens that tracks it closely reads as handheld shake, because "
         "the centre jumps every time a unit on one flank dies";
  ASSERT_EQ(shot.keys.size(), 2U);
  EXPECT_FLOAT_EQ(shot.keys.front().yaw, shot.keys.back().yaw)
      << "the lens is locked; only the distance moves";
  EXPECT_FLOAT_EQ(shot.keys.front().pitch, shot.keys.back().pitch);
  EXPECT_LT(shot.keys.back().distance, shot.keys.front().distance);
}

TEST(ArenaMatchupShortTest, TheArmiesCloseAcrossTheFrameNotUpIt) {
  const auto matchup = parsed("2 carthage swordsman vs 5 iron sepulcher swordsman");
  const auto scenario = Arena::Matchup::build_scenario(matchup);
  const auto spec = Arena::Matchup::build_spec(matchup);

  ASSERT_FALSE(spec.shots.front().keys.empty());
  EXPECT_FLOAT_EQ(spec.shots.front().keys.front().yaw, 0.0F)
      << "a yaw of zero sits the lens on the Z axis, which puts the separation "
         "between the armies across the screen rather than up it";

  for (const auto& group : scenario.groups) {
    EXPECT_FLOAT_EQ(group.spacing.x(), 0.0F);
    EXPECT_GT(group.spacing.z(), 0.0F)
        << "a line of units runs into the depth of the frame, so it is the tall "
           "side of a 9:16 clip that holds it";
  }
}

TEST(ArenaMatchupShortTest, ABiggerSideIsFramedFromFurtherBack) {
  const auto small = Arena::Matchup::build_spec(parsed("2 swordsman vs 2 archer"));
  const auto large = Arena::Matchup::build_spec(parsed("18 swordsman vs 18 archer"));

  ASSERT_FALSE(small.shots.front().keys.empty());
  ASSERT_FALSE(large.shots.front().keys.empty());
  EXPECT_GT(large.shots.front().keys.front().distance,
            small.shots.front().keys.front().distance)
      << "eighteen a side has to fit the frame that two a side filled";
}

} // namespace
