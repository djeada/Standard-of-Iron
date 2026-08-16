#pragma once

namespace Game::Audio {

namespace Cue {

inline constexpr const char* k_ui_hover = "ui.hover";
inline constexpr const char* k_ui_click = "ui.click";
inline constexpr const char* k_ui_back = "ui.back";
inline constexpr const char* k_ui_tab_switch = "ui.tab_switch";
inline constexpr const char* k_ui_panel_open = "ui.panel_open";
inline constexpr const char* k_ui_panel_close = "ui.panel_close";
inline constexpr const char* k_ui_toggle = "ui.toggle";
inline constexpr const char* k_ui_error = "ui.error";
inline constexpr const char* k_ui_notification = "ui.notification";
inline constexpr const char* k_ui_select_unit = "ui.select_unit";
inline constexpr const char* k_ui_select_group = "ui.select_group";
inline constexpr const char* k_ui_deselect = "ui.deselect";

inline constexpr const char* k_order_move = "order.move";
inline constexpr const char* k_order_attack = "order.attack";
inline constexpr const char* k_order_patrol = "order.patrol";
inline constexpr const char* k_order_stop = "order.stop";
inline constexpr const char* k_order_hold = "order.hold";
inline constexpr const char* k_order_guard = "order.guard";
inline constexpr const char* k_order_run = "order.run";
inline constexpr const char* k_order_formation = "order.formation";
inline constexpr const char* k_order_formation_placed = "order.formation_placed";
inline constexpr const char* k_order_gate_mode = "order.gate_mode";
inline constexpr const char* k_order_rally_set = "order.rally_set";

inline constexpr const char* k_build_placement_begin = "build.placement_begin";
inline constexpr const char* k_build_placement_confirmed = "build.placement_confirmed";
inline constexpr const char* k_build_placement_rejected = "build.placement_rejected";
inline constexpr const char* k_build_construction_started =
    "build.construction_started";
inline constexpr const char* k_build_construction_complete =
    "build.construction_complete";
inline constexpr const char* k_build_unit_queued = "build.unit_queued";
inline constexpr const char* k_build_unit_ready = "build.unit_ready";
inline constexpr const char* k_build_building_destroyed = "build.building_destroyed";
inline constexpr const char* k_build_gate_open = "build.gate_open";
inline constexpr const char* k_build_gate_close = "build.gate_close";

inline constexpr const char* k_alert_low_resources = "alert.low_resources";
inline constexpr const char* k_alert_population_limit = "alert.population_limit";
inline constexpr const char* k_alert_base_under_attack = "alert.base_under_attack";
inline constexpr const char* k_alert_reinforcements = "alert.reinforcements";
inline constexpr const char* k_alert_enemy_reinforcements =
    "alert.enemy_reinforcements";
inline constexpr const char* k_alert_objective_complete = "alert.objective_complete";
inline constexpr const char* k_alert_objective_failed = "alert.objective_failed";
inline constexpr const char* k_alert_unit_lost = "alert.unit_lost";

inline constexpr const char* k_combat_hit_sword = "combat.hit.sword";
inline constexpr const char* k_combat_hit_spear = "combat.hit.spear";
inline constexpr const char* k_combat_hit_arrow = "combat.hit.arrow";
inline constexpr const char* k_combat_hit_cavalry = "combat.hit.cavalry";
inline constexpr const char* k_combat_hit_elephant = "combat.hit.elephant";
inline constexpr const char* k_combat_hit_siege = "combat.hit.siege";
inline constexpr const char* k_combat_hit_generic = "combat.hit.generic";
inline constexpr const char* k_combat_death = "combat.death";
inline constexpr const char* k_combat_arrow_launch = "combat.arrow_launch";
inline constexpr const char* k_combat_bow_draw = "combat.bow_draw";
inline constexpr const char* k_combat_bow_full_draw = "combat.bow_full_draw";
inline constexpr const char* k_combat_bow_strain = "combat.bow_strain";
inline constexpr const char* k_combat_bow_loose_heavy = "combat.bow_loose_heavy";
inline constexpr const char* k_combat_arrow_flyby = "combat.arrow_flyby";
inline constexpr const char* k_combat_arrow_volley = "combat.arrow_volley";
inline constexpr const char* k_combat_siege_launch = "combat.siege_launch";
inline constexpr const char* k_combat_siege_impact = "combat.siege_impact";
inline constexpr const char* k_combat_charge = "combat.charge";
inline constexpr const char* k_combat_heal = "combat.heal";
inline constexpr const char* k_combat_guard_raise = "combat.guard_raise";
inline constexpr const char* k_combat_block = "combat.block";
inline constexpr const char* k_combat_perfect_guard = "combat.perfect_guard";
inline constexpr const char* k_combat_guard_break = "combat.guard_break";
inline constexpr const char* k_combat_dodge = "combat.dodge";
inline constexpr const char* k_combat_jump = "combat.jump";
inline constexpr const char* k_combat_land = "combat.land";
inline constexpr const char* k_combat_shield_bash = "combat.shield_bash";
inline constexpr const char* k_combat_vanguard_rush = "combat.vanguard_rush";
inline constexpr const char* k_combat_second_wind = "combat.second_wind";
inline constexpr const char* k_combat_ability_refused = "combat.ability_refused";
inline constexpr const char* k_combat_lock_on = "combat.lock_on";

inline constexpr const char* k_move_footstep = "move.footstep";
inline constexpr const char* k_move_footstep_hard = "move.footstep_hard";
inline constexpr const char* k_move_footstep_run = "move.footstep_run";

inline constexpr const char* k_wildlife_wolf_hunt = "wildlife.wolf_hunt";
inline constexpr const char* k_wildlife_wolf_bite = "wildlife.wolf_bite";

inline constexpr const char* k_state_victory = "state.victory";
inline constexpr const char* k_state_defeat = "state.defeat";
inline constexpr const char* k_state_pause = "state.pause";
inline constexpr const char* k_state_resume = "state.resume";
inline constexpr const char* k_state_speed_change = "state.speed_change";
inline constexpr const char* k_state_save_complete = "state.save_complete";
inline constexpr const char* k_state_load_complete = "state.load_complete";
inline constexpr const char* k_state_commander_enter = "state.commander_enter";
inline constexpr const char* k_state_commander_exit = "state.commander_exit";

inline constexpr std::array<const char*, 86> k_all = {
    k_ui_hover,
    k_ui_click,
    k_ui_back,
    k_ui_tab_switch,
    k_ui_panel_open,
    k_ui_panel_close,
    k_ui_toggle,
    k_ui_error,
    k_ui_notification,
    k_ui_select_unit,
    k_ui_select_group,
    k_ui_deselect,
    k_order_move,
    k_order_attack,
    k_order_patrol,
    k_order_stop,
    k_order_hold,
    k_order_guard,
    k_order_run,
    k_order_formation,
    k_order_formation_placed,
    k_order_gate_mode,
    k_order_rally_set,
    k_build_placement_begin,
    k_build_placement_confirmed,
    k_build_placement_rejected,
    k_build_construction_started,
    k_build_construction_complete,
    k_build_unit_queued,
    k_build_unit_ready,
    k_build_building_destroyed,
    k_build_gate_open,
    k_build_gate_close,
    k_alert_low_resources,
    k_alert_population_limit,
    k_alert_base_under_attack,
    k_alert_reinforcements,
    k_alert_enemy_reinforcements,
    k_alert_objective_complete,
    k_alert_objective_failed,
    k_alert_unit_lost,
    k_combat_hit_sword,
    k_combat_hit_spear,
    k_combat_hit_arrow,
    k_combat_hit_cavalry,
    k_combat_hit_elephant,
    k_combat_hit_siege,
    k_combat_hit_generic,
    k_combat_death,
    k_combat_arrow_launch,
    k_combat_bow_draw,
    k_combat_bow_full_draw,
    k_combat_bow_strain,
    k_combat_bow_loose_heavy,
    k_combat_arrow_flyby,
    k_combat_arrow_volley,
    k_combat_siege_launch,
    k_combat_siege_impact,
    k_combat_charge,
    k_combat_heal,
    k_combat_guard_raise,
    k_combat_block,
    k_combat_perfect_guard,
    k_combat_guard_break,
    k_combat_dodge,
    k_combat_jump,
    k_combat_land,
    k_combat_shield_bash,
    k_combat_vanguard_rush,
    k_combat_second_wind,
    k_combat_ability_refused,
    k_combat_lock_on,
    k_move_footstep,
    k_move_footstep_hard,
    k_move_footstep_run,
    k_wildlife_wolf_hunt,
    k_wildlife_wolf_bite,
    k_state_victory,
    k_state_defeat,
    k_state_pause,
    k_state_resume,
    k_state_speed_change,
    k_state_save_complete,
    k_state_load_complete,
    k_state_commander_enter,
    k_state_commander_exit,
};

} // namespace Cue

} // namespace Game::Audio
