#pragma once

#include <QElapsedTimer>
#include <QVariantList>
#include <QVariantMap>

#include "app/economy/economy_overview.h"
#include "game/systems/resource_types.h"

class CampaignManager;

namespace Engine::Core {
class World;
}

namespace Game::Session {
class SessionContext;
}

namespace App::ViewModels {
class EconomyViewModel;
}

namespace App::Core {

struct EconomySyncInputs {
  Engine::Core::World* world = nullptr;
  Game::Session::SessionContext* session = nullptr;
  const CampaignManager* campaign = nullptr;
  int selected_player_id = 0;
  int local_owner_id = 1;
  int manpower_cap = 0;
  bool spectator_mode = false;
  bool mission_match = false;
  bool loading = false;
};

class EconomyReadModel {
public:
  explicit EconomyReadModel(App::ViewModels::EconomyViewModel* view_model);

  [[nodiscard]] static auto build_player_state(Game::Session::SessionContext& session,
                                               int owner_id,
                                               int manpower_cap) -> QVariantMap;

  [[nodiscard]] static auto build_owner_info(Game::Session::SessionContext& session,
                                             int local_owner_id,
                                             int manpower_cap) -> QVariantList;

  [[nodiscard]] auto selected_player_state() const -> const QVariantMap& {
    return m_selected_player_state;
  }

  [[nodiscard]] auto sync_selected_player_state(Game::Session::SessionContext& session,
                                                int owner_id,
                                                int manpower_cap) -> bool;

  void sync(const EconomySyncInputs& inputs);

  void reset();

private:
  [[nodiscard]] static auto mission_objective_resources(const CampaignManager* campaign)
      -> Game::Systems::ResourceAmounts;

  App::ViewModels::EconomyViewModel* m_view_model;
  QVariantMap m_selected_player_state;
  QVariantList m_resources;
  QVariantMap m_help;
  QVariantMap m_coach;
  EconomyCoachBaseline m_coach_baseline;
  bool m_coach_available = false;
  QElapsedTimer m_refresh_timer;
};

} // namespace App::Core
