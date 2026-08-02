#pragma once

#include <array>
#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "audio_system.h"

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

inline constexpr const char* k_combat_hit_sword = "combat.hit.sword";
inline constexpr const char* k_combat_hit_spear = "combat.hit.spear";
inline constexpr const char* k_combat_hit_arrow = "combat.hit.arrow";
inline constexpr const char* k_combat_hit_cavalry = "combat.hit.cavalry";
inline constexpr const char* k_combat_hit_elephant = "combat.hit.elephant";
inline constexpr const char* k_combat_hit_siege = "combat.hit.siege";
inline constexpr const char* k_combat_hit_generic = "combat.hit.generic";
inline constexpr const char* k_combat_death = "combat.death";
inline constexpr const char* k_combat_arrow_launch = "combat.arrow_launch";
inline constexpr const char* k_combat_arrow_flyby = "combat.arrow_flyby";
inline constexpr const char* k_combat_arrow_volley = "combat.arrow_volley";
inline constexpr const char* k_combat_siege_launch = "combat.siege_launch";
inline constexpr const char* k_combat_siege_impact = "combat.siege_impact";
inline constexpr const char* k_combat_charge = "combat.charge";
inline constexpr const char* k_combat_heal = "combat.heal";

inline constexpr const char* k_state_victory = "state.victory";
inline constexpr const char* k_state_defeat = "state.defeat";

inline constexpr std::array<const char*, 51> k_all = {
    k_ui_hover,
    k_ui_click,
    k_ui_back,
    k_ui_tab_switch,
    k_ui_panel_open,
    k_ui_panel_close,
    k_ui_toggle,
    k_ui_error,
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
    k_combat_hit_sword,
    k_combat_hit_spear,
    k_combat_hit_arrow,
    k_combat_hit_cavalry,
    k_combat_hit_elephant,
    k_combat_hit_siege,
    k_combat_hit_generic,
    k_combat_death,
    k_combat_arrow_launch,
    k_combat_arrow_flyby,
    k_combat_arrow_volley,
    k_combat_siege_launch,
    k_combat_siege_impact,
    k_combat_charge,
    k_combat_heal,
    k_state_victory,
    k_state_defeat,
};

} // namespace Cue

struct CueBinding {
  std::vector<std::string> resource_ids;
  AudioCategory category{AudioCategory::SFX};
  float volume{AudioConstants::DEFAULT_VOLUME};
  int priority{AudioConstants::DEFAULT_PRIORITY};
  int cooldown_ms{0};
  bool loop{false};
};

class CueRegistry {
public:
  static auto instance() -> CueRegistry&;

  void bind(const std::string& cue_id, CueBinding binding);
  void clear();

  auto play(const std::string& cue_id,
            float volume_scale = AudioConstants::DEFAULT_VOLUME) -> bool;

  [[nodiscard]] auto is_bound(const std::string& cue_id) const -> bool;

  [[nodiscard]] auto silent_cues() const -> std::vector<std::string>;

private:
  CueRegistry() = default;

  CueRegistry(const CueRegistry&) = delete;
  auto operator=(const CueRegistry&) -> CueRegistry& = delete;

  auto choose_resource_locked(const std::string& cue_id,
                              const CueBinding& binding) -> std::string;

  mutable std::mutex m_mutex;
  std::unordered_map<std::string, CueBinding> m_bindings;
  std::unordered_map<std::string, std::chrono::steady_clock::time_point> m_last_played;
  std::unordered_map<std::string, std::string> m_last_resource;
  std::unordered_map<std::string, unsigned> m_silent_requests;
};

auto play_cue(const std::string& cue_id,
              float volume_scale = AudioConstants::DEFAULT_VOLUME) -> bool;

} // namespace Game::Audio
