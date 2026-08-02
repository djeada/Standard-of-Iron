#pragma once

#include <QString>

#include <vector>

#include "arena_scenario.h"

namespace Arena::Scenarios {

inline constexpr char k_sword_duel_id[] = "sword_duel";
inline constexpr char k_spear_duel_id[] = "spear_duel";
inline constexpr char k_bow_exchange_id[] = "bow_exchange";
inline constexpr char k_held_weapon_stances_id[] = "held_weapon_stances";
inline constexpr char k_mounted_charge_id[] = "mounted_charge";
inline constexpr char k_braced_spear_charge_id[] = "braced_spear_charge";
inline constexpr char k_elephant_trample_id[] = "elephant_trample";
inline constexpr char k_catapult_impact_id[] = "catapult_impact";
inline constexpr char k_ballista_impact_id[] = "ballista_impact";
inline constexpr char k_structure_melee_assault_id[] = "structure_melee_assault";
inline constexpr char k_structure_projectile_assault_id[] =
    "structure_projectile_assault";
inline constexpr char k_structure_flaming_siege_id[] = "structure_flaming_siege";
inline constexpr char k_structure_melee_destruction_id[] =
    "structure_melee_destruction";
inline constexpr char k_catapult_ammunition_retarget_id[] =
    "catapult_ammunition_retarget";
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
inline constexpr char k_testudo_missile_defense_id[] = "testudo_missile_defense";
inline constexpr char k_shield_wall_cavalry_impact_id[] = "shield_wall_cavalry_impact";
inline constexpr char k_hold_stance_review_id[] = "hold_stance_review";
inline constexpr char k_hold_toggle_cycle_id[] = "hold_toggle_cycle";
inline constexpr char k_hold_transition_interrupts_id[] = "hold_transition_interrupts";
inline constexpr char k_lod_switch_id[] = "lod_switch";
inline constexpr char k_commander_aura_pulse_id[] = "commander_aura_pulse";
inline constexpr char k_rpg_melee_contact_id[] = "rpg_melee_contact";
inline constexpr char k_rpg_defense_contact_id[] = "rpg_defense_contact";
inline constexpr char k_rpg_projectile_block_id[] = "rpg_projectile_block";
inline constexpr char k_rpg_escort_crowd_id[] = "rpg_escort_crowd";
inline constexpr char k_rpg_locomotion_id[] = "rpg_locomotion";
inline constexpr char k_rpg_close_quarters_id[] = "rpg_close_quarters";
inline constexpr char k_commander_identity_lineup_id[] = "commander_identity_lineup";
inline constexpr char k_healer_identity_lineup_id[] = "healer_identity_lineup";
inline constexpr char k_healer_lod_probe_id[] = "healer_lod_probe";
inline constexpr char k_troop_identity_lineup_id[] = "troop_identity_lineup";
inline constexpr char k_worker_identity_lineup_id[] = "worker_identity_lineup";
inline constexpr char k_roman_settlement_works_id[] = "roman_settlement_works";
inline constexpr char k_carthage_settlement_works_id[] = "carthage_settlement_works";
inline constexpr char k_commander_consul_vs_broker_id[] = "commander_consul_vs_broker";
inline constexpr char k_commander_field_vs_cavalry_id[] = "commander_field_vs_cavalry";
inline constexpr char k_commander_legion_vs_elephant_id[] =
    "commander_legion_vs_elephant";
inline constexpr char k_path_bridge_crossing_id[] = "path_bridge_crossing";
inline constexpr char k_path_uphill_advance_id[] = "path_uphill_advance";
inline constexpr char k_path_wall_detour_id[] = "path_wall_detour";
inline constexpr char k_path_wall_breach_id[] = "path_wall_breach";
inline constexpr char k_road_junction_showcase_id[] = "road_junction_showcase";
inline constexpr char k_road_slope_showcase_id[] = "road_slope_showcase";
inline constexpr char k_road_bridge_approach_id[] = "road_bridge_approach";
inline constexpr char k_gate_friendly_passage_id[] = "gate_friendly_passage";
inline constexpr char k_gate_allied_access_id[] = "gate_allied_access";
inline constexpr char k_gate_enemy_blocked_id[] = "gate_enemy_blocked";
inline constexpr char k_gate_destroyed_breach_id[] = "gate_destroyed_breach";
inline constexpr char k_gate_consecutive_transit_id[] = "gate_consecutive_transit";

inline constexpr char k_three_swords_vs_two_spears_id[] = "three_swords_vs_two_spears";
inline constexpr char k_multi_front_melee_id[] = "multi_front_melee";
inline constexpr char k_survivor_compaction_id[] = "survivor_compaction";
inline constexpr char k_spear_walk_contact_id[] = "spear_walk_contact";
inline constexpr char k_archer_stability_id[] = "archer_stability";
inline constexpr char k_infantry_idle_ambient_id[] = "infantry_idle_ambient";
inline constexpr char k_mounted_idle_ambient_id[] = "mounted_idle_ambient";
inline constexpr char k_idle_ambient_interrupt_id[] = "idle_ambient_interrupt";
inline constexpr char k_archer_melee_lock_id[] = "archer_melee_lock";
inline constexpr char k_infantry_charge_id[] = "infantry_charge";
inline constexpr char k_flank_ambush_id[] = "flank_ambush";
inline constexpr char k_reserve_release_id[] = "reserve_release";
inline constexpr char k_mixed_roles_id[] = "mixed_roles";
inline constexpr char k_bot_skirmish_id[] = "bot_skirmish";
inline constexpr char k_crossing_formations_id[] = "crossing_formations";
inline constexpr char k_sustained_battle_id[] = "sustained_battle";
inline constexpr char k_fog_of_war_recon_id[] = "fog_of_war_recon";
inline constexpr char k_render_continuity_id[] = "render_continuity";
inline constexpr char k_performance_20v20_id[] = "performance_20v20";
inline constexpr char k_performance_30v30_id[] = "performance_30v30";
inline constexpr char k_campaign_scale_battle_id[] = "campaign_scale_battle";
inline constexpr char k_roman_marching_camp_id[] = "roman_marching_camp";
inline constexpr char k_carthage_trade_town_id[] = "carthage_trade_town";
inline constexpr char k_architecture_and_props_showcase_id[] =
    "architecture_and_props_showcase";
inline constexpr char k_world_prop_lineup_id[] = "world_prop_lineup";
inline constexpr char k_humanoid_gait_review_id[] = "humanoid_gait_review";
inline constexpr char k_sanctuary_precinct_day_id[] = "sanctuary_precinct_day";
inline constexpr char k_sanctuary_precinct_night_id[] = "sanctuary_precinct_night";
inline constexpr char k_sanctuary_precinct_storm_id[] = "sanctuary_precinct_storm";
inline constexpr char k_roman_fortification_showcase_id[] =
    "roman_fortification_showcase";
inline constexpr char k_carthage_fortification_showcase_id[] =
    "carthage_fortification_showcase";
inline constexpr char k_rival_economies_id[] = "rival_economies";
inline constexpr char k_village_harvest_cycle_id[] = "village_harvest_cycle";
inline constexpr char k_colony_founding_id[] = "colony_founding";
inline constexpr char k_village_raid_id[] = "village_raid";
inline constexpr char k_frontier_outpost_id[] = "frontier_outpost";
inline constexpr char k_riverside_mill_town_id[] = "riverside_mill_town";
inline constexpr char k_quarry_camp_id[] = "quarry_camp";
inline constexpr char k_trade_road_convoy_id[] = "trade_road_convoy";
inline constexpr char k_sepulcher_roster_lineup_id[] = "sepulcher_roster_lineup";
inline constexpr char k_sepulcher_spell_fx_showcase_id[] =
    "sepulcher_spell_fx_showcase";
inline constexpr char k_sepulcher_vs_rome_infantry_id[] = "sepulcher_vs_rome_infantry";
inline constexpr char k_sepulcher_vs_rome_ranged_id[] = "sepulcher_vs_rome_ranged";
inline constexpr char k_sepulcher_vs_carthage_infantry_id[] =
    "sepulcher_vs_carthage_infantry";
inline constexpr char k_sepulcher_vs_carthage_cavalry_id[] =
    "sepulcher_vs_carthage_cavalry";
inline constexpr char k_sepulcher_shrine_awakening_id[] = "sepulcher_shrine_awakening";
inline constexpr char k_sepulcher_ruins_awakening_waves_id[] =
    "sepulcher_ruins_awakening_waves";
inline constexpr char k_sepulcher_shrine_siege_id[] = "sepulcher_shrine_siege";

inline constexpr char k_water_showcase_id[] = "water_showcase";
inline constexpr char k_wall_corner_showcase_id[] = "wall_corner_showcase";
inline constexpr char k_lighting_sunrise_sunset_id[] = "lighting_sunrise_sunset";
inline constexpr char k_lighting_sunset_id[] = "lighting_sunset";
inline constexpr char k_lighting_moonlit_night_id[] = "lighting_moonlit_night";
inline constexpr char k_lighting_heavy_rain_id[] = "lighting_heavy_rain";
inline constexpr char k_lighting_dense_battle_id[] = "lighting_dense_battle";
inline constexpr char k_lighting_world_materials_id[] = "lighting_world_materials";
inline constexpr char k_lighting_midday_id[] = "lighting_midday";
inline constexpr char k_lighting_dawn_to_day_id[] = "lighting_dawn_to_day";
inline constexpr char k_lighting_afternoon_to_night_id[] =
    "lighting_afternoon_to_night";
inline constexpr char k_lighting_structure_shadows_id[] = "lighting_structure_shadows";
inline constexpr char k_lighting_sepulcher_readability_id[] =
    "lighting_sepulcher_readability";
inline constexpr char k_lighting_parity_instanced_id[] = "lighting_parity_instanced";
inline constexpr char k_lighting_parity_single_id[] = "lighting_parity_single";
inline constexpr char k_lighting_commander_closeup_id[] = "lighting_commander_closeup";
inline constexpr char k_lighting_shadow_quality_low_id[] =
    "lighting_shadow_quality_low";
inline constexpr char k_lighting_shadow_quality_medium_id[] =
    "lighting_shadow_quality_medium";
inline constexpr char k_lighting_shadow_quality_high_id[] =
    "lighting_shadow_quality_high";

inline constexpr char k_promo_last_stand_id[] = "promo_last_stand";
inline constexpr char k_promo_night_of_the_dead_id[] = "promo_night_of_the_dead";
inline constexpr char k_promo_storm_charge_id[] = "promo_storm_charge";

inline constexpr char k_weather_rain_light_id[] = "weather_rain_light";
inline constexpr char k_weather_rain_medium_id[] = "weather_rain_medium";
inline constexpr char k_weather_rain_heavy_id[] = "weather_rain_heavy";
inline constexpr char k_weather_snow_light_id[] = "weather_snow_light";
inline constexpr char k_weather_snow_medium_id[] = "weather_snow_medium";
inline constexpr char k_weather_snow_heavy_id[] = "weather_snow_heavy";
inline constexpr char k_weather_snow_crosswind_id[] = "weather_snow_crosswind";
inline constexpr char k_weather_rain_budget_low_id[] = "weather_rain_budget_low";
inline constexpr char k_weather_rain_budget_ultra_id[] = "weather_rain_budget_ultra";

struct ScenarioOption {
  QString id;
  QString label;
  QString description;
};

[[nodiscard]] auto options() -> const std::vector<ScenarioOption>&;
[[nodiscard]] auto find_option(const QString& scenario_id) -> const ScenarioOption*;

} // namespace Arena::Scenarios
