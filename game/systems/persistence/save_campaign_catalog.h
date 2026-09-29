#pragma once

#include <QVariantList>
#include <QVariantMap>

#include <vector>

#include "map/campaign_definition.h"

namespace Game::Systems::SaveCampaignCatalog {

auto load_campaign_definitions() -> std::vector<Game::Campaign::CampaignDefinition>;

auto build_campaign_entry(const Game::Campaign::CampaignDefinition& campaign,
                          const QVariantList& missions_progress) -> QVariantMap;

} // namespace Game::Systems::SaveCampaignCatalog
