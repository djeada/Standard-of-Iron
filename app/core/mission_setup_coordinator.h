#pragma once

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector3D>

#include <optional>
#include <vector>

#include "entity_cache.h"
#include "game/map/mission_definition.h"
#include "game/systems/game_state_serializer.h"
#include "game/systems/nation_id.h"

class CampaignManager;

namespace Engine::Core {
class World;
}

namespace App::Core {

struct PendingMissionWave {
  int owner_id = 0;
  Game::Systems::NationID nation_id = Game::Systems::NationID::RomanRepublic;
  QString ai_id;
  QString label;
  float trigger_time = 0.0F;
  Game::Mission::WaveTriggerMode trigger = Game::Mission::WaveTriggerMode::Time;
  float grace_seconds = Game::Mission::k_default_wave_grace_seconds;
  float warning_seconds = Game::Mission::k_default_wave_warning_seconds;
  std::optional<int> authored_phase;
  int phase_index = 1;
  int phase_count = 1;
  QVector3D entry_world_position{0.0F, 0.0F, 0.0F};
  std::vector<QVector3D> entry_world_positions;
  QVector3D defense_reference_world_position{0.0F, 0.0F, 0.0F};
  std::vector<Game::Mission::WaveComposition> composition;
  Game::Systems::ResourceAmounts clear_reward;
  bool final_wave = false;
  bool spawned = false;
  bool warned = false;
  bool cleared = false;
  float ready_at = -1.0F;
  float cleared_at = -1.0F;
  std::vector<Engine::Core::EntityID> spawned_entity_ids;

  [[nodiscard]] auto entry_positions() const -> std::vector<QVector3D> {
    if (!entry_world_positions.empty()) {
      return entry_world_positions;
    }
    return {entry_world_position};
  }
};

struct PendingMissionEvent {
  float trigger_time = 0.0F;
  QString text;
  bool fired = false;
};

[[nodiscard]] auto
build_pending_mission_events(const Game::Mission::MissionDefinition& mission)
    -> std::vector<PendingMissionEvent>;

struct MissionWaveBuildContext {
  const Game::Mission::MissionDefinition& mission;
  QString mission_difficulty;
  const Game::Systems::LevelSnapshot& level;
  QVector3D defense_reference_world_position{0.0F, 0.0F, 0.0F};
};

[[nodiscard]] auto build_pending_mission_waves(const MissionWaveBuildContext& ctx)
    -> std::vector<PendingMissionWave>;

struct MissionSetupApplyContext {
  Engine::Core::World& world;
  CampaignManager& campaign;
  Game::Systems::LevelSnapshot& level;
  int& selected_player_id;
  int local_owner_id;
  std::vector<PendingMissionWave>& pending_waves;
  EntityCache& entity_cache;
};

struct MissionSetupEffects {
  bool selected_player_changed = false;
  bool center_camera_on_local_forces = false;
  bool troop_count_changed = false;
  bool owner_info_changed = false;
};

struct SkirmishCommanderSetupContext {
  Engine::Core::World& world;
  CampaignManager* campaign;
  const Game::Systems::LevelSnapshot& level;
  int local_owner_id;
};

struct SkirmishCommanderSetupEffects {
  QStringList mission_announcements;
};

struct MissionWaveContext {
  Engine::Core::World& world;
  const Game::Systems::LevelSnapshot& level;
  float campaign_mission_elapsed;
};

struct MissionWaveEffects {
  QStringList mission_announcements;
  std::vector<Engine::Core::EntityID> spawned_entity_ids;
};

[[nodiscard]] auto resolve_defense_reference(Engine::Core::World& world,
                                             int local_owner_id) -> QVector3D;

[[nodiscard]] auto wave_unit_total(const PendingMissionWave& wave) -> int;

[[nodiscard]] auto
wave_incoming_announcement(const PendingMissionWave& wave) -> QString;

[[nodiscard]] auto wave_cleared_announcement(const PendingMissionWave& wave) -> QString;

[[nodiscard]] auto
all_waves_cleared_announcement(const PendingMissionWave& wave) -> QString;

class MissionSetupCoordinator {
public:
  [[nodiscard]] auto
  apply_mission_setup(const MissionSetupApplyContext& ctx) const -> MissionSetupEffects;

  [[nodiscard]] auto apply_skirmish_commander_setup(
      const SkirmishCommanderSetupContext& ctx,
      const QVariantList& player_configs) const -> SkirmishCommanderSetupEffects;

  [[nodiscard]] auto
  spawn_wave(const MissionWaveContext& ctx,
             const PendingMissionWave& wave) const -> MissionWaveEffects;
};

} // namespace App::Core
