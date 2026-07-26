#pragma once

#include <vector>

#include "game/systems/mission_wave_query.h"
#include "mission_setup_coordinator.h"

namespace Engine::Core {
class World;
}

namespace App::Core {

// Reports mission wave progress to the VictoryService. A wave phase counts as
// cleared once it has spawned and every unit it spawned is gone, which is what
// the `survive_waves` victory condition waits on.
class MissionWaveTracker : public Game::Systems::MissionWaveQuery {
public:
  void bind(const std::vector<PendingMissionWave>* waves, Engine::Core::World* world) {
    m_waves = waves;
    m_world = world;
  }

  [[nodiscard]] auto total_wave_count() const -> int override;
  [[nodiscard]] auto cleared_wave_count() const -> int override;

private:
  const std::vector<PendingMissionWave>* m_waves = nullptr;
  Engine::Core::World* m_world = nullptr;
};

} // namespace App::Core
