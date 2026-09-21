#include <gtest/gtest.h>
#include <limits>
#include <memory>

#include "game/mission/difficulty_forces.h"
#include "game/mission/difficulty_profile.h"
#include "game/systems/owner_registry.h"

namespace {

using Game::Mission::DifficultyPreset;

TEST(DifficultyProfileTest, TheFourPresetsCarryTheBalanceContract) {
  struct Expectation {
    const char* id;
    DifficultyPreset preset;
    float resources;
    float units;
    float waves;
  };

  for (const auto& expected :
       {Expectation{"easy", DifficultyPreset::Easy, 0.80F, 0.80F, 0.75F},
        Expectation{"normal", DifficultyPreset::Normal, 1.00F, 1.00F, 1.00F},
        Expectation{"hard", DifficultyPreset::Hard, 1.50F, 1.50F, 1.50F},
        Expectation{"very_hard", DifficultyPreset::VeryHard, 2.00F, 2.00F, 2.00F}}) {
    const auto profile =
        Game::Mission::resolve_difficulty(QString::fromLatin1(expected.id));
    EXPECT_EQ(profile.preset, expected.preset) << expected.id;
    EXPECT_EQ(profile.id(), QString::fromLatin1(expected.id));
    EXPECT_FLOAT_EQ(profile.resource_multiplier, expected.resources) << expected.id;
    EXPECT_FLOAT_EQ(profile.starting_unit_multiplier, expected.units) << expected.id;
    EXPECT_FLOAT_EQ(profile.wave_multiplier, expected.waves) << expected.id;
  }
}

TEST(DifficultyProfileTest, BrutalIsPersistedAsVeryHard) {
  EXPECT_EQ(Game::Mission::normalize_difficulty_id(QStringLiteral("brutal")),
            QStringLiteral("very_hard"));
  EXPECT_EQ(Game::Mission::difficulty_id_for(DifficultyPreset::VeryHard),
            QStringLiteral("very_hard"));
}

TEST(DifficultyProfileTest, UnknownAndEmptyIdsFallBackToNormal) {
  for (const char* id : {"", "  ", "impossible", "nightmare", "7"}) {
    EXPECT_EQ(Game::Mission::normalize_difficulty_id(QString::fromLatin1(id)),
              QStringLiteral("normal"))
        << id;
  }
}

TEST(DifficultyProfileTest, LegacyAuthoredNamesStillResolve) {
  EXPECT_EQ(Game::Mission::normalize_difficulty_id(QStringLiteral("medium")),
            QStringLiteral("normal"));
  EXPECT_EQ(Game::Mission::normalize_difficulty_id(QStringLiteral("recruit")),
            QStringLiteral("easy"));
  EXPECT_EQ(Game::Mission::normalize_difficulty_id(QStringLiteral("legendary")),
            QStringLiteral("very_hard"));
  EXPECT_EQ(Game::Mission::normalize_difficulty_id(QStringLiteral("  HARD  ")),
            QStringLiteral("hard"));
}

TEST(DifficultyProfileTest, NormalizingIsIdempotent) {
  for (const auto preset : Game::Mission::difficulty_presets()) {
    const QString once = Game::Mission::normalize_difficulty_id(
        Game::Mission::difficulty_id_for(preset));
    EXPECT_EQ(Game::Mission::normalize_difficulty_id(once), once);
    EXPECT_EQ(Game::Mission::resolve_difficulty(once).preset, preset);
  }
}

TEST(DifficultyProfileTest, ForceCountsRoundToWholeUnitsAndNeverEmptyAGroup) {
  EXPECT_EQ(Game::Mission::scaled_force_count(0, 2.0F), 0);
  EXPECT_EQ(Game::Mission::scaled_force_count(-3, 2.0F), 0);

  EXPECT_EQ(Game::Mission::scaled_force_count(1, 0.8F), 1);
  EXPECT_EQ(Game::Mission::scaled_force_count(2, 0.8F), 2);
  EXPECT_EQ(Game::Mission::scaled_force_count(5, 0.8F), 4);
  EXPECT_EQ(Game::Mission::scaled_force_count(4, 1.5F), 6);
  EXPECT_EQ(Game::Mission::scaled_force_count(5, 1.5F), 8);
  EXPECT_EQ(Game::Mission::scaled_force_count(7, 2.0F), 14);
}

TEST(DifficultyProfileTest, ResourceAmountsScaleButLeaveEmptyStocksEmpty) {
  EXPECT_EQ(Game::Mission::scaled_resource_amount(0, 2.0F), 0);
  EXPECT_EQ(Game::Mission::scaled_resource_amount(450, 1.0F), 450);
  EXPECT_EQ(Game::Mission::scaled_resource_amount(450, 1.5F), 675);
  EXPECT_EQ(Game::Mission::scaled_resource_amount(450, 2.0F), 900);
  EXPECT_EQ(Game::Mission::scaled_resource_amount(450, 0.8F), 360);
}

TEST(DifficultyProfileTest, AnEnormousAuthoredAmountSaturatesInsteadOfWrapping) {
  const int huge = std::numeric_limits<int>::max();
  EXPECT_EQ(Game::Mission::scaled_resource_amount(huge, 2.0F), huge);
  EXPECT_EQ(Game::Mission::scaled_force_count(huge, 8.0F), huge);
  EXPECT_GT(Game::Mission::scaled_resource_amount(huge, 1.5F), 0);
  EXPECT_GT(Game::Mission::scaled_force_count(huge / 2, 2.0F), 0);
}

TEST(DifficultyProfileTest, AbsurdMultipliersAreClamped) {
  EXPECT_EQ(Game::Mission::scaled_force_count(10, 1000.0F), 80);
  EXPECT_EQ(Game::Mission::scaled_force_count(10, 0.0F), 1);
  EXPECT_EQ(Game::Mission::scaled_resource_amount(100, -5.0F), 10);
}

TEST(DifficultyProfileTest, EachOpponentCanCarryItsOwnPreset) {
  Game::Mission::MatchDifficulty difficulty;
  difficulty.set_owner(2, QStringLiteral("hard"));
  difficulty.set_owner(3, QStringLiteral("brutal"));

  EXPECT_EQ(difficulty.baseline_id(), QStringLiteral("normal"));
  EXPECT_EQ(difficulty.id_for(2), QStringLiteral("hard"));
  EXPECT_EQ(difficulty.id_for(3), QStringLiteral("very_hard"));
  EXPECT_EQ(difficulty.id_for(4), QStringLiteral("normal"));
  EXPECT_FLOAT_EQ(difficulty.profile_for(2).resource_multiplier, 1.5F);
  EXPECT_FLOAT_EQ(difficulty.profile_for(3).resource_multiplier, 2.0F);
  EXPECT_FLOAT_EQ(difficulty.profile_for(4).resource_multiplier, 1.0F);
  EXPECT_FALSE(difficulty.is_baseline());
}

TEST(DifficultyProfileTest, AnAllNormalMatchReportsItselfAsBaseline) {
  Game::Mission::MatchDifficulty difficulty;
  EXPECT_TRUE(difficulty.is_baseline());
  difficulty.set_owner(2, QStringLiteral("medium"));
  EXPECT_TRUE(difficulty.is_baseline());
  difficulty.set_baseline(QStringLiteral("easy"));
  EXPECT_FALSE(difficulty.is_baseline());
}

TEST(DifficultyProfileTest, TheBaselineAppliesToEveryOwnerThatHasNoPresetOfItsOwn) {
  Game::Mission::MatchDifficulty difficulty(QStringLiteral("very_hard"));
  EXPECT_EQ(difficulty.id_for(2), QStringLiteral("very_hard"));
  difficulty.set_owner(3, QStringLiteral("easy"));
  EXPECT_EQ(difficulty.id_for(3), QStringLiteral("easy"));
  EXPECT_EQ(difficulty.id_for(2), QStringLiteral("very_hard"));
}

constexpr int k_human = 1;
constexpr int k_allied_ai = 2;
constexpr int k_enemy_ai = 3;
constexpr int k_neutral = 4;

auto mixed_table() -> std::unique_ptr<Game::Systems::OwnerRegistry> {
  auto owners = std::make_unique<Game::Systems::OwnerRegistry>();
  owners->register_owner_with_id(k_human, Game::Systems::OwnerType::Player, "human");
  owners->register_owner_with_id(k_allied_ai, Game::Systems::OwnerType::AI, "ally");
  owners->register_owner_with_id(k_enemy_ai, Game::Systems::OwnerType::AI, "enemy");
  owners->register_owner_with_id(
      k_neutral, Game::Systems::OwnerType::Neutral, "wildlife");
  owners->set_local_player_id(k_human);
  owners->set_owner_team(k_human, 1);
  owners->set_owner_team(k_allied_ai, 1);
  owners->set_owner_team(k_enemy_ai, 2);
  owners->set_owner_team(k_neutral, 0);
  return owners;
}

TEST(DifficultyEligibilityTest, AMissionBaselineOnlyReachesTheHumansEnemies) {
  const auto owners = mixed_table();
  const Game::Mission::MatchDifficulty difficulty(QStringLiteral("very_hard"));

  EXPECT_FALSE(
      Game::Mission::difficulty_applies_to(difficulty, *owners, k_human, k_human));
  EXPECT_FALSE(
      Game::Mission::difficulty_applies_to(difficulty, *owners, k_allied_ai, k_human));
  EXPECT_FALSE(
      Game::Mission::difficulty_applies_to(difficulty, *owners, k_neutral, k_human));
  EXPECT_TRUE(
      Game::Mission::difficulty_applies_to(difficulty, *owners, k_enemy_ai, k_human));
}

TEST(DifficultyEligibilityTest, ASkirmishSeatUsesItsOwnPresetWhateverTeamItIsOn) {
  const auto owners = mixed_table();
  Game::Mission::MatchDifficulty difficulty;
  difficulty.set_owner(k_allied_ai, QStringLiteral("hard"));
  difficulty.set_owner(k_enemy_ai, QStringLiteral("easy"));

  EXPECT_TRUE(
      Game::Mission::difficulty_applies_to(difficulty, *owners, k_allied_ai, k_human));
  EXPECT_TRUE(
      Game::Mission::difficulty_applies_to(difficulty, *owners, k_enemy_ai, k_human));
  EXPECT_FALSE(
      Game::Mission::difficulty_applies_to(difficulty, *owners, k_human, k_human));
  EXPECT_FALSE(
      Game::Mission::difficulty_applies_to(difficulty, *owners, k_neutral, k_human));
  EXPECT_FLOAT_EQ(difficulty.profile_for(k_allied_ai).starting_unit_multiplier, 1.5F);
  EXPECT_FLOAT_EQ(difficulty.profile_for(k_enemy_ai).starting_unit_multiplier, 0.8F);
}

TEST(DifficultyEligibilityTest, AnObservedMatchStillScalesTheSeatTheCameraSitsOn) {
  Game::Systems::OwnerRegistry owners;
  owners.register_owner_with_id(2, Game::Systems::OwnerType::AI, "red");
  owners.register_owner_with_id(3, Game::Systems::OwnerType::AI, "blue");
  owners.set_owner_team(2, 1);
  owners.set_owner_team(3, 2);

  Game::Mission::MatchDifficulty difficulty;
  difficulty.set_owner(2, QStringLiteral("hard"));
  difficulty.set_owner(3, QStringLiteral("very_hard"));

  EXPECT_TRUE(Game::Mission::difficulty_applies_to(difficulty, owners, 2, 2));
  EXPECT_TRUE(Game::Mission::difficulty_applies_to(difficulty, owners, 3, 2));
}

TEST(DifficultyEligibilityTest, NoOwnerInheritsAnotherSeatsPreset) {
  const auto owners = mixed_table();
  Game::Mission::MatchDifficulty difficulty;
  difficulty.set_owner(k_enemy_ai, QStringLiteral("very_hard"));

  EXPECT_TRUE(difficulty.has_owner(k_enemy_ai));
  EXPECT_FALSE(difficulty.has_owner(k_allied_ai));

  EXPECT_FALSE(
      Game::Mission::difficulty_applies_to(difficulty, *owners, k_allied_ai, k_human));
  EXPECT_FLOAT_EQ(difficulty.profile_for(k_allied_ai).starting_unit_multiplier, 1.0F);
}

} // namespace
