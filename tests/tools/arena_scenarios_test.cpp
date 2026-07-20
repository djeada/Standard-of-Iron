#include <algorithm>
#include <gtest/gtest.h>
#include <vector>

#include "tools/arena/arena_scenarios.h"

TEST(ArenaScenariosTest, ListsAllPhaseOneScenarioIds) {
  std::vector<QString> ids;
  ids.reserve(Arena::Scenarios::options().size());
  for (const auto& option : Arena::Scenarios::options()) {
    ids.push_back(option.id);
  }

  EXPECT_NE(std::find(ids.begin(),
                      ids.end(),
                      QString::fromLatin1(Arena::Scenarios::k_sword_duel_id)),
            ids.end());
  EXPECT_NE(std::find(ids.begin(),
                      ids.end(),
                      QString::fromLatin1(Arena::Scenarios::k_spear_duel_id)),
            ids.end());
  EXPECT_NE(std::find(ids.begin(),
                      ids.end(),
                      QString::fromLatin1(Arena::Scenarios::k_bow_exchange_id)),
            ids.end());
  EXPECT_NE(std::find(ids.begin(),
                      ids.end(),
                      QString::fromLatin1(Arena::Scenarios::k_held_weapon_stances_id)),
            ids.end());
  EXPECT_NE(std::find(ids.begin(),
                      ids.end(),
                      QString::fromLatin1(Arena::Scenarios::k_mounted_charge_id)),
            ids.end());
  EXPECT_NE(std::find(ids.begin(),
                      ids.end(),
                      QString::fromLatin1(Arena::Scenarios::k_braced_spear_charge_id)),
            ids.end());
  for (auto const* transition_id :
       {Arena::Scenarios::k_archer_action_transition_id,
        Arena::Scenarios::k_swordsman_action_transition_id,
        Arena::Scenarios::k_spearman_action_transition_id,
        Arena::Scenarios::k_horse_archer_action_transition_id,
        Arena::Scenarios::k_mounted_knight_action_transition_id,
        Arena::Scenarios::k_horse_spearman_action_transition_id}) {
    EXPECT_NE(std::find(ids.begin(), ids.end(), QString::fromLatin1(transition_id)),
              ids.end());
  }
  EXPECT_NE(std::find(ids.begin(),
                      ids.end(),
                      QString::fromLatin1(Arena::Scenarios::k_melee_lock_id)),
            ids.end());
  EXPECT_NE(std::find(ids.begin(),
                      ids.end(),
                      QString::fromLatin1(Arena::Scenarios::k_chase_to_attack_id)),
            ids.end());
  EXPECT_NE(std::find(ids.begin(),
                      ids.end(),
                      QString::fromLatin1(Arena::Scenarios::k_attack_to_chase_id)),
            ids.end());
  EXPECT_NE(std::find(ids.begin(),
                      ids.end(),
                      QString::fromLatin1(Arena::Scenarios::k_target_death_id)),
            ids.end());
  EXPECT_NE(std::find(ids.begin(),
                      ids.end(),
                      QString::fromLatin1(Arena::Scenarios::k_retargeting_id)),
            ids.end());
  EXPECT_NE(std::find(ids.begin(),
                      ids.end(),
                      QString::fromLatin1(Arena::Scenarios::k_hold_guard_exit_id)),
            ids.end());
  EXPECT_NE(std::find(ids.begin(),
                      ids.end(),
                      QString::fromLatin1(Arena::Scenarios::k_lod_switch_id)),
            ids.end());
  for (auto const* settlement_id : {Arena::Scenarios::k_roman_marching_camp_id,
                                    Arena::Scenarios::k_carthage_trade_town_id,
                                    Arena::Scenarios::k_rival_economies_id}) {
    EXPECT_NE(std::find(ids.begin(), ids.end(), QString::fromLatin1(settlement_id)),
              ids.end());
  }
}

TEST(ArenaScenariosTest, ResolvesDescriptionsForKnownScenarios) {
  const auto* scenario = Arena::Scenarios::find_option(
      QString::fromLatin1(Arena::Scenarios::k_lod_switch_id));
  ASSERT_NE(scenario, nullptr);
  EXPECT_EQ(scenario->label, QStringLiteral("LOD Switch"));
  EXPECT_TRUE(
      scenario->description.contains(QStringLiteral("camera"), Qt::CaseInsensitive));
}

TEST(ArenaScenariosTest, RejectsUnknownScenarioIds) {
  EXPECT_EQ(Arena::Scenarios::find_option(QStringLiteral("not_a_real_scenario")),
            nullptr);
}
