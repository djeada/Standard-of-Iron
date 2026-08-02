#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <cstddef>
#include <set>
#include <vector>

#include "game/systems/mission_wave_query.h"
#include "game/systems/resource_types.h"
#include "mission_setup_coordinator.h"

namespace Engine::Core {
class World;
}

namespace App::Core {

class MissionWaveDirector : public Game::Systems::MissionWaveQuery {
public:
  struct Effects {
    QStringList announcements;
    QStringList audio_cues;
    std::vector<std::size_t> waves_to_spawn;
    Game::Systems::ResourceAmounts reward;
    bool status_changed = false;
    bool all_cleared = false;
  };

  void bind(std::vector<PendingMissionWave>* waves, Engine::Core::World* world);
  void reset();

  [[nodiscard]] auto elapsed() const -> float { return m_elapsed; }
  void set_elapsed(float elapsed) { m_elapsed = elapsed; }

  [[nodiscard]] auto advance() -> Effects;
  void note_spawned(std::size_t index,
                    std::vector<Engine::Core::EntityID> spawned_entity_ids);

  [[nodiscard]] auto status() const -> QVariantMap;

  [[nodiscard]] auto serialize() const -> QJsonObject;
  void restore(const QJsonObject& state);

  [[nodiscard]] auto total_wave_count() const -> int override;
  [[nodiscard]] auto cleared_wave_count() const -> int override;

private:
  [[nodiscard]] auto phase_indices() const -> std::vector<int>;
  [[nodiscard]] auto is_phase_cleared(int phase_index) const -> bool;
  [[nodiscard]] auto is_phase_spawned(int phase_index) const -> bool;
  [[nodiscard]] auto live_units_in_phase(int phase_index) const -> int;
  [[nodiscard]] auto wave_is_cleared(const PendingMissionWave& wave) const -> bool;
  [[nodiscard]] auto ready_time_for(std::size_t index) const -> float;
  void refresh_ready_times();

  std::vector<PendingMissionWave>* m_waves = nullptr;
  Engine::Core::World* m_world = nullptr;
  float m_elapsed = 0.0F;
  int m_announced_cleared_phases = 0;
  bool m_announced_all_cleared = false;
  std::set<int> m_settled_phases;
  QVariantMap m_last_status;
};

} // namespace App::Core
