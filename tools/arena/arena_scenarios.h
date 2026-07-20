#pragma once

#include <QString>

#include <vector>

#include "arena_scenario.h"

namespace Arena::Scenarios {

inline constexpr char k_sword_duel_id[] = "sword_duel";
inline constexpr char k_spear_duel_id[] = "spear_duel";
inline constexpr char k_bow_exchange_id[] = "bow_exchange";
inline constexpr char k_mounted_charge_id[] = "mounted_charge";
inline constexpr char k_braced_spear_charge_id[] = "braced_spear_charge";
inline constexpr char k_mounted_sword_duel_id[] = "mounted_sword_duel";
inline constexpr char k_mounted_spear_duel_id[] = "mounted_spear_duel";
inline constexpr char k_mounted_bow_exchange_id[] = "mounted_bow_exchange";
inline constexpr char k_infantry_locomotion_matrix_id[] = "infantry_locomotion_matrix";
inline constexpr char k_mounted_locomotion_matrix_id[] = "mounted_locomotion_matrix";
inline constexpr char k_elephant_locomotion_matrix_id[] = "elephant_locomotion_matrix";
inline constexpr char k_infantry_damage_matrix_id[] = "infantry_damage_matrix";
inline constexpr char k_mounted_damage_matrix_id[] = "mounted_damage_matrix";
inline constexpr char k_archer_action_transition_id[] = "archer_action_transition";
inline constexpr char k_swordsman_action_transition_id[] =
    "swordsman_action_transition";
inline constexpr char k_spearman_action_transition_id[] = "spearman_action_transition";
inline constexpr char k_horse_archer_action_transition_id[] =
    "horse_archer_action_transition";
inline constexpr char k_mounted_knight_action_transition_id[] =
    "mounted_knight_action_transition";
inline constexpr char k_horse_spearman_action_transition_id[] =
    "horse_spearman_action_transition";
inline constexpr char k_melee_lock_id[] = "melee_lock";
inline constexpr char k_chase_to_attack_id[] = "chase_to_attack";
inline constexpr char k_attack_to_chase_id[] = "attack_to_chase";
inline constexpr char k_target_death_id[] = "target_death";
inline constexpr char k_retargeting_id[] = "retargeting";
inline constexpr char k_hold_guard_exit_id[] = "hold_guard_exit";
inline constexpr char k_lod_switch_id[] = "lod_switch";

inline constexpr char k_three_swords_vs_two_spears_id[] = "three_swords_vs_two_spears";
inline constexpr char k_spear_walk_contact_id[] = "spear_walk_contact";
inline constexpr char k_archer_stability_id[] = "archer_stability";
inline constexpr char k_infantry_charge_id[] = "infantry_charge";
inline constexpr char k_flank_ambush_id[] = "flank_ambush";
inline constexpr char k_reserve_release_id[] = "reserve_release";
inline constexpr char k_mixed_roles_id[] = "mixed_roles";
inline constexpr char k_bot_skirmish_id[] = "bot_skirmish";
inline constexpr char k_crossing_formations_id[] = "crossing_formations";
inline constexpr char k_sustained_battle_id[] = "sustained_battle";
inline constexpr char k_roman_marching_camp_id[] = "roman_marching_camp";
inline constexpr char k_carthage_trade_town_id[] = "carthage_trade_town";
inline constexpr char k_rival_economies_id[] = "rival_economies";

struct ScenarioOption {
  QString id;
  QString label;
  QString description;
};

[[nodiscard]] auto options() -> const std::vector<ScenarioOption>&;
[[nodiscard]] auto find_option(const QString& scenario_id) -> const ScenarioOption*;

} // namespace Arena::Scenarios
