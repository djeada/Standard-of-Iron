#include "app/world/battle_stats.h"

#include <QCoreApplication>
#include <QJsonArray>

#include <algorithm>

#include "game/core/component_gameplay.h"
#include "game/core/world.h"
#include "game/session/session_context.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/nation_id.h"
#include "game/systems/troop_count_registry.h"
#include "game/units/spawn_type.h"

namespace App::World {

namespace {

constexpr int k_stats_version = 1;

auto engine_text(const char* source) -> QString {
  return QCoreApplication::translate("GameEngine", source);
}

} // namespace

auto format_defeat_announcement(const PlayerDefeatWatcher::Defeat& defeat) -> QString {
  if (defeat.commander_name.isEmpty()) {
    return defeat.ally
               ? engine_text("Our ally %1 has been defeated.").arg(defeat.owner_name)
               : engine_text("%1 has been defeated.").arg(defeat.owner_name);
  }
  if (defeat.ally) {
    return engine_text("Our ally %1 is finished - %2 has fallen.")
        .arg(defeat.owner_name, defeat.commander_name);
  }
  return engine_text("%1 is finished - %2 has fallen.")
      .arg(defeat.owner_name, defeat.commander_name);
}

auto BattleStats::player_stats(Game::Session::SessionContext& session,
                               int owner_id) -> QVariantMap {
  QVariantMap result;
  const auto* stats = session.stats().get_stats(owner_id);

  if (stats != nullptr) {
    result["troopsRecruited"] = stats->troops_recruited;
    result["enemiesKilled"] = stats->enemies_killed;
    result["losses"] = stats->losses;
    result["barracksOwned"] = stats->barracks_owned;
    result["playTimeSec"] = stats->play_time_sec;
    result["gameEnded"] = stats->game_ended;
  } else {
    result["troopsRecruited"] = 0;
    result["enemiesKilled"] = 0;
    result["losses"] = 0;
    result["barracksOwned"] = 0;
    result["playTimeSec"] = 0.0F;
    result["gameEnded"] = false;
  }

  return result;
}

auto BattleStats::reset() -> bool {
  m_units_defeated = 0;
  if (m_troops_defeated == 0) {
    return false;
  }
  m_troops_defeated = 0;
  return true;
}

auto BattleStats::note_unit_died(const Engine::Core::UnitDiedEvent& event,
                                 Engine::Core::World* world,
                                 int local_owner_id) -> bool {
  if (!Game::Units::is_troop_spawn(event.spawn_type) ||
      event.owner_id == local_owner_id || event.killer_owner_id != local_owner_id) {
    return false;
  }
  const auto* unit = world != nullptr
                         ? world->try_get<Engine::Core::UnitComponent>(event.unit_id)
                         : nullptr;
  m_troops_defeated +=
      unit != nullptr
          ? std::max(1, Game::Systems::squad_men(*unit))
          : std::max(1,
                     Game::Systems::troop_type_men(
                         Game::Systems::NationID::RomanRepublic, event.spawn_type));
  ++m_units_defeated;
  return true;
}

void BattleStats::announce_defeats(
    Engine::Core::World& world,
    int local_owner_id,
    float dt,
    const PlayerDefeatWatcher::Announce& announce,
    const PlayerDefeatWatcher::StillExpected& still_expected) {
  m_defeat_watcher.update(world, local_owner_id, dt, announce, still_expected);
}

auto BattleStats::serialize(Game::Session::SessionContext* session) const
    -> QJsonObject {
  QJsonObject state;
  state["version"] = k_stats_version;
  state["enemy_troops_defeated"] = m_troops_defeated;
  state["enemy_units_defeated"] = m_units_defeated;
  if (session != nullptr) {
    state["players"] = session->stats().serialize_counters();
  }
  return state;
}

auto BattleStats::restore(const QJsonObject& state,
                          Game::Session::SessionContext* session) -> bool {
  if (state.isEmpty() || state.value("version").toInt(0) > k_stats_version) {
    return false;
  }
  m_units_defeated = state.value("enemy_units_defeated").toInt();
  const int troops = state.value("enemy_troops_defeated").toInt();
  const bool troops_changed = m_troops_defeated != troops;
  m_troops_defeated = troops;
  if (session != nullptr) {
    session->stats().restore_counters(state.value("players").toArray());
  }
  return troops_changed;
}

} // namespace App::World
