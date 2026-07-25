#pragma once

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector3D>

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
  float trigger_time = 0.0F;
  int phase_index = 1;
  int phase_count = 1;
  QVector3D entry_world_position{0.0F, 0.0F, 0.0F};
  QVector3D defense_reference_world_position{0.0F, 0.0F, 0.0F};
  std::vector<Game::Mission::WaveComposition> composition;
  bool spawned = false;
};

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
};

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
