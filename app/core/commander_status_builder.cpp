#include "commander_status_builder.h"

#include <QString>
#include <QVariantMap>

#include <algorithm>

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/systems/rpg_combat_system/rpg_targeting.h"
#include "game/util/asset_text.h"

namespace App::Core {

auto build_controlled_commander_status(const CommanderStatusInput& input)
    -> QVariantMap {
  auto* world = const_cast<Engine::Core::World*>(input.world);
  QVariantMap result;
  result["has_commander"] = false;
  result["id"] = 0;
  result["name"] = QString();
  result["nation"] = QString();
  result["alive"] = false;
  result["health"] = 0;
  result["max_health"] = 0;
  result["health_ratio"] = 0.0;
  result["stamina_ratio"] = 1.0;
  result["is_running"] = false;
  result["can_run"] = false;
  result["rally_cooldown"] = 0.0;
  result["rally_cooldown_remaining"] = 0.0;
  result["rally_feedback_time"] = 0.0;
  result["rally_ready"] = false;
  result["rally_placing"] = false;
  result["rally_in_progress"] = false;
  result["rally_is_planting"] = false;
  result["rally_has_flag"] = false;
  result["rally_action_progress"] = 0.0;
  result["aura_active"] = false;
  result["aura_available"] = false;
  result["aura_duration"] = 0.0;
  result["aura_remaining"] = 0.0;
  result["aura_cooldown"] = 0.0;
  result["aura_cooldown_remaining"] = 0.0;
  result["aura_ready"] = false;
  result["posture"] = 0.0;
  result["posture_max"] = 100.0;
  result["posture_ratio"] = 0.0;
  result["punish_window_remaining"] = 0.0;
  result["punish_active"] = false;
  result["perfect_guard_remaining"] = 0.0;
  result["perfect_guard_active"] = false;
  result["guard_break_remaining"] = 0.0;
  result["guard_broken"] = false;
  result["guard_active"] = false;
  result["combat_phase"] = 0;
  result["attack_direction"] = 0;
  result["is_attacking"] = false;
  result["dodge_active"] = false;
  result["aim_candidate_in_range"] = false;
  result["finisher_ready"] = false;
  result["camera_mode"] = QStringLiteral("Chase");
  result["shield_bash_cooldown"] = 3.0;
  result["shield_bash_cooldown_remaining"] = 0.0;
  result["shield_bash_ready"] = true;
  result["vanguard_rush_cooldown"] = 4.5;
  result["vanguard_rush_cooldown_remaining"] = 0.0;
  result["vanguard_rush_ready"] = true;
  result["second_wind_cooldown"] = 8.0;
  result["second_wind_cooldown_remaining"] = 0.0;
  result["second_wind_ready"] = true;

  if (world == nullptr || input.controlled_commander_id == 0) {
    return result;
  }

  auto* entity = world->get_entity(input.controlled_commander_id);
  auto* unit = entity != nullptr ? entity->get_component<Engine::Core::UnitComponent>()
                                 : nullptr;
  auto* commander = entity != nullptr
                        ? entity->get_component<Engine::Core::CommanderComponent>()
                        : nullptr;
  if (entity == nullptr || unit == nullptr || commander == nullptr) {
    return result;
  }

  QString name;
  int health = 0;
  int max_health = 0;
  bool is_building = false;
  bool alive = false;
  QString nation;
  if (!input.get_unit_info(input.controlled_commander_id,
                           name,
                           health,
                           max_health,
                           is_building,
                           alive,
                           nation)) {
    return result;
  }

  float stamina_ratio = 1.0F;
  bool is_running = false;
  bool can_run = false;
  (void)input.get_unit_stamina_info(
      input.controlled_commander_id, stamina_ratio, is_running, can_run);

  if (!commander->display_name.empty()) {
    name =
        Game::Util::tr_asset(Game::Util::k_commanders_context, commander->display_name);
  }

  result["has_commander"] = true;
  result["id"] = static_cast<int>(input.controlled_commander_id);
  result["name"] = name;
  result["nation"] = nation;
  result["alive"] = alive;

  auto* commander_entity = world->get_entity(input.controlled_commander_id);
  auto* rpg = commander_entity != nullptr
                  ? commander_entity->get_component<Engine::Core::RpgHealthComponent>()
                  : nullptr;
  if (rpg != nullptr && rpg->active) {
    result["health"] = rpg->rpg_hp;
    result["max_health"] = rpg->rpg_max_hp;
    result["health_ratio"] =
        rpg->rpg_max_hp > 0
            ? static_cast<double>(rpg->rpg_hp) / static_cast<double>(rpg->rpg_max_hp)
            : 0.0;
  } else {
    result["health"] = health;
    result["max_health"] = max_health;
    result["health_ratio"] =
        max_health > 0 ? static_cast<double>(health) / static_cast<double>(max_health)
                       : 0.0;
  }
  result["stamina_ratio"] = stamina_ratio;
  result["is_running"] = is_running;
  result["can_run"] = can_run;
  result["rally_cooldown"] = commander->rally_cooldown;
  result["rally_cooldown_remaining"] = commander->rally_cooldown_remaining;
  result["rally_feedback_time"] = commander->rally_feedback_time;
  result["rally_placing"] = input.rally_placing;
  result["rally_in_progress"] = commander->flag_rally_in_progress;
  result["rally_is_planting"] = commander->is_flag_rally_planting();
  result["rally_has_flag"] = commander->flag_rally_flag_active;
  result["rally_action_progress"] =
      commander->is_flag_rally_planting() && commander->flag_rally_cost > 0.0F
          ? std::clamp(
                static_cast<double>(1.0F - (commander->flag_rally_animation_timer /
                                            commander->flag_rally_cost)),
                0.0,
                1.0)
          : (commander->flag_rally_flag_active ? 1.0 : 0.0);
  result["rally_ready"] = commander->rally_cooldown_remaining <= 0.0F &&
                          !commander->flag_rally_in_progress && !input.rally_placing;
  result["aura_active"] = commander->aura_ability_active;
  result["aura_available"] = commander->aura_active && !commander->wounded;
  result["aura_duration"] = commander->aura_ability_duration;
  result["aura_remaining"] = commander->aura_ability_remaining;
  result["aura_cooldown"] = commander->aura_ability_cooldown;
  result["aura_cooldown_remaining"] = commander->aura_ability_cooldown_remaining;
  result["aura_ready"] = commander->can_activate_aura_ability();
  result["posture"] = static_cast<double>(commander->posture);
  result["posture_max"] = static_cast<double>(commander->posture_max);
  result["posture_ratio"] =
      commander->posture_max > 0.0F
          ? static_cast<double>(commander->posture / commander->posture_max)
          : 0.0;
  result["punish_window_remaining"] =
      static_cast<double>(commander->punish_window_remaining);
  result["punish_active"] = commander->punish_window_remaining > 0.0F;
  result["finisher_ready"] = commander->combo_step >= 3;
  result["camera_mode"] =
      commander->close_camera_mode ? QStringLiteral("Close") : QStringLiteral("Chase");

  result["combo_step"] = commander->combo_step;
  result["shield_bash_cooldown"] = 3.0;
  result["shield_bash_cooldown_remaining"] =
      static_cast<double>(commander->shield_bash_cooldown_remaining);
  result["shield_bash_ready"] = commander->shield_bash_cooldown_remaining <= 0.0F;
  result["vanguard_rush_cooldown"] = 4.5;
  result["vanguard_rush_cooldown_remaining"] =
      static_cast<double>(commander->vanguard_rush_cooldown_remaining);
  result["vanguard_rush_ready"] = commander->vanguard_rush_cooldown_remaining <= 0.0F;
  result["second_wind_cooldown"] = 8.0;
  result["second_wind_cooldown_remaining"] =
      static_cast<double>(commander->second_wind_cooldown_remaining);
  result["second_wind_ready"] = commander->second_wind_cooldown_remaining <= 0.0F;

  if (auto* guard =
          commander_entity != nullptr
              ? commander_entity->get_component<Engine::Core::CommanderGuardComponent>()
              : nullptr) {
    result["perfect_guard_remaining"] =
        static_cast<double>(guard->perfect_guard_remaining);
    result["perfect_guard_active"] = guard->perfect_guard_remaining > 0.0F;
    result["guard_break_remaining"] = static_cast<double>(guard->guard_break_remaining);
    result["guard_broken"] = guard->guard_break_remaining > 0.0F;
    result["guard_active"] = guard->active;
  }

  if (auto* combat_state =
          commander_entity != nullptr
              ? commander_entity->get_component<Engine::Core::CombatStateComponent>()
              : nullptr) {
    result["combat_phase"] = static_cast<int>(combat_state->animation_state);
    result["attack_direction"] = static_cast<int>(combat_state->attack_direction);
    result["is_attacking"] =
        combat_state->animation_state != Engine::Core::CombatAnimationState::Idle;
  }

  result["dodge_active"] = input.dodge_active;

  result["locked_target_name"] = QString();
  result["locked_target_hp"] = 0;
  result["locked_target_max_hp"] = 0;
  result["locked_target_hp_ratio"] = 0.0;
  result["locked_target_staggered"] = false;
  result["locked_target_guard_broken"] = false;

  result["focus_marker_valid"] = false;
  result["focus_marker_locked"] = false;
  result["focus_marker_x"] = 0.0;
  result["focus_marker_y"] = 0.0;
  result["focus_marker_z"] = 0.0;

  Engine::Core::EntityID locked_id = input.locked_target_id;
  std::uint16_t locked_slot =
      Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot;
  Engine::Core::EntityID aim_id = 0;
  std::uint16_t aim_slot = Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot;
  if (auto* rpg_targets =
          commander_entity != nullptr
              ? commander_entity
                    ->get_component<Engine::Core::RpgCommanderTargetComponent>()
              : nullptr) {
    locked_id = rpg_targets->explicit_lock_target_id;
    locked_slot = rpg_targets->explicit_lock_soldier_slot;
    result["aim_candidate_in_range"] =
        rpg_targets->aim_candidate_in_range && rpg_targets->aim_candidate_id != 0;
    if (rpg_targets->aim_candidate_in_range) {
      aim_id = rpg_targets->aim_candidate_id;
      aim_slot = rpg_targets->aim_candidate_soldier_slot;
    }
  }

  {
    const bool prefer_lock = locked_id != 0;
    const Engine::Core::EntityID focus_id = prefer_lock ? locked_id : aim_id;
    const std::uint16_t focus_slot = prefer_lock ? locked_slot : aim_slot;
    auto* focus_entity = focus_id != 0 ? world->get_entity(focus_id) : nullptr;
    auto* focus_unit = focus_entity != nullptr
                           ? focus_entity->get_component<Engine::Core::UnitComponent>()
                           : nullptr;
    if (focus_entity != nullptr && focus_unit != nullptr && focus_unit->health > 0) {
      if (auto const sample = Game::Systems::RpgCombat::resolve_soldier_target(
              *focus_entity, focus_slot)) {
        result["focus_marker_valid"] = true;
        result["focus_marker_locked"] = prefer_lock;
        result["focus_marker_x"] = static_cast<double>(sample->position.x());
        result["focus_marker_y"] = static_cast<double>(sample->position.y());
        result["focus_marker_z"] = static_cast<double>(sample->position.z());
      }
    }
  }
  if (locked_id != 0 && world != nullptr) {
    auto* locked_ent = world->get_entity(locked_id);
    if (locked_ent != nullptr) {
      auto* locked_unit = locked_ent->get_component<Engine::Core::UnitComponent>();
      auto* locked_cmd = locked_ent->get_component<Engine::Core::CommanderComponent>();
      if (locked_unit != nullptr && locked_unit->health > 0) {
        QString lname;
        int lhp = 0;
        int lmax = 0;
        bool lb = false;
        bool la = false;
        QString lnation;
        if (input.get_unit_info(locked_id, lname, lhp, lmax, lb, la, lnation)) {
          if (locked_cmd != nullptr && !locked_cmd->display_name.empty()) {
            lname = Game::Util::tr_asset(Game::Util::k_commanders_context,
                                         locked_cmd->display_name);
          }
          result["locked_target_name"] = lname;
          result["locked_target_hp"] = lhp;
          result["locked_target_max_hp"] = lmax;
          result["locked_target_hp_ratio"] =
              lmax > 0 ? static_cast<double>(lhp) / static_cast<double>(lmax) : 0.0;
        }

        auto* locked_stagger =
            locked_ent->get_component<Engine::Core::StaggerComponent>();
        result["locked_target_staggered"] =
            locked_stagger != nullptr && locked_stagger->remaining > 0.0F;
        auto* locked_guard =
            locked_ent->get_component<Engine::Core::CommanderGuardComponent>();
        result["locked_target_guard_broken"] =
            locked_guard != nullptr && locked_guard->guard_break_remaining > 0.0F;
      }
    }
  }

  return result;
}

} // namespace App::Core
