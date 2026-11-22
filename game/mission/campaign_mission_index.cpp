#include "game/mission/campaign_mission_index.h"

#include <QChar>

#include <algorithm>

#include "game/map/mission_definition.h"
#include "game/map/mission_loader.h"
#include "game/mission/campaign_manager.h"
#include "game/systems/persistence/save_campaign_catalog.h"

namespace Game::Mission {

namespace {

auto is_reference_part(const QString& part) -> bool {
  return !part.isEmpty() && !part.contains(QLatin1Char('/'));
}

} // namespace

auto build_campaign_mission_index(const std::vector<Game::Campaign::CampaignDefinition>&
                                      campaigns) -> CampaignMissionIndex {
  CampaignMissionIndex index;
  if (campaigns.empty()) {
    index.problems.append(QStringLiteral("no campaign definitions could be loaded"));
    return index;
  }

  std::vector<const Game::Campaign::CampaignDefinition*> ordered;
  ordered.reserve(campaigns.size());
  for (const auto& campaign : campaigns) {
    ordered.push_back(&campaign);
  }
  std::stable_sort(ordered.begin(),
                   ordered.end(),
                   [](const auto* lhs, const auto* rhs) { return lhs->id < rhs->id; });

  for (const auto* campaign : ordered) {
    if (!is_reference_part(campaign->id)) {
      index.problems.append(
          QStringLiteral("campaign id '%1' cannot be used in a campaign_id/mission_id "
                         "reference")
              .arg(campaign->id));
      continue;
    }
    if (campaign->missions.empty()) {
      index.problems.append(
          QStringLiteral("campaign '%1' lists no missions").arg(campaign->id));
      continue;
    }

    std::vector<const Game::Campaign::CampaignMission*> missions;
    missions.reserve(campaign->missions.size());
    for (const auto& mission : campaign->missions) {
      missions.push_back(&mission);
    }
    std::stable_sort(
        missions.begin(), missions.end(), [](const auto* lhs, const auto* rhs) {
          if (lhs->order_index != rhs->order_index) {
            return lhs->order_index < rhs->order_index;
          }
          return lhs->mission_id < rhs->mission_id;
        });

    for (const auto* mission : missions) {
      if (!is_reference_part(mission->mission_id)) {
        index.problems.append(
            QStringLiteral("campaign '%1' has a mission with an unusable id '%2'")
                .arg(campaign->id, mission->mission_id));
        continue;
      }
      const QString reference =
          QStringLiteral("%1/%2").arg(campaign->id, mission->mission_id);
      if (index.references.contains(reference)) {
        continue;
      }
      const QString mission_file =
          CampaignManager::resolve_mission_file(mission->mission_id);
      MissionDefinition definition;
      QString error;
      if (!MissionLoader::load_from_json_file(mission_file, definition, &error)) {
        index.problems.append(
            QStringLiteral("%1: mission file %2 could not be loaded: %3")
                .arg(reference, mission_file, error));
        continue;
      }
      index.references.append(reference);
    }
  }

  return index;
}

auto load_campaign_mission_index() -> CampaignMissionIndex {
  return build_campaign_mission_index(
      Game::Systems::SaveCampaignCatalog::load_campaign_definitions());
}

} // namespace Game::Mission
