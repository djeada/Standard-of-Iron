#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <functional>
#include <memory>
#include <optional>

#include "game/map/mission_stage_tracker.h"
#include "game/mission/difficulty_profile.h"
#include "game/mission/mission_setup_coordinator.h"
#include "game/mission/mission_wave_runtime.h"

class CampaignManager;
class MinimapManager;

namespace Engine::Core {
class World;
}

namespace Game::Session {
class SessionContext;
}

namespace Game::Systems {
struct LevelSnapshot;
class VictoryService;
} // namespace Game::Systems

namespace App::ViewModels {
class MissionViewModel;
class WaveViewModel;
} // namespace App::ViewModels

namespace App::Mission {

struct MissionBinding {
  Engine::Core::World* world = nullptr;
  Game::Session::SessionContext* session = nullptr;
  CampaignManager* campaign = nullptr;
  Game::Systems::LevelSnapshot* level = nullptr;
  Game::Systems::VictoryService* victory_service = nullptr;
  const MinimapManager* minimap = nullptr;
  int local_owner_id = 1;
};

struct WaveFrameResult {
  bool reward_granted = false;
  bool owner_info_changed = false;
};

class MissionRuntime {
public:
  using Announce = std::function<void(const QString& text)>;

  MissionRuntime(App::ViewModels::MissionViewModel* mission_view_model,
                 App::ViewModels::WaveViewModel* wave_view_model,
                 Announce announce);

  [[nodiscard]] static auto resolve_difficulty(const CampaignManager* campaign,
                                               const QVariantList& player_configs)
      -> Game::Mission::MatchDifficulty;

  [[nodiscard]] auto difficulty() const -> const Game::Mission::MatchDifficulty& {
    return m_difficulty;
  }
  void set_difficulty(const Game::Mission::MatchDifficulty& difficulty) {
    m_difficulty = difficulty;
  }

  [[nodiscard]] auto waves() -> Game::Mission::MissionWaveRuntime& { return m_waves; }
  [[nodiscard]] auto waves() const -> const Game::Mission::MissionWaveRuntime& {
    return m_waves;
  }
  [[nodiscard]] auto stages() const -> const Game::Mission::MissionStageTracker& {
    return m_stages;
  }
  [[nodiscard]] auto serialize_stages() const -> QJsonObject {
    return m_stages.serialize();
  }

  void reset();

  [[nodiscard]] auto bind_setup(const MissionBinding& binding, int& selected_player_id)
      -> std::optional<Game::Mission::MissionSetupEffects>;
  void apply_skirmish_commander_setup(const MissionBinding& binding,
                                      const QVariantList& player_configs);

  [[nodiscard]] auto configure_stages(const MissionBinding& binding) -> bool;
  void restore_stages(const MissionBinding& binding, const QJsonObject& stage_state);
  void restore_waves(const MissionBinding& binding, const QJsonObject& wave_state);

  [[nodiscard]] auto
  advance_waves(const MissionBinding& binding,
                float dt,
                bool tutorial_holds_clock,
                const std::function<void()>& on_reward_granted) -> WaveFrameResult;
  void advance_stages(const MissionBinding& binding, float dt);

  void publish_wave_status(const MissionBinding& binding);
  void publish_stages(const MissionBinding& binding);
  void publish_deadline(const MissionBinding& binding);

  void queue_announcement(const QString& text);
  void flush_announcements(float dt);

private:
  void publish_optional_objectives(const MissionBinding& binding);
  void publish_victory_objectives(const MissionBinding& binding);
  [[nodiscard]] auto
  wave_binding(const MissionBinding& binding) -> Game::Mission::MissionWaveBinding;
  [[nodiscard]] auto stage_facts() -> Game::Mission::StageWorldFacts;

  App::ViewModels::MissionViewModel* m_mission_view_model;
  App::ViewModels::WaveViewModel* m_wave_view_model;
  Announce m_announce;
  std::unique_ptr<Game::Mission::MissionSetupCoordinator> m_setup;
  Game::Mission::MatchDifficulty m_difficulty;
  Game::Mission::MissionStageTracker m_stages;
  Game::Mission::MissionWaveRuntime m_waves;
  float m_stage_poll_accumulator = 0.0F;
  QStringList m_pending_announcements;
  float m_announcement_cooldown = 0.0F;
};

} // namespace App::Mission
