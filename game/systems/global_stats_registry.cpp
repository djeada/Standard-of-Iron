#include "global_stats_registry.h"
#include "../core/component.h"
#include "../core/world.h"
#include "../units/troop_config.h"
#include "core/event_manager.h"
#include "owner_registry.h"
#include "units/spawn_type.h"
#include <chrono>
#include <unordered_map>

namespace Game::Systems {

auto GlobalStatsRegistry::instance() -> GlobalStatsRegistry & {
  static GlobalStatsRegistry inst;
  return inst;
}

void GlobalStatsRegistry::initialize() {
  m_unit_spawned_subscription =
      Engine::Core::ScopedEventSubscription<Engine::Core::UnitSpawnedEvent>(
          [this](const Engine::Core::UnitSpawnedEvent &e) {
            on_unit_spawned(e);
          });

  m_unit_died_subscription =
      Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent>(
          [this](const Engine::Core::UnitDiedEvent &e) { on_unit_died(e); });

  m_barrack_captured_subscription =
      Engine::Core::ScopedEventSubscription<Engine::Core::BarrackCapturedEvent>(
          [this](const Engine::Core::BarrackCapturedEvent &e) {
            on_barrack_captured(e);
          });
}

void GlobalStatsRegistry::clear() { m_player_stats.clear(); }

auto GlobalStatsRegistry::getStats(int owner_id) const -> const PlayerStats * {
  auto it = m_player_stats.find(owner_id);
  if (it != m_player_stats.end()) {
    return &it->second;
  }
  return nullptr;
}

auto GlobalStatsRegistry::getStats(int owner_id) -> PlayerStats * {
  auto it = m_player_stats.find(owner_id);
  if (it != m_player_stats.end()) {
    return &it->second;
  }
  return nullptr;
}

void GlobalStatsRegistry::mark_game_start(int owner_id) {
  auto &stats = m_player_stats[owner_id];
  stats.game_start_time = std::chrono::steady_clock::now();
  stats.game_ended = false;
  stats.play_time_sec = 0.0F;
}

void GlobalStatsRegistry::mark_game_end(int owner_id) {
  auto it = m_player_stats.find(owner_id);
  if (it != m_player_stats.end() && !it->second.game_ended) {
    it->second.game_end_time = std::chrono::steady_clock::now();
    it->second.game_ended = true;

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        it->second.game_end_time - it->second.game_start_time);
    it->second.play_time_sec = duration.count() / 1000.0F;
  }
}

void GlobalStatsRegistry::on_unit_spawned(
    const Engine::Core::UnitSpawnedEvent &event) {

  auto &stats = m_player_stats[event.owner_id];

  if (event.spawn_type != Game::Units::SpawnType::Barracks) {
    int const production_cost =
        Game::Units::TroopConfig::instance().getProductionCost(
            event.spawn_type);
    stats.troops_recruited += production_cost;
  } else {
    stats.barracks_owned++;
  }
}

void GlobalStatsRegistry::on_unit_died(
    const Engine::Core::UnitDiedEvent &event) {

  if (event.spawn_type == Game::Units::SpawnType::Barracks) {
    auto it = m_player_stats.find(event.owner_id);
    if (it != m_player_stats.end()) {
      it->second.barracks_owned--;
      if (it->second.barracks_owned < 0) {
        it->second.barracks_owned = 0;
      }
    }
  }

  if (event.killer_owner_id != 0 && event.killer_owner_id != event.owner_id) {

    auto &owner_registry = OwnerRegistry::instance();

    if (owner_registry.are_enemies(event.killer_owner_id, event.owner_id)) {
      auto &stats = m_player_stats[event.killer_owner_id];

      if (event.spawn_type != Game::Units::SpawnType::Barracks) {
        int const production_cost =
            Game::Units::TroopConfig::instance().getProductionCost(
                event.spawn_type);
        stats.enemies_killed += production_cost;
      }
    }
  }
}

void GlobalStatsRegistry::on_barrack_captured(
    const Engine::Core::BarrackCapturedEvent &event) {

  auto prev_it = m_player_stats.find(event.previous_owner_id);
  if (prev_it != m_player_stats.end() && event.previous_owner_id != -1) {
    prev_it->second.barracks_owned--;
    if (prev_it->second.barracks_owned < 0) {
      prev_it->second.barracks_owned = 0;
    }
  }

  auto &new_stats = m_player_stats[event.new_owner_id];
  new_stats.barracks_owned++;
}

void GlobalStatsRegistry::rebuild_from_world(Engine::Core::World &world) {

  std::unordered_map<int, std::chrono::steady_clock::time_point> start_times;
  for (auto &[owner_id, stats] : m_player_stats) {
    start_times[owner_id] = stats.game_start_time;
  }

  m_player_stats.clear();

  for (auto &[owner_id, startTime] : start_times) {
    m_player_stats[owner_id].game_start_time = startTime;
  }

  auto entities = world.get_entities_with<Engine::Core::UnitComponent>();
  for (auto *e : entities) {
    auto *unit = e->get_component<Engine::Core::UnitComponent>();
    if ((unit == nullptr) || unit->health <= 0) {
      continue;
    }

    auto &stats = m_player_stats[unit->owner_id];

    if (unit->spawn_type == Game::Units::SpawnType::Barracks) {
      stats.barracks_owned++;
    } else {
      int const production_cost =
          Game::Units::TroopConfig::instance().getProductionCost(
              unit->spawn_type);
      stats.troops_recruited += production_cost;
    }
  }
}

} // namespace Game::Systems
