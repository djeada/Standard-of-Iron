#include "global_stats_registry.h"
#include "../core/component.h"
#include "../core/world.h"
#include "../units/troop_config.h"
#include "owner_registry.h"
#include <QDebug>

namespace Game::Systems {

GlobalStatsRegistry &GlobalStatsRegistry::instance() {
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
}

void GlobalStatsRegistry::clear() { m_playerStats.clear(); }

const PlayerStats *GlobalStatsRegistry::getStats(int ownerId) const {
  auto it = m_playerStats.find(ownerId);
  if (it != m_playerStats.end()) {
    return &it->second;
  }
  return nullptr;
}

PlayerStats *GlobalStatsRegistry::getStats(int ownerId) {
  auto it = m_playerStats.find(ownerId);
  if (it != m_playerStats.end()) {
    return &it->second;
  }
  return nullptr;
}

void GlobalStatsRegistry::markGameStart(int ownerId) {
  auto &stats = m_playerStats[ownerId];
  stats.gameStartTime = std::chrono::steady_clock::now();
  stats.gameEnded = false;
  stats.playTimeSec = 0.0f;
}

void GlobalStatsRegistry::markGameEnd(int ownerId) {
  auto it = m_playerStats.find(ownerId);
  if (it != m_playerStats.end() && !it->second.gameEnded) {
    it->second.gameEndTime = std::chrono::steady_clock::now();
    it->second.gameEnded = true;
    
    // Calculate play time in seconds
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        it->second.gameEndTime - it->second.gameStartTime);
    it->second.playTimeSec = duration.count() / 1000.0f;
  }
}

void GlobalStatsRegistry::onUnitSpawned(
    const Engine::Core::UnitSpawnedEvent &event) {
  
  auto &stats = m_playerStats[event.ownerId];

  // Track troops recruited (exclude barracks themselves)
  if (event.unitType != "barracks") {
    int individualsPerUnit =
        Game::Units::TroopConfig::instance().getIndividualsPerUnit(
            event.unitType);
    stats.troopsRecruited += individualsPerUnit;
  } else {
    // Track barracks ownership
    stats.barracksOwned++;
  }
}

void GlobalStatsRegistry::onUnitDied(const Engine::Core::UnitDiedEvent &event) {
  // Update barracks ownership count
  if (event.unitType == "barracks") {
    auto it = m_playerStats.find(event.ownerId);
    if (it != m_playerStats.end()) {
      it->second.barracksOwned--;
      if (it->second.barracksOwned < 0) {
        it->second.barracksOwned = 0;
      }
    }
  }

  // Track enemies killed
  if (event.killerOwnerId != 0 && event.killerOwnerId != event.ownerId) {
    // Increment kill count for the killer's owner
    auto &ownerRegistry = OwnerRegistry::instance();
    
    // Only count kills if they are enemies (not allies)
    if (ownerRegistry.areEnemies(event.killerOwnerId, event.ownerId)) {
      auto &stats = m_playerStats[event.killerOwnerId];
      
      // Count individuals killed, not just units
      if (event.unitType != "barracks") {
        int individualsPerUnit =
            Game::Units::TroopConfig::instance().getIndividualsPerUnit(
                event.unitType);
        stats.enemiesKilled += individualsPerUnit;
      }
    }
  }
}

void GlobalStatsRegistry::rebuildFromWorld(Engine::Core::World &world) {
  // Clear current stats but preserve game start times
  std::unordered_map<int, std::chrono::steady_clock::time_point> startTimes;
  for (auto &[ownerId, stats] : m_playerStats) {
    startTimes[ownerId] = stats.gameStartTime;
  }
  
  m_playerStats.clear();

  // Restore start times
  for (auto &[ownerId, startTime] : startTimes) {
    m_playerStats[ownerId].gameStartTime = startTime;
  }

  // Count current troops and barracks
  auto entities = world.getEntitiesWith<Engine::Core::UnitComponent>();
  for (auto *e : entities) {
    auto *unit = e->getComponent<Engine::Core::UnitComponent>();
    if (!unit || unit->health <= 0)
      continue;

    auto &stats = m_playerStats[unit->ownerId];

    if (unit->unitType == "barracks") {
      stats.barracksOwned++;
    } else {
      int individualsPerUnit =
          Game::Units::TroopConfig::instance().getIndividualsPerUnit(
              unit->unitType);
      stats.troopsRecruited += individualsPerUnit;
    }
  }
}

} // namespace Game::Systems
