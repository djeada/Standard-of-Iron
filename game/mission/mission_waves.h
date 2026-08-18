#pragma once

#include <QString>
#include <QVector3D>

#include <vector>

#include "game/mission/mission_setup_coordinator.h"

namespace Engine::Core {
class World;
}

namespace Game::Mission {

[[nodiscard]] auto build_pending_mission_waves(const MissionWaveBuildContext& ctx)
    -> std::vector<PendingMissionWave>;

[[nodiscard]] auto
build_pending_mission_events(const Game::Mission::MissionDefinition& mission)
    -> std::vector<PendingMissionEvent>;

[[nodiscard]] auto resolve_defense_reference(Engine::Core::World& world,
                                             int local_owner_id) -> QVector3D;

[[nodiscard]] auto wave_unit_total(const PendingMissionWave& wave) -> int;

[[nodiscard]] auto
wave_incoming_announcement(const PendingMissionWave& wave) -> QString;
[[nodiscard]] auto wave_cleared_announcement(const PendingMissionWave& wave) -> QString;
[[nodiscard]] auto
all_waves_cleared_announcement(const PendingMissionWave& wave) -> QString;

class MissionWaves {
public:
  [[nodiscard]] auto spawn(const MissionWaveContext& ctx,
                           const PendingMissionWave& wave) const -> MissionWaveEffects;
};

} // namespace Game::Mission
