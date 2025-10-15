#pragma once

#include "game/core/event_manager.h"
#include <QString>
#include <functional>
#include <memory>
#include <vector>

namespace Engine {
namespace Core {
class World;
}
} // namespace Engine

namespace Game {
namespace Map {
struct VictoryConfig;
}
} // namespace Game

namespace Game {
namespace Systems {

class VictoryService {
public:
  enum class VictoryType { Elimination, SurviveTime, Custom };

  enum class DefeatCondition { NoUnits, NoKeyStructures, TimeExpired };

  VictoryService();
  ~VictoryService();

  void configure(const Game::Map::VictoryConfig &config, int localOwnerId);

  void update(Engine::Core::World &world, float deltaTime);

  QString getVictoryState() const { return m_victoryState; }

  bool isGameOver() const { return !m_victoryState.isEmpty(); }

  using VictoryCallback = std::function<void(const QString &state)>;
  void setVictoryCallback(VictoryCallback callback) {
    m_victoryCallback = callback;
  }

private:
  void onUnitDied(const Engine::Core::UnitDiedEvent &event);
  void onBarrackCaptured(const Engine::Core::BarrackCapturedEvent &event);
  void checkVictoryConditions(Engine::Core::World &world);
  void checkDefeatConditions(Engine::Core::World &world);

  bool checkElimination(Engine::Core::World &world);
  bool checkSurviveTime();
  bool checkNoUnits(Engine::Core::World &world);
  bool checkNoKeyStructures(Engine::Core::World &world);

  VictoryType m_victoryType = VictoryType::Elimination;
  std::vector<QString> m_keyStructures;
  std::vector<DefeatCondition> m_defeatConditions;

  float m_surviveTimeDuration = 0.0f;
  float m_elapsedTime = 0.0f;

  int m_localOwnerId = 1;
  QString m_victoryState;

  VictoryCallback m_victoryCallback;

  Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent>
      m_unitDiedSubscription;
  Engine::Core::ScopedEventSubscription<Engine::Core::BarrackCapturedEvent>
      m_barrackCapturedSubscription;

  Engine::Core::World *m_worldPtr = nullptr;
};

} // namespace Systems
} // namespace Game
