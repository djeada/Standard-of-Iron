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
  m_unitSpawnedSubscription =
      Engine::Core::ScopedEventSubscription<Engine::Core::UnitSpawnedEvent>(
          [this](const Engine::Core::UnitSpawnedEvent &e) {
            onUnitSpawned(e);
          });

  m_unitDiedSubscription =
      Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent>(
          [this](const Engine::Core::UnitDiedEvent &e) { onUnitDied(e); });

  m_barrackCapturedSubscription =
      Engine::Core::ScopedEventSubscription<Engine::Core::BarrackCapturedEvent>(
          [this](const Engine::Core::BarrackCapturedEvent &e) {
            onBarrackCaptured(e);
          });
}

void GlobalStatsRegistry::clear() { m_playerStats.clear(); }

auto GlobalStatsRegistry::getStats(int owner_id) const -> const PlayerStats * {
  auto it = m_playerStats.find(owner_id);
  if (it != m_playerStats.end()) {
    return &it->second;
  }
  return nullptr;
}

auto GlobalStatsRegistry::getStats(int owner_id) -> PlayerStats * {
  auto it = m_playerStats.find(owner_id);
  if (it != m_playerStats.end()) {
    return &it->second;
  }
  return nullptr;
}

void GlobalStatsRegistry::markGameStart(int owner_id) {
  auto &stats = m_playerStats[owner_id];
  stats.gameStartTime = std::chrono::steady_clock::now();
  stats.gameEnded = false;
  stats.playTimeSec = 0.0F;
}

void GlobalStatsRegistry::markGameEnd(int owner_id) {
  auto it = m_playerStats.find(owner_id);
  if (it != m_playerStats.end() && !it->second.gameEnded) {
    it->second.gameEndTime = std::chrono::steady_clock::now();
    it->second.gameEnded = true;

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        it->second.gameEndTime - it->second.gameStartTime);
    it->second.playTimeSec = duration.count() / 1000.0F;
  }
}

void GlobalStatsRegistry::onUnitSpawned(
    const Engine::Core::UnitSpawnedEvent &event) {

  auto &stats = m_playerStats[event.owner_id];

  if (event.spawn_type != Game::Units::SpawnType::Barracks) {
    int const individuals_per_unit =
        Game::Units::TroopConfig::instance().getIndividualsPerUnit(
            event.spawn_type);
    stats.troopsRecruited += individuals_per_unit;
  } else {

    stats.barracksOwned++;
  }
}

void GlobalStatsRegistry::onUnitDied(const Engine::Core::UnitDiedEvent &event) {

  if (event.spawn_type == Game::Units::SpawnType::Barracks) {
    auto it = m_playerStats.find(event.owner_id);
    if (it != m_playerStats.end()) {
      it->second.barracksOwned--;
      if (it->second.barracksOwned < 0) {
        it->second.barracksOwned = 0;
      }
    }
  }

  if (event.killer_owner_id != 0 && event.killer_owner_id != event.owner_id) {

    auto &owner_registry = OwnerRegistry::instance();

    if (owner_registry.areEnemies(event.killer_owner_id, event.owner_id)) {
      auto &stats = m_playerStats[event.killer_owner_id];

      if (event.spawn_type != Game::Units::SpawnType::Barracks) {
        int const individuals_per_unit =
            Game::Units::TroopConfig::instance().getIndividualsPerUnit(
                event.spawn_type);
        stats.enemiesKilled += individuals_per_unit;
      }
    }
  }
}

void GlobalStatsRegistry::onBarrackCaptured(
    const Engine::Core::BarrackCapturedEvent &event) {

  auto prev_it = m_playerStats.find(event.previous_owner_id);
  if (prev_it != m_playerStats.end() && event.previous_owner_id != -1) {
    prev_it->second.barracksOwned--;
    if (prev_it->second.barracksOwned < 0) {
      prev_it->second.barracksOwned = 0;
    }
  }

  auto &new_stats = m_playerStats[event.newOwnerId];
  new_stats.barracksOwned++;
}

void GlobalStatsRegistry::rebuildFromWorld(Engine::Core::World &world) {

  std::unordered_map<int, std::chrono::steady_clock::time_point> start_times;
  for (auto &[owner_id, stats] : m_playerStats) {
    start_times[owner_id] = stats.gameStartTime;
  }

  m_playerStats.clear();

  for (auto &[owner_id, startTime] : start_times) {
    m_playerStats[owner_id].gameStartTime = startTime;
  }

  auto entities = world.getEntitiesWith<Engine::Core::UnitComponent>();
  for (auto *e : entities) {
    auto *unit = e->getComponent<Engine::Core::UnitComponent>();
    if ((unit == nullptr) || unit->health <= 0) {
      continue;
    }

    auto &stats = m_playerStats[unit->owner_id];

    if (unit->spawn_type == Game::Units::SpawnType::Barracks) {
      stats.barracksOwned++;
    } else {
      int const individuals_per_unit =
          Game::Units::TroopConfig::instance().getIndividualsPerUnit(
              unit->spawn_type);
      stats.troopsRecruited += individuals_per_unit;
    }
  }
}

} // namespace Game::Systems
