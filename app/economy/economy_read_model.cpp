#include "app/economy/economy_read_model.h"

#include <QColor>
#include <QMetaObject>
#include <QString>

#include <algorithm>
#include <utility>
#include <vector>

#include "app/viewmodels/economy_view_model.h"
#include "game/map/mission_definition.h"
#include "game/mission/campaign_manager.h"
#include "game/session/session_context.h"
#include "game/systems/nation_id.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/player_resource_registry.h"

namespace App::Core {

namespace {

constexpr qint64 k_refresh_interval_ms = 250;

auto build_resource_map(Game::Session::SessionContext& session,
                        int owner_id) -> QVariantMap {
  QVariantMap resources;
  Game::Systems::ResourceAmounts const amounts = session.economy().get_all(owner_id);
  for (Game::Systems::ResourceType const type : Game::Systems::k_all_resource_types) {
    resources[QLatin1String(Game::Systems::resource_type_key(type))] =
        amounts.get(type);
  }
  return resources;
}

} // namespace

EconomyReadModel::EconomyReadModel(App::ViewModels::EconomyViewModel* view_model)
    : m_view_model(view_model) {
  m_refresh_timer.start();
}

auto EconomyReadModel::build_player_state(Game::Session::SessionContext& session,
                                          int owner_id,
                                          int manpower_cap) -> QVariantMap {
  QVariantMap state = manpower_summary_map(
      build_manpower_summary(&session.world(), owner_id, manpower_cap));
  state["owner_id"] = owner_id;
  state["resources"] = build_resource_map(session, owner_id);
  return state;
}

auto EconomyReadModel::build_owner_info(Game::Session::SessionContext& session,
                                        int local_owner_id,
                                        int manpower_cap) -> QVariantList {
  QVariantList result;
  const auto& owner_registry = session.owners();
  const auto& nations = session.nations();
  const auto& owners = owner_registry.get_all_owners();

  for (const auto& owner : owners) {
    QVariantMap owner_map;
    owner_map["id"] = owner.owner_id;
    owner_map["name"] = QString::fromStdString(owner.name);
    owner_map["team_id"] = owner.team_id;

    QString type_str;
    switch (owner.type) {
    case Game::Systems::OwnerType::Player:
      type_str = "Player";
      break;
    case Game::Systems::OwnerType::AI:
      type_str = "AI";
      break;
    case Game::Systems::OwnerType::Neutral:
      type_str = "Neutral";
      break;
    }
    owner_map["type"] = type_str;
    owner_map["isLocal"] = (owner.owner_id == local_owner_id);
    owner_map["color"] =
        QColor::fromRgbF(owner.color[0], owner.color[1], owner.color[2]);

    const auto* owner_nation = nations.get_nation_for_player(owner.owner_id);
    owner_map["nation"] =
        owner_nation != nullptr
            ? QString::fromStdString(
                  Game::Systems::nation_id_to_string(owner_nation->id))
            : QString();
    owner_map["is_contender"] = owner.type != Game::Systems::OwnerType::Neutral &&
                                (owner_nation == nullptr || owner_nation->has_economy);
    owner_map["state"] = build_player_state(session, owner.owner_id, manpower_cap);

    result.append(owner_map);
  }

  return result;
}

auto EconomyReadModel::sync_selected_player_state(
    Game::Session::SessionContext& session, int owner_id, int manpower_cap) -> bool {
  QVariantMap const next_state = build_player_state(session, owner_id, manpower_cap);
  if (m_selected_player_state == next_state) {
    return false;
  }
  m_selected_player_state = next_state;
  return true;
}

auto EconomyReadModel::mission_objective_resources(const CampaignManager* campaign)
    -> Game::Systems::ResourceAmounts {
  Game::Systems::ResourceAmounts required;
  if (campaign == nullptr) {
    return required;
  }
  const auto& mission = campaign->current_mission_definition();
  if (!mission.has_value()) {
    return required;
  }
  const auto note = [&required](const std::vector<Game::Mission::Condition>& list) {
    for (const auto& condition : list) {
      if (!condition.resources.has_value()) {
        continue;
      }
      for (const auto type : Game::Systems::k_all_resource_types) {
        required.set(type,
                     std::max(required.get(type), condition.resources->get(type)));
      }
    }
  };
  note(mission->victory_conditions);
  note(mission->optional_objectives);
  return required;
}

void EconomyReadModel::reset() {
  m_coach_baseline = {};
  m_coach_available = false;
  m_resources.clear();
  m_help.clear();
  m_coach.clear();
  if (m_view_model != nullptr) {
    QMetaObject::invokeMethod(
        m_view_model,
        [view_model = m_view_model]() { view_model->clear(); },
        Qt::QueuedConnection);
  }
}

void EconomyReadModel::sync(const EconomySyncInputs& inputs) {
  if (m_view_model == nullptr || inputs.world == nullptr || inputs.loading) {
    return;
  }
  if (m_refresh_timer.isValid() && m_refresh_timer.elapsed() < k_refresh_interval_ms) {
    return;
  }
  m_refresh_timer.restart();

  int const owner_id =
      inputs.selected_player_id > 0 ? inputs.selected_player_id : inputs.local_owner_id;
  auto& nations = inputs.session->nations();
  const auto* nation = nations.get_nation_for_player(owner_id);
  const EconomyOverviewRequest request{
      .world = inputs.world,
      .nations = &nations,
      .resources = &inputs.session->economy(),
      .owner_id = owner_id,
      .nation_id = nation != nullptr ? nation->id : nations.default_nation_id(),
      .manpower_cap = inputs.manpower_cap,
      .objective_resources = mission_objective_resources(inputs.campaign)};

  const bool coach_available = !inputs.spectator_mode && !inputs.mission_match &&
                               owner_id == inputs.local_owner_id &&
                               (nation == nullptr || nation->has_economy);
  if (coach_available && !m_coach_baseline.captured) {
    m_coach_baseline = capture_economy_coach_baseline(request);
  }

  QVariantList resources = build_resource_overview(request);
  QVariantMap help = build_production_help(request);
  QVariantMap coach = coach_available
                          ? build_economy_coach_state(request, m_coach_baseline)
                          : QVariantMap{};

  const bool resources_changed = resources != m_resources;
  const bool help_changed = help != m_help;
  const bool coach_changed = coach != m_coach;
  const bool availability_changed = coach_available != m_coach_available;
  if (!resources_changed && !help_changed && !coach_changed && !availability_changed) {
    return;
  }
  m_coach_available = coach_available;
  if (resources_changed) {
    m_resources = resources;
  }
  if (help_changed) {
    m_help = help;
  }
  if (coach_changed) {
    m_coach = coach;
  }
  QMetaObject::invokeMethod(
      m_view_model,
      [view_model = m_view_model,
       resources = std::move(resources),
       help = std::move(help),
       coach = std::move(coach),
       resources_changed,
       help_changed,
       coach_changed,
       coach_available]() {
        if (resources_changed) {
          view_model->set_resources(resources);
        }
        if (help_changed) {
          view_model->set_help(help);
        }
        if (coach_changed) {
          view_model->set_coach(coach);
        }
        view_model->set_coach_available(coach_available);
      },
      Qt::QueuedConnection);
}

} // namespace App::Core
