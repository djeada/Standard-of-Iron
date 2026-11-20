#pragma once

#include <QString>
#include <QStringList>

#include <vector>

#include "game/map/campaign_definition.h"

namespace Game::Mission {

// Every `campaign_id/mission_id` reference `--campaign-mission` accepts, in a
// fixed order: campaigns by id, then each campaign's missions by order_index
// (mission id breaks ties). `problems` names every campaign mission that could
// not be turned into a startable reference; `ok()` is false when there is any
// such problem or when no campaign was found at all.
struct CampaignMissionIndex {
  QStringList references;
  QStringList problems;

  [[nodiscard]] auto ok() const -> bool {
    return problems.isEmpty() && !references.isEmpty();
  }
};

// Builds the index from already loaded campaign definitions. A mission is
// listed only if its reference splits into exactly a campaign id and a mission
// id and its mission file loads through the same lookup
// CampaignManager::start_campaign_mission uses.
[[nodiscard]] auto build_campaign_mission_index(
    const std::vector<Game::Campaign::CampaignDefinition>& campaigns)
    -> CampaignMissionIndex;

// Loads the shipped campaigns with the same loader the campaign menu uses and
// builds the index from them.
[[nodiscard]] auto load_campaign_mission_index() -> CampaignMissionIndex;

} // namespace Game::Mission
