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
  enum class VictoryType {
    Elimination,
    SurviveTime,
    ControlStructures,
    CaptureStructures,
    Custom
  };

  enum class DefeatCondition { NoUnits, NoKeyStructures, TimeExpired };

  VictoryService();
  ~VictoryService();

  void configure(const Game::Map::VictoryConfig &config, int local_owner_id);

  void reset();

  void update(Engine::Core::World &world, float delta_time);

  [[nodiscard]] auto get_victory_state() const -> QString {
    return m_victoryState;
  }

  [[nodiscard]] auto is_game_over() const -> bool {
    return !m_victoryState.isEmpty();
  }

  using VictoryCallback = std::function<void(const QString &state)>;
  void set_victory_callback(VictoryCallback callback) {
    m_victoryCallback = std::move(callback);
  }

private:
  void on_unit_died(const Engine::Core::UnitDiedEvent &event);
  void on_barrack_captured(const Engine::Core::BarrackCapturedEvent &event);
  void check_victory_conditions(Engine::Core::World &world);
  void check_defeat_conditions(Engine::Core::World &world);

  auto check_elimination(Engine::Core::World &world) -> bool;
  [[nodiscard]] auto check_survive_time() const -> bool;
  auto check_control_structures(Engine::Core::World &world,
                                bool require_captured) const -> bool;
  auto check_no_units(Engine::Core::World &world) const -> bool;
  auto check_no_key_structures(Engine::Core::World &world) -> bool;

  VictoryType m_victoryType = VictoryType::Elimination;
  std::vector<QString> m_keyStructures;
  std::vector<DefeatCondition> m_defeatConditions;
  int m_requiredKeyStructures = 0;

  float m_survive_time_duration = 0.0F;
  float m_elapsed_time = 0.0F;
  float m_startup_delay = 0.0F;

  int m_local_owner_id = 1;
  QString m_victoryState;

  VictoryCallback m_victoryCallback;

  Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent>
      m_unit_died_subscription;
  Engine::Core::ScopedEventSubscription<Engine::Core::BarrackCapturedEvent>
      m_barrack_captured_subscription;

  Engine::Core::World *m_worldPtr = nullptr;

  Game::Systems::GlobalStatsRegistry &m_stats_registry;
  Game::Systems::OwnerRegistry &m_owner_registry;
};

} // namespace Game::Systems
