#pragma once

#include <QJsonObject>
#include <QObject>
#include <QVariantMap>

#include <memory>

#include "app/mission/tutorial_observation.h"
#include "app/orders/order_feedback.h"
#include "game/mission/mission_wave_runtime.h"

class CampaignManager;
class MinimapManager;

namespace Engine::Core {
class World;
}

namespace Game::Session {
class SessionContext;
}

namespace App::ViewModels {
class PlacementViewModel;
}

namespace App::Mission {

struct TutorialTick {
  Engine::Core::World* world = nullptr;
  Game::Session::SessionContext* session = nullptr;
  const MinimapManager* minimap = nullptr;
  const App::ViewModels::PlacementViewModel* placement = nullptr;
  const Game::Mission::MissionWaveRuntime* waves = nullptr;
  QString victory_state;
  int local_owner_id = 1;
  int enemy_units_defeated = 0;
  bool mission_running = false;
};

class TutorialRuntime {
public:
  explicit TutorialRuntime(QObject* parent);
  ~TutorialRuntime();
  TutorialRuntime(const TutorialRuntime&) = delete;
  auto operator=(const TutorialRuntime&) -> TutorialRuntime& = delete;
  TutorialRuntime(TutorialRuntime&&) = delete;
  auto operator=(TutorialRuntime&&) -> TutorialRuntime& = delete;

  [[nodiscard]] auto director() const -> Game::Mission::TutorialDirector* {
    return m_director.get();
  }
  [[nodiscard]] auto notes() -> TutorialFrameNotes& { return m_notes; }

  [[nodiscard]] auto holds_mission_clock() const -> bool;
  [[nodiscard]] auto serialize() const -> QJsonObject;

  void end_match();
  void activate_if_configured(const CampaignManager* campaign);
  void restore(const CampaignManager* campaign,
               const QJsonObject& state,
               int cleared_wave_count);

  void note_order_outcome(const App::Core::OrderOutcome& outcome);

  void update(float real_dt, const TutorialTick& tick);

private:
  void publish_focus_points(const TutorialTick& tick, const QVariantMap& wave_status);

  std::unique_ptr<Game::Mission::TutorialDirector> m_director;
  TutorialFrameNotes m_notes;
  float m_observe_accumulator = 0.0F;
};

} // namespace App::Mission
