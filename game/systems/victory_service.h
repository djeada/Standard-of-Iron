#pragma once

#include "game/core/event_manager.h"
#include <QString>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace Engine::Core {
class World;
}

namespace Game::Map {
struct VictoryConfig;
}

namespace Game::Systems {

class GlobalStatsRegistry;
class OwnerRegistry;

class VictoryService {
public:
  enum class VictoryType { Elimination, SurviveTime, Custom };

  enum class DefeatCondition { NoUnits, NoKeyStructures, TimeExpired };

  VictoryService();
  ~VictoryService();

  void configure(const Game::Map::VictoryConfig &config, int localOwnerId);

  void reset();

  void update(Engine::Core::World &world, float deltaTime);

  [[nodiscard]] auto getVictoryState() const -> QString {
    return m_victoryState;
  }

  [[nodiscard]] auto isGameOver() const -> bool {
    return !m_victoryState.isEmpty();
  }

  using VictoryCallback = std::function<void(const QString &state)>;
  void setVictoryCallback(VictoryCallback callback) {
    m_victoryCallback = std::move(callback);
  }

private:
  void onUnitDied(const Engine::Core::UnitDiedEvent &event);
  void onBarrackCaptured(const Engine::Core::BarrackCapturedEvent &event);
  void checkVictoryConditions(Engine::Core::World &world);
  void checkDefeatConditions(Engine::Core::World &world);

  auto checkElimination(Engine::Core::World &world) -> bool;
  [[nodiscard]] auto checkSurviveTime() const -> bool;
  auto checkNoUnits(Engine::Core::World &world) const -> bool;
  auto checkNoKeyStructures(Engine::Core::World &world) -> bool;

  VictoryType m_victoryType = VictoryType::Elimination;
  std::vector<QString> m_keyStructures;
  std::vector<DefeatCondition> m_defeatConditions;

  float m_surviveTimeDuration = 0.0F;
  float m_elapsedTime = 0.0F;
  float m_startupDelay = 0.0F;

  int m_localOwnerId = 1;
  QString m_victoryState;

  VictoryCallback m_victoryCallback;

  Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent>
      m_unitDiedSubscription;
  Engine::Core::ScopedEventSubscription<Engine::Core::BarrackCapturedEvent>
      m_barrackCapturedSubscription;

  Engine::Core::World *m_worldPtr = nullptr;

  Game::Systems::GlobalStatsRegistry &m_stats_registry;
  Game::Systems::OwnerRegistry &m_owner_registry;
};

} // namespace Game::Systems
