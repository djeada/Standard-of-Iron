#include <gtest/gtest.h>

#include "game/map/mission_victory_rules.h"

TEST(MissionVictoryRulesTest, BuildsIndependentVictoryAndDefeatRules) {
  Game::Mission::MissionDefinition mission;

  Game::Mission::Condition capture_condition;
  capture_condition.type = QStringLiteral("capture_structures");
  capture_condition.structure_types = {QStringLiteral("village")};
  capture_condition.min_count = 2;

  Game::Mission::Condition survive_condition;
  survive_condition.type = QStringLiteral("survive_duration");
  survive_condition.duration = 180.0F;

  Game::Mission::Condition lose_structure_condition;
  lose_structure_condition.type = QStringLiteral("lose_structure");
  lose_structure_condition.structure_type = QStringLiteral("defense_tower");

  Game::Mission::Condition lose_all_units_condition;
  lose_all_units_condition.type = QStringLiteral("lose_all_units");

  Game::Mission::Condition lose_commander_condition;
  lose_commander_condition.type = QStringLiteral("lose_commander");

  Game::Mission::Condition only_commander_remaining_condition;
  only_commander_remaining_condition.type = QStringLiteral("only_commander_remaining");

  mission.victory_conditions = {capture_condition, survive_condition};
  mission.defeat_conditions = {lose_structure_condition,
                               lose_all_units_condition,
                               lose_commander_condition,
                               only_commander_remaining_condition};

  const auto rules = Game::Mission::build_victory_rules(mission);

  ASSERT_EQ(rules.victory_rules.size(), 2);
  auto const* capture_rule = std::get_if<Game::Systems::CaptureStructuresVictoryRule>(
      rules.victory_rules.data());
  ASSERT_NE(capture_rule, nullptr);
  ASSERT_EQ(capture_rule->target.structure_types.size(), 1);
  EXPECT_EQ(capture_rule->target.structure_types[0], QStringLiteral("barracks"));
  EXPECT_EQ(capture_rule->target.required_count, 2);

  auto const* survive_rule =
      std::get_if<Game::Systems::SurviveTimeVictoryRule>(&rules.victory_rules[1]);
  ASSERT_NE(survive_rule, nullptr);
  EXPECT_FLOAT_EQ(survive_rule->duration, 180.0F);

  ASSERT_EQ(rules.defeat_rules.size(), 4);
  auto const* lose_structure_rule =
      std::get_if<Game::Systems::NoKeyStructuresDefeatRule>(rules.defeat_rules.data());
  ASSERT_NE(lose_structure_rule, nullptr);
  ASSERT_EQ(lose_structure_rule->structure_types.size(), 1);
  EXPECT_EQ(lose_structure_rule->structure_types[0], QStringLiteral("defense_tower"));
  EXPECT_TRUE(
      std::holds_alternative<Game::Systems::NoUnitsDefeatRule>(rules.defeat_rules[1]));
  EXPECT_TRUE(std::holds_alternative<Game::Systems::NoCommanderDefeatRule>(
      rules.defeat_rules[2]));
  auto const* isolated_commander_rule =
      std::get_if<Game::Systems::OnlyCommanderRemainingDefeatRule>(
          &rules.defeat_rules[3]);
  ASSERT_NE(isolated_commander_rule, nullptr);
  ASSERT_EQ(isolated_commander_rule->structure_types.size(), 1);
  EXPECT_EQ(isolated_commander_rule->structure_types[0], QStringLiteral("barracks"));
}

TEST(MissionVictoryRulesTest, FallsBackToCommanderDefaultsWhenMissionOmitsDefeatRules) {
  Game::Mission::MissionDefinition mission;

  Game::Mission::Condition control_condition;
  control_condition.type = QStringLiteral("control_structures");
  control_condition.structure_types = {QStringLiteral("village"),
                                       QStringLiteral("barracks")};
  mission.victory_conditions = {control_condition};

  const auto rules = Game::Mission::build_victory_rules(mission);

  ASSERT_EQ(rules.victory_rules.size(), 1);
  ASSERT_EQ(rules.defeat_rules.size(), 2);
  EXPECT_TRUE(std::holds_alternative<Game::Systems::NoCommanderDefeatRule>(
      rules.defeat_rules[0]));
  auto const* isolated_commander_rule =
      std::get_if<Game::Systems::OnlyCommanderRemainingDefeatRule>(
          &rules.defeat_rules[1]);
  ASSERT_NE(isolated_commander_rule, nullptr);
  ASSERT_EQ(isolated_commander_rule->structure_types.size(), 1);
  EXPECT_EQ(isolated_commander_rule->structure_types[0], QStringLiteral("barracks"));
}

TEST(MissionVictoryRulesTest, OnlyCommanderRemainingKeepsLegacyVillageNormalization) {
  Game::Mission::MissionDefinition mission;
  Game::Mission::Condition survive_condition;
  survive_condition.type = QStringLiteral("survive_duration");
  survive_condition.duration = 60.0F;

  Game::Mission::Condition isolated_commander_condition;
  isolated_commander_condition.type = QStringLiteral("only_commander_remaining");
  isolated_commander_condition.structure_type = QStringLiteral("village");

  mission.victory_conditions = {survive_condition};
  mission.defeat_conditions = {isolated_commander_condition};

  const auto rules = Game::Mission::build_victory_rules(mission);

  ASSERT_EQ(rules.defeat_rules.size(), 1);
  auto const* isolated_commander_rule =
      std::get_if<Game::Systems::OnlyCommanderRemainingDefeatRule>(
          rules.defeat_rules.data());
  ASSERT_NE(isolated_commander_rule, nullptr);
  ASSERT_EQ(isolated_commander_rule->structure_types.size(), 1);
  EXPECT_EQ(isolated_commander_rule->structure_types[0], QStringLiteral("barracks"));
}

TEST(MissionVictoryRulesTest, BuildsUndeadObjectiveRulesAndAmbientFlag) {
  Game::Mission::MissionDefinition mission;
  mission.include_ambient_undead = true;

  Game::Mission::Condition clear_zone_condition;
  clear_zone_condition.type = QStringLiteral("clear_undead_zone");
  clear_zone_condition.zone_id = QStringLiteral("sepulcher_ruin");

  Game::Mission::Condition survive_wave_condition;
  survive_wave_condition.type = QStringLiteral("survive_undead_wave");
  survive_wave_condition.zone_id = QStringLiteral("sepulcher_ruin");
  survive_wave_condition.wave_count = 2;

  mission.victory_conditions = {clear_zone_condition, survive_wave_condition};

  const auto rules = Game::Mission::build_victory_rules(mission);

  EXPECT_TRUE(rules.include_ambient_undead);
  ASSERT_EQ(rules.victory_rules.size(), 2);
  auto const* clear_rule = std::get_if<Game::Systems::ClearUndeadZoneVictoryRule>(
      rules.victory_rules.data());
  ASSERT_NE(clear_rule, nullptr);
  EXPECT_EQ(clear_rule->zone_id, QStringLiteral("sepulcher_ruin"));

  auto const* wave_rule =
      std::get_if<Game::Systems::SurviveUndeadWaveVictoryRule>(&rules.victory_rules[1]);
  ASSERT_NE(wave_rule, nullptr);
  EXPECT_EQ(wave_rule->zone_id, QStringLiteral("sepulcher_ruin"));
  EXPECT_EQ(wave_rule->required_wave_count, 2);
}
