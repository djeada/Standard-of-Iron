#pragma once

#include <QJsonObject>
#include <QStringList>
#include <QVariantMap>
#include <QVector3D>

#include <vector>

#include "game/mission/mission_setup_coordinator.h"
#include "game/mission/mission_wave_director.h"
#include "game/mission/mission_waves.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems {
struct LevelSnapshot;
class VictoryService;
} // namespace Game::Systems

class CampaignManager;

namespace App::Mission {

struct MissionFrameEffects {
  QStringList announcements;
  QStringList audio_cues;
  Game::Systems::ResourceAmounts reward;
  bool reward_granted = false;
  bool wave_status_changed = false;
  bool owner_info_changed = false;
};

struct MissionWaveBinding {
  Engine::Core::World* world = nullptr;
  const Game::Systems::LevelSnapshot* level = nullptr;
  CampaignManager* campaign = nullptr;
  Game::Systems::VictoryService* victory_service = nullptr;
  int local_owner_id = 1;
};

class MissionWaveRuntime {
public:
  [[nodiscard]] auto elapsed() const -> float { return m_elapsed; }
  [[nodiscard]] auto director() -> Game::Mission::MissionWaveDirector& {
    return m_director;
  }
  [[nodiscard]] auto status() const -> QVariantMap { return m_director.status(); }
  [[nodiscard]] auto has_waves() const -> bool { return !m_waves.empty(); }

  void reset();

  void bind_after_setup(const MissionWaveBinding& binding,
                        std::vector<Game::Mission::PendingMissionWave> waves,
                        std::vector<Game::Mission::PendingMissionEvent> events);

  void restore(const MissionWaveBinding& binding, const QJsonObject& wave_state);

  [[nodiscard]] auto advance(const MissionWaveBinding& binding,
                             float delta_seconds,
                             bool hold_clock) -> MissionFrameEffects;

private:
  [[nodiscard]] auto fire_due_events() -> QStringList;

  float m_elapsed = 0.0F;
  std::vector<Game::Mission::PendingMissionWave> m_waves;
  std::vector<Game::Mission::PendingMissionEvent> m_events;
  Game::Mission::MissionWaveDirector m_director;
  Game::Mission::MissionWaves m_spawner;
};

} // namespace App::Mission
