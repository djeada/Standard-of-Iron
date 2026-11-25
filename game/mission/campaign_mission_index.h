#pragma once

#include <QString>
#include <QStringList>

#include <vector>

#include "game/map/campaign_definition.h"

namespace Game::Mission {

struct CampaignMissionIndex {
  QStringList references;
  QStringList problems;

  [[nodiscard]] auto ok() const -> bool {
    return problems.isEmpty() && !references.isEmpty();
  }
};

[[nodiscard]] auto build_campaign_mission_index(
    const std::vector<Game::Campaign::CampaignDefinition>& campaigns)
    -> CampaignMissionIndex;

[[nodiscard]] auto load_campaign_mission_index() -> CampaignMissionIndex;

} // namespace Game::Mission
